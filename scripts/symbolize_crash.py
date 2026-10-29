#!/usr/bin/env python3
"""Symbolise crash-record addresses against the binaries that produced them.

An address on its own means nothing: it has to be turned into a position inside a
specific PE image before a symbol table can answer anything. Two conversions are
needed and this tool performs both:

  runtime address --(minus load base)--> RVA
  RVA             --(plus  ImageBase)--> address the DWARF tables use

The crash record stores "module+0xRVA" next to every address, so the second step
needs only the ImageBase of the file on disk. Skipping the second step is what
makes a symboliser look broken: a bare RVA falls below every recorded range and
resolves to nothing, while a hand-guessed base resolves to a plausible, wrong
function.

Subcommands
  self-check   prove the toolchain still resolves a known symbol to itself
  crash-log    resolve every address in a crash record
  addr         resolve one address
  modules      print the image identity of the shipped binaries

Run from the repository root. The binutils directory is taken from
MINENGINE_BINUTILS, then PATH, then the compiler recorded in the contract build.
"""

import argparse
import bisect
import os
import re
import shutil
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN_DIR = os.path.join(REPO_ROOT, "minEngine", "bin")
BUILD_CACHE = os.path.join(REPO_ROOT, "minEngine", "build", "CMakeCache.txt")
DEFAULT_DLLS = ("libminEngined.dll", "libMaximumEditord.dll", "MaximumEditor.exe")
DEFAULT_CRASH_LOG = os.path.join(BIN_DIR, "ed_crash.log")

#: Symbols the conversion was verified against by hand. Each one is a fixed point:
#: objdump reports its section offset, and addr2line must answer with the same name.
ANCHOR_SYMBOLS = (
    "_ZN9minEngine9OpenGLRHI17RHICmdDrawIndexedEjji",
    "_ZN9minEngine21SpriteMaterialFactory14CreateInstanceERNS_3RHIEb",
)

TOOL_NAMES = ("objdump", "addr2line")


class ToolError(RuntimeError):
    pass


def find_binutils(explicit=None):
    """Locate objdump/addr2line: explicit flag, env var, PATH, then the build's compiler."""
    candidates = []
    if explicit:
        candidates.append(explicit)
    env = os.environ.get("MINENGINE_BINUTILS")
    if env:
        candidates.append(env)
    found = {}
    for name in TOOL_NAMES:
        path = shutil.which(name)
        if path:
            found[name] = path
    if len(found) == len(TOOL_NAMES):
        return os.path.dirname(found["objdump"])

    if os.path.isfile(BUILD_CACHE):
        with open(BUILD_CACHE, "r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                match = re.match(r"CMAKE_CXX_COMPILER:\w+=(.+)", line.strip())
                if match:
                    candidates.append(os.path.dirname(match.group(1).strip()))
                    break

    for directory in candidates:
        if not directory:
            continue
        if all(os.path.isfile(os.path.join(directory, tool + ".exe"))
               or os.path.isfile(os.path.join(directory, tool)) for tool in TOOL_NAMES):
            return directory
    raise ToolError(
        "objdump and addr2line were not found. Pass --binutils, set MINENGINE_BINUTILS, "
        "or put the MinGW bin directory on PATH.")


def tool(directory, name):
    for candidate in (name + ".exe", name):
        path = os.path.join(directory, candidate)
        if os.path.isfile(path):
            return path
    raise ToolError("%s was not found in %s" % (name, directory))


def run(command, cwd=None):
    completed = subprocess.run(command, capture_output=True, text=True,
                               errors="replace", cwd=cwd)
    if completed.returncode != 0 and not completed.stdout:
        raise ToolError("%s failed: %s" % (command[0], completed.stderr.strip()))
    return completed.stdout


def read_pe(path):
    """Image identity and section table of a PE file (headers only, no full read)."""
    with open(path, "rb") as handle:
        header = handle.read(0x4000)
    if header[:2] != b"MZ":
        raise ToolError("%s is not a PE file" % path)
    pe_offset = struct.unpack_from("<I", header, 0x3C)[0]
    if header[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ToolError("%s has no PE signature" % path)
    coff = pe_offset + 4
    machine, section_count, timestamp = struct.unpack_from("<HHI", header, coff)
    optional_size = struct.unpack_from("<H", header, coff + 16)[0]
    optional = coff + 20
    magic = struct.unpack_from("<H", header, optional)[0]
    if magic != 0x20B:
        raise ToolError("%s is not a 64-bit image" % path)
    image_base = struct.unpack_from("<Q", header, optional + 24)[0]
    size_of_image = struct.unpack_from("<I", header, optional + 56)[0]

    sections = []
    table = optional + optional_size
    for index in range(section_count):
        entry = table + index * 40
        name = header[entry:entry + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, virtual_address = struct.unpack_from("<II", header, entry + 8)
        sections.append({
            "name": name,
            "vma": image_base + virtual_address,
            "size": virtual_size,
        })
    return {
        "path": path,
        "name": os.path.basename(path),
        "machine": machine,
        "image_base": image_base,
        "size_of_image": size_of_image,
        "timestamp": timestamp,
        "sections": sections,
    }


SYMBOL_LINE = re.compile(
    r"\[[ 0-9]+\]\(sec\s+(\d+)\)\(fl (\S+)\)\(ty\s+(\w+)\)\(scl\s+(\d+)\)\s+\(nx \d+\)\s+"
    r"(0x[0-9a-f]+)\s+(\S+)")
SECTION_LINE = re.compile(
    r"\s*(\d+) (\.\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)")


def text_symbols(image, binutils):
    """(vma, mangled name) for every function in .text, from the static symbol table.

    objdump -h numbers sections from 0 while objdump -t refers to them from 1, and the
    offset in a -t line is relative to its section. Getting either wrong shifts every
    address by a constant, which is exactly the failure this table exists to rule out.
    """
    sections = []
    for line in run([tool(binutils, "objdump"), "-h", image["path"]]).splitlines():
        match = SECTION_LINE.match(line)
        if match:
            sections.append((match.group(2), int(match.group(4), 16)))

    symbols = []
    for line in run([tool(binutils, "objdump"), "-t", image["path"]]).splitlines():
        match = SYMBOL_LINE.match(line)
        if not match:
            continue
        section_number, symbol_type, offset, name = (
            int(match.group(1)), match.group(3), int(match.group(5), 16), match.group(6))
        if symbol_type != "20" or section_number < 1 or section_number > len(sections):
            continue
        section_name, section_vma = sections[section_number - 1]
        if section_name != ".text":
            continue
        symbols.append((section_vma + offset, name))
    symbols.sort()
    return symbols


def resolve_many(image, binutils, virtual_addresses, demangle=True):
    """[(function, file:line)] for the given DWARF addresses.

    demangle=False keeps the mangled spelling, which is the only form that can be compared
    with the symbol table for equality; demangled names are for reading.
    """
    results = []
    chunk = 400
    for start in range(0, len(virtual_addresses), chunk):
        batch = virtual_addresses[start:start + chunk]
        command = [tool(binutils, "addr2line"), "-f", "-e", image["path"]]
        if demangle:
            command.insert(2, "-C")
        command.extend("0x%X" % address for address in batch)
        lines = run(command).splitlines()
        for index in range(0, len(lines), 2):
            function = lines[index].strip() if index < len(lines) else "??"
            location = lines[index + 1].strip() if index + 1 < len(lines) else "??:0"
            results.append((function, location))
    return results


def resolve_one(image, binutils, virtual_address, demangle=True):
    return resolve_many(image, binutils, [virtual_address], demangle)[0]


def rva_to_vma(image, rva):
    """The conversion the whole tool rests on: RVA is relative to the image, DWARF is not."""
    return image["image_base"] + rva


def self_check(args):
    binutils = find_binutils(args.binutils)
    print("binutils: %s" % binutils)
    print("addr2line: %s" % run([tool(binutils, "addr2line"), "--version"]).splitlines()[0].strip())

    names = args.dll or list(DEFAULT_DLLS)
    failures = 0
    for name in names:
        path = name if os.path.isfile(name) else os.path.join(BIN_DIR, name)
        if not os.path.isfile(path):
            print("\n%s: not built; skipped" % name)
            continue
        image = read_pe(path)
        print("\n=== %s ===" % image["name"])
        print("  ImageBase=0x%X SizeOfImage=0x%X TimeDateStamp=0x%08X"
              % (image["image_base"], image["size_of_image"], image["timestamp"]))
        text_section = next((s for s in image["sections"] if s["name"] == ".text"), None)
        if text_section is None:
            print("  no .text section; skipped")
            failures += 1
            continue
        print("  .text VMA=0x%X size=0x%X" % (text_section["vma"], text_section["size"]))

        symbols = text_symbols(image, binutils)
        if not symbols:
            print("  no .text functions in the symbol table; skipped")
            failures += 1
            continue

        anchors = [(vma, symbol) for vma, symbol in symbols if symbol in ANCHOR_SYMBOLS]
        for vma, symbol in anchors:
            mangled, _ = resolve_one(image, binutils, vma, demangle=False)
            readable, location = resolve_one(image, binutils, vma)
            verdict = "OK" if mangled == symbol else "MISMATCH"
            print("  anchor %s" % symbol)
            print("    VMA=0x%X -> %s  %s  %s" % (vma, readable, location, verdict))
            if mangled != symbol:
                print("    addr2line answered: %s" % mangled)
                failures += 1

        sample_count = min(args.sample, len(symbols))
        stride = max(1, len(symbols) // sample_count)
        sample = symbols[::stride][:sample_count]
        resolved = resolve_many(image, binutils, [vma for vma, _ in sample], demangle=False)

        starts = sorted(set(vma for vma, _ in symbols))
        names_at = {}
        for vma, symbol in symbols:
            names_at.setdefault(vma, set()).add(symbol)

        identical = respelled = crt = relocated = 0
        relocated_examples = []
        for (vma, expected), (function, location) in zip(sample, resolved):
            if function == expected:
                identical += 1
                continue
            if function.startswith("??"):
                # CRT start-up objects and prebuilt static libraries carry line info in the
                # combined line table but no function entry of their own.
                crt += 1
                continue

            # A constructor's C1/C2 pair, a lambda and a template member all answer with a
            # different spelling at the same address. Prove the address really is inside the
            # named function instead of trusting the spelling.
            own = sorted(candidate for candidate, names in names_at.items() if function in names)
            if not own:
                respelled += 1
                continue
            position = bisect.bisect_right(starts, own[0])
            following = starts[position] if position < len(starts) else None
            if own[0] <= vma and (following is None or vma < following):
                respelled += 1
            else:
                relocated += 1
                if len(relocated_examples) < 3:
                    relocated_examples.append((vma, expected, function, location))

        total = len(sample)
        described = identical + respelled
        named = total - crt
        print("  sampled %d of %d .text functions" % (total, len(symbols)))
        print("    identical mangled name            : %d (%.1f%%)"
              % (identical, 100.0 * identical / total))
        print("    same function, different spelling : %d (%.1f%%)  [C1/C2, lambdas, template members]"
              % (respelled, 100.0 * respelled / total))
        print("    no function entry in DWARF        : %d (%.1f%%)  [CRT / prebuilt objects]"
              % (crt, 100.0 * crt / total))
        print("    resolved outside the named symbol  : %d" % relocated)
        for vma, expected, function, location in relocated_examples:
            print("      0x%X %s -> %s %s" % (vma, expected, function, location))

        if args.strict:
            ok = identical == total
        else:
            ok = relocated == 0 and named > 0 and described >= 0.95 * named
        print("    verdict: %s" % ("PASS" if ok else "FAIL"))
        if not ok:
            failures += 1

    return 0 if failures == 0 else 1


RECORD_HEADER = re.compile(r"^=== (.+) crash (.+) ===$")
MODULE_LINE = re.compile(r"^\s+(\S+)\s+base=(0x[0-9A-Fa-f]+)(?:\s+imagebase=(0x[0-9A-Fa-f]+))?"
                         r"(?:\s+size=(0x[0-9A-Fa-f]+))?(?:\s+timestamp=(0x[0-9A-Fa-f]+))?")
ADDRESS_LINE = re.compile(r"(0x[0-9A-Fa-f]+)\s+\(([^)]*)\)")


def parse_crash_log(path):
    """Records with their loaded modules and every (label, runtime address, module, RVA)."""
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()

    records = []
    record = None
    module_section = False
    for line in lines:
        header = RECORD_HEADER.match(line.strip())
        if header:
            record = {"header": line.strip(), "modules": {}, "addresses": []}
            records.append(record)
            module_section = False
            continue
        if record is None:
            continue
        if line.startswith("LoadedModules") or line.startswith("RuntimeModuleBases"):
            module_section = True
            continue
        if module_section:
            module = MODULE_LINE.match(line)
            if module:
                entry = record["modules"].setdefault(module.group(1), {})
                entry["base"] = int(module.group(2), 16)
                if module.group(3):
                    entry["imagebase"] = int(module.group(3), 16)
                if module.group(4):
                    entry["size"] = int(module.group(4), 16)
                if module.group(5):
                    entry["timestamp"] = int(module.group(5), 16)
                continue
            if line.strip() and not line.startswith(" "):
                module_section = False
        for match in ADDRESS_LINE.finditer(line):
            stripped = line.strip()
            slot = re.match(r"^\[(\d+)\]", stripped) or re.match(r"^#(\d+)", stripped)
            if slot:
                label = ("word" if stripped.startswith("[") else "frame") + slot.group(1)
            elif "=" in stripped:
                label = stripped.split("=")[0].strip()
            else:
                label = stripped
            module_text = match.group(2)
            module_name, _, offset = module_text.partition("+")
            rva = None
            if offset.startswith("0x"):
                try:
                    rva = int(offset, 16)
                except ValueError:
                    rva = None
            record["addresses"].append({
                "label": label,
                "address": int(match.group(1), 16),
                "module": module_name,
                "rva": rva,
            })
    return records


def is_system_module(name):
    lowered = name.lower()
    return any(token in lowered for token in (
        "nvoglv", "nvogl", "nvd3d", "amdvlk", "atio6axx", "atig6txx", "igd", "opengl32",
        "kernel32", "kernelbase", "ntdll", "user32", "gdi32", "dbghelp", "glfw", "libstdc++",
        "libgcc", "libwinpthread", "assimp"))


def describe(record, image, binutils, entry):
    module = entry["module"]
    if entry["rva"] is not None and image is not None:
        vma = rva_to_vma(image, entry["rva"])
        function, location = resolve_one(image, binutils, vma)
        return function, location, "rva+imagebase"
    if entry["rva"] is not None:
        return "??", "??:0", "rva without an image on disk"
    known = record["modules"].get(module)
    if known is None or image is None:
        return "??", "??:0", "no module identity"
    vma = entry["address"] - known["base"] + image["image_base"]
    function, location = resolve_one(image, binutils, vma)
    return function, location, "address-loadbase+imagebase"


def crash_log(args):
    binutils = find_binutils(args.binutils)
    path = args.path or DEFAULT_CRASH_LOG
    if not os.path.isfile(path):
        print("no crash record at %s" % path)
        return 1

    records = parse_crash_log(path)
    if not records:
        print("no complete crash record in %s" % path)
        return 1
    records = records[-args.last:]

    images = {}
    for name in os.listdir(BIN_DIR):
        if not name.lower().endswith((".dll", ".exe")):
            continue
        try:
            images[name] = read_pe(os.path.join(BIN_DIR, name))
        except ToolError:
            continue

    engine_frames = 0
    for record in records:
        print("\n%s" % record["header"])
        on_disk = None
        for name in ("libminEngined.dll", "libMaximumEditord.dll", "MaximumEditor.exe"):
            image = images.get(name)
            if image is None:
                continue
            on_disk = image
            break

        for entry in record["addresses"]:
            module = entry["module"]
            image = images.get(module)
            if image is None and module.startswith("libminEngine"):
                image = images.get("libminEngined.dll")
            if image is None and module.startswith("libMaximumEditor"):
                image = images.get("libMaximumEditord.dll")

            function, location, method = describe(record, image, binutils, entry)
            known = record["modules"].get(module)
            stale = ""
            if image is not None and known is not None and known.get("timestamp") is not None:
                if known["timestamp"] != image["timestamp"]:
                    stale = "  [STALE: log timestamp 0x%08X, file 0x%08X]" % (
                        known["timestamp"], image["timestamp"])

            if is_system_module(module):
                verdict = "driver/system"
            elif function.startswith("??"):
                verdict = "unresolved"
            else:
                verdict = "engine"
                engine_frames += 1

            print("  %-10s %-22s %s" % (entry["label"][:10], module, verdict))
            print("      0x%X -> %s  %s  (%s)%s"
                  % (entry["address"], function, location, method, stale))

    print("\nengine frames resolved: %d" % engine_frames)
    return 0


def addr(args):
    binutils = find_binutils(args.binutils)
    path = args.module if os.path.isfile(args.module) else os.path.join(BIN_DIR, args.module)
    image = read_pe(path)
    print("%s ImageBase=0x%X SizeOfImage=0x%X TimeDateStamp=0x%08X"
          % (image["name"], image["image_base"], image["size_of_image"], image["timestamp"]))

    value = int(args.address, 16)
    if args.rva:
        vma = rva_to_vma(image, value)
        method = "RVA + ImageBase"
    elif args.base:
        vma = value - int(args.base, 16) + image["image_base"]
        method = "address - load base + ImageBase"
    else:
        vma = value
        method = "address assumed to be a DWARF address already"

    if not (image["image_base"] <= vma < image["image_base"] + image["size_of_image"]):
        print("warning: 0x%X is outside this image; the address belongs to another binary" % vma)
    function, location = resolve_one(image, binutils, vma)
    print("0x%X -> %s  %s  (%s)" % (vma, function, location, method))
    return 0 if not function.startswith("??") else 1


def modules(args):
    for name in args.dll or list(DEFAULT_DLLS):
        path = name if os.path.isfile(name) else os.path.join(BIN_DIR, name)
        if not os.path.isfile(path):
            print("%-26s <not built>" % name)
            continue
        image = read_pe(path)
        text_section = next((s for s in image["sections"] if s["name"] == ".text"), None)
        print("%-26s ImageBase=0x%-11X SizeOfImage=0x%-9X TimeDateStamp=0x%08X .text=0x%X"
              % (image["name"], image["image_base"], image["size_of_image"], image["timestamp"],
                 text_section["vma"] if text_section else 0))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binutils", help="directory holding objdump and addr2line")
    subparsers = parser.add_subparsers(dest="command", required=True)

    check = subparsers.add_parser("self-check", help="prove the address conversion round-trips")
    check.add_argument("--dll", action="append", help="image to check (repeatable)")
    check.add_argument("--sample", type=int, default=300, help="functions to sample per image")
    check.add_argument("--strict", action="store_true", help="only identical names count")
    check.set_defaults(handler=self_check)

    log = subparsers.add_parser("crash-log", help="resolve the addresses of a crash record")
    log.add_argument("path", nargs="?", help="crash log to read (default: minEngine/bin/ed_crash.log)")
    log.add_argument("--last", type=int, default=3, help="how many records to resolve")
    log.set_defaults(handler=crash_log)

    single = subparsers.add_parser("addr", help="resolve one address")
    single.add_argument("address", help="address in hex")
    single.add_argument("--module", required=True, help="image the address belongs to")
    single.add_argument("--rva", action="store_true", help="the address is an RVA into the image")
    single.add_argument("--base", help="runtime load base of the image")
    single.set_defaults(handler=addr)

    table = subparsers.add_parser("modules", help="print image identity of the shipped binaries")
    table.add_argument("--dll", action="append")
    table.set_defaults(handler=modules)

    args = parser.parse_args(argv)
    try:
        return args.handler(args)
    except ToolError as error:
        print("error: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
