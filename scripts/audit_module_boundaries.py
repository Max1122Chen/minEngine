#!/usr/bin/env python3
"""BUILD-F04 S01: editor/module boundary baseline + assertions.

Two uses (see docs/ai/Platform/ProductBuild/BUILD-F04_S01_S03_EDITOR_DLL_IMPLEMENTATION.md):

  --record-baseline PATH   freeze the measured state (engine export count, editor host
                           sizes, per-host engine / editor-DLL import counts) as JSON
  --assert --fail          re-measure and assert the boundary still holds

Anti-false-green rule of this script: every assertion starts with "does the artifact exist".
A missing artifact is a FAILURE, never a skipped check - a vacuously-passing boundary check is
exactly the failure this slice exists to prevent (same shape as BUILD-F02 S01 / F03 Q8).

The PE primitives are imported from scripts/audit_dll_exports.py, not re-implemented.
"""

import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from audit_dll_exports import (  # noqa: E402  (path set up above)
    ExportTable,
    ImportTable,
    PEError,
    classify,
    display_path,
)

REPO_ROOT = os.path.dirname(HERE)
BIN_DIR = os.path.join(REPO_ROOT, "minEngine", "bin")
DEFAULT_BASELINE = os.path.join(
    REPO_ROOT, "docs", "ai", "Platform", "ProductBuild", "BUILD-F04_S01_BASELINE.json")

# Debug and release spellings of both artifacts. The build is MinGW Debug today (CMAKE_DEBUG_POSTFIX=d),
# so the contract must not hard-code one spelling.
ENGINE_DLL_NAMES = ("libminEngined.dll", "libminEngine.dll")
EDITOR_DLL_NAMES = ("libMaximumEditord.dll", "libMaximumEditor.dll")
EDITOR_HOSTS = ("MaximumEditor.exe", "SampleEditor.exe")
# Every editor host must reach the editor DLL; S02 measured exactly one entry symbol.
MIN_EDITOR_DLL_IMPORTS = 1


def find_bin(bin_dir, names):
    """First existing artifact among the accepted spellings (None when absent)."""
    for name in names:
        path = os.path.join(bin_dir, name)
        if os.path.isfile(path):
            return path
    return None


def engine_export_report(engine_path):
    exports = ExportTable(engine_path)
    names = exports.name_set()
    return {
        "dll": os.path.basename(engine_path),
        "exports": exports.number_of_functions,
        "max_named_ordinal": exports.max_ordinal,
        "headroom": exports.headroom,
        "imgui_exports": sum(1 for name in names if classify(name) == "imgui"),
    }


def editor_dll_report(editor_path):
    if editor_path is None:
        return {"name": EDITOR_DLL_NAMES[0], "present": False}
    exports = ExportTable(editor_path)
    names = exports.name_set()
    return {
        "name": os.path.basename(editor_path),
        "present": True,
        "bytes": os.path.getsize(editor_path),
        "exports": exports.number_of_functions,
        "max_named_ordinal": exports.max_ordinal,
        "headroom": exports.headroom,
        "export_names": sorted(names),
        "imgui_exports": sum(1 for name in names if classify(name) == "imgui"),
    }


def host_report(host_path, engine_name):
    table = ImportTable(host_path)
    engine_names = table.names_for(engine_name)
    editor_names = []
    for module, names in table.per_dll.items():
        if module in {name.lower() for name in EDITOR_DLL_NAMES}:
            editor_names.extend(names)
    return {
        "name": os.path.basename(host_path),
        "bytes": os.path.getsize(host_path),
        "engine_imports": len(engine_names),
        "imgui_imports": sum(1 for name in engine_names if classify(name) == "imgui"),
        "editor_dll_imports": len(editor_names),
        "editor_dll_symbols": sorted(set(editor_names)),
        "mtime": int(os.path.getmtime(host_path)),
    }


def measure(bin_dir):
    engine_path = find_bin(bin_dir, ENGINE_DLL_NAMES)
    if engine_path is None:
        raise SystemExit("error: no engine DLL (%s) under %s" % (
            " / ".join(ENGINE_DLL_NAMES), bin_dir))
    hosts = []
    for host_name in EDITOR_HOSTS:
        host_path = os.path.join(bin_dir, host_name)
        if not os.path.isfile(host_path):
            raise SystemExit("error: editor host missing: %s" % host_path)
        hosts.append(host_report(host_path, os.path.basename(engine_path)))
    return {
        "engine": engine_export_report(engine_path),
        "editor_dll": editor_dll_report(find_bin(bin_dir, EDITOR_DLL_NAMES)),
        "editor_hosts": hosts,
    }


def load_previous(path):
    if not os.path.isfile(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return {}


def record_baseline(path, bin_dir, stage, editor_export_cap, host_size_cap):
    state = measure(bin_dir)
    previous = load_previous(path)
    before = {host["name"]: host for host in previous.get("editor_hosts", [])}
    editor_dll = {key: value for key, value in state["editor_dll"].items()
                  if key != "export_names"}
    editor_dll.update({
        "required": stage == "editor-dll",
        "max_exports": editor_export_cap,
        "min_editor_dll_imports": MIN_EDITOR_DLL_IMPORTS,
        "required_symbols": ["CreateApplication"],
    })
    hosts = []
    for host in state["editor_hosts"]:
        entry = {key: value for key, value in host.items() if key != "mtime"}
        prior = before.get(host["name"], {})
        # Recorded state is the new ceiling: this slice may only shrink these numbers.
        entry["max_bytes"] = host_size_cap if stage == "editor-dll" else host["bytes"]
        entry["max_engine_imports"] = host["engine_imports"]
        entry["bytes_before"] = prior.get("bytes", host["bytes"])
        entry["engine_imports_before"] = prior.get("engine_imports", host["engine_imports"])
        entry["imgui_imports_before"] = prior.get("imgui_imports", host["imgui_imports"])
        hosts.append(entry)
    manifest = {
        "id": "BUILD-F04-S01",
        "recorded_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "stage": stage,
        "bin_dir": display_path(bin_dir),
        "engine": state["engine"],
        "editor_dll": editor_dll,
        "editor_hosts": hosts,
    }
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")
    print("wrote baseline %s (stage=%s)" % (display_path(path), stage))
    print_hosts(manifest)
    return 0


def print_hosts(manifest):
    for host in manifest["editor_hosts"]:
        print("  %-20s %12d B (was %12d)  engine_imports %4d (was %4d)  imgui %3d -> %d  "
              "editor_dll %d" % (
                  host["name"], host.get("bytes", 0), host.get("bytes_before", 0),
                  host.get("engine_imports", 0), host.get("engine_imports_before", 0),
                  host.get("imgui_imports_before", 0), host.get("imgui_imports", 0),
                  host.get("editor_dll_imports", 0)))
    editor = manifest["editor_dll"]
    if editor.get("present"):
        print("  %-20s %12d B  exports %d" % (editor["name"], editor["bytes"], editor["exports"]))
    else:
        print("  %-20s %12s" % (editor["name"], "absent"))


class Checks:
    def __init__(self):
        self.failures = []

    def ok(self, label, detail=""):
        print("  PASS %-46s %s" % (label, detail))

    def fail(self, label, detail):
        print("  FAIL %-46s %s" % (label, detail))
        self.failures.append("%s: %s" % (label, detail))

    def require(self, condition, label, detail):
        if condition:
            self.ok(label, detail)
        else:
            self.fail(label, detail)
        return condition


def assert_boundary(path, bin_dir):
    if not os.path.isfile(path):
        print("FAIL: baseline manifest not found: %s" % display_path(path), file=sys.stderr)
        return 1
    with open(path, "r", encoding="utf-8") as handle:
        baseline = json.load(handle)
    stage = baseline.get("stage")
    if stage not in ("static-editor", "editor-dll"):
        print("FAIL: baseline stage %r is not static-editor|editor-dll" % (stage), file=sys.stderr)
        return 1

    checks = Checks()
    print("BUILD-F04 boundary assertions (baseline %s, stage=%s):" % (
        display_path(path), stage))

    # --- 1. the artifacts must exist before anything is measured --------------------
    engine_path = find_bin(bin_dir, ENGINE_DLL_NAMES)
    if not checks.require(engine_path is not None, "engine DLL exists",
                          engine_path and display_path(engine_path) or "MISSING"):
        report(checks)
        return 1

    # --- 2. engine DLL is no longer count-gated ----------------------------------
    # Explicit exports (MODULE_API + auto->whitelist->explicit, BUILD-F03 knife 2) are the
    # living contract for the engine symbol surface; a frozen baseline count is obsolete.
    # Engine existence is asserted above; the forbidden third-party bucket audit runs in
    # audit_dll_exports.py (glad/glfw/imgui must never reach the export table).

    # --- 3. editor DLL ------------------------------------------------------------
    editor_spec = baseline["editor_dll"]
    editor_path = find_bin(bin_dir, EDITOR_DLL_NAMES)
    editor_present = editor_path is not None
    if stage == "editor-dll":
        if not checks.require(editor_present, "editor DLL exists (S02 artifact)",
                              editor_path and display_path(editor_path) or "MISSING"):
            report(checks)
            return 1
        editor = editor_dll_report(editor_path)
        max_exports = editor_spec["max_exports"]
        checks.require(editor["exports"] <= max_exports, "editor DLL exports within budget",
                       "%d <= %d" % (editor["exports"], max_exports))
        for marker in editor_spec.get("required_symbols", []):
            hit = [name for name in editor["export_names"] if marker in name]
            checks.require(bool(hit), "editor DLL exports %s" % marker,
                           hit[0] if hit else "NOT EXPORTED")
        checks.require(editor["imgui_exports"] == 0, "editor DLL leaks no imgui export",
                       str(editor["imgui_exports"]))
    else:
        checks.ok("editor DLL not required at stage static-editor",
                  "present" if editor_present else "absent")

    # --- 4. editor hosts ----------------------------------------------------------
    expected_hosts = {host["name"]: host for host in baseline["editor_hosts"]}
    checks.require(sorted(expected_hosts) == sorted(EDITOR_HOSTS),
                   "baseline covers both editor hosts", ", ".join(sorted(expected_hosts)))
    editor_dll_mtime = os.path.getmtime(editor_path) if editor_present else 0
    for host_name in EDITOR_HOSTS:
        host_path = os.path.join(bin_dir, host_name)
        if not checks.require(os.path.isfile(host_path), "%s exists" % host_name,
                              host_path if os.path.isfile(host_path) else "MISSING"):
            continue
        spec = expected_hosts.get(host_name, {})
        host = host_report(host_path, os.path.basename(engine_path))
        max_bytes = spec.get("max_bytes")
        if max_bytes is not None:
            checks.require(host["bytes"] <= max_bytes, "%s size within cap" % host_name,
                           "%d <= %d B (baseline before %d B)" % (
                               host["bytes"], max_bytes, spec.get("bytes_before", 0)))
        max_engine = spec.get("max_engine_imports")
        if max_engine is not None:
            checks.require(host["engine_imports"] <= max_engine,
                           "%s engine imports did not increase" % host_name,
                           "%d <= %d" % (host["engine_imports"], max_engine))
        if stage == "editor-dll":
            checks.require(host["imgui_imports"] == 0,
                           "%s imports no imgui directly" % host_name,
                           str(host["imgui_imports"]))
            checks.require(host["editor_dll_imports"] >= spec.get(
                "min_editor_dll_imports", MIN_EDITOR_DLL_IMPORTS),
                "%s imports the editor DLL" % host_name,
                "%d symbol(s)" % host["editor_dll_imports"])
            if editor_dll_mtime:
                # A host older than the editor DLL is a stale artifact: exactly the shape that
                # lets a 188 MB exe keep passing a size assertion it no longer deserves.
                checks.require(host["mtime"] >= editor_dll_mtime,
                               "%s is not stale" % host_name,
                               "host mtime >= editor DLL mtime")
    report(checks)
    return 0 if not checks.failures else 1


def report(checks):
    if checks.failures:
        print("FAIL: %d boundary assertion(s) violated" % len(checks.failures),
              file=sys.stderr)
        for failure in checks.failures:
            print("  - %s" % failure, file=sys.stderr)
    else:
        print("PASS: editor/module boundary holds")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="BUILD-F04 S01 editor/module boundary baseline and assertions.")
    parser.add_argument("--baseline", default=DEFAULT_BASELINE,
                        help="baseline manifest (default: %s)" % display_path(DEFAULT_BASELINE))
    parser.add_argument("--bin-dir", default=BIN_DIR, help="artifact directory")
    parser.add_argument("--record-baseline", action="store_true",
                        help="write the baseline manifest from the artifacts on disk")
    parser.add_argument("--stage", default="editor-dll",
                        choices=("static-editor", "editor-dll"),
                        help="stage recorded with --record-baseline (default editor-dll)")
    parser.add_argument("--assert", dest="do_assert", action="store_true",
                        help="assert the boundary described by the baseline")
    parser.add_argument("--fail", action="store_true",
                        help="exit non-zero on any violated assertion (required with --assert)")
    parser.add_argument("--editor-export-cap", type=int, default=8,
                        help="max editor DLL export entries while recording (O6: <= 8)")
    parser.add_argument("--host-size-cap", type=int, default=16 * 1024 * 1024,
                        help="max bytes per editor host while recording")
    args = parser.parse_args(argv)

    if args.record_baseline:
        return record_baseline(args.baseline, args.bin_dir, args.stage,
                               args.editor_export_cap, args.host_size_cap)

    if args.do_assert:
        return assert_boundary(args.baseline, args.bin_dir)

    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
