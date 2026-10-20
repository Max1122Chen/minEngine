#!/usr/bin/env python3
"""Assert the engine initializes its own GL function table before it uses it.

BUILD-F03 knife 1 removed the 5 glad entries from the engine export table, so the engine's glad
copy is no longer initialized by a consumer's gladLoadGLLoader call: glad's pointers live once per
MODULE, and the tests/hosts now carry their own copy. EnsureOpenGLFunctionTableLoaded() (see
Runtime/Function/Render/OpenGL/OpenGLFunctionTable.h) is how this DLL loads its own table.

That rule is only as good as the enforcement: a new gl* call in a function that forgot the guard
is a null-pointer call that only some paths hit. This script makes the rule MECHANICAL, so nobody
has to remember it during review:

  RULE 1  every function in the OpenGL backend that calls a gl* function must call
          EnsureOpenGLFunctionTableLoaded() before its first such call.
  RULE 2  no engine source outside the allowlist may call gl* at all; GLFWWindowSystem.cpp is
          allowlisted because it loads glad itself at window creation (verified below).
  RULE 3  the scan must find something. A gate that verified 0 files is not a pass.

Exit code 0 = the invariant holds, 1 = violation, 2 = the scan could not verify anything.
"""

import argparse
import os
import re
import sys

ENGINE_SRC = os.path.join("minEngine", "minEngine", "src")
# The engine's GL backend: guarded per function (RULE 1).
GL_BACKEND_DIR = os.path.join("Runtime", "Function", "Render", "OpenGL")
GUARD_CALL = "EnsureOpenGLFunctionTableLoaded()"
# GL files allowed outside the backend, with the reason their own loader is enough (RULE 2).
EXTRA_ALLOWED_GL_FILES = {
    os.path.join("Runtime", "Function", "Render", "GLFWWindowSystem.cpp"):
        "loads glad itself (gladLoadGLLoader) when it creates the GL context, before any use",
}
OWN_LOADER_CALL = "gladLoadGLLoader"

GL_CALL = re.compile(r"\b(gl[A-Z][A-Za-z0-9]*)\s*\(")
CONTROL = re.compile(r"\b(if|for|while|switch|catch)\s*\(")
SCAN_SUFFIXES = (".c", ".cc", ".cpp", ".h", ".hpp")


def strip_noise(line):
    """Line without string literals and // comments, so brace/paren counting is not fooled."""
    line = re.sub(r'"(\\.|[^"\\])*"', '""', line)
    index = line.find("//")
    return line[:index] if index >= 0 else line


def functions(lines):
    """[(open_idx, close_idx, name)] for every function-like block, nested scopes included.

    Brace tracking per scope frame is what lets namespaces and lambdas coexist with class-method
    definitions: a block is function-like when the text accumulated for the enclosing scope ends
    in a parameter list, which is exactly the shape of "T name(...) {" and of a lambda.
    """
    found = []
    frames = [{"header": "", "func": False, "open": -1, "name": None}]
    for index, raw in enumerate(lines):
        clean = strip_noise(raw)
        frames[-1]["header"] += " " + clean.strip()
        for char in clean:
            if char == "{":
                header = frames[-1]["header"]
                names = re.findall(r"([A-Za-z_~][\w:~]*)\s*\(", header)
                is_func = ("(" in header) and not CONTROL.search(header)
                frames.append({"header": "", "func": is_func, "open": index,
                               "name": names[-1] if names else "<lambda>"})
            elif char == "}":
                if len(frames) > 1:
                    frame = frames.pop()
                    if frame["func"]:
                        found.append((frame["open"], index, frame["name"]))
                frames[-1]["header"] = ""
            elif char == ";":
                frames[-1]["header"] = ""
    return found


def gl_functions(path, lines):
    """[(open_idx, close_idx, name, first_gl_idx, calls)] inside one file."""
    result = []
    for open_idx, close_idx, name in functions(lines):
        calls = []
        first = None
        for offset, line in enumerate(lines[open_idx + 1:close_idx], start=open_idx + 1):
            match = GL_CALL.search(strip_noise(line))
            if match:
                calls.append(match.group(1))
                if first is None:
                    first = offset
        if first is not None:
            result.append((open_idx, close_idx, name, first, calls))
    return result


def iter_sources(root):
    for base, dirs, files in os.walk(root):
        dirs[:] = sorted(name for name in dirs if name not in ("Third-Party", "Generated"))
        for name in sorted(files):
            if name.endswith(SCAN_SUFFIXES):
                yield os.path.join(base, name)


def display(path):
    return os.path.relpath(path, os.path.dirname(os.path.dirname(os.path.abspath(__file__)))).replace(os.sep, "/")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--src-root", default=None,
                        help="engine source root (default: %s)" % ENGINE_SRC)
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_root = args.src_root or os.path.join(repo_root, ENGINE_SRC)
    if not os.path.isdir(src_root):
        print("FAIL: engine source root not found: %s (nothing was verified)" % src_root,
              file=sys.stderr)
        return 2

    violations = []
    backend_files, gl_files, guarded_functions = [], [], 0
    for path in iter_sources(src_root):
        relative = os.path.relpath(path, src_root)
        lines = open(path, "r", encoding="utf-8", errors="replace").read().split("\n")
        in_backend = relative.startswith(GL_BACKEND_DIR)
        hits = gl_functions(path, lines)
        if hits:
            gl_files.append(relative)
        if in_backend and hits:
            backend_files.append(relative)
            for _open_idx, _close_idx, name, first_gl, calls in hits:
                prelude = "\n".join(lines[_open_idx + 1:first_gl + 1])
                if GUARD_CALL not in prelude:
                    violations.append("%s:%d: %s calls %s without %s%s first" % (
                        display(path), _open_idx + 1, name, calls[0], GUARD_CALL,
                        " - a null function pointer on the paths that reach it"))
                else:
                    guarded_functions += 1
        if hits and not in_backend and relative not in EXTRA_ALLOWED_GL_FILES:
            violations.append("%s: calls %s outside the guarded OpenGL backend - move it into %s/ "
                              "or document why this file loads the table itself" % (
                                  display(path), hits[0][4][0], GL_BACKEND_DIR))
    for relative, reason in EXTRA_ALLOWED_GL_FILES.items():
        path = os.path.join(src_root, relative)
        if not os.path.isfile(path):
            violations.append("%s: allowlisted GL file is missing - the allowlist entry (%s) is "
                              "stale" % (relative, reason))
            continue
        text = open(path, "r", encoding="utf-8", errors="replace").read()
        if OWN_LOADER_CALL not in text:
            violations.append("%s: allowlisted because it %s, but that call is gone" % (
                relative, reason))

    # RULE 3: a scan that verified nothing must not look like a pass.
    if not backend_files or guarded_functions == 0:
        print("FAIL: the scan verified nothing (backend files: %d, guarded functions: %d) - "
              "an empty scan is not a pass" % (len(backend_files), guarded_functions),
              file=sys.stderr)
        return 2

    if violations:
        print("FAIL: engine GL function-table load invariant violated (%d finding(s)):" % (
            len(violations)), file=sys.stderr)
        for violation in violations:
            print("  %s" % violation, file=sys.stderr)
        return 1

    if not args.quiet:
        print("PASS: %d function(s) in %d GL backend file(s) call %s before using glad" % (
            guarded_functions, len(backend_files), GUARD_CALL))
        print("PASS: no engine source outside the allowlist calls a gl* function (%d GL file(s) "
              "total)" % len(gl_files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
