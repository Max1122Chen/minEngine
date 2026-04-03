#!/usr/bin/env python3
"""
minEngine lightweight header tool (.gen.h mode).
- Detects ME_CLASS / ME_PROPERTY markers
- Generates one .gen.h file per reflected class
- Generated code uses existing reflection registration macros directly
- Uses file-level + class-level incremental regeneration
- Scans changed files in parallel
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import re
from dataclasses import asdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any

CLASS_DECL_RE = re.compile(r"\b(class|struct)\s+(\w+)\s*[^\{;]*\{", re.MULTILINE)
PROPERTY_MARK_RE = re.compile(r"^\s*ME_PROPERTY\s*\((.*?)\)\s*$")
MEMBER_DECL_RE = re.compile(r"^\s*([\w:<>]+)\s+(\w+)\s*(?:\{[^;]*\}|=[^;]*)?\s*;\s*(?://.*)?$")


@dataclass
class PropertyMeta:
    name: str


@dataclass
class ClassMeta:
    namespace: str
    class_name: str
    source_file: str
    source_include: str
    source_rel: str
    class_hash: str
    properties: list[PropertyMeta]


@dataclass
class FileRecord:
    mtime_ns: int
    size: int
    file_hash: str
    has_markers: bool
    class_keys: list[str]


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def normalize_path(path: Path) -> str:
    return str(path).replace("\\", "/")


def parse_namespace_prefix(source: str, class_pos: int) -> str:
    prefix = source[:class_pos]
    ns_matches = list(re.finditer(r"\bnamespace\s+([\w:]+)\s*\{", prefix))
    if not ns_matches:
        return ""
    return ns_matches[-1].group(1)


def find_matching_brace(source: str, open_brace_index: int) -> int:
    depth = 0
    i = open_brace_index
    while i < len(source):
        ch = source[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def has_class_marker(source: str, class_start: int) -> bool:
    window_start = max(0, class_start - 256)
    window = source[window_start:class_start]
    return re.search(r"ME_CLASS\s*\((.*?)\)", window, re.DOTALL) is not None


def parse_reflected_classes(file_path: Path, src_root: Path, source: str) -> list[ClassMeta]:
    classes: list[ClassMeta] = []

    for match in CLASS_DECL_RE.finditer(source):
        class_name = match.group(2)
        class_start = match.start()
        open_brace = source.find("{", match.end() - 1)
        if open_brace < 0:
            continue

        close_brace = find_matching_brace(source, open_brace)
        if close_brace < 0:
            continue

        if not has_class_marker(source, class_start):
            continue

        body = source[open_brace + 1:close_brace]
        body_lines = body.splitlines()
        props: list[PropertyMeta] = []

        i = 0
        while i < len(body_lines):
            marker_match = PROPERTY_MARK_RE.match(body_lines[i])
            if not marker_match:
                i += 1
                continue

            j = i + 1
            while j < len(body_lines) and not body_lines[j].strip():
                j += 1

            if j >= len(body_lines):
                break

            member_match = MEMBER_DECL_RE.match(body_lines[j])
            if member_match:
                field_name = member_match.group(2)
                props.append(PropertyMeta(name=field_name))
                i = j + 1
            else:
                i = j + 1

        if not props:
            continue

        class_text = source[class_start:close_brace + 1]
        class_hash = sha256_text(class_text)
        namespace_name = parse_namespace_prefix(source, class_start)

        source_file = normalize_path(file_path)
        source_include = normalize_path(file_path.relative_to(src_root))
        source_rel = source_include

        classes.append(
            ClassMeta(
                namespace=namespace_name,
                class_name=class_name,
                source_file=source_file,
                source_include=source_include,
                source_rel=source_rel,
                class_hash=class_hash,
                properties=props,
            )
        )

    return classes


def class_key(meta: ClassMeta) -> str:
    if meta.namespace:
        return f"{meta.namespace}::{meta.class_name}"
    return meta.class_name


def full_type_name(meta: ClassMeta) -> str:
    return class_key(meta)


def class_name_from_key(key: str) -> str:
    return key.split("::")[-1]


def short_class_hash(key: str) -> str:
    return sha256_text(key)[:8]


def header_output_name(key: str, class_name_counts: dict[str, int]) -> str:
    class_name = class_name_from_key(key)
    if class_name_counts.get(class_name, 0) <= 1:
        return f"{class_name}.gen.h"
    return f"{class_name}_{short_class_hash(key)}.gen.h"


def render_class_gen_header(meta: ClassMeta) -> str:
    lines: list[str] = []
    type_name = full_type_name(meta)

    lines.append("// Auto-generated by minEngine_header_tool.py. Do not edit manually.")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "Runtime/Core/Reflection/ReflectionMacros.h"')
    lines.append("")
    lines.append(f"ME_REFLECT_TYPE_BEGIN({type_name})")
    for prop in meta.properties:
        lines.append(f"    ME_REFLECT_FIELD({type_name}, {prop.name})")
    lines.append(f"ME_REFLECT_TYPE_END({type_name})")
    lines.append("")

    return "\n".join(lines)


def class_meta_from_manifest_entry(key: str, entry: dict[str, Any]) -> ClassMeta:
    namespace = ""
    class_name = key
    if "::" in key:
        namespace, class_name = key.rsplit("::", 1)

    properties = [PropertyMeta(name=name) for name in entry.get("properties", [])]
    return ClassMeta(
        namespace=namespace,
        class_name=class_name,
        source_file=entry.get("source_file", ""),
        source_include=entry.get("source_include", ""),
        source_rel=entry.get("source_rel", entry.get("source_include", "")),
        class_hash=entry.get("class_hash", ""),
        properties=properties,
    )


def parse_file_task(file_path: Path, src_root: Path) -> tuple[str, FileRecord, list[ClassMeta]]:
    rel_path = normalize_path(file_path.relative_to(src_root))
    stat = file_path.stat()
    text = file_path.read_text(encoding="utf-8", errors="ignore")
    file_hash = sha256_text(text)

    has_markers = "ME_CLASS(" in text and "ME_PROPERTY(" in text
    if not has_markers:
        record = FileRecord(
            mtime_ns=stat.st_mtime_ns,
            size=stat.st_size,
            file_hash=file_hash,
            has_markers=False,
            class_keys=[],
        )
        return rel_path, record, []

    classes = parse_reflected_classes(file_path, src_root, text)
    keys = sorted(class_key(meta) for meta in classes)
    record = FileRecord(
        mtime_ns=stat.st_mtime_ns,
        size=stat.st_size,
        file_hash=file_hash,
        has_markers=True,
        class_keys=keys,
    )
    return rel_path, record, classes


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def load_manifest(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"files": {}, "classes": {}}
    return json.loads(path.read_text(encoding="utf-8"))


def save_manifest(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")


def collect_headers(src_root: Path) -> list[Path]:
    patterns = ["**/*.h", "**/*.hpp", "**/*.hh"]
    files: list[Path] = []
    for pat in patterns:
        files.extend(src_root.glob(pat))
    return sorted(set(files))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate reflection .gen.h code from marker macros")
    parser.add_argument("--src-root", required=True, help="Source root to scan")
    parser.add_argument("--out-dir", required=True, help="Output directory root for generated headers")
    parser.add_argument("--manifest", required=True, help="Path to manifest json")
    args = parser.parse_args()

    src_root = Path(args.src_root).resolve()
    out_dir = Path(args.out_dir).resolve()
    manifest_path = Path(args.manifest).resolve()

    manifest = load_manifest(manifest_path)
    previous_files: dict[str, Any] = manifest.get("files", {})
    previous_classes: dict[str, Any] = manifest.get("classes", {})

    parsed_by_key: dict[str, ClassMeta] = {}
    new_file_records: dict[str, FileRecord] = {}
    headers = collect_headers(src_root)

    files_to_parse: list[Path] = []
    for header in headers:
        rel = normalize_path(header.relative_to(src_root))
        stat = header.stat()
        prev = previous_files.get(rel)

        if prev and prev.get("mtime_ns") == stat.st_mtime_ns and prev.get("size") == stat.st_size:
            record = FileRecord(
                mtime_ns=prev.get("mtime_ns", stat.st_mtime_ns),
                size=prev.get("size", stat.st_size),
                file_hash=prev.get("file_hash", ""),
                has_markers=prev.get("has_markers", False),
                class_keys=prev.get("class_keys", []),
            )
            new_file_records[rel] = record

            if record.has_markers:
                all_cached = True
                for key in record.class_keys:
                    if key not in previous_classes:
                        all_cached = False
                        break
                if all_cached:
                    for key in record.class_keys:
                        parsed_by_key[key] = class_meta_from_manifest_entry(key, previous_classes[key])
                else:
                    files_to_parse.append(header)
            continue

        files_to_parse.append(header)

    if files_to_parse:
        max_workers = min(8, max(1, len(files_to_parse)))
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as pool:
            futures = [pool.submit(parse_file_task, path, src_root) for path in files_to_parse]
            for future in concurrent.futures.as_completed(futures):
                rel_path, record, classes = future.result()
                new_file_records[rel_path] = record
                for meta in classes:
                    parsed_by_key[class_key(meta)] = meta

    # Remove file records for deleted headers.
    current_rel_paths = {normalize_path(path.relative_to(src_root)) for path in headers}
    stale_file_keys = [key for key in new_file_records.keys() if key not in current_rel_paths]
    for key in stale_file_keys:
        del new_file_records[key]

    generated_count = 0
    removed_count = 0

    class_name_counts: dict[str, int] = {}
    for key in parsed_by_key.keys():
        class_name = class_name_from_key(key)
        class_name_counts[class_name] = class_name_counts.get(class_name, 0) + 1

    class_entries: dict[str, Any] = {}
    for key in sorted(parsed_by_key.keys()):
        cls = parsed_by_key[key]
        old = previous_classes.get(key, {})

        output_name = header_output_name(key, class_name_counts)
        header_path = out_dir / output_name

        header_content = render_class_gen_header(cls)
        header_hash = sha256_text(header_content)

        old_output = old.get("output_header")
        if old_output and normalize_path(Path(old_output)) != normalize_path(header_path):
            old_path = Path(old_output)
            if old_path.exists():
                old_path.unlink()
                removed_count += 1

        if old.get("class_hash") != cls.class_hash or old.get("content_hash") != header_hash or not header_path.exists():
            if write_if_changed(header_path, header_content):
                generated_count += 1

        class_entries[key] = {
            "namespace": cls.namespace,
            "class_name": cls.class_name,
            "class_hash": cls.class_hash,
            "content_hash": header_hash,
            "output_header": normalize_path(header_path),
            "source_file": cls.source_file,
            "source_include": cls.source_include,
            "source_rel": cls.source_rel,
            "properties": [p.name for p in cls.properties],
        }

    for key, old in previous_classes.items():
        if key in class_entries:
            continue
        old_header = old.get("output_header") or old.get("output_cpp")
        if old_header:
            old_path = Path(old_header)
            if old_path.exists():
                old_path.unlink()
                removed_count += 1

    # Cleanup stale generated headers that are not referenced anymore.
    referenced_outputs = {entry["output_header"] for entry in class_entries.values()}
    for generated_header in out_dir.glob("*.gen.h"):
        generated_header_norm = normalize_path(generated_header)
        if generated_header_norm not in referenced_outputs:
            generated_header.unlink()
            removed_count += 1

    # Cleanup legacy generated outputs in the output tree.
    for legacy_cpp in out_dir.rglob("*.reflection.gen.cpp"):
        legacy_cpp.unlink()
        removed_count += 1
    legacy_aggregate = out_dir / "ReflectionAutoRegister.gen.cpp"
    if legacy_aggregate.exists():
        legacy_aggregate.unlink()
        removed_count += 1
    for legacy_header in out_dir.rglob("*.reflection.gen.h"):
        legacy_header.unlink()
        removed_count += 1

    # Remove empty legacy subdirectories after flattening output layout.
    for directory in sorted((p for p in out_dir.rglob("*") if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
        if directory == out_dir:
            continue
        try:
            directory.rmdir()
        except OSError:
            pass

    index_payload = {
        "generated_count": generated_count,
        "removed_count": removed_count,
        "scanned_files": len(headers),
        "parsed_files": len(files_to_parse),
        "classes": sorted(parsed_by_key.keys()),
    }
    write_if_changed(out_dir / "index.reflection.json", json.dumps(index_payload, indent=2, ensure_ascii=False) + "\n")

    new_manifest = {
        "files": {k: asdict(v) for k, v in sorted(new_file_records.items())},
        "classes": class_entries,
    }
    save_manifest(manifest_path, new_manifest)

    print(f"[minEngine_header_tool] classes={len(parsed_by_key)} generated={generated_count} removed={removed_count}")
    print(f"[minEngine_header_tool] output={normalize_path(out_dir)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
