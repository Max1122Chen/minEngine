#!/usr/bin/env python3
"""Render thread must never read game-thread component state.

The render pipeline runs against scene proxies, which are the game thread's mirror of the
components it owns. A pass that dereferences a component instead reads live state that the game
thread may be writing, and it silently couples every render pass to component internals.

This gate makes the boundary mechanical:

  rule 1  Runtime/Function/Render/RenderPipeline/** may not call a component's state query
          (IsActive / IsActiveAndVisible / IsVisible and their *Self / *InHierarchy spellings).
  rule 2  The only file allowed to ask a component that question is RenderScene.cpp, which is the
          game thread's update path: it is where the answer is copied onto the proxy. A missing
          whitelist file is a failure, because a rename would otherwise disable the rule silently.
  rule 3  A scan that matches nothing is a failure. The audit proves its own pattern still bites
          and that the proxy fields it guards are still written by the game thread and read by the
          pipeline, so no path or regexp drift can turn this into a vacuous pass.

Usage (from the repo root):  python scripts/audit_render_thread_state.py
Exit code is 0 only when every rule holds.
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(HERE)
RENDER_ROOT = os.path.join(
    REPO_ROOT, "minEngine", "minEngine", "src", "Runtime", "Function", "Render")
PIPELINE_ROOT = os.path.join(RENDER_ROOT, "RenderPipeline")

SOURCE_EXTENSIONS = (".h", ".hpp", ".inl", ".cpp")

# Any arrow call into a component's presentation state. Covered spellings: IsActive, IsActiveSelf,
# IsActiveInHierarchy, IsVisible, IsVisibleSelf, IsVisibleInHierarchy, IsActiveAndVisible.
COMPONENT_STATE_CALL = re.compile(r"->\s*Is(?:Active|Visible)(?:AndVisible|Self|InHierarchy)?\s*\(")

# Proxy fields that carry the game thread's decision into the render thread.
ENABLED_WRITE = re.compile(r"->\s*m_Enabled\s*=")
VISIBLE_WRITE = re.compile(r"->\s*m_bVisible\s*=(?!=)")
ENABLED_READ = re.compile(r"->\s*m_Enabled\b(?!\s*=[^=])")
VISIBLE_READ = re.compile(r"->\s*m_bVisible\b(?!\s*=[^=])")

# The game thread's update path. Registration follows the component being active; the proxy's own
# bit follows active-and-visible, and it is written here.
WHITELIST = {
    "RenderScene.cpp": "game-thread update path; publishes the proxy bit the pipeline reads",
}


def display(path):
    try:
        return os.path.relpath(path, REPO_ROOT).replace("\\", "/")
    except ValueError:
        return path.replace("\\", "/")


def iter_sources(root):
    for walk_root, _dirs, files in os.walk(root):
        for name in sorted(files):
            if name.endswith(SOURCE_EXTENSIONS):
                yield os.path.join(walk_root, name)


def scan_line(pattern, path):
    hits = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for number, line in enumerate(handle, start=1):
            if pattern.search(line):
                hits.append((number, line.strip()))
    return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verbose", action="store_true", help="list every allowed hit")
    parser.add_argument("--render-root", default=RENDER_ROOT,
                        help="scan a different Render directory (self-test of the empty-scan rule)")
    args = parser.parse_args()

    render_root = os.path.abspath(args.render_root)
    pipeline_root = os.path.join(render_root, "RenderPipeline")

    failures = []
    files = list(iter_sources(render_root)) if os.path.isdir(render_root) else []
    pipeline_files = [p for p in files if p.startswith(pipeline_root + os.sep)]

    if not os.path.isdir(render_root):
        failures.append("scan root is missing: {}".format(display(render_root)))
    if not files:
        failures.append("scan root holds no C++ source: {}".format(display(render_root)))
    if not pipeline_files:
        failures.append("no pipeline source found: {}".format(display(pipeline_root)))

    # rule 2: the whitelist has to exist, otherwise the allowance is stale and silent.
    for relative, reason in WHITELIST.items():
        path = os.path.join(render_root, relative)
        if not os.path.isfile(path):
            failures.append("whitelist file is missing: {} ({})".format(relative, reason))

    pipeline_hits = []
    whitelist_hits = []
    stray_hits = []
    for path in files:
        if not os.path.isfile(path):
            continue
        name = os.path.basename(path)
        allowed = name in WHITELIST and os.path.dirname(path) == render_root
        for number, text in scan_line(COMPONENT_STATE_CALL, path):
            entry = (display(path), number, text)
            if not allowed:
                if path.startswith(pipeline_root + os.sep):
                    pipeline_hits.append(entry)
                else:
                    stray_hits.append(entry)
            else:
                whitelist_hits.append(entry)

    # rule 1 / rule 2: nothing outside the game-thread update path may ask a component.
    for path, number, text in pipeline_hits:
        failures.append("{}:{}: render pipeline reads component state: {}".format(path, number, text))
    for path, number, text in stray_hits:
        failures.append("{}:{}: render source outside the whitelist reads component state: {}".format(
            path, number, text))

    # rule 3: prove the audit is not passing because it matched nothing.
    if not whitelist_hits:
        failures.append(
            "the pattern matches no component state call in {}, so this audit is vacuous".format(
                "/".join(sorted(WHITELIST))))

    scene_path = os.path.join(render_root, "RenderScene.cpp")
    if os.path.isfile(scene_path):
        writes = len(scan_line(ENABLED_WRITE, scene_path)) + len(scan_line(VISIBLE_WRITE, scene_path))
        if writes == 0:
            failures.append(
                "{} writes no proxy draw bit, so the state never reaches the pipeline".format(
                    display(scene_path)))

    reads = 0
    for path in pipeline_files:
        reads += len(scan_line(ENABLED_READ, path)) + len(scan_line(VISIBLE_READ, path))
    if reads == 0:
        failures.append(
            "no pipeline source reads a proxy draw bit, so the read-only path is gone")

    print("render-thread-state: {} source files scanned, {} under RenderPipeline".format(
        len(files), len(pipeline_files)))
    print("  rule 1 component state reads in the render pipeline: {}".format(len(pipeline_hits)))
    print("  rule 2 reads outside the whitelist: {}".format(len(stray_hits)))
    print("  rule 3 game-thread publish sites: {}, pipeline proxy reads: {}".format(
        writes if os.path.isfile(scene_path) else 0, reads))
    if args.verbose:
        for path, number, text in whitelist_hits:
            print("  allowed {}:{}: {}".format(path, number, text))

    if failures:
        print("")
        for failure in failures:
            print("FAIL: {}".format(failure))
        print("")
        print("Render passes must read the proxy, not the component. Publish the state on the "
              "proxy from RenderScene.cpp and read the proxy field instead.")
        return 1

    print("ok: the render pipeline reads proxy state only")
    return 0


if __name__ == "__main__":
    sys.exit(main())
