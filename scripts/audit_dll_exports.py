#!/usr/bin/env python3
"""Audit PE exports of the minEngine DLL against the imports of its consumers.

Three uses (see docs/ai/Platform/ProductBuild/BUILD-F02_S01_EXPORT_WHITELIST_IMPLEMENTATION.md):

  --report                         human/JSON report: export count, ordinal headroom, composition
  --max-exports N --fail           assertion for verify.ps1 / CI (exit 1 on violation)
  --emit-whitelist PATH            generate the .def whitelist from CONSUMER IMPORT TABLES
  --verify-coverage                per-consumer + union "exported >= consumed" check
                                   (missing == 0). Pair it with --consumer-dir to audit LIVE
                                   external consumers (BUILD-F03 Q8); it exits non-zero when
                                   any consumer import is missing from the exports, and also
                                   when the requested scan found no consumer at all - a
                                   vacuous "0 missing" must never look like a pass.

  --write-export-baseline PATH     freeze the current export symbol set into JSON
  --assert-no-removals-vs PATH     assert no symbol the baseline recorded has vanished
                                   (BUILD-F03 knife 1 removal guard). The ONLY sanctioned way
                                   to delete a symbol is to refresh the baseline, which then
                                   shows up as a deletion in the commit diff.

  --write-export-snapshot PATH     freeze the current export symbol list into a plain-text
                                   snapshot (one symbol name per line, sorted)
  --snapshot PATH --assert-max-growth N --fail
                                   incremental gate: fail when the table gained more than N
                                   symbols since the snapshot, or lost a symbol the removal
                                   baseline has not sanctioned. The delta is what says
                                   something about ONE change; the absolute budget only
                                   catches a blow-up.

The whitelist is built from what consumers actually import, never from the DLL export
table: dumping the current export table into a .def does not lower the entry count and
therefore does not remove the 65535-ordinal bomb.
"""

import argparse
import collections
import glob
import json
import mmap
import os
import re
import struct
import sys
import time

DEFAULT_DLL = os.path.join("minEngine", "bin", "libminEngined.dll")
DEFAULT_CONSUMER_DIR = os.path.join("minEngine", "bin")
# Accident ceiling, not a decision input. At ~3000 live exports a number this round carries no
# information about a normal change, and using it as the daily gate only forces pointless
# "is this class worth a test export" trade-offs. What the gate asserts now is the DELTA
# against the committed snapshot (--assert-max-growth); this number stays as the backstop for
# an explosion (a template-instantiation cascade, a relink under --export-all-symbols).
DEFAULT_BUDGET = 3500
DEFAULT_MIN_HEADROOM = 60000
# Stale artifacts kept out of the contract by default (see design P3): Maximum.exe is the
# retired Playground build and imports symbols the engine no longer defines.
DEFAULT_EXCLUDE_CONSUMERS = ("Maximum.exe", "libminEngine.dll", "libminEngined.dll")
PE_SUFFIXES = (".exe", ".dll")
# Directory names never worth descending into when --consumer-dir is scanned recursively
# (build trees, VCS metadata, generated sites). Keeping the walk shallow keeps a scan of a
# whole multi-repo workspace cheap enough to run in verify.ps1 and as a post-build assert.
CONSUMER_SCAN_PRUNE = (".git", ".vs", ".idea", "build", "cmake-build-debug",
                       "cmake-build-release", "CMakeFiles", "node_modules", "__pycache__",
                       "site", "Saved")
# The engine ships ONE export contract under two file names: release "libminEngine.dll" and
# debug "libminEngined.dll". External repos import the release name while a local debug build
# produces the debug name, so import tables must be matched on the contract, not the file
# name - matching the file name made --consumer-dir silently find 0 consumers (BUILD-F03 P7/Q8).
DLL_CONTRACT_ALIASES = {
    "libminengine.dll": "libminengine",
    "libminengined.dll": "libminengine",
}
# Hand-curated block inside the generated .def. --emit-whitelist preserves it verbatim and
# still validates its entries, so external-debt symbols survive regeneration instead of
# being hand-edited back line by line.
LEGACY_BLOCK_BEGIN = "; ==== BEGIN LEGACY-EXTERNAL-DEBT"
LEGACY_BLOCK_END = "; ==== END LEGACY-EXTERNAL-DEBT"


def contract_key(module_name):
    """Export contract a PE module name binds to (debug and release share one)."""
    base = os.path.basename(module_name).lower()
    return DLL_CONTRACT_ALIASES.get(base, base)


def parse_def_entry(line):
    """Export name on one .def line, or None for comments, blanks and the EXPORTS header."""
    entry = line.strip()
    if not entry or entry.startswith(";") or entry.upper() == "EXPORTS":
        return None
    return entry.split()[0]


IMAGE_DIRECTORY_ENTRY_EXPORT = 0
IMAGE_DIRECTORY_ENTRY_IMPORT = 1


class PEError(Exception):
    pass


class PEImage:
    """Minimal PE32/PE32+ reader: sections + data directories."""

    def __init__(self, path):
        self.path = path
        # The engine DLL and its product hosts are 100-800 MB; mapping them keeps a scan of a
        # whole multi-repo workspace inside the seconds a verify gate can afford.
        with open(path, "rb") as handle:
            try:
                self.data = mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ)
            except (OSError, ValueError):
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
        end = self.data.find(b"\0", offset)
        if end < 0:
            raise PEError("%s: unterminated string at RVA 0x%X" % (self.path, rva))
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
        self.per_contract = collections.OrderedDict()
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
                self.per_contract.setdefault(contract_key(dll_name), []).extend(names)
            if ordinals:
                self.ordinal_imports.setdefault(dll_name, []).extend(ordinals)
            descriptor += 20

    def names_for(self, dll_name):
        """Imported names for the contract this module name belongs to."""
        return self.per_contract.get(contract_key(dll_name), [])


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
def expand_consumer_dirs(patterns):
    """Expand --consumer-dir values that contain glob magic (quote them in the shell).

    A pattern such as ../minCraft*/bin keeps the contract description portable: the caller names
    which live product bins are part of the contract without hard-coding one workspace layout.
    """
    dirs, unmatched = [], []
    for pattern in patterns:
        if glob.has_magic(pattern):
            matched = sorted(glob.glob(pattern))
            if not matched:
                unmatched.append(pattern)
            dirs.extend(matched)
        else:
            dirs.append(pattern)
    return dirs, unmatched


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def display_path(path):
    """Path relative to the repo root: two different files share the name libminCraft.dll, and
    basename-only reporting is what let a stale copy stand in for the live consumer."""
    try:
        return os.path.relpath(os.path.abspath(path), REPO_ROOT).replace(os.sep, "/")
    except ValueError:
        return path


def iter_pe_files(directory, recursive):
    """PE files under one root. --consumer-dir may point at a whole multi-repo workspace."""
    if not recursive:
        for entry in sorted(os.listdir(directory)):
            full = os.path.join(directory, entry)
            if os.path.isfile(full) and entry.lower().endswith(PE_SUFFIXES):
                yield os.path.abspath(full)
        return
    for root, dirs, files in os.walk(directory):
        dirs[:] = sorted(name for name in dirs if name not in CONSUMER_SCAN_PRUNE)
        for entry in sorted(files):
            if entry.lower().endswith(PE_SUFFIXES):
                yield os.path.abspath(os.path.join(root, entry))


def find_consumers(dll_path, consumer_dirs, explicit, exclude_names, include_names,
                   export_names, recursive=True):
    """Return [(path, ImportTable)] for every PE that imports the target DLL."""
    target = os.path.basename(dll_path).lower()
    candidates = []
    for path in explicit:
        candidates.append(os.path.abspath(path))
    for directory in consumer_dirs:
        if not os.path.isdir(directory):
            print("warning: consumer dir not found: %s" % directory, file=sys.stderr)
            continue
        candidates.extend(iter_pe_files(directory, recursive))
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


def check_assertions(exports, budget, min_headroom, forbidden_buckets=()):
    failures = []
    if not exports.present:
        failures.append("DLL has no PE export directory")
    if exports.number_of_functions > budget:
        failures.append("NumberOfFunctions %d > budget %d" % (exports.number_of_functions, budget))
    if exports.headroom < min_headroom:
        failures.append("ordinal headroom %d < required %d" % (exports.headroom, min_headroom))
    if forbidden_buckets:
        # BUILD-F03 C2 / BUILD-F04 S04: an empty or export-less DLL must not satisfy a
        # "bucket X is absent" assertion by vacuity - that is the same false-green shape the
        # coverage check refuses three functions below.
        if not exports.present or exports.number_of_functions == 0:
            failures.append("forbid-buckets %s: %s has %d export entries, so the assertion would "
                            "pass vacuously" % (
                                ",".join(sorted(forbidden_buckets)), "the DLL",
                                exports.number_of_functions))
        else:
            names = exports.name_set()
            for bucket in sorted(forbidden_buckets):
                hits = sorted(name for name in names if classify(name) == bucket)
                if hits:
                    failures.append("forbidden bucket '%s': %d export(s), e.g. %s" % (
                        bucket, len(hits), ", ".join(hits[:3])))
    return failures


def read_legacy_block(path):
    """(lines, names) of the preserved external-debt block in an existing .def file.

    The block is curated by hand because its entries are API debt, not a derived set; it is
    copied through regeneration verbatim (comments and DATA keywords included) and its names
    are merged into the whitelist so they are validated like every other entry.
    """
    if not path or not os.path.isfile(path):
        return [], []
    lines, names, inside = [], [], False
    with open(path, "r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.rstrip("\n")
            if line.startswith(LEGACY_BLOCK_BEGIN):
                inside = True
            if not inside:
                continue
            lines.append(line)
            name = parse_def_entry(line)
            if name:
                names.append(name)
            if line.startswith(LEGACY_BLOCK_END):
                inside = False
    return lines, names


def empty_scan_roots(consumers, scan_dirs):
    """Requested scan roots that produced no consumer. A root that verifies nothing must be
    reported as a failure, never as a quiet "0 missing" (the original F02 failure shape)."""
    empty = []
    for root in scan_dirs:
        prefix = os.path.normcase(os.path.abspath(root)) + os.sep
        if not any(os.path.normcase(os.path.abspath(path)).startswith(prefix)
                   for path, _table in consumers):
            empty.append(root)
    return empty


def check_coverage(dll_path, exports, consumers, scan_dirs, unmatched_patterns=None,
                  transitional_buckets=()):
    """Assert every consumer import resolves to an export. Returns the process exit code.

    BUILD-F04 S04 transitional exception: --transitional-missing-bucket lets a named symbol BUCKET
    be missing for consumers that live OUTSIDE this repository. It exists because deleting the
    imgui bucket from the engine contract (BUILD-F03 C2) makes every pre-S04 external editor host
    stale by definition - a missing imgui symbol cannot be a contract regression any more. The
    exception is scoped by bucket, not by path, so it retires itself: once the external repos are
    rebuilt against a post-S04 engine they import no imgui, and the tolerated count falls to zero
    with no one having to remember to delete a flag. It never applies to in-repo artifacts, it
    still fails on any other missing symbol, and it prints every tolerated name.
    """
    exported = exports.name_set()
    target = os.path.basename(dll_path).lower()
    repo_prefix = os.path.normcase(os.path.abspath(REPO_ROOT)) + os.sep
    tolerated_buckets = set(transitional_buckets)
    print("coverage (exported >= consumed), contract %s:" % contract_key(target))
    total_missing = 0
    in_repo_missing = 0
    unexpected_missing = 0
    tolerated_total = 0
    tolerated_by_bucket = collections.Counter()
    union = set()
    for path, table in consumers:
        imported = set(table.names_for(target))
        union |= imported
        missing = imported - exported
        in_repo = os.path.normcase(os.path.abspath(path)).startswith(repo_prefix)
        tolerated = set()
        if tolerated_buckets and not in_repo:
            tolerated = {name for name in missing if classify(name) in tolerated_buckets}
        fatal = missing - tolerated
        total_missing += len(missing)
        if in_repo:
            in_repo_missing += len(missing)
        unexpected_missing += len(fatal)
        tolerated_total += len(tolerated)
        for name in tolerated:
            tolerated_by_bucket[classify(name)] += 1
        print("  %-52s imported %5d  missing %5d%s" % (
            display_path(path), len(imported), len(missing),
            "  (%d tolerated: %s)" % (len(tolerated), ",".join(sorted(tolerated_buckets)))
            if tolerated else ""))
        for name in sorted(tolerated):
            print("      tolerated[%s]: %s" % (classify(name), name))
        for name in sorted(fatal)[:20]:
            print("      missing: %s" % name)
        if len(fatal) > 20:
            print("      ... %d more" % (len(fatal) - 20))
    print("  %-52s imported %5d  missing %d" % ("UNION", len(union), len(union - exported)))
    print("coverage total missing = %d" % total_missing)
    print("coverage in-repo missing = %d (never tolerated)" % in_repo_missing)
    print("coverage unexpected missing = %d (anything outside the tolerated bucket(s))" % (
        unexpected_missing))
    if tolerated_total:
        print("WARN TRANSITIONAL EXCEPTION: %d missing symbols tolerated in bucket '%s' - external "
              "editor hosts are pre-S04 artifacts and must be rebuilt against a post-S04 engine" % (
                  tolerated_total, ",".join(sorted(tolerated_buckets))))
        print("WARN the tolerated names are printed above, one per line; this exception retires "
              "itself when the external hosts stop importing that bucket")
    if unexpected_missing:
        print("FAIL: %d consumer imports are not exported by %s (%d tolerated, %d in-repo)" % (
            unexpected_missing, dll_path, tolerated_total, in_repo_missing), file=sys.stderr)
        return 1
    if not consumers and scan_dirs:
        print("FAIL: no PE under %s imports the %s contract - nothing was verified" % (
            ", ".join(scan_dirs), contract_key(target)), file=sys.stderr)
        return 1
    failed = empty_scan_roots(consumers, scan_dirs) + list(unmatched_patterns or [])
    for root in failed:
        print("FAIL: no consumer of the %s contract under %s - the requested scan verified "
              "nothing" % (contract_key(target), root), file=sys.stderr)
    if failed:
        return 1
    return 0

# --- export baseline: the "deleting a symbol is a failure" guard -----------------------
# The budget and forbid-bucket assertions only bound what may APPEAR in the export table. None
# of them notices a symbol DISAPPEARING, which is the failure mode that breaks an external
# product repo: its next link fails on an import the engine quietly stopped exporting. The
# baseline is the frozen symbol set; the only sanctioned way to remove an export is to refresh
# the baseline, because that deletion is then visible in the commit diff. There is deliberately
# no "--allow-removed" escape hatch: a bypass flag turns the guard into decoration.
BASELINE_SCHEMA = 1
ENGINE_VERSION_HEADER = os.path.join("minEngine", "minEngine", "src", "Runtime", "Core",
                                     "EngineVersion.h")


def read_engine_version():
    """Engine version for baseline metadata, read from EngineVersion.h (informational only)."""
    path = os.path.join(REPO_ROOT, ENGINE_VERSION_HEADER)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError:
        return "unknown"
    match = re.search(r"kEngineVersion\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)", text)
    return ".".join(match.groups()) if match else "unknown"


def write_export_baseline(path, dll_path, exports):
    """Freeze the current export symbol set into a diff-readable JSON baseline."""
    names = sorted(exports.name_set())
    payload = {
        "schema": BASELINE_SCHEMA,
        "contract": contract_key(os.path.basename(dll_path)),
        "engine_version": read_engine_version(),
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "export_count": len(names),
        "data_count": sum(1 for name in names if exports.kinds.get(name) == "data"),
        "symbols": names,
    }
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")
    print("wrote %s: %d export symbols (contract %s, engine %s)" % (
        path, len(names), payload["contract"], payload["engine_version"]))
    return payload


def assert_no_removals_vs(baseline_path, dll_path, exports):
    """Assert every symbol the baseline recorded is still exported. Returns the exit code.

    Exit 1 means "a symbol was removed" (a contract regression or an unrefreshed baseline);
    exit 2 means "the guard could not verify anything" - a missing/unreadable/empty baseline or
    a baseline belonging to another module. Both are failures: a guard that skips itself when
    its input is missing is the same false green --verify-coverage refuses for a 0-consumer scan.
    """
    if not os.path.isfile(baseline_path):
        print("FAIL: export baseline not found: %s" % baseline_path, file=sys.stderr)
        print("      nothing was compared; regenerate it with --write-export-baseline %s" % (
            baseline_path), file=sys.stderr)
        return 2
    try:
        with open(baseline_path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, ValueError) as exc:
        print("FAIL: export baseline is unreadable: %s (%s)" % (baseline_path, exc),
              file=sys.stderr)
        return 2
    symbols = payload.get("symbols") if isinstance(payload, dict) else None
    if not isinstance(symbols, list) or not all(isinstance(name, str) for name in symbols):
        print("FAIL: export baseline %s has no string 'symbols' list (schema %s expected)" % (
            baseline_path, BASELINE_SCHEMA), file=sys.stderr)
        return 2
    if not symbols:
        print("FAIL: export baseline %s holds 0 symbols - an empty baseline passes every "
              "removal check by vacuity" % baseline_path, file=sys.stderr)
        return 2
    target = contract_key(os.path.basename(dll_path))
    baseline_contract = payload.get("contract")
    if baseline_contract and baseline_contract != target:
        print("FAIL: export baseline %s is for contract '%s', but the DLL belongs to '%s'" % (
            baseline_path, baseline_contract, target), file=sys.stderr)
        return 2
    baseline = set(symbols)
    current = exports.name_set()
    removed = sorted(baseline - current)
    added = sorted(current - baseline)
    print("baseline ....... %s" % baseline_path)
    print("  frozen ....... %d symbols (engine %s, written %s, schema %s)" % (
        len(baseline), payload.get("engine_version", "unknown"),
        payload.get("generated_at", "unknown"), payload.get("schema", "unknown")))
    print("  current DLL .. %s (%d exports)" % (dll_path, len(current)))
    if added:
        print("  NOTE: %d export(s) are newer than the baseline; refresh it with "
              "--write-export-baseline so the addition is recorded" % len(added))
        for name in added[:10]:
            print("      added: %s" % name)
        if len(added) > 10:
            print("      ... %d more" % (len(added) - 10))
    if not removed:
        print("PASS: all %d baseline symbols are still exported by %s" % (
            len(baseline), os.path.basename(dll_path)))
        return 0
    print("FAIL: %d symbol(s) recorded in the baseline are no longer exported by %s:" % (
        len(removed), dll_path), file=sys.stderr)
    for name in removed:
        print("  removed: %s" % name, file=sys.stderr)
    print("      If the removal is intended, refresh the baseline with "
          "--write-export-baseline %s: the deletion then appears in the commit diff, which is "
          "the review step. There is no allow-removal flag." % baseline_path, file=sys.stderr)
    return 1


# --- export snapshot: the incremental gate ---------------------------------------------
# The budget bounds the table, the removal baseline protects what consumers import. Neither
# notices "this change quietly exported 300 more symbols", which is the shape that actually
# erodes a DLL boundary: each individual addition looks reasonable, the sum does not. The
# snapshot is the committed export list of the last accepted state, so the gate can assert the
# DELTA of one change. Regenerating it is the explicit confirmation for an intentional
# addition, and because it is a tracked one-name-per-line file that confirmation is a diff a
# reviewer reads without running anything.
DEFAULT_MAX_GROWTH = 25
# Printed names per failure. Enough to recognise the family that leaked, short enough that the
# gate's own output stays readable in a verify log.
GROWTH_PRINT_LIMIT = 40


def read_export_snapshot(path):
    """Symbol set of a committed snapshot. Raises ValueError when the file cannot verify.

    The format is deliberately boring: sorted symbol names, one per line. Blank lines are
    ignored so a trailing newline is not a failure; comment lines are an error rather than
    silently skipped, because a snapshot that carries prose has been hand-edited and is not
    the generated artifact the gate compares against.
    """
    names = []
    with open(path, "r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line:
                continue
            if line.startswith(";"):
                raise ValueError("comment line %r (the snapshot is generated, not authored)"
                                 % line[:40])
            names.append(line)
    if not names:
        raise ValueError("holds 0 symbols - an empty snapshot grows by nothing and passes every "
                         "growth assertion by vacuity")
    if len(names) != len(set(names)):
        duplicates = sorted(name for name, count in collections.Counter(names).items()
                            if count > 1)
        raise ValueError("holds %d duplicate line(s), e.g. %s" % (
            len(duplicates), ", ".join(duplicates[:3])))
    return set(names)


def write_export_snapshot(path, dll_path, exports):
    """Freeze the current export list into the committed snapshot (sorted, one name per line)."""
    names = sorted(exports.name_set())
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(names) + "\n")
    print("wrote %s: %d export symbols (contract %s, %d data)" % (
        path, len(names), contract_key(os.path.basename(dll_path)),
        sum(1 for name in names if exports.kinds.get(name) == "data")))
    return names


def read_removal_baseline(path):
    """Symbol set of the removal baseline, or None when it is absent/unusable.

    Used to label a disappearance: a symbol the baseline still records was imported by a
    consumer, so its removal breaks that consumer's next link. It never excuses a removal.
    """
    if not path or not os.path.isfile(path):
        return None
    try:
        with open(path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, ValueError):
        return None
    symbols = payload.get("symbols") if isinstance(payload, dict) else None
    if not isinstance(symbols, list) or not all(isinstance(name, str) for name in symbols):
        return None
    return set(symbols)


def assert_max_growth(snapshot_path, dll_path, exports, max_growth, removal_baseline_path):
    """Assert the table grew by at most max_growth and lost nothing unsanctioned.

    Exit 1 is a real violation; exit 2 means the gate could not verify anything (missing,
    empty, duplicated or hand-authored snapshot). Both fail: a growth gate that skips itself
    when its input is missing is the false green the coverage check already refuses.
    """
    if max_growth is None:
        max_growth = DEFAULT_MAX_GROWTH
    try:
        frozen = read_export_snapshot(snapshot_path)
    except OSError as exc:
        print("FAIL: export snapshot unreadable: %s (%s)" % (snapshot_path, exc), file=sys.stderr)
        print("      nothing was compared; regenerate it with "
              "--write-export-snapshot %s" % snapshot_path, file=sys.stderr)
        return 2
    except ValueError as exc:
        print("FAIL: export snapshot %s %s" % (snapshot_path, exc), file=sys.stderr)
        print("      nothing was compared; regenerate it with "
              "--write-export-snapshot %s" % snapshot_path, file=sys.stderr)
        return 2
    current = exports.name_set()
    added = sorted(current - frozen)
    removed = sorted(frozen - current)
    sanctioned = read_removal_baseline(removal_baseline_path) or set()
    print("snapshot ....... %s" % snapshot_path)
    print("  frozen ....... %d symbols" % len(frozen))
    print("  current DLL .. %s (%d exports)" % (dll_path, len(current)))
    exit_code = 0
    if len(added) > max_growth:
        print("FAIL: export table grew by %d symbols, limit %d" % (len(added), max_growth),
              file=sys.stderr)
        for name in added[:GROWTH_PRINT_LIMIT]:
            print("  new: %s" % name, file=sys.stderr)
        if len(added) > GROWTH_PRINT_LIMIT:
            print("  ... %d more new symbols" % (len(added) - GROWTH_PRINT_LIMIT),
                  file=sys.stderr)
        print("      If the addition is intended, record it by regenerating the snapshot "
              "(--write-export-snapshot %s) and name each new export in the commit message."
              % snapshot_path, file=sys.stderr)
        exit_code = 1
    if removed:
        # Every disappearance fails, including one the removal baseline never recorded: an export
        # nobody imports today is still a symbol someone's product may call tomorrow, and a gate
        # that exempts exactly the symbols nobody is watching is the false green this repository
        # keeps designing against. Accepting an intended removal costs one regenerated snapshot,
        # which puts the deletion in the commit diff.
        in_baseline = [name for name in removed if name in sanctioned]
        print("FAIL: %d symbol(s) listed in the snapshot are no longer exported by %s" % (
            len(removed), dll_path), file=sys.stderr)
        for name in removed[:GROWTH_PRINT_LIMIT]:
            print("  removed: %s%s" % (
                name, "   (recorded in the removal baseline: consumer contract regression)"
                if name in sanctioned else ""), file=sys.stderr)
        if len(removed) > GROWTH_PRINT_LIMIT:
            print("  ... %d more removed symbols" % (
                len(removed) - GROWTH_PRINT_LIMIT), file=sys.stderr)
        if in_baseline:
            print("      %d of them are recorded in the removal baseline, so a consumer that "
                  "imports them breaks on its next link." % len(in_baseline), file=sys.stderr)
        print("      Restore the export, or - if the removal is intended and reviewed through a "
              "baseline refresh - regenerate the snapshot (--write-export-snapshot %s) so the "
              "deletion is recorded in the commit diff." % snapshot_path, file=sys.stderr)
        exit_code = 1
    if exit_code == 0:
        print("PASS: export growth %d <= %d, no symbol left the export table" % (
            len(added), max_growth))
    return exit_code


def write_whitelist(path, dll_path, exports, consumers, extra_defs):
    target = os.path.basename(dll_path).lower()
    existing = exports.name_set()
    union = consumed_union(consumers, target)
    extra = set()
    for extra_path in extra_defs:
        with open(extra_path, "r", encoding="utf-8") as handle:
            for line in handle:
                name = parse_def_entry(line)
                if name:
                    extra.add(name)
    legacy_lines, legacy_list = read_legacy_block(path)
    legacy = set(legacy_list)
    wanted = union | extra | legacy
    # Legacy entries are emitted inside their curated block with their authored DATA keywords;
    # keeping them out of the sorted list avoids duplicate export names.
    keep = sorted(name for name in wanted if name in existing and name not in legacy)
    # A .def data export becomes a code thunk ("T <name>" in .text) inside the generated
    # import library, which collides with the COMDAT copy of the same vtable/typeinfo that
    # consumers emit themselves ("multiple definition of 'vtable for ...'"). The DATA
    # keyword makes ld emit a plain import stub (__imp_/__nm_) instead.
    data_names = {name for name in keep if exports.kinds.get(name) == "data"}
    dropped = sorted(name for name in wanted - existing if name not in extra and name not in legacy)
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    # BUILD-F04 S02: this generator now serves two modules (engine + editor), so the header is
    # derived from the module being generated instead of naming the engine unconditionally.
    module = os.path.basename(dll_path)
    lines = [
        "; PE export whitelist for %s - GENERATED, do not hand-edit one line at a time." % module,
        "; Source: PE import tables of the consumers of this DLL, not the DLL export table.",
        "; Contract: %s - module names that normalise to this key are ONE contract, and a" % (
            contract_key(module)),
        ";   consumer may import either spelling (BUILD-F03 P7 / Q8).",
        "; Regenerate from the consumer import tables:",
        ";   python scripts/audit_dll_exports.py --dll <this DLL> --emit-whitelist <this file> \\",
        ";       [--consumer-dir \"../minCraft*/bin\"]   # add live external bins when they consume it",
        "; The legacy block at the end of this file is preserved verbatim by that command.",
        "; Consumers:",
    ]
    for consumer_path, table in consumers:
        count = len(set(table.names_for(target)))
        lines.append(";   %-42s %5d names" % (display_path(consumer_path), count))
    lines.extend([
        "; Entries: %d (consumed union %d, DLL exports %d; %d marked DATA; %d in legacy block)" % (
            len(keep) + len(legacy), len(union), len(existing), len(data_names), len(legacy)),
        "EXPORTS",
    ])
    for name in keep:
        lines.append(name + " DATA" if name in data_names else name)
    if legacy_lines:
        lines.extend(legacy_lines)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    print("wrote %s: %d entries (consumed union %d, legacy block %d)" % (
        path, len(keep) + len(legacy), len(union), len(legacy)))
    for name in sorted(legacy - existing):
        print("warning: legacy entry is not exported by the current DLL: %s" % name)
        print("         (.def export of an undefined symbol fails the link; fix the TU rather "
              "than deleting the entry silently)")
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
                        help="extra directory scanned recursively for consumer binaries "
                             "(repeatable; point it at a whole multi-repo workspace root)")
    parser.add_argument("--no-recursive", action="store_true",
                        help="scan only the top level of each --consumer-dir")
    parser.add_argument("--include-consumer", action="append", default=[],
                        help="scan this consumer even if it is in the default exclusion list")
    parser.add_argument("--exclude-consumer", action="append", default=[],
                        help="skip this consumer file name (repeatable)")
    parser.add_argument("--extra-def", action="append", default=[],
                        help="merge hand-maintained export names from this .def (repeatable)")
    parser.add_argument("--forbid-buckets", action="append", default=[], metavar="BUCKET[,BUCKET]",
                        help="with --fail: assert the export table contains NO entry of these "
                             "composition buckets (BUILD-F03 C1/C2, e.g. imgui). Repeatable, "
                             "comma-separated allowed. An unknown bucket name is an error, and a "
                             "DLL with no export table fails instead of passing vacuously.")
    parser.add_argument("--transitional-missing-bucket", action="append", default=[],
                        metavar="BUCKET",
                        help="with --verify-coverage: tolerate missing symbols of this bucket for "
                             "consumers OUTSIDE this repository only (never for in-repo artifacts), "
                             "still failing on anything else. Prints every tolerated name. BUILD-F04 "
                             "S04, see the check_coverage docstring.")
    parser.add_argument("--write-export-baseline", default=None, metavar="PATH",
                        help="freeze the current DLL export symbol set into a JSON baseline "
                             "(BUILD-F03 knife 1 removal guard)")
    parser.add_argument("--assert-no-removals-vs", default=None, metavar="PATH",
                        help="assert no symbol recorded in this baseline has disappeared from the "
                             "DLL; exits non-zero on any removal AND when the baseline is missing, "
                             "unreadable, empty, or belongs to another contract (a guard that "
                             "cannot verify must not pass)")
    parser.add_argument("--write-export-snapshot", default=None, metavar="PATH",
                        help="freeze the current export symbol list into a sorted plain-text "
                             "snapshot (one name per line) for the incremental export gate")
    parser.add_argument("--snapshot", default=None, metavar="PATH",
                        help="with --fail: compare the export table against this committed "
                             "snapshot; a growth above --assert-max-growth, or a symbol the "
                             "removal baseline has not sanctioned, fails")
    parser.add_argument("--assert-max-growth", type=int, default=None, metavar="N",
                        help="with --snapshot: allowed number of exports newer than the snapshot "
                             "(default %d)" % DEFAULT_MAX_GROWTH)
    parser.add_argument("--removal-baseline", default=None, metavar="PATH",
                        help="with --snapshot: baseline used to label a removal as a consumer "
                             "contract regression. Diagnosis only - a removal always fails")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    # A bucket name is a key of the classifier, so validate it here: a typo must not silently
    # assert nothing (the whole point of C1/C2 is that the assertion really runs).
    forbidden_buckets = []
    for value in args.forbid_buckets:
        for bucket in value.split(","):
            bucket = bucket.strip()
            if not bucket:
                continue
            if bucket not in BUCKETS:
                print("error: unknown bucket '%s' in --forbid-buckets (known: %s)" % (
                    bucket, ", ".join(BUCKETS)), file=sys.stderr)
                return 2
            forbidden_buckets.append(bucket)
    transitional_buckets = []
    for bucket in args.transitional_missing_bucket:
        bucket = bucket.strip()
        if bucket not in BUCKETS:
            print("error: unknown bucket '%s' in --transitional-missing-bucket (known: %s)" % (
                bucket, ", ".join(BUCKETS)), file=sys.stderr)
            return 2
        transitional_buckets.append(bucket)
    if forbidden_buckets and not args.fail:
        print("error: --forbid-buckets requires --fail (otherwise it asserts nothing)",
              file=sys.stderr)
        return 2
    if transitional_buckets and not args.verify_coverage:
        print("error: --transitional-missing-bucket requires --verify-coverage (otherwise it "
              "tolerates nothing)", file=sys.stderr)
        return 2
    if args.assert_max_growth is not None and not args.snapshot:
        print("error: --assert-max-growth requires --snapshot (there is nothing to compare "
              "against)", file=sys.stderr)
        return 2
    if args.snapshot and not args.fail:
        print("error: --snapshot requires --fail (otherwise it asserts nothing)", file=sys.stderr)
        return 2
    if args.removal_baseline and not args.snapshot:
        print("error: --removal-baseline requires --snapshot (otherwise it labels nothing)",
              file=sys.stderr)
        return 2

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
    consumer_dirs, unmatched_dirs = [], []
    if need_consumers:
        # The in-repo bin directory is always part of the contract; --consumer-dir adds the
        # LIVE external consumers on top of it (BUILD-F03 Q8), never instead of it.
        consumer_dirs, unmatched_dirs = expand_consumer_dirs(args.consumer_dir)
        local_dir = os.path.dirname(os.path.abspath(dll_path))
        if os.path.normcase(local_dir) not in {os.path.normcase(os.path.abspath(d))
                                               for d in consumer_dirs}:
            consumer_dirs.append(local_dir)
        exclude = set(DEFAULT_EXCLUDE_CONSUMERS) | set(args.exclude_consumer)
        consumers = find_consumers(
            dll_path, consumer_dirs, args.consumer, exclude,
            set(args.include_consumer), exports.name_set(),
            recursive=not args.no_recursive)

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
    if args.write_export_baseline:
        write_export_baseline(args.write_export_baseline, dll_path, exports)
    if args.write_export_snapshot:
        write_export_snapshot(args.write_export_snapshot, dll_path, exports)

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
    if args.assert_no_removals_vs:
        # Runs before --fail: a removal is the more specific failure and must not be masked by
        # a budget assertion that happens to pass on the smaller table.
        exit_code = assert_no_removals_vs(args.assert_no_removals_vs, dll_path, exports)
    if args.snapshot:
        # Before --fail: "this change exported 300 new symbols" is more actionable than the
        # budget, and a growth failure must not be masked by a passing ceiling.
        exit_code = max(exit_code, assert_max_growth(args.snapshot, dll_path, exports,
                                                     args.assert_max_growth,
                                                     args.removal_baseline))
    if args.verify_coverage:
        # max(), not assignment: a removal failure (1) or an unverifiable guard (2) must never be
        # overwritten by a later assertion that happens to pass in the same invocation.
        exit_code = max(exit_code, check_coverage(dll_path, exports, consumers,
                                   expand_consumer_dirs(args.consumer_dir)[0],
                                   unmatched_dirs,
                                   transitional_buckets))
    if args.fail:
        failures = check_assertions(exports, args.max_exports, args.min_headroom,
                                    forbidden_buckets)
        if failures:
            for failure in failures:
                print("FAIL: %s" % failure, file=sys.stderr)
            exit_code = max(exit_code, 1)
        else:
            print("PASS: NumberOfFunctions=%d <= %d, ordinal headroom=%d >= %d" % (
                exports.number_of_functions, args.max_exports,
                exports.headroom, args.min_headroom))
            if forbidden_buckets:
                print("PASS: export table contains no entry of bucket(s) %s" % (
                    ",".join(sorted(set(forbidden_buckets)))))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
