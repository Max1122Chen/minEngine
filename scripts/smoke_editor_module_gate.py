#!/usr/bin/env python3
"""BUILD-F04 S05: the editor game-module gate, proven on the real binaries.

The failure being eliminated: MaximumEditor.exe opens a project whose C++ game module it does not
link, "succeeds", and the operator reads the result as "my components are gone". A link step cannot
observe that, and neither can any static assertion - only launching the editor can. So this script
launches it, four times, over the behaviour matrix of BUILD-F04 design section 12 Q4:

  case            project declares      editor host built with   expected
  --------------- --------------------- ------------------------ -------------------------------
  asset-only      (nothing)             (none)                   boots, asset-only notice
  no-module       "MinCraft-gameplay"    (none)                   exit 3, recovery guidance
  mismatch        "NotTheStubModule"     "GameModuleStub"         exit 3, mismatch + guidance
  match           "GameModuleStub"      "GameModuleStub"         boots, no gate error

Plus one static case that needs no desktop:

  identity        both editor hosts import SetHostEditorGameModule from the editor DLL, i.e. the
                  compile-time identity injected by minengine_add_editor_executable() really
                  reaches the shared editor library. Remove the injection and this case goes red.

The temporary projects deliberately have an empty Assets directory: the gate is what is under test,
and a project with no assets also means this script writes nothing into the repository.

Usage (from the repo root):
  python scripts/smoke_editor_module_gate.py                # every case
  python scripts/smoke_editor_module_gate.py --case no-module
Exit code 0 only when every selected case behaves as the matrix says.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import uuid

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN_DIR = os.path.join(REPO_ROOT, "minEngine", "bin")
DEFAULT_PROJECT = os.path.join(REPO_ROOT, "minEngine", "MyMEProject", "MyMEProject.meproject")
EDITOR_DLL_NAMES = ("libMaximumEditord.dll", "libMaximumEditor.dll")

# Exit code of a refused start (Editor/src/EditorHostGameModule.h). Asserted exactly: "non-zero"
# would also accept a crash, and a crash is not the behaviour this slice promises.
EXIT_MODULE_REQUIREMENT_UNMET = 3
ASSET_ONLY_IDENTITY = "(none)"
GAME_MODULE_HOST = "GameModuleStub"
DECLARED_ONLY_ELSEWHERE = "MinCraft-gameplay"
DECLARED_MISMATCH = "NotTheStubModule"
NOT_BOOTED_TEXT = "refusing to open project"


def make_project(required_module, name="GateProbe"):
    """A throwaway project: descriptor + empty Assets dir, so nothing lands in the repo."""
    root = tempfile.mkdtemp(prefix="me-gate-%s-" % name.lower())
    os.makedirs(os.path.join(root, "Assets"), exist_ok=True)
    descriptor = {
        "$schemaVersion": 1,
        "ProjectName": name,
        "ProjectId": {"High": 111, "Low": 222},
        "ProjectRoot": root.replace("\\", "/"),
    }
    if required_module is not None:
        descriptor["RequiredGameModule"] = required_module
    path = os.path.join(root, "%s.meproject" % name)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(descriptor, handle, indent=4)
        handle.write("\n")
    return root, path


class Launch:
    """One editor process: stdout+stderr go to a file (never a pipe - the editor logs megabytes)."""

    def __init__(self, exe, project_path, work, timeout, want_ready_file):
        self.pipe_name = "me-gate-%d-%s" % (os.getpid(), uuid.uuid4().hex[:8])
        self.ready_file = os.path.join(work, "ready-%s.json" % uuid.uuid4().hex[:8])
        self.log_path = os.path.join(work, "editor-%s.log" % uuid.uuid4().hex[:8])
        self.exited_early = False
        command = [exe]
        if want_ready_file:
            command += ["--automation-pipe", self.pipe_name,
                        "--automation-ready-file", self.ready_file]
        command += ["-p", os.path.abspath(project_path)]
        self.command = command
        self._log = open(self.log_path, "wb")
        self.process = subprocess.Popen(command, cwd=os.path.dirname(os.path.abspath(exe)),
                                        stdout=self._log, stderr=subprocess.STDOUT)
        self.booted = False
        deadline = time.time() + timeout
        while time.time() < deadline:
            if want_ready_file and os.path.isfile(self.ready_file):
                self.booted = True
                break
            if self.process.poll() is not None:
                self.exited_early = True
                break
            time.sleep(0.2)

    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=20)
        self._log.close()
        with open(self.log_path, "rb") as handle:
            return handle.read().decode("utf-8", "replace")

    @property
    def returncode(self):
        return self.process.returncode


def static_identity_case(bin_dir):
    """No desktop needed: does each editor host import the identity setter from the editor DLL?"""
    sys.path.insert(0, os.path.join(REPO_ROOT, "scripts"))
    from audit_dll_exports import ImportTable  # noqa: E402  (repo tool, reused on purpose)

    problems = []
    editor_dll = next((name for name in EDITOR_DLL_NAMES
                       if os.path.isfile(os.path.join(bin_dir, name))), None)
    if editor_dll is None:
        return ["editor DLL missing in %s" % bin_dir], "(no editor DLL)"
    for host in ("MaximumEditor.exe", "SampleEditor.exe"):
        host_path = os.path.join(bin_dir, host)
        if not os.path.isfile(host_path):
            problems.append("%s missing" % host)
            continue
        imports = ImportTable(host_path)
        names = []
        for dll_name, dll_names in imports.per_dll.items():
            if "maximumeditor" in dll_name:
                names.extend(dll_names)
        if not any("SetHostEditorGameModule" in name for name in names):
            problems.append("%s does not import SetHostEditorGameModule from %s (identity "
                            "injection missing)" % (host, editor_dll))
    return problems, "both hosts import the identity setter"


def negative_case(bin_dir, exe_name, required_module, work, timeout, expected_host_identity,
                  label):
    """A declared module the host does not provide must be a hard stop with guidance."""
    problems = []
    exe = os.path.join(bin_dir, exe_name)
    if not os.path.isfile(exe):
        return ["%s missing" % exe_name], "(no launch)"
    root, path = make_project(required_module, name="Gate" + label)
    launch = Launch(exe, path, work, timeout, want_ready_file=True)
    text = launch.stop()
    code = launch.returncode
    shutil.rmtree(root, ignore_errors=True)

    if launch.booted:
        problems.append("editor booted (automation ready file appeared) - the gate did not fire")
    if code != EXIT_MODULE_REQUIREMENT_UNMET:
        problems.append("exit code %s, expected %d" % (code, EXIT_MODULE_REQUIREMENT_UNMET))
    for needle, why in (
            (required_module, "the required module name"),
            (expected_host_identity, "the editor's own identity"),
            ("minengine_add_editor_executable", "the how-to-build recovery step"),
            ("GAME_MODULE", "the GAME_MODULE argument in the recovery step"),
            ("<Project>Editor.exe", "the project-editor recovery step"),
            (NOT_BOOTED_TEXT, "an explicit refusal instead of a silent downgrade")):
        if needle not in text:
            problems.append("log is missing %s (%r)" % (why, needle))
    summary = "exit %s, %d log line(s)" % (code, text.count("\n"))
    return problems, summary


def positive_case(bin_dir, exe_name, project_path, work, timeout, required_module,
                  expected_identity, expect_notice, label):
    problems = []
    exe = os.path.join(bin_dir, exe_name)
    if not os.path.isfile(exe):
        return ["%s missing" % exe_name], "(no launch)"
    if required_module is None:
        temp_root = None
        path = project_path
    else:
        temp_root, path = make_project(required_module, name="Gate" + label)
    launch = Launch(exe, path, work, timeout, want_ready_file=True)
    text = launch.stop()
    if temp_root:
        shutil.rmtree(temp_root, ignore_errors=True)

    if not launch.booted:
        problems.append("editor did not reach the automation ready file (exit %s)"
                        % launch.returncode)
    if NOT_BOOTED_TEXT in text:
        problems.append("the gate refused a project it must accept")
    identity_line = "host game module identity '%s'" % expected_identity
    if identity_line not in text:
        problems.append("startup identity line %r not found" % identity_line)
    if expect_notice and "declares no RequiredGameModule" not in text:
        problems.append("the asset-only notice was not printed")
    summary = "booted=%s, identity %s" % (launch.booted, expected_identity)
    return problems, summary


def main(argv=None):
    parser = argparse.ArgumentParser(description="BUILD-F04 S05 editor module gate matrix.")
    parser.add_argument("--bin-dir", default=BIN_DIR)
    parser.add_argument("--project", default=DEFAULT_PROJECT,
                        help="project used by the asset-only case (no RequiredGameModule)")
    parser.add_argument("--timeout", type=float, default=180.0,
                        help="seconds to wait for a boot or an exit")
    parser.add_argument("--case", action="append", default=None,
                        choices=("all", "identity", "asset-only", "no-module", "mismatch", "match"),
                        help="run only these cases (default: all)")
    args = parser.parse_args(argv)

    selected = args.case or ["all"]
    cases = ["identity", "asset-only", "no-module", "mismatch", "match"] if "all" in selected \
        else selected
    work = tempfile.mkdtemp(prefix="me-gate-work-")
    failures = 0
    try:
        for case in cases:
            if case == "identity":
                problems, summary = static_identity_case(args.bin_dir)
            elif case == "asset-only":
                problems, summary = positive_case(
                    args.bin_dir, "MaximumEditor.exe", args.project, work, args.timeout,
                    required_module=None, expected_identity=ASSET_ONLY_IDENTITY,
                    expect_notice=True, label="AssetOnly")
            elif case == "no-module":
                problems, summary = negative_case(
                    args.bin_dir, "MaximumEditor.exe", DECLARED_ONLY_ELSEWHERE, work, args.timeout,
                    expected_host_identity=ASSET_ONLY_IDENTITY, label="NoModule")
            elif case == "mismatch":
                problems, summary = negative_case(
                    args.bin_dir, "SampleEditor.exe", DECLARED_MISMATCH, work, args.timeout,
                    expected_host_identity=GAME_MODULE_HOST, label="Mismatch")
            else:
                problems, summary = positive_case(
                    args.bin_dir, "SampleEditor.exe", None, work, args.timeout,
                    required_module=GAME_MODULE_HOST, expected_identity=GAME_MODULE_HOST,
                    expect_notice=False, label="Match")
            print("%s %-11s %s" % ("PASS" if not problems else "FAIL", case, summary))
            for problem in problems:
                print("     - %s" % problem, file=sys.stderr)
            failures += bool(problems)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("module gate matrix %s" % ("PASS" if failures == 0 else "FAIL"))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
