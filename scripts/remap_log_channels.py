"""Remap ME_LOG channels by approved CORE-F17 post-cutover rules."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "minEngine"
ME_LOG_RE = re.compile(r"ME_LOG\(\s*(Log\w+)\s*,")

SKIP_PARTS = {"Third-Party", "build", ".git"}
# Never remap these (correct by construction / self-tests).
SKIP_FILES = {
    "LogSystem.cpp",
    "LogBuiltinChannels.cpp",
    "LogChannel.h",
    "LogSystem.h",
    "LoggingChannelsTest.cpp",  # intentionally mixes LogCore / LogTest
}


def decide_channel(rel: str) -> str | None:
    """Return target channel for a file, or None to leave unchanged."""
    norm = rel.replace("\\", "/")
    parts = Path(norm).parts

    if parts[0] == "Tests":
        return "LogTest"

    if parts[0] == "Editor":
        return "LogEditor"

    if parts[0] == "Playground":
        return "LogApp"

    # Engine sources under minEngine/src/...
    if "Runtime" not in parts:
        return None

    i = parts.index("Runtime")
    rest = parts[i + 1 :]
    joined = "/".join(rest)

    if joined.startswith("Core/Log/"):
        return None  # keep LogCore in LogSystem

    if joined.startswith("Core/Serialization"):
        return "LogSerialization"

    if joined.startswith("Core/"):
        return "LogCore"

    if rest and rest[0] == "Engine.cpp":
        return "LogCore"

    if joined.startswith("Platform/"):
        return "LogPlatform"

    if joined.startswith("Test/"):
        return "LogTest"

    if joined.startswith("Resource/"):
        return "LogAsset"

    if "GLFWWindowSystem" in joined:
        return "LogPlatform"

    if joined.startswith("Function/Render/Vulkan/") or joined.startswith("Function/Render/OpenGL/"):
        return "LogRHI"

    if joined.startswith("Function/Render/"):
        return "LogRender"

    if joined.startswith("Function/Physics/"):
        return "LogPhysics"

    if joined.startswith("Function/Audio/"):
        return "LogAudio"

    if joined.startswith("Function/Animation/"):
        return "LogAnimation"

    if "SkeletalMeshComponent" in joined:
        return "LogAnimation"

    if joined.startswith("Function/Scripting/") or "LuaComponent" in joined:
        return "LogScript"

    if joined.startswith("Function/UI/"):
        return "LogUI"

    if joined.startswith("Function/Debug/"):
        return "LogRender"

    if joined.startswith("Function/Framework/"):
        return "LogCore"

    if joined.startswith("Function/GameplayFramework/"):
        return "LogCore"

    if joined.startswith("Function/Input/"):
        return "LogCore"

    return "LogCore"


def main() -> None:
    files_changed = 0
    sites = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {".cpp", ".h", ".hpp"}:
            continue
        if any(part in SKIP_PARTS for part in path.parts):
            continue
        if path.name in SKIP_FILES:
            continue

        rel = str(path.relative_to(ROOT))
        target = decide_channel(rel)
        if target is None:
            continue

        raw = path.read_bytes()
        text = None
        enc = "utf-8"
        for candidate in ("utf-8", "utf-8-sig", "gbk", "latin-1"):
            try:
                text = raw.decode(candidate)
                enc = candidate
                break
            except UnicodeDecodeError:
                continue
        if text is None:
            print(f"SKIP encoding {rel}")
            continue

        new_parts: list[str] = []
        last = 0
        file_sites = 0
        for match in ME_LOG_RE.finditer(text):
            old = match.group(1)
            new_parts.append(text[last : match.start()])
            if old == target:
                new_parts.append(match.group(0))
            else:
                new_parts.append(f"ME_LOG({target},")
                file_sites += 1
            last = match.end()
        if file_sites == 0:
            continue
        new_parts.append(text[last:])
        new_text = "".join(new_parts)

        if enc in {"utf-8", "utf-8-sig"}:
            path.write_text(new_text, encoding=enc, newline="\n")
        else:
            path.write_bytes(new_text.encode(enc))
        files_changed += 1
        sites += file_sites
        print(f"{file_sites:4d} -> {target:16s}  {rel}")

    print(f"Updated {files_changed} files, remapped {sites} ME_LOG channel tokens")


if __name__ == "__main__":
    main()
