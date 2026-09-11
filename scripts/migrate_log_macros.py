"""Hard-cut old ME_* / ME_CORE_* log macros to ME_LOG(LogCore|LogApp, ...)."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "minEngine"
EXTS = {".cpp", ".h", ".hpp", ".c", ".cc", ".cxx"}
SKIP_PARTS = {"Third-Party", "build", ".git"}

CORE_MAP = [
    ("ME_CORE_CRITICAL", "Fatal", "LogCore"),
    ("ME_CORE_ERROR", "Error", "LogCore"),
    ("ME_CORE_WARN", "Warn", "LogCore"),
    ("ME_CORE_INFO", "Info", "LogCore"),
    ("ME_CORE_DEBUG", "Debug", "LogCore"),
    ("ME_CORE_TRACE", "Trace", "LogCore"),
]

APP_MAP = [
    ("ME_CRITICAL", "Fatal", "LogApp"),
    ("ME_ERROR", "Error", "LogApp"),
    ("ME_WARN", "Warn", "LogApp"),
    ("ME_INFO", "Info", "LogApp"),
    ("ME_DEBUG", "Debug", "LogApp"),
    ("ME_TRACE", "Trace", "LogApp"),
]


def should_skip(path: Path) -> bool:
    return any(part in SKIP_PARTS for part in path.parts)


def replace_macros(text: str) -> tuple[str, int]:
    count = 0
    for name, sev, channel in CORE_MAP:
        pattern = re.compile(rf"\b{name}\s*\(")
        text, n = pattern.subn(f"ME_LOG({channel}, {sev}, ", text)
        count += n
    for name, sev, channel in APP_MAP:
        # Avoid matching ME_CORE_* leftovers (already handled) and identifiers like FOO_ME_INFO
        pattern = re.compile(rf"(?<![A-Z0-9_]){name}\s*\(")
        text, n = pattern.subn(f"ME_LOG({channel}, {sev}, ", text)
        count += n
    return text, count


def main() -> None:
    total = 0
    files = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in EXTS or should_skip(path):
            continue
        if path.name in {"LogSystem.h", "migrate_log_macros.py"}:
            continue
        try:
            original = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            print(f"SKIP (encoding) {path.relative_to(ROOT)}")
            continue
        updated, n = replace_macros(original)
        if n == 0:
            continue
        path.write_text(updated, encoding="utf-8", newline="\n")
        total += n
        files += 1
        print(f"{n:4d}  {path.relative_to(ROOT)}")
    print(f"Replaced {total} call sites in {files} files")


if __name__ == "__main__":
    main()
