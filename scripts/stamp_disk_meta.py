"""Stamp $schemaVersion / $engineVersion onto JSON engine asset roots (CORE-F18)."""
from __future__ import annotations

import json
from pathlib import Path

ROOTS = [
    Path(r"d:/Dev/GitRepo/minEngine/minEngine"),
    Path(r"d:/Dev/GitRepo/minEngine/Launcher/Templates"),
]
EXTENSIONS = {
    ".mescene",
    ".memtl",
    ".meenv",
    ".meproject",
    ".mesettings",
    ".meconfig",
    ".meshader",
    ".meta",  # AssetManager sidecar JSON (also goes through Serializer)
}
SCHEMA = 1
ENGINE = "0.0.9"


def stamp(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    data = json.loads(text)
    if not isinstance(data, dict):
        print(f"SKIP non-object {path}")
        return False
    data["$schemaVersion"] = SCHEMA
    data["$engineVersion"] = ENGINE
    # Keep meta keys near the top for readability.
    ordered = {}
    for key in ("$schemaVersion", "$engineVersion"):
        ordered[key] = data.pop(key)
    ordered.update(data)
    path.write_text(json.dumps(ordered, indent=4, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return True


def main() -> None:
    count = 0
    for root in ROOTS:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in EXTENSIONS:
                continue
            if "Third-Party" in path.parts or "build" in path.parts:
                continue
            if stamp(path):
                count += 1
                print(path)
    print(f"Stamped {count} files with schema={SCHEMA} engine={ENGINE}")


if __name__ == "__main__":
    main()
