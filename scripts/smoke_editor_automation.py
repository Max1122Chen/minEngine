#!/usr/bin/env python3
"""BUILD-F04 S04: launch the editor, prove it boots, quit it - a smoke that is not "it linked".

Why this exists: BUILD-F04 moved the imgui family from the engine DLL into the editor DLL. A link
step cannot tell a single ImGuiContext from two, so the slice needs at least one run of the real
editor process. This drives the Editor Automation Host (ED-F17 / CLI-F02):

  MaximumEditor.exe <project> --automation-pipe <name> --automation-ready-file <path>
  -> wait for the ready file (written after Listen, i.e. after the editor initialized)
  -> named-pipe JSON frames: editor.ping, editor.get_state, editor.get_capabilities, editor.get_log
  -> kill the process (the automation surface has no quit op; a smoke does not need one)

It is NOT headless: the editor still creates its window and needs a desktop. There is no
--headless / -batch editor mode in this repo (the "test" subcommand runs the headless TEST suite,
not the editor), so this is the closest thing to an automated startup check.

Usage (from the repo root):
  python scripts/smoke_editor_automation.py --project minEngine/MyMEProject/MyMEProject.meproject
Exit code 0 only when the editor answered every request and reported no automation errors.
"""

import argparse
import json
import os
import struct
import subprocess
import sys
import tempfile
import time
import uuid

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_EDITOR = os.path.join(REPO_ROOT, "minEngine", "bin", "MaximumEditor.exe")
PROTOCOL_VERSION = 1


def send_request(pipe, op, kind="query", payload=None, timeout=30.0, session_id=""):
    request = {
        "protocolVersion": PROTOCOL_VERSION,
        "requestId": uuid.uuid4().hex,
        "kind": kind,
        "op": op,
        "sessionId": session_id,
        "payload": payload or {},
    }
    body = json.dumps(request).encode("utf-8")
    pipe.write(struct.pack("<I", len(body)) + body)
    header = read_exact(pipe, 4, timeout)
    if header is None:
        raise RuntimeError("no response frame header for %s" % op)
    (length,) = struct.unpack("<I", header)
    raw = read_exact(pipe, length, timeout)
    if raw is None:
        raise RuntimeError("truncated response frame for %s" % op)
    return json.loads(raw.decode("utf-8"))


def read_exact(pipe, count, timeout):
    deadline = time.time() + timeout
    chunks, got = [], 0
    while got < count:
        if time.time() > deadline:
            return None
        chunk = pipe.read(count - got)
        if not chunk:
            time.sleep(0.01)
            continue
        chunks.append(chunk)
        got += len(chunk)
    return b"".join(chunks)


def summarize(result, limit=220):
    """One-line shape of an automation result: counts for lists, keys for objects."""
    if result is None:
        return "(no result)"
    if isinstance(result, list):
        return "%d item(s)" % len(result)
    if isinstance(result, dict):
        parts = []
        for key in sorted(result):
            value = result[key]
            if isinstance(value, list):
                parts.append("%s=%d" % (key, len(value)))
            elif isinstance(value, (dict, str, int, float, bool)) or value is None:
                parts.append("%s=%s" % (key, str(value)[:40]))
        text = " ".join(parts)
    else:
        text = str(result)
    return text[:limit]


def connect(pipe_name, timeout):
    path = "\\\\.\\pipe\\" + pipe_name
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        try:
            return open(path, "r+b", buffering=0)
        except OSError as exc:
            last = exc
            time.sleep(0.2)
    raise RuntimeError("could not connect to %s: %s" % (path, last))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Editor automation smoke (BUILD-F04 S04).")
    parser.add_argument("--editor", default=DEFAULT_EDITOR)
    parser.add_argument("--project", default=os.path.join(
        REPO_ROOT, "minEngine", "MyMEProject", "MyMEProject.meproject"))
    parser.add_argument("--rhi", default=None, choices=("opengl", "vulkan"))
    parser.add_argument("--timeout", type=float, default=180.0,
                        help="seconds to wait for the ready file (editor init + asset scan)")
    parser.add_argument("--keep-running", action="store_true",
                        help="leave the editor running (interactive debugging)")
    args = parser.parse_args(argv)

    if not os.path.isfile(args.editor):
        print("error: editor not found: %s" % args.editor, file=sys.stderr)
        return 2

    work = tempfile.mkdtemp(prefix="me-smoke-")
    ready_file = os.path.join(work, "ready.json")
    pipe_name = "me-smoke-%d-%s" % (os.getpid(), uuid.uuid4().hex[:8])
    # Option order matters: the CLI rejects flags that follow a bare positional project path
    # ("The following arguments were not expected"), so the project goes through -p at the end.
    command = [args.editor]
    if args.rhi:
        command += ["--rhi", args.rhi]
    command += ["--automation-pipe", pipe_name, "--automation-ready-file", ready_file]
    if os.path.isfile(args.project):
        command += ["-p", os.path.abspath(args.project)]

    print("launch: %s" % " ".join(command))
    process = subprocess.Popen(command, cwd=os.path.dirname(os.path.abspath(args.editor)),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    status = 1
    try:
        deadline = time.time() + args.timeout
        while time.time() < deadline:
            if os.path.isfile(ready_file):
                break
            if process.poll() is not None:
                print("FAIL: editor exited before writing the ready file (code %d)" % (
                    process.returncode), file=sys.stderr)
                return 1
            time.sleep(0.2)
        else:
            print("FAIL: ready file not written within %.0fs" % args.timeout, file=sys.stderr)
            return 1
        print("ready: %s" % open(ready_file, encoding="utf-8").read().strip())

        with connect(pipe_name, 30.0) as pipe:
            failures = 0
            sessions = []
            for op in ("editor.ping", "editor.get_state", "editor.get_capabilities",
                       "editor.get_log"):
                try:
                    response = send_request(pipe, op)
                except (RuntimeError, ValueError) as exc:
                    print("FAIL %-24s %s" % (op, exc), file=sys.stderr)
                    failures += 1
                    continue
                ok = response.get("ok")
                result = response.get("result")
                print("%s %-24s %s" % ("PASS" if ok else "FAIL", op, summarize(result)))
                if ok and op == "editor.get_state":
                    sessions = [s.get("sessionId", s.get("id", ""))
                                for s in (result or {}).get("sessions", []) or []
                                if isinstance(s, dict)]
                if not ok:
                    failures += 1
                    for error in response.get("errors", []):
                        print("    %s: %s" % (error.get("code"), error.get("message")),
                              file=sys.stderr)
            # The editor's UI is drawn around a live scene session and a live reflection registry;
            # querying both proves the process reached a steady frame loop with imgui initialized.
            probes = [("reflect.list_classes", "", None)]
            if sessions:
                probes.append(("scene.get_hierarchy", sessions[0], None))
            for op, session_id, payload in probes:
                try:
                    response = send_request(pipe, op, session_id=session_id, payload=payload)
                except (RuntimeError, ValueError) as exc:
                    print("FAIL %-24s %s" % (op, exc), file=sys.stderr)
                    failures += 1
                    continue
                ok = response.get("ok")
                print("%s %-24s %s" % ("PASS" if ok else "FAIL", op,
                                       summarize(response.get("result"))))
                if not ok:
                    failures += 1
                    for error in response.get("errors", []):
                        print("    %s: %s" % (error.get("code"), error.get("message")),
                              file=sys.stderr)
            status = 0 if failures == 0 else 1
    finally:
        if args.keep_running:
            print("editor left running, pid %d" % process.pid)
        elif process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
            print("editor stopped (pid %d)" % process.pid)
    print("smoke %s" % ("PASS" if status == 0 else "FAIL"))
    return status


if __name__ == "__main__":
    sys.exit(main())
