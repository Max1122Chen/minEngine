#!/usr/bin/env python3
"""Audit PE exports of the minEngine DLL against the imports of its consumers.

Three uses (see docs/ai/Platform/ProductBuild/BUILD-F02_S01_EXPORT_WHITELIST_IMPLEMENTATION.md):

  --report                         human/JSON report: export count, ordinal headroom, composition
  --max-exports N --fail           assertion for verify.ps1 / CI (exit 1 on violation)
  --emit-whitelist PATH            generate the .def whitelist from CONSUMER IMPORT TABLES
  --verify-coverage                per-consumer "exported >= consumed" check (missing == 0)

The whitelist is built from what consumers actually import, never from the DLL export
table: dumping the current export table into a .def does not lower the entry count and
therefore does not remove the 65535-ordinal bomb.
"""

import argparse
import collections
import json
import os
import re
import struct
import sys

DEFAULT_DLL = os.path.join("minEngine", "bin", "libminEngined.dll")
DEFAULT_CONSUMER_DIR = os.path.join("minEngine", "bin")
DEFAULT_BUDGET = 3000
DEFAULT_MIN_HEADROOM = 60000
# Stale artifacts kept out of the contract by default (see design P3): Maximum.exe is the
# retired Playground build and imports symbols the engine no longer defines.
DEFAULT_EXCLUDE_CONSUMERS = ("Maximum.exe", "libminEngine.dll", "libminEngined.dll")
PE_SUFFIXES = (".exe", ".dll")

IMAGE_DIRECTORY_ENTRY_EXPORT = 0
IMAGE_DIRECTORY_ENTRY_IMPORT = 1


class PEError(Exception):
    pass


class PEImage:
    """Minimal PE32/PE32+ reader: sections + data directories."""

    def __init__(self, path):
        self.path = path
        with open(path, "rb") as handle:
            self.data = handle.read()
        data = self.data
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise PEError("%s: not a PE image (no MZ header)" % path)
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        if data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
            raise PEError("%s: not a PE image (no PE signature)" % path)
        coff = e_lfanew + 4
        self.machine, self.number_of_sections = struct.unpack_from("<HH", data, coff)
        size_of_optional_header = struct.unpack_from("<H", data, coff + 16)[0]
        self.optional = coff + 20
        magic = struct.unpack_from("<H", data, self.optional)[0]
        if magic == 0x20B:
            self.is_pe32_plus = True
            self.image_base = struct.unpack_from("<Q", data, self.optional + 24)[0]
            dirs = self.optional + 112
        elif magic == 0x10B:
            self.is_pe32_plus = False
            self.image_base = struct.unpack_from("<I", data, self.optional + 28)[0]
            dirs = self.optional + 96
        else:
            raise PEError("%s: unknown optional header magic 0x%X" % (path, magic))
        self.size_of_headers = struct.unpack_from("<I", data, self.optional + 60)[0]
        self.dirs = []
        for index in range(16):
            self.dirs.append(struct.unpack_from("<II", data, dirs + 8 * index))
        self.sections = []
        table = self.optional + size_of_optional_header
        for index in range(self.number_of_sections):
            off = table + 40 * index
            virtual_size, virtual_address, raw_size, raw_ptr = struct.unpack_from(
                "<IIII", data, off + 8)
            characteristics = struct.unpack_from("<I", data, off + 36)[0]
            self.sections.append(
                (virtual_address, virtual_size, raw_ptr, raw_size, characteristics))

    def directory(self, index):
        return self.dirs[index]

    def read_c_string(self, rva):
        offset = self.rva_to_offset(rva)
        end = self.data.index(b"\0", offset)
        return self.data[offset:end].decode("ascii", "replace")

    def is_executable_rva(self, rva):
        for virtual_address, virtual_size, _raw_ptr, raw_size, characteristics in self.sections:
            span = max(virtual_size, raw_size)
            if virtual_address <= rva < virtual_address + span:
                return bool(characteristics & 0x20000000)  # IMAGE_SCN_MEM_EXECUTE
        return False

    def rva_to_offset(self, rva):
        for virtual_address, virtual_size, raw_ptr, raw_size, _characteristics in self.sections:
            span = max(virtual_size, raw_size)
            if virtual_address <= rva < virtual_address + span:
                delta = rva - virtual_address
                if delta >= raw_size:
                    raise PEError("%s: RVA 0x%X is not backed by file data" % (self.path, rva))
                return raw_ptr + delta
        if rva < self.size_of_headers:
            return rva
        raise PEError("%s: RVA 0x%X maps to no section" % (self.path, rva))


class ExportTable:
    def __init__(self, path):
        self.path = path
        image = PEImage(path)
        rva, size = image.directory(IMAGE_DIRECTORY_ENTRY_EXPORT)
        self.directory_rva = rva
        self.directory_size = size
        self.present = rva != 0
        self.ordinal_base = 0
        self.number_of_functions = 0
        self.number_of_names = 0
        self.names = []          # [(ordinal, name)] sorted by ordinal
        self.kinds = {}          # name -> "data" | "code" (PE section of the export RVA)
        self.max_ordinal = 0
        if not self.present:
            return
        off = image.rva_to_offset(rva)
        data = image.data
        self.ordinal_base = struct.unpack_from("<I", data, off + 16)[0]
        self.number_of_functions, self.number_of_names = struct.unpack_from("<II", data, off + 20)
        address_of_functions, address_of_names, address_of_ordinals = struct.unpack_from(
            "<III", data, off + 28)
        function_rvas = [
            struct.unpack_from("<I", data, image.rva_to_offset(address_of_functions) + 4 * i)[0]
            for i in range(self.number_of_functions)
        ]
        for index in range(self.number_of_names):
            name_rva = struct.unpack_from("<I", data, image.rva_to_offset(address_of_names) + 4 * index)[0]
            ordinal = struct.unpack_from(
                "<H", data, image.rva_to_offset(address_of_ordinals) + 2 * index)[0]
            name = image.read_c_string(name_rva)
            self.names.append((self.ordinal_base + ordinal, name))
            rva = function_rvas[ordinal] if ordinal < len(function_rvas) else 0
            self.kinds[name] = "code" if rva and image.is_executable_rva(rva) else "data"
        # Ordinals of forwarder-only or unnamed entries still occupy the function table.
        used = [self.ordinal_base + i for i, value in enumerate(function_rvas) if value != 0]
        self.max_ordinal = max(used) if used else 0

    @property
    def headroom(self):
        return 65535 - self.max_ordinal

    def name_set(self):
        return {name for _, name in self.names}


class ImportTable:
    def __init__(self, path):
        self.path = path
        image = PEImage(path)
        rva, _size = image.directory(IMAGE_DIRECTORY_ENTRY_IMPORT)
        self.present = rva != 0
        self.per_dll = collections.OrderedDict()
        self.ordinal_imports = collections.OrderedDict()
        if not self.present:
            return
        data = image.data
        descriptor = image.rva_to_offset(rva)
        thunk_size = 8 if image.is_pe32_plus else 4
        ordinal_flag = 1 << (63 if image.is_pe32_plus else 31)
        while True:
            original_thunk, _stamp, _chain, name_rva, _first_thunk = struct.unpack_from(
                "<IIIII", data, descriptor)
            if original_thunk == 0 and name_rva == 0:
                break
            dll_name = image.read_c_string(name_rva).lower()
            names = []
            ordinals = []
            walk = original_thunk or _first_thunk
            while walk:
                entry = struct.unpack_from(
                    "<Q" if image.is_pe32_plus else "<I", data, image.rva_to_offset(walk))[0]
                if entry == 0:
                    break
                if entry & ordinal_flag:
                    ordinals.append(entry & 0xFFFF)
                else:
                    names.append(image.read_c_string((entry & 0x7FFFFFFF) + 2))
                walk += thunk_size
            if names:
                self.per_dll.setdefault(dll_name, []).extend(names)
            if ordinals:
                self.ordinal_imports.setdefault(dll_name, []).extend(ordinals)
            descriptor += 20

    def names_for(self, dll_name):
        return self.per_dll.get(dll_name.lower(), [])


# --- composition classification -----------------------------------------------------
# Rules mirror design P2 but are expressed on the mangled name only (no c++filt dependency),
# so the same buckets can be compared before/after the whitelist switch.
_RE_ENGINE = re.compile(r"^_ZNK?9minEngine")
_RE_MINENGINE_ANY = re.compile(r"9minEngine")
_RE_STD = re.compile(r"^_ZNK?St|^_ZSt|^__cxa|^_ZdlPv|^_Znwm|^_ZdlPvm|^_ZdaPv")
_RE_IMGUI = re.compile(r"ImGui|ImGizmo|imgui|ImDrawList|ImFont|ImVec2|ImVec4|ImTexture|ImVector|ImDrawCmd|ImPool")
_RE_GLAD = re.compile(r"^glad_|gladLoad|GLAD_")
_RE_GLFW = re.compile(r"^glfw")
_RE_JOLT = re.compile(r"^_ZN(K)?3JPH|JPH")
_RE_STB = re.compile(r"^stbi_")
_RE_FMT = re.compile(r"^_ZNK?3fmt|^_ZN(K)?3fmt|fmt::|(^|_)v?format_")
_RE_SPDLOG = re.compile(r"^(spdlog|_ZN(K)?6spdlog)")
_RE_NLOHMANN = re.compile(r"nlohmann|^_ZN(K)?8nlohmann")
_RE_SOL2 = re.compile(r"^_ZNK?3sol|^_ZN(K)?3sol")
_RE_SYSTEM = re.compile(r"^(?!_).*(_GUID$|_Property$|_IID$)")
_RE_THIRD_C = re.compile(r"^(?:lua_|nfd_|ad_|efsw_|ma_|FT_|png_|zlib|inflate|deflate)")

BUCKETS = ("minEngine", "libstdcxx", "imgui", "glad", "glfw", "jolt", "stb", "fmt/spdlog",
           "nlohmann/sol2", "system-guid", "thirdparty-c", "other")


def classify(name):
    if _RE_ENGINE.match(name):
        return "minEngine"
    if _RE_STD.match(name):
        return "libstdcxx"
    if _RE_IMGUI.search(name):
        return "imgui"
    if _RE_GLAD.search(name):
        return "glad"
    if _RE_GLFW.search(name):
        return "glfw"
    if _RE_JOLT.search(name):
        return "jolt"
    if _RE_STB.match(name):
        return "stb"
    if _RE_SPDLOG.match(name):
        return "fmt/spdlog"
    if _RE_FMT.match(name):
        return "fmt/spdlog"
    if _RE_NLOHMANN.match(name) or name.startswith("nlohmann"):
        return "nlohmann/sol2"
    if _RE_SOL2.match(name):
        return "nlohmann/sol2"
    if _RE_SYSTEM.match(name):
        return "system-guid"
    if _RE_THIRD_C.match(name):
        return "thirdparty-c"
    if _RE_MINENGINE_ANY.search(name):
        return "minEngine"
    return "other"


def composition(names):
    counts = collections.Counter(classify(name) for name in names)
    return {bucket: counts.get(bucket, 0) for bucket in BUCKETS if counts.get(bucket, 0)}


# --- consumer discovery -------------------------------------------------------------
def find_consumers(dll_path, consumer_dirs, explicit, exclude_names, include_names,
                   export_names):
    """Return [(path, ImportTable)] for every PE that imports the target DLL."""
    target = os.path.basename(dll_path).lower()
    candidates = []
    for path in explicit:
        candidates.append(os.path.abspath(path))
    for directory in consumer_dirs:
        if not os.path.isdir(directory):
            continue
        for entry in sorted(os.listdir(directory)):
            full = os.path.join(directory, entry)
            if os.path.isfile(full) and entry.lower().endswith(PE_SUFFIXES):
                candidates.append(os.path.abspath(full))
    seen = set()
    consumers = []
    for path in candidates:
        key = os.path.normcase(path)
        if key in seen:
            continue
        seen.add(key)
        base = os.path.basename(path)
        if base in exclude_names and base not in include_names:
            continue
        try:
            table = ImportTable(path)
        except PEError as exc:
            print("warning: skipping %s (%s)" % (path, exc), file=sys.stderr)
            continue
        names = table.names_for(target)
        if names:
            consumers.append((path, table))
    return consumers


def load_export_names(dll_path):
    return ExportTable(dll_path)


def consumed_union(consumers, target):
    union = set()
    for _path, table in consumers:
        union.update(table.names_for(target))
    return union


# --- reporting ----------------------------------------------------------------------
def report(dll_path, exports, consumers, as_json):
    target = os.path.basename(dll_path).lower()
    names = exports.name_set()
    union = consumed_union(consumers, target)
    payload = {
        "dll": os.path.abspath(dll_path),
        "ordinal_base": exports.ordinal_base,
        "number_of_functions": exports.number_of_functions,
        "number_of_names": exports.number_of_names,
        "max_named_ordinal": exports.max_ordinal,
        "pe_limit": 65535,
        "headroom": exports.headroom,
        "export_directory_bytes": exports.directory_size,
        "composition": composition(names),
        "consumers": {},
        "consumed_union": len(union),
        "consumed_missing": sorted(union - names),
    }
    for path, table in consumers:
        imported = table.names_for(target)
        missing = sorted(set(imported) - names)
        payload["consumers"][os.path.basename(path)] = {
            "path": os.path.abspath(path),
            "imports": len(imported),
            "unique_imports": len(set(imported)),
            "missing": missing,
        }
    if as_json:
        return payload
    print("DLL ............ %s" % payload["dll"])
    print("OrdinalBase .... %d" % exports.ordinal_base)
    print("NumberOfFunctions ... %d   (NumberOfNames %d)" % (
        exports.number_of_functions, exports.number_of_names))
    print("maxNamedOrdinal ... %d   PE limit 65535   headroom %d" % (
        exports.max_ordinal, exports.headroom))
    print("export dir bytes ... %d" % exports.directory_size)
    print("composition:")
    for bucket, count in payload["composition"].items():
        print("  %-16s %6d" % (bucket, count))
    print("consumers importing %s: %d" % (target, len(consumers)))
    for name, info in payload["consumers"].items():
        print("  %-26s imports %5d (unique %5d)  missing %d" % (
            name, info["imports"], info["unique_imports"], len(info["missing"])))
        for missing in info["missing"][:20]:
            print("        missing: %s" % missing)
        if len(info["missing"]) > 20:
            print("        ... %d more" % (len(info["missing"]) - 20))
    print("consumed union ... %d   (imported but not exported: %d)" % (
        payload["consumed_union"], len(payload["consumed_missing"])))
    return payload


def check_assertions(exports, budget, min_headroom):
    failures = []
    if not exports.present:
        failures.append("DLL has no PE export directory")
    if exports.number_of_functions > budget:
        failures.append("NumberOfFunctions %d > budget %d" % (exports.number_of_functions, budget))
    if exports.headroom < min_headroom:
        failures.append("ordinal headroom %d < required %d" % (exports.headroom, min_headroom))
    return failures


def write_whitelist(path, dll_path, exports, consumers, extra_defs):
    target = os.path.basename(dll_path).lower()
    existing = exports.name_set()
    union = consumed_union(consumers, target)
    extra = set()
    for extra_path in extra_defs:
        with open(extra_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line or line.startswith(";") or line.upper() == "EXPORTS":
                    continue
                extra.add(line.split()[0])
    wanted = union | extra
    keep = sorted(name for name in wanted if name in existing)
    # A .def data export becomes a code thunk ("T <name>" in .text) inside the generated
    # import library, which collides with the COMDAT copy of the same vtable/typeinfo that
    # consumers emit themselves ("multiple definition of 'vtable for ...'"). The DATA
    # keyword makes ld emit a plain import stub (__imp_/__nm_) instead.
    data_names = {name for name in keep if exports.kinds.get(name) == "data"}
    dropped = sorted(name for name in wanted - existing if name not in extra)
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    lines = [
        "; minEngine export whitelist - GENERATED, do not hand-edit one line at a time.",
        "; Source: PE import tables of the engine consumers, not the DLL export table.",
        "; Regenerate:",
        ";   python scripts/audit_dll_exports.py --emit-whitelist minEngine/cmake/minEngine_exports.def",
        "; Consumers:",
    ]
    for consumer_path, table in consumers:
        count = len(set(table.names_for(target)))
        lines.append(";   %-28s %5d names" % (os.path.basename(consumer_path), count))
    lines.extend([
        "; Entries: %d (consumed union %d, DLL exports %d; %d marked DATA)" % (
            len(keep), len(union), len(existing), len(data_names)),
        "EXPORTS",
    ])
    for name in keep:
        lines.append(name + " DATA" if name in data_names else name)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    print("wrote %s: %d entries (consumed union %d)" % (path, len(keep), len(union)))
    if dropped:
        print("warning: %d imported names are not exported by %s (stale consumer):" % (
            len(dropped), dll_path))
        for name in dropped[:40]:
            print("  %s" % name)
    return keep, dropped


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Audit minEngine DLL exports against consumer imports.",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dll", default=None,
                        help="target DLL (default: %s relative to the repo root)" % DEFAULT_DLL)
    parser.add_argument("--report", action="store_true", help="print the export report")
    parser.add_argument("--json", default=None, help="write the report as JSON to this path")
    parser.add_argument("--max-exports", type=int, default=DEFAULT_BUDGET,
                        help="export entry budget (default %d)" % DEFAULT_BUDGET)
    parser.add_argument("--min-headroom", type=int, default=DEFAULT_MIN_HEADROOM,
                        help="required 65535-maxOrdinal headroom (default %d)" % DEFAULT_MIN_HEADROOM)
    parser.add_argument("--fail", action="store_true",
                        help="exit non-zero when the export budget/headroom is violated")
    parser.add_argument("--emit-whitelist", default=None,
                        help="write a .def whitelist generated from consumer imports")
    parser.add_argument("--emit-export-dump", default=None,
                        help="write the raw DLL export list to a .def (audit artifact only)")
    parser.add_argument("--verify-coverage", action="store_true",
                        help="assert exported >= consumed for every consumer")
    parser.add_argument("--consumer", action="append", default=[],
                        help="extra consumer binary (repeatable)")
    parser.add_argument("--consumer-dir", action="append", default=[],
                        help="extra directory scanned for consumer binaries (repeatable)")
    parser.add_argument("--include-consumer", action="append", default=[],
                        help="scan this consumer even if it is in the default exclusion list")
    parser.add_argument("--exclude-consumer", action="append", default=[],
                        help="skip this consumer file name (repeatable)")
    parser.add_argument("--extra-def", action="append", default=[],
                        help="merge hand-maintained export names from this .def (repeatable)")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    dll_path = args.dll
    if dll_path is None:
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        dll_path = os.path.join(repo_root, DEFAULT_DLL)
    if not os.path.isfile(dll_path):
        print("error: DLL not found: %s" % dll_path, file=sys.stderr)
        return 2

    exports = ExportTable(dll_path)
    # Consumer import tables are only needed for reports/whitelists; the --fail assertion
    # must stay cheap enough to run as a post-build guard on every link.
    need_consumers = bool(args.report or args.json or args.emit_whitelist
                          or args.verify_coverage)
    consumers = []
    if need_consumers:
        consumer_dirs = args.consumer_dir or [os.path.dirname(os.path.abspath(dll_path))]
        exclude = set(DEFAULT_EXCLUDE_CONSUMERS) | set(args.exclude_consumer)
        consumers = find_consumers(
            dll_path, consumer_dirs, args.consumer, exclude,
            set(args.include_consumer), exports.name_set())

    if args.emit_export_dump:
        names = [name for _, name in exports.names]
        with open(args.emit_export_dump, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("; audit-only dump of %s (%d names)\nEXPORTS\n" % (
                os.path.basename(dll_path), len(names)))
            handle.write("\n".join(names) + "\n")
        print("wrote %s: %d entries (audit artifact, NOT an export contract)" % (
            args.emit_export_dump, len(names)))

    if args.emit_whitelist:
        write_whitelist(args.emit_whitelist, dll_path, exports, consumers, args.extra_def)

    payload = None
    if args.report or args.json:
        payload = report(dll_path, exports, consumers, args.json is not None)
    if args.json:
        with open(args.json, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(payload, handle, indent=2)
            handle.write("\n")
        if not args.quiet:
            print("wrote %s" % args.json)

    exit_code = 0
    if args.verify_coverage:
        bad = 0
        target = os.path.basename(dll_path).lower()
        exported = exports.name_set()
        print("coverage (exported >= consumed):")
        for path, table in consumers:
            imported = set(table.names_for(target))
            missing = imported - exported
            bad += len(missing)
            print("  %-26s imported %5d  missing %d" % (os.path.basename(path), len(imported), len(missing)))
            for name in sorted(missing)[:20]:
                print("      missing: %s" % name)
        print("coverage total missing = %d" % bad)
        if bad:
            exit_code = 1
    if args.fail:
        failures = check_assertions(exports, args.max_exports, args.min_headroom)
        if failures:
            for failure in failures:
                print("FAIL: %s" % failure, file=sys.stderr)
            exit_code = 1
        else:
            print("PASS: NumberOfFunctions=%d <= %d, ordinal headroom=%d >= %d" % (
                exports.number_of_functions, args.max_exports,
                exports.headroom, args.min_headroom))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
