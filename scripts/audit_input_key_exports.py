#!/usr/bin/env python3
"""Assert every key identity the InputKeys header declares is actually exported by the DLL.

The header is the source of truth for which keys exist; the DLL export table is the source of
truth for which of them a consumer can link against. Declaring a key and forgetting to export it
compiles cleanly on the engine side and fails only in a consumer, one vague link error per key,
so the two lists are compared here instead of being noticed downstream.

Exit 1 on any of: a declared key missing from the export table, zero declared keys parsed, or
zero exports read from the DLL. An empty scan must never look like a pass.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audit_dll_exports import ExportTable, PEError  # noqa: E402  (path set above)

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_HEADER = os.path.join("minEngine", "minEngine", "src", "Runtime", "Function", "Input",
                             "InputKeys.h")
DEFAULT_DLL = os.path.join("minEngine", "bin", "libminEngined.dll")
NAMESPACE = "minEngine"
CLASS = "InputKeys"

# "static const InputKey Key_F1;" - the storage specifier, the type and the member name.
DECL_RE = re.compile(r"^\s*static\s+const\s+InputKey\s+([A-Za-z_]\w*)\s*;")


def parse_declared_keys(header_path):
    """Member names of every static const InputKey declaration, in declaration order."""
    with open(header_path, "r", encoding="utf-8") as handle:
        text = handle.read()
    names = []
    for line in text.splitlines():
        match = DECL_RE.match(line)
        if match and match.group(1) not in names:
            names.append(match.group(1))
    return names


def mangle(member):
    """Itanium name of a namespace-scope static data member of the key class."""
    return "_ZN%d%s%d%s%d%sE" % (len(NAMESPACE), NAMESPACE, len(CLASS), CLASS,
                                 len(member), member)


def demangle_member(symbol):
    """Member name of a static data member of the key class, or None for anything else.

    The declared length prefix is checked, not guessed: without it every exported member
    function of the class ("...6AddKeyERKNS_8InputKeyE") would parse as a data member named
    "AddKey..." and the two lists could never be compared by count.
    """
    prefix = "_ZN%d%s%d%s" % (len(NAMESPACE), NAMESPACE, len(CLASS), CLASS)
    if not symbol.startswith(prefix):
        return None
    rest = symbol[len(prefix):]
    index = 0
    while index < len(rest) and rest[index].isdigit():
        index += 1
    if index == 0:
        return None
    length = int(rest[:index])
    name = rest[index:index + length]
    tail = rest[index + length:]
    if tail != "E" or len(name) != length:
        return None
    return name


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Assert every InputKeys member declared in the header is exported.")
    parser.add_argument("--header", default=os.path.join(REPO_ROOT, DEFAULT_HEADER),
                        help="InputKeys header (default: the engine copy)")
    parser.add_argument("--dll", default=os.path.join(REPO_ROOT, DEFAULT_DLL),
                        help="target DLL carrying the export contract")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    if not os.path.isfile(args.header):
        print("FAIL: header not found: %s" % args.header, file=sys.stderr)
        return 1
    if not os.path.isfile(args.dll):
        print("FAIL: DLL not found: %s" % args.dll, file=sys.stderr)
        return 1

    declared = parse_declared_keys(args.header)
    if not declared:
        print("FAIL: no 'static const InputKey <Name>;' declaration found in %s - nothing was "
              "checked" % args.header, file=sys.stderr)
        return 1

    try:
        exports = ExportTable(args.dll)
    except PEError as exc:
        print("FAIL: cannot read the export table: %s" % exc, file=sys.stderr)
        return 1
    if not exports.names:
        print("FAIL: %s exports 0 names - nothing was checked" % args.dll, file=sys.stderr)
        return 1

    exported = set(exports.name_set())
    exported_members = {}
    for symbol in exported:
        member = demangle_member(symbol)
        if member is not None:
            exported_members[member] = symbol

    missing = [name for name in declared if name not in exported_members]
    removed_from_header = sorted(set(exported_members) - set(declared))
    key_count = sum(1 for name in declared if name.startswith("Key_"))

    if not args.quiet:
        print("header ......... %s" % os.path.relpath(args.header, REPO_ROOT).replace(os.sep, "/"))
        print("  declared ..... %d keys (%d named Key_*, %d other InputKeys)" % (
            len(declared), key_count, len(declared) - key_count))
        print("DLL ............ %s" % os.path.relpath(args.dll, REPO_ROOT).replace(os.sep, "/"))
        print("  exported ..... %d keys (%d exports in total)" % (
            len(exported_members), len(exported)))
        if removed_from_header:
            print("  NOTE: %d exported key(s) are no longer declared in the header; refresh the "
                  "header or delete the symbols:" % len(removed_from_header))
            for name in removed_from_header:
                print("      undeclared: %s" % name)

    if missing:
        print("FAIL: %d declared key(s) are not exported by %s:" % (len(missing), args.dll),
              file=sys.stderr)
        for name in missing:
            print("  missing: %s (%s)" % (name, mangle(name)), file=sys.stderr)
        print("      Declaring a key in the header does not export it. The class carries "
              "MINENGINE_API, so a missing entry means the key is not reachable from the "
              "declaration the consumer includes.", file=sys.stderr)
        return 1

    if len(exported_members) != len(declared):
        print("FAIL: %d keys are exported but %d are declared - the two lists must match" % (
            len(exported_members), len(declared)), file=sys.stderr)
        return 1

    if not args.quiet:
        print("PASS: all %d declared keys are exported (count matches: %d == %d)" % (
            len(declared), len(exported_members), len(declared)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
