#!/usr/bin/env python3
"""
minEngine lightweight header tool (.gen.h + .gen.cpp mode, new MEReflection pipeline).
- Detects ME_CLASS / ME_PROPERTY markers
- Generates one .gen.h + one .gen.cpp per reflection source file
- Uses ME_REFLECTION_* macros so property category dispatch is handled by C++ type traits
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

CLASS_DECL_RE = re.compile(
    r"^\s*(?:ME_(?:CLASS|STRUCT)\s*\([^\n]*\)\s*)?(?P<class_kind>class|struct)\s+(?P<decl_tail>[^\{;]+)\{",
    re.MULTILINE,
)
ENUM_DECL_RE = re.compile(r"^\s*enum(\s+class)?\s+(\w+)\s*(?:\:\s*[\w:<>]+)?\s*\{", re.MULTILINE)
PROPERTY_MARK_PREFIX_RE = re.compile(r"^\s*ME_PROPERTY\s*\(")
FUNCTION_MARK_PREFIX_RE = re.compile(r"^\s*ME_FUNCTION\s*\(")
MEMBER_DECL_RE = re.compile(r"^\s*([\w:\<\>,\s\*&]+?)\s+(\w+)\s*(?:\{[^;]*\}|=[^;]*)?\s*;\s*(?://.*)?$")
FUNCTION_DECL_RE = re.compile(
    r"^\s*(?:(static)\s+)?([\w:\<\>,\s\*&~]+?)\s+(\w+)\s*\((.*)\)\s*(const)?\s*;\s*(?://.*)?$"
)
CLASS_MARK_RE = re.compile(r"ME_(?:CLASS|STRUCT)\s*\(", re.DOTALL)

TOOL_CACHE_VERSION = 14

PROPERTY_SPECIFIER_MAP = {
    "transient": "Transient",
    "editanywhere": "EditAnywhere",
    "editdefaultsonly": "EditDefaultsOnly",
    "editinstanceonly": "EditInstanceOnly",
    "visibleanywhere": "VisibleAnywhere",
    "visibledefaultsonly": "VisibleDefaultsOnly",
    "visibleinstanceonly": "VisibleInstanceOnly",
    "invisible": "Invisible",
    "instanced": "Instanced",
}

CLASS_SPECIFIER_MAP = {
    "abstract": "Abstract",
    "transient": "Transient",
    "editoronly": "EditorOnly",
}

FUNCTION_SPECIFIER_MAP = {
    "blueprintcallable": "BlueprintCallable",
    "blueprintpure": "BlueprintPure",
    "exec": "Exec",
    "deprecated": "Deprecated",
}

CLASS_DECL_SKIP_TOKENS = {
    "MINENGINE_API",
}


@dataclass
class PropertyMeta:
    name: str
    type_name: str
    specifiers: list[str]
    metadata: dict[str, str]


@dataclass
class FunctionParamMeta:
    name: str
    type_name: str
    role: str
    pass_kind: str


@dataclass
class FunctionMeta:
    name: str
    return_type: str
    is_static: bool
    is_const: bool
    has_return: bool
    has_out_params: bool
    specifiers: list[str]
    metadata: dict[str, str]
    params: list[FunctionParamMeta]


@dataclass
class ClassMeta:
    namespace: str
    class_name: str
    source_file: str
    source_include: str
    source_rel: str
    decl_pos: int
    class_hash: str
    specifiers: list[str]
    metadata: dict[str, str]
    base_types: list[str]
    has_virtual_inheritance: bool
    properties: list[PropertyMeta]
    functions: list[FunctionMeta]


@dataclass
class EnumValueMeta:
    name: str
    value_expr: str


@dataclass
class EnumMeta:
    namespace: str
    enum_name: str
    source_file: str
    source_include: str
    source_rel: str
    decl_pos: int
    enum_hash: str
    scoped: bool
    values: list[EnumValueMeta]


@dataclass
class FileRecord:
    mtime_ns: int
    size: int
    file_hash: str
    has_markers: bool
    class_keys: list[str]
    enum_keys: list[str]


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def normalize_path(path: Path) -> str:
    return str(path).replace("\\", "/")


def trim_quotes(text: str) -> str:
    text = text.strip()
    if len(text) >= 2 and ((text[0] == '"' and text[-1] == '"') or (text[0] == "'" and text[-1] == "'")):
        return text[1:-1]
    return text


def escape_cpp_string(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def split_top_level_args(arg_text: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    depth = 0
    quote = ""

    for ch in arg_text:
        if quote:
            current.append(ch)
            if ch == quote:
                quote = ""
            continue

        if ch in ('"', "'"):
            quote = ch
            current.append(ch)
            continue

        if ch == '(':
            depth += 1
            current.append(ch)
            continue

        if ch == ')':
            depth = max(0, depth - 1)
            current.append(ch)
            continue

        if ch == ',' and depth == 0:
            part = "".join(current).strip()
            if part:
                parts.append(part)
            current = []
            continue

        current.append(ch)

    part = "".join(current).strip()
    if part:
        parts.append(part)

    return parts


def extract_class_name_from_decl_tail(decl_tail: str) -> str | None:
    decl_prefix = decl_tail.split(":", 1)[0]
    tokens = [token.strip() for token in decl_prefix.split() if token.strip()]

    identifier_re = re.compile(r"^[A-Za-z_]\w*$")
    for token in tokens:
        if token in {"class", "struct", "final"}:
            continue
        if token in CLASS_DECL_SKIP_TOKENS or token.endswith("_API"):
            continue
        if identifier_re.fullmatch(token):
            return token

    return None


def extract_balanced_parentheses_content(text: str, open_paren_index: int) -> str | None:
    depth = 0
    for idx in range(open_paren_index, len(text)):
        ch = text[idx]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[open_paren_index + 1:idx]
    return None


def extract_last_class_marker_args(window: str) -> str | None:
    matches = list(CLASS_MARK_RE.finditer(window))
    if not matches:
        return None

    last = matches[-1]
    open_paren_index = window.find("(", last.start())
    if open_paren_index < 0:
        return None

    return extract_balanced_parentheses_content(window, open_paren_index)


def parse_annotation_metadata(arg_text: str) -> dict[str, str]:
    metadata: dict[str, str] = {}
    for part in split_top_level_args(arg_text):
        if "=" in part:
            key, value = part.split("=", 1)
            metadata[key.strip()] = trim_quotes(value.strip())
        else:
            metadata[part.strip()] = "true"
    return metadata


def normalize_property_specifier(token: str) -> str | None:
    key = "".join(token.split()).lower()
    return PROPERTY_SPECIFIER_MAP.get(key)


def normalize_class_specifier(token: str) -> str | None:
    key = "".join(token.split()).lower()
    return CLASS_SPECIFIER_MAP.get(key)


def parse_property_annotations(arg_text: str) -> tuple[list[str], dict[str, str]]:
    specifiers: list[str] = []
    metadata: dict[str, str] = {}

    for part in split_top_level_args(arg_text):
        token = part.strip()
        if not token:
            continue

        if "=" in token:
            key, value = token.split("=", 1)
            key = key.strip()
            if key.lower() != "meta":
                raise ValueError(f"Unsupported assignment token '{token}'. Use meta = (key = value, ...).")

            value = value.strip()
            if len(value) < 2 or not value.startswith("(") or not value.endswith(")"):
                raise ValueError(f"Invalid meta format '{token}'. Expected meta = (key = value, ...).")

            inner_text = value[1:-1].strip()
            if not inner_text:
                continue

            for entry in split_top_level_args(inner_text):
                entry = entry.strip()
                if not entry:
                    continue
                if "=" not in entry:
                    raise ValueError(f"Invalid meta entry '{entry}'. Expected key = value.")

                meta_key, meta_value = entry.split("=", 1)
                meta_key = meta_key.strip()
                if not meta_key:
                    raise ValueError(f"Invalid meta entry '{entry}'. Key cannot be empty.")

                metadata[meta_key] = trim_quotes(meta_value.strip())
            continue

        normalized_specifier = normalize_property_specifier(token)
        if normalized_specifier is None:
            raise ValueError(f"Unknown property specifier '{token}'.")
        specifiers.append(normalized_specifier)

    dedup_specifiers = list(dict.fromkeys(specifiers))
    return dedup_specifiers, metadata


def parse_class_annotations(arg_text: str) -> tuple[list[str], dict[str, str]]:
    specifiers: list[str] = []
    metadata: dict[str, str] = {}

    for part in split_top_level_args(arg_text):
        token = part.strip()
        if not token:
            continue

        if "=" in token:
            key, value = token.split("=", 1)
            key = key.strip()
            if key.lower() != "meta":
                raise ValueError(f"Unsupported assignment token '{token}'. Use meta = (key = value, ...).")

            value = value.strip()
            if len(value) < 2 or not value.startswith("(") or not value.endswith(")"):
                raise ValueError(f"Invalid meta format '{token}'. Expected meta = (key = value, ...).")

            inner_text = value[1:-1].strip()
            if not inner_text:
                continue

            for entry in split_top_level_args(inner_text):
                entry = entry.strip()
                if not entry:
                    continue
                if "=" not in entry:
                    raise ValueError(f"Invalid meta entry '{entry}'. Expected key = value.")

                meta_key, meta_value = entry.split("=", 1)
                meta_key = meta_key.strip()
                if not meta_key:
                    raise ValueError(f"Invalid meta entry '{entry}'. Key cannot be empty.")

                metadata[meta_key] = trim_quotes(meta_value.strip())
            continue

        normalized_specifier = normalize_class_specifier(token)
        if normalized_specifier is None:
            raise ValueError(f"Unknown class specifier '{token}'.")
        specifiers.append(normalized_specifier)

    dedup_specifiers = list(dict.fromkeys(specifiers))
    return dedup_specifiers, metadata


def parse_function_annotations(arg_text: str) -> tuple[list[str], dict[str, str]]:
    specifiers: list[str] = []
    metadata: dict[str, str] = {}

    for part in split_top_level_args(arg_text):
        token = part.strip()
        if not token:
            continue

        if "=" in token:
            key, value = token.split("=", 1)
            key = key.strip()
            if key.lower() != "meta":
                raise ValueError(f"Unsupported assignment token '{token}'. Use meta = (key = value, ...).")

            value = value.strip()
            if len(value) < 2 or not value.startswith("(") or not value.endswith(")"):
                raise ValueError(f"Invalid meta format '{token}'. Expected meta = (key = value, ...).")

            inner_text = value[1:-1].strip()
            if not inner_text:
                continue

            for entry in split_top_level_args(inner_text):
                entry = entry.strip()
                if not entry:
                    continue
                if "=" not in entry:
                    raise ValueError(f"Invalid meta entry '{entry}'. Expected key = value.")
                meta_key, meta_value = entry.split("=", 1)
                meta_key = meta_key.strip()
                if not meta_key:
                    raise ValueError(f"Invalid meta entry '{entry}'. Key cannot be empty.")
                metadata[meta_key] = trim_quotes(meta_value.strip())
            continue

        normalized = FUNCTION_SPECIFIER_MAP.get("".join(token.split()).lower())
        if normalized is None:
            raise ValueError(f"Unknown function specifier '{token}'.")
        specifiers.append(normalized)

    return list(dict.fromkeys(specifiers)), metadata


def parse_property_marker_line(line: str) -> tuple[str, str] | None:
    if PROPERTY_MARK_PREFIX_RE.match(line) is None:
        return None

    open_paren_index = line.find("(")
    if open_paren_index < 0:
        return None

    depth = 0
    for idx in range(open_paren_index, len(line)):
        ch = line[idx]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                arg_text = line[open_paren_index + 1:idx]
                trailing_text = line[idx + 1:].strip()
                return arg_text, trailing_text

    return None


def parse_function_marker_line(line: str) -> tuple[str, str] | None:
    if FUNCTION_MARK_PREFIX_RE.match(line) is None:
        return None

    open_paren_index = line.find("(")
    if open_paren_index < 0:
        return None

    depth = 0
    for idx in range(open_paren_index, len(line)):
        ch = line[idx]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                arg_text = line[open_paren_index + 1:idx]
                trailing_text = line[idx + 1:].strip()
                return arg_text, trailing_text

    return None


def to_param_display_name(param_name: str) -> str:
    if not param_name:
        return param_name
    return param_name[0].upper() + param_name[1:]


def parse_function_param(param_text: str, namespace_name: str) -> FunctionParamMeta | None:
    token = param_text.strip()
    if not token or token == "void":
        return None

    if "=" in token:
        token = token.split("=", 1)[0].strip()

    name_match = re.match(r"^(.*\S)\s+([A-Za-z_]\w*)$", token)
    if not name_match:
        return None

    type_part = name_match.group(1).strip()
    name_part = name_match.group(2).strip()

    is_const = type_part.startswith("const ")
    is_ref = "&" in type_part
    role = "Out" if is_ref and name_part.lower().startswith("out") else "In"
    if is_ref:
        pass_kind = "ConstRef" if is_const else ("Value" if role == "Out" else "Ref")
    else:
        pass_kind = "ConstValue" if is_const else "Value"

    clean_type = type_part.replace("&", " ").replace("*", " * ").strip()
    if clean_type.startswith("const "):
        clean_type = clean_type[len("const "):].strip()
    clean_type = " ".join(clean_type.split())
    qualified_type = qualify_field_type_name(clean_type, namespace_name)
    return FunctionParamMeta(
        name=to_param_display_name(name_part),
        type_name=qualified_type,
        role=role,
        pass_kind=pass_kind,
    )


def parse_reflected_function(line: str, namespace_name: str, specifiers: list[str], metadata: dict[str, str]) -> FunctionMeta | None:
    match = FUNCTION_DECL_RE.match(line)
    if match is None:
        return None

    is_static = match.group(1) is not None
    raw_return_type = " ".join(match.group(2).split())
    normalized_return = raw_return_type.replace("const", "").replace(" ", "")
    is_void_return = normalized_return == "void"
    return_type = "void" if is_void_return else qualify_field_type_name(raw_return_type, namespace_name)
    function_name = match.group(3).strip()
    param_list_text = match.group(4).strip()
    is_const = match.group(5) is not None

    params: list[FunctionParamMeta] = []
    if param_list_text:
        for raw_param in split_top_level_args(param_list_text):
            param = parse_function_param(raw_param, namespace_name)
            if param is not None:
                params.append(param)

    has_return = not is_void_return
    has_out_params = any(param.role == "Out" for param in params)
    return FunctionMeta(
        name=function_name,
        return_type=return_type,
        is_static=is_static,
        is_const=is_const,
        has_return=has_return,
        has_out_params=has_out_params,
        specifiers=specifiers,
        metadata=metadata,
        params=params,
    )


def parse_namespace_prefix(source: str, class_pos: int) -> str:
    prefix = source[:class_pos]
    ns_matches = list(re.finditer(r"\bnamespace\s+([\w:]+)\s*\{", prefix))
    if not ns_matches:
        return ""
    return ns_matches[-1].group(1)


def qualify_type_name(type_name: str, namespace_name: str) -> str:
    type_name = type_name.strip()
    if not type_name:
        return type_name

    if "::" in type_name:
        return type_name

    if namespace_name:
        return f"{namespace_name}::{type_name}"
    return type_name


def qualify_field_type_name(field_type: str, namespace_name: str) -> str:
    primitive_types = {
        "void",
        "bool", "char", "signed char", "unsigned char",
        "short", "unsigned short", "int", "unsigned int",
        "long", "unsigned long", "long long", "unsigned long long",
        "float", "double", "long double",
        "size_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    }

    token = field_type.strip()
    if not token:
        return token

    if token in primitive_types or "::" in token:
        return token

    const_prefix = ""
    if token.startswith("const "):
        const_prefix = "const "
        token = token[len("const "):].strip()

    suffix = ""
    while token.endswith("*") or token.endswith("&"):
        suffix = token[-1] + suffix
        token = token[:-1].strip()

    if token in primitive_types or "::" in token:
        qualified_core = token
    else:
        qualified_core = qualify_type_name(token, namespace_name)

    return f"{const_prefix}{qualified_core}{suffix}"


def parse_base_type_list(class_decl_head: str, namespace_name: str) -> tuple[list[str], bool]:
    if ":" not in class_decl_head:
        return [], False

    _, base_clause = class_decl_head.split(":", 1)
    base_specs = split_top_level_args(base_clause)
    base_types: list[str] = []
    has_virtual_inheritance = False

    for base_spec in base_specs:
        if not base_spec:
            continue

        sanitized_spec = base_spec.strip()
        tokens = [token for token in sanitized_spec.split() if token]
        if not tokens:
            continue

        if "virtual" in tokens:
            has_virtual_inheritance = True

        filtered_tokens = [token for token in tokens if token not in {"public", "protected", "private", "virtual", "final"}]
        if not filtered_tokens:
            continue

        base_type_name = " ".join(filtered_tokens)
        base_types.append(qualify_type_name(base_type_name, namespace_name))

    return base_types, has_virtual_inheritance


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
    return extract_last_class_marker_args(window) is not None


def parse_class_annotations_at(source: str, class_start: int) -> tuple[list[str], dict[str, str]]:
    window_start = max(0, class_start - 256)
    window = source[window_start:class_start]
    marker_arg_text = extract_last_class_marker_args(window)
    if marker_arg_text is None:
        return [], {}

    marker_arg_text = marker_arg_text.strip()
    if not marker_arg_text:
        return [], {}

    return parse_class_annotations(marker_arg_text)


def has_enum_marker(source: str, enum_start: int) -> bool:
    window_start = max(0, enum_start - 256)
    window = source[window_start:enum_start]
    return re.search(r"ME_ENUM\s*\((.*?)\)", window, re.DOTALL) is not None


def parse_reflected_classes(file_path: Path, src_root: Path, source: str) -> list[ClassMeta]:
    classes: list[ClassMeta] = []

    for match in CLASS_DECL_RE.finditer(source):
        class_decl_tail = match.group("decl_tail")
        class_start = match.start("class_kind")
        open_brace = source.find("{", match.end() - 1)
        if open_brace < 0:
            continue

        close_brace = find_matching_brace(source, open_brace)
        if close_brace < 0:
            continue

        if not has_class_marker(source, class_start):
            continue

        class_name = extract_class_name_from_decl_tail(class_decl_tail)
        if not class_name:
            class_line = source[:class_start].count("\n") + 1
            raise ValueError(f"{normalize_path(file_path)}:{class_line}: Failed to resolve class name from declaration tail '{class_decl_tail.strip()}'.")

        body = source[open_brace + 1:close_brace]
        class_decl_head = source[class_start:open_brace]
        namespace_name = parse_namespace_prefix(source, class_start)
        try:
            class_specifiers, class_metadata = parse_class_annotations_at(source, class_start)
        except ValueError as exc:
            class_line = source[:class_start].count("\n") + 1
            raise ValueError(f"{normalize_path(file_path)}:{class_line}: {exc}") from exc
        base_types, has_virtual_inheritance = parse_base_type_list(class_decl_head, namespace_name)
        body_lines = body.splitlines()
        body_start_line = source[:open_brace + 1].count("\n") + 1
        props: list[PropertyMeta] = []
        funcs: list[FunctionMeta] = []

        i = 0
        while i < len(body_lines):
            prop_marker_info = parse_property_marker_line(body_lines[i])
            func_marker_info = parse_function_marker_line(body_lines[i])

            if prop_marker_info is None and func_marker_info is None:
                i += 1
                continue

            if prop_marker_info is not None:
                marker_arg_text, inline_decl_text = prop_marker_info

                try:
                    specifiers, metadata = parse_property_annotations(marker_arg_text)
                except ValueError as exc:
                    marker_line = body_start_line + i
                    raise ValueError(f"{normalize_path(file_path)}:{marker_line}: {exc}") from exc

                member_line_index = i
                member_candidate = inline_decl_text
                member_match = MEMBER_DECL_RE.match(member_candidate)

                if member_match is None:
                    j = i + 1
                    while j < len(body_lines) and not body_lines[j].strip():
                        j += 1

                    if j >= len(body_lines):
                        break

                    member_line_index = j
                    member_candidate = body_lines[j]
                    member_match = MEMBER_DECL_RE.match(member_candidate)

                if member_match:
                    field_type = qualify_field_type_name(member_match.group(1), namespace_name)
                    field_name = member_match.group(2)
                    props.append(PropertyMeta(name=field_name,
                                              type_name=field_type,
                                              specifiers=specifiers,
                                              metadata=metadata))
                    i = member_line_index + 1
                else:
                    i = member_line_index + 1
                continue

            marker_arg_text, inline_decl_text = func_marker_info
            try:
                func_specifiers, func_metadata = parse_function_annotations(marker_arg_text)
            except ValueError as exc:
                marker_line = body_start_line + i
                raise ValueError(f"{normalize_path(file_path)}:{marker_line}: {exc}") from exc
            function_line_index = i
            function_candidate = inline_decl_text
            function_meta = parse_reflected_function(function_candidate, namespace_name, func_specifiers, func_metadata)

            if function_meta is None:
                j = i + 1
                while j < len(body_lines) and not body_lines[j].strip():
                    j += 1

                if j >= len(body_lines):
                    break

                function_line_index = j
                function_candidate = body_lines[j]
                function_meta = parse_reflected_function(function_candidate, namespace_name, func_specifiers, func_metadata)

            if function_meta is not None:
                funcs.append(function_meta)
            i = function_line_index + 1

        class_text = source[class_start:close_brace + 1]
        class_hash = sha256_text(class_text)

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
                decl_pos=class_start,
                class_hash=class_hash,
                specifiers=class_specifiers,
                metadata=class_metadata,
                base_types=base_types,
                has_virtual_inheritance=has_virtual_inheritance,
                properties=props,
                functions=funcs,
            )
        )

    return classes


def strip_line_comment(text: str) -> str:
    return text.split("//", 1)[0].strip()


def enum_key(meta: EnumMeta) -> str:
    if meta.namespace:
        return f"{meta.namespace}::{meta.enum_name}"
    return meta.enum_name


def full_enum_name(meta: EnumMeta) -> str:
    return enum_key(meta)


def parse_reflected_enums(file_path: Path, src_root: Path, source: str) -> list[EnumMeta]:
    enums: list[EnumMeta] = []

    for match in ENUM_DECL_RE.finditer(source):
        scoped = bool(match.group(1))
        enum_name = match.group(2)
        enum_start = match.start()
        open_brace = source.find("{", match.end() - 1)
        if open_brace < 0:
            continue

        close_brace = find_matching_brace(source, open_brace)
        if close_brace < 0:
            continue

        if not has_enum_marker(source, enum_start):
            continue

        namespace_name = parse_namespace_prefix(source, enum_start)
        full_enum_qualifier = f"{namespace_name}::{enum_name}" if namespace_name else enum_name

        body = source[open_brace + 1:close_brace]
        enum_values: list[EnumValueMeta] = []
        for raw_value in split_top_level_args(body):
            entry = strip_line_comment(raw_value).strip()
            if not entry:
                continue

            if "=" in entry:
                name, value_expr = entry.split("=", 1)
                value_name = name.strip()
                value_expression = value_expr.strip()
            else:
                value_name = entry.strip()
                if scoped:
                    value_expression = f"{full_enum_qualifier}::{value_name}"
                else:
                    if namespace_name:
                        value_expression = f"{namespace_name}::{value_name}"
                    else:
                        value_expression = value_name

            if value_name:
                enum_values.append(EnumValueMeta(name=value_name, value_expr=value_expression))

        if not enum_values:
            continue

        enum_text = source[enum_start:close_brace + 1]
        enum_hash = sha256_text(enum_text)

        source_file = normalize_path(file_path)
        source_include = normalize_path(file_path.relative_to(src_root))
        source_rel = source_include

        enums.append(
            EnumMeta(
                namespace=namespace_name,
                enum_name=enum_name,
                source_file=source_file,
                source_include=source_include,
                source_rel=source_rel,
                decl_pos=enum_start,
                enum_hash=enum_hash,
                scoped=scoped,
                values=enum_values,
            )
        )

    return enums


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


def source_output_name(source_rel: str, source_name_counts: dict[str, int]) -> str:
    source_stem = Path(source_rel).stem
    if source_name_counts.get(source_stem, 0) <= 1:
        return f"{source_stem}.gen.h"
    return f"{source_stem}_{sha256_text(source_rel)[:8]}.gen.h"


def source_cpp_output_name(header_output_name: str) -> str:
    if header_output_name.endswith(".gen.h"):
        return header_output_name[:-1] + "cpp"
    return f"{Path(header_output_name).stem}.cpp"


def class_registration_symbol(meta: ClassMeta) -> str:
    key = class_key(meta)
    class_name = class_name_from_key(key)
    return f"GReflectionClassRegister_{class_name}_{short_class_hash(key)}"


def enum_registration_symbol(meta: EnumMeta) -> str:
    key = enum_key(meta)
    enum_name = class_name_from_key(key)
    return f"GReflectionEnumRegister_{enum_name}_{short_class_hash(key)}"


def render_property_specifier_mask_expr(specifiers: list[str]) -> str:
    if not specifiers:
        return "static_cast<minEngine::Reflection::PropertySpecifierMask>(minEngine::Reflection::PropertySpecifier::None)"

    specifier_expr = " | ".join(
        f"static_cast<minEngine::Reflection::PropertySpecifierMask>(minEngine::Reflection::PropertySpecifier::{name})"
        for name in specifiers
    )
    return f"({specifier_expr})"


def render_property_metadata_expr(metadata: dict[str, str]) -> str:
    if not metadata:
        return "(minEngine::Reflection::PropertyMetadata{})"

    entries = []
    for key in sorted(metadata.keys()):
        value = metadata[key]
        entries.append(
            f'{{"{escape_cpp_string(key)}", "{escape_cpp_string(value)}"}}'
        )

    return f"(minEngine::Reflection::PropertyMetadata{{{', '.join(entries)}}})"


def render_class_specifier_mask_expr(specifiers: list[str]) -> str:
    if not specifiers:
        return "static_cast<minEngine::Reflection::ClassSpecifierMask>(minEngine::Reflection::ClassSpecifier::None)"

    specifier_expr = " | ".join(
        f"static_cast<minEngine::Reflection::ClassSpecifierMask>(minEngine::Reflection::ClassSpecifier::{name})"
        for name in specifiers
    )
    return f"({specifier_expr})"


def render_class_metadata_expr(metadata: dict[str, str]) -> str:
    if not metadata:
        return "(minEngine::Reflection::ClassMetadata{})"

    entries = []
    for key in sorted(metadata.keys()):
        value = metadata[key]
        entries.append(
            f'{{"{escape_cpp_string(key)}", "{escape_cpp_string(value)}"}}'
        )

    return f"(minEngine::Reflection::ClassMetadata{{{', '.join(entries)}}})"


def render_class_registration_declaration(meta: ClassMeta) -> list[str]:
    type_name = full_type_name(meta)
    symbol_name = class_registration_symbol(meta)
    return [f"ME_REFLECTION_CLASS_DECLARE({type_name}, {symbol_name})"]


def render_class_registration_definition(meta: ClassMeta) -> list[str]:
    lines: list[str] = []
    type_name = full_type_name(meta)
    symbol_name = class_registration_symbol(meta)
    lines.append(f"ME_REFLECTION_CLASS_DEFINE_BEGIN({type_name}, {symbol_name})")
    class_specifier_expr = render_class_specifier_mask_expr(meta.specifiers)
    class_metadata_expr = render_class_metadata_expr(meta.metadata)
    lines.append(f"    ME_REFLECTION_CLASS_SET_ANNOTATIONS({class_specifier_expr}, {class_metadata_expr})")
    for base_type in meta.base_types:
        lines.append(f"    ME_REFLECTION_CLASS_SUPER({base_type})")
    for prop in meta.properties:
        specifier_expr = render_property_specifier_mask_expr(prop.specifiers)
        metadata_expr = render_property_metadata_expr(prop.metadata)
        lines.append(
            f"    ME_REFLECTION_CLASS_ADD_FIELD({type_name}, {prop.name}, {specifier_expr}, {metadata_expr})"
        )
    for function in meta.functions:
        lines.extend(render_function_registration_definition(meta, function))
    lines.append(f"ME_REFLECTION_CLASS_DEFINE_END({type_name})")
    return lines


def render_function_flags_expr(function: FunctionMeta) -> str:
    flags = ["Native"]
    if function.is_static:
        flags.append("Static")
    if function.is_const:
        flags.append("ConstMethod")
    if function.has_return:
        flags.append("HasReturn")
    if function.has_out_params:
        flags.append("HasOutParams")
    joined = " | ".join(
        f"static_cast<uint32_t>(minEngine::Reflection::MEFunctionFlags::{flag})" for flag in flags
    )
    return f"static_cast<minEngine::Reflection::MEFunctionFlags>({joined})"


def render_function_specifier_mask_expr(specifiers: list[str]) -> str:
    if not specifiers:
        return "static_cast<minEngine::Reflection::FunctionSpecifierMask>(minEngine::Reflection::FunctionSpecifier::None)"
    specifier_expr = " | ".join(
        f"static_cast<minEngine::Reflection::FunctionSpecifierMask>(minEngine::Reflection::FunctionSpecifier::{name})"
        for name in specifiers
    )
    return f"({specifier_expr})"


def render_function_metadata_expr(metadata: dict[str, str]) -> str:
    if not metadata:
        return "(minEngine::Reflection::FunctionMetadata{})"
    entries = []
    for key in sorted(metadata.keys()):
        entries.append(f'{{"{escape_cpp_string(key)}", "{escape_cpp_string(metadata[key])}"}}')
    return f"(minEngine::Reflection::FunctionMetadata{{{', '.join(entries)}}})"


def render_function_registration_definition(owner: ClassMeta, function: FunctionMeta) -> list[str]:
    lines: list[str] = []
    owner_type = full_type_name(owner)
    param_signature = ",".join(f"{param.type_name}:{param.pass_kind}:{param.role}" for param in function.params)
    function_key = f"{owner_type}::{function.name}({param_signature})->{function.return_type}"
    function_var = f"functionInfo_{sha256_text(function_key)[:8]}"
    lines.append("    {")
    lines.append(
        f'        ME_REFLECTION_FUNCTION_BEGIN({function_var}, "{function.name}", {render_function_flags_expr(function)}, '
        f'{render_function_specifier_mask_expr(function.specifiers)}, {render_function_metadata_expr(function.metadata)})'
    )
    for param in function.params:
        lines.append(
            f'        ME_REFLECTION_FUNCTION_PARAM({function_var}, "{param.name}", {param.role}, {param.pass_kind}, {param.type_name})'
        )
    if function.has_return:
        lines.append(
            f"        ME_REFLECTION_FUNCTION_RETURN({function_var}, {function.return_type})"
        )
    lines.append(
        f"        ME_REFLECTION_FUNCTION_BIND_NATIVE({function_var}, {owner_type}, {function.name})"
    )
    lines.append(f"        ME_REFLECTION_FUNCTION_END({function_var})")
    lines.append("    }")
    return lines


def render_class_accessor(meta: ClassMeta) -> list[str]:
    lines: list[str] = []
    type_name = full_type_name(meta)
    lines.append(f"ME_REFLECTION_ACCESSOR_BEGIN({type_name})")
    for prop in meta.properties:
        lines.append(f"    ME_REFLECTION_ACCESSOR_FIELD({type_name}, {prop.name})")
    lines.append("ME_REFLECTION_ACCESSOR_END()")
    return lines


def render_enum_registration_declaration(meta: EnumMeta) -> list[str]:
    enum_name = full_enum_name(meta)
    symbol_name = enum_registration_symbol(meta)
    return [f"ME_REFLECTION_ENUM_DECLARE({enum_name}, {symbol_name})"]


def render_enum_registration_definition(meta: EnumMeta) -> list[str]:
    lines: list[str] = []
    enum_name = full_enum_name(meta)
    symbol_name = enum_registration_symbol(meta)
    lines.append(f"ME_REFLECTION_ENUM_DEFINE_BEGIN({enum_name}, {symbol_name})")
    for value in meta.values:
        lines.append(f"    ME_REFLECTION_ENUM_VALUE({value.name}, {value.value_expr})")
    lines.append(f"ME_REFLECTION_ENUM_DEFINE_END({enum_name})")
    return lines


def render_source_gen_header(classes: list[ClassMeta], enums: list[EnumMeta]) -> str:
    lines: list[str] = []
    lines.append("// Auto-generated by minEngine_header_tool_new.py. Do not edit manually.")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "Runtime/Core/Reflection/ReflectionMacros.h"')
    lines.append("")

    ordered_items: list[tuple[int, str, Any]] = []
    ordered_items.extend((meta.decl_pos, "class", meta) for meta in classes)
    ordered_items.extend((meta.decl_pos, "enum", meta) for meta in enums)
    ordered_items.sort(key=lambda item: item[0])

    for idx, (_, kind, meta) in enumerate(ordered_items):
        if kind == "class":
            lines.extend(render_class_accessor(meta))
            lines.append("")
            lines.extend(render_class_registration_declaration(meta))
        else:
            enum_lines = render_enum_registration_declaration(meta)
            if enum_lines:
                lines.extend(enum_lines)

        if idx != len(ordered_items) - 1:
            lines.append("")

    lines.append("")
    return "\n".join(lines)


def render_source_gen_cpp(source_include: str, classes: list[ClassMeta], enums: list[EnumMeta]) -> str:
    lines: list[str] = []
    lines.append("// Auto-generated by minEngine_header_tool_new.py. Do not edit manually.")
    lines.append(f'#include "{source_include}"')
    if any(meta.functions for meta in classes):
        lines.append('#include "Runtime/Core/Reflection/ReflectionFunctionNativeThunkTemplates.h"')
    lines.append("")

    ordered_items: list[tuple[int, str, Any]] = []
    ordered_items.extend((meta.decl_pos, "class", meta) for meta in classes)
    ordered_items.extend((meta.decl_pos, "enum", meta) for meta in enums)
    ordered_items.sort(key=lambda item: item[0])

    for idx, (_, kind, meta) in enumerate(ordered_items):
        if kind == "class":
            lines.extend(render_class_registration_definition(meta))
        else:
            enum_lines = render_enum_registration_definition(meta)
            if enum_lines:
                lines.extend(enum_lines)

        if idx != len(ordered_items) - 1:
            lines.append("")

    lines.append("")
    return "\n".join(lines)


def class_meta_from_manifest_entry(key: str, entry: dict[str, Any]) -> ClassMeta:
    namespace = ""
    class_name = key
    if "::" in key:
        namespace, class_name = key.rsplit("::", 1)

    properties: list[PropertyMeta] = []
    for property_entry in entry.get("properties", []):
        if isinstance(property_entry, str):
            properties.append(PropertyMeta(name=property_entry, type_name="", specifiers=[], metadata={}))
        else:
            properties.append(
                PropertyMeta(
                    name=property_entry.get("name", ""),
                    type_name=property_entry.get("type_name", ""),
                    specifiers=property_entry.get("specifiers", []),
                    metadata=property_entry.get("metadata", {}),
                )
            )
    return ClassMeta(
        namespace=namespace,
        class_name=class_name,
        source_file=entry.get("source_file", ""),
        source_include=entry.get("source_include", ""),
        source_rel=entry.get("source_rel", entry.get("source_include", "")),
        decl_pos=entry.get("decl_pos", 0),
        class_hash=entry.get("class_hash", ""),
        specifiers=entry.get("specifiers", []),
        metadata=entry.get("metadata", {}),
        base_types=entry.get("base_types", []),
        has_virtual_inheritance=entry.get("has_virtual_inheritance", False),
        properties=properties,
        functions=[
            FunctionMeta(
                name=f.get("name", ""),
                return_type=f.get("return_type", "void"),
                is_static=f.get("is_static", False),
                is_const=f.get("is_const", False),
                has_return=f.get("has_return", False),
                has_out_params=f.get("has_out_params", False),
                specifiers=f.get("specifiers", []),
                metadata=f.get("metadata", {}),
                params=[
                    FunctionParamMeta(
                        name=p.get("name", ""),
                        type_name=p.get("type_name", ""),
                        role=p.get("role", "In"),
                        pass_kind=p.get("pass_kind", "Value"),
                    )
                    for p in f.get("params", [])
                ],
            )
            for f in entry.get("functions", [])
        ],
    )


def detect_cycle(classes_by_key: dict[str, ClassMeta]) -> list[str]:
    errors: list[str] = []
    color: dict[str, int] = {}
    stack: list[str] = []

    def dfs(node: str) -> None:
        state = color.get(node, 0)
        if state == 1:
            cycle_path = stack[stack.index(node):] + [node]
            errors.append("Detected inheritance cycle: " + " -> ".join(cycle_path))
            return
        if state == 2:
            return

        color[node] = 1
        stack.append(node)
        cls = classes_by_key.get(node)
        if cls:
            for base in cls.base_types:
                if base in classes_by_key:
                    dfs(base)
        stack.pop()
        color[node] = 2

    for key in classes_by_key.keys():
        if color.get(key, 0) == 0:
            dfs(key)

    return errors


def detect_diamond(classes_by_key: dict[str, ClassMeta]) -> list[str]:
    errors: list[str] = []

    for root in classes_by_key.keys():
        counts: dict[str, int] = {}

        def dfs(current: str) -> None:
            cls = classes_by_key.get(current)
            if cls is None:
                return
            for base in cls.base_types:
                if base not in classes_by_key:
                    continue
                counts[base] = counts.get(base, 0) + 1
                dfs(base)

        dfs(root)
        duplicated_ancestors = [name for name, count in counts.items() if count > 1]
        if duplicated_ancestors:
            duplicate_text = ", ".join(sorted(set(duplicated_ancestors)))
            errors.append(f"Detected diamond inheritance for {root}: duplicated ancestors [{duplicate_text}]")

    return errors


def validate_inheritance_constraints(classes_by_key: dict[str, ClassMeta]) -> list[str]:
    errors: list[str] = []

    for class_key_name, class_meta in classes_by_key.items():
        if class_meta.has_virtual_inheritance:
            errors.append(f"Virtual inheritance is not supported: {class_key_name}")

        if class_key_name in class_meta.base_types:
            errors.append(f"Self inheritance is invalid: {class_key_name}")

    errors.extend(detect_cycle(classes_by_key))
    errors.extend(detect_diamond(classes_by_key))
    return errors


def enum_meta_from_manifest_entry(key: str, entry: dict[str, Any]) -> EnumMeta:
    namespace = ""
    enum_name = key
    if "::" in key:
        namespace, enum_name = key.rsplit("::", 1)

    values = [
        EnumValueMeta(name=value_entry.get("name", ""), value_expr=value_entry.get("value_expr", ""))
        for value_entry in entry.get("values", [])
    ]

    return EnumMeta(
        namespace=namespace,
        enum_name=enum_name,
        source_file=entry.get("source_file", ""),
        source_include=entry.get("source_include", ""),
        source_rel=entry.get("source_rel", entry.get("source_include", "")),
        decl_pos=entry.get("decl_pos", 0),
        enum_hash=entry.get("enum_hash", ""),
        scoped=entry.get("scoped", False),
        values=values,
    )


def parse_file_task(file_path: Path, src_root: Path) -> tuple[str, FileRecord, list[ClassMeta], list[EnumMeta]]:
    rel_path = normalize_path(file_path.relative_to(src_root))
    stat = file_path.stat()
    text = file_path.read_text(encoding="utf-8", errors="ignore")
    file_hash = sha256_text(text)

    has_markers = (
        "ME_ENUM(" in text
        or "ME_CLASS(" in text
        or "ME_STRUCT(" in text
    )
    if not has_markers:
        record = FileRecord(
            mtime_ns=stat.st_mtime_ns,
            size=stat.st_size,
            file_hash=file_hash,
            has_markers=False,
            class_keys=[],
            enum_keys=[],
        )
        return rel_path, record, [], []

    classes = parse_reflected_classes(file_path, src_root, text)
    enums = parse_reflected_enums(file_path, src_root, text)
    keys = sorted(class_key(meta) for meta in classes)
    enum_keys = sorted(enum_key(meta) for meta in enums)
    record = FileRecord(
        mtime_ns=stat.st_mtime_ns,
        size=stat.st_size,
        file_hash=file_hash,
        has_markers=True,
        class_keys=keys,
        enum_keys=enum_keys,
    )
    return rel_path, record, classes, enums


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def load_manifest(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"files": {}, "classes": {}, "enums": {}}
    return json.loads(path.read_text(encoding="utf-8"))


def save_manifest(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")


def collect_headers(src_root: Path) -> list[Path]:
    patterns = ["**/*.h", "**/*.hpp", "**/*.hh"]
    files: list[Path] = []
    for pat in patterns:
        files.extend(src_root.glob(pat))

    unique_files = sorted(set(files))
    excluded_prefixes = [
        normalize_path(src_root / "Runtime/Core/Serialization/Legacy") + "/",
        normalize_path(src_root / "Runtime/Core/Reflection/Legacy") + "/",
    ]
    return [
        path
        for path in unique_files
        if not any(normalize_path(path).startswith(prefix) for prefix in excluded_prefixes)
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate reflection .gen.h/.gen.cpp code from marker macros")
    parser.add_argument("--src-root", required=True, help="Source root to scan")
    parser.add_argument("--out-dir", required=True, help="Output directory root for generated headers")
    parser.add_argument("--manifest", required=True, help="Path to manifest json")
    args = parser.parse_args()

    src_root = Path(args.src_root).resolve()
    out_dir = Path(args.out_dir).resolve()
    manifest_path = Path(args.manifest).resolve()

    manifest = load_manifest(manifest_path)
    cache_compatible = manifest.get("tool_cache_version") == TOOL_CACHE_VERSION
    previous_files: dict[str, Any] = manifest.get("files", {}) if cache_compatible else {}
    previous_classes: dict[str, Any] = manifest.get("classes", {}) if cache_compatible else {}
    previous_enums: dict[str, Any] = manifest.get("enums", {}) if cache_compatible else {}

    parsed_classes_by_key: dict[str, ClassMeta] = {}
    parsed_enums_by_key: dict[str, EnumMeta] = {}
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
                enum_keys=prev.get("enum_keys", []),
            )
            new_file_records[rel] = record

            if record.has_markers:
                all_cached = True
                for key in record.class_keys:
                    if key not in previous_classes:
                        all_cached = False
                        break
                for key in record.enum_keys:
                    if key not in previous_enums:
                        all_cached = False
                        break
                if all_cached:
                    for key in record.class_keys:
                        parsed_classes_by_key[key] = class_meta_from_manifest_entry(key, previous_classes[key])
                    for key in record.enum_keys:
                        parsed_enums_by_key[key] = enum_meta_from_manifest_entry(key, previous_enums[key])
                else:
                    files_to_parse.append(header)
            continue

        files_to_parse.append(header)

    if files_to_parse:
        max_workers = min(8, max(1, len(files_to_parse)))
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as pool:
            futures = [pool.submit(parse_file_task, path, src_root) for path in files_to_parse]
            for future in concurrent.futures.as_completed(futures):
                rel_path, record, classes, enums = future.result()
                new_file_records[rel_path] = record
                for meta in classes:
                    parsed_classes_by_key[class_key(meta)] = meta
                for meta in enums:
                    parsed_enums_by_key[enum_key(meta)] = meta

    # Remove file records for deleted headers.
    current_rel_paths = {normalize_path(path.relative_to(src_root)) for path in headers}
    stale_file_keys = [key for key in new_file_records.keys() if key not in current_rel_paths]
    for key in stale_file_keys:
        del new_file_records[key]

    inheritance_errors = validate_inheritance_constraints(parsed_classes_by_key)
    if inheritance_errors:
        for error in inheritance_errors:
            print(f"[minEngine_header_tool][ERROR] {error}")
        return 2

    generated_count = 0
    removed_count = 0

    sources_with_reflection = {
        meta.source_rel for meta in parsed_classes_by_key.values()
    } | {
        meta.source_rel for meta in parsed_enums_by_key.values()
    }
    source_name_counts: dict[str, int] = {}
    for source_rel in sources_with_reflection:
        source_stem = Path(source_rel).stem
        source_name_counts[source_stem] = source_name_counts.get(source_stem, 0) + 1

    output_header_for_source: dict[str, str] = {}
    output_cpp_for_source: dict[str, str] = {}
    for source_rel in sorted(sources_with_reflection):
        output_header_for_source[source_rel] = source_output_name(source_rel, source_name_counts)
        output_cpp_for_source[source_rel] = source_cpp_output_name(output_header_for_source[source_rel])

    source_to_classes: dict[str, list[ClassMeta]] = {}
    for meta in parsed_classes_by_key.values():
        source_to_classes.setdefault(meta.source_rel, []).append(meta)

    source_to_enums: dict[str, list[EnumMeta]] = {}
    for meta in parsed_enums_by_key.values():
        source_to_enums.setdefault(meta.source_rel, []).append(meta)

    for source_rel in sorted(sources_with_reflection):
        classes = source_to_classes.get(source_rel, [])
        enums = source_to_enums.get(source_rel, [])
        header_path = out_dir / output_header_for_source[source_rel]
        cpp_path = out_dir / output_cpp_for_source[source_rel]
        header_content = render_source_gen_header(classes, enums)
        cpp_content = render_source_gen_cpp(source_rel, classes, enums)

        if write_if_changed(header_path, header_content):
            generated_count += 1
        if write_if_changed(cpp_path, cpp_content):
            generated_count += 1

    class_entries: dict[str, Any] = {}
    for key in sorted(parsed_classes_by_key.keys()):
        cls = parsed_classes_by_key[key]
        output_header_name = output_header_for_source.get(cls.source_rel, source_output_name(cls.source_rel, source_name_counts))
        output_cpp_name = output_cpp_for_source.get(cls.source_rel, source_cpp_output_name(output_header_name))
        header_path = out_dir / output_header_name
        cpp_path = out_dir / output_cpp_name
        class_entries[key] = {
            "namespace": cls.namespace,
            "class_name": cls.class_name,
            "decl_pos": cls.decl_pos,
            "class_hash": cls.class_hash,
            "specifiers": cls.specifiers,
            "metadata": cls.metadata,
            "base_types": cls.base_types,
            "has_virtual_inheritance": cls.has_virtual_inheritance,
            "content_hash": sha256_text(render_class_registration_definition(cls).__repr__()),
            "output_header": normalize_path(header_path),
            "output_source": normalize_path(cpp_path),
            "source_file": cls.source_file,
            "source_include": cls.source_include,
            "source_rel": cls.source_rel,
            "properties": [
                {
                    "name": p.name,
                    "type_name": p.type_name,
                    "specifiers": p.specifiers,
                    "metadata": p.metadata,
                }
                for p in cls.properties
            ],
            "functions": [
                {
                    "name": fn.name,
                    "return_type": fn.return_type,
                    "is_static": fn.is_static,
                    "is_const": fn.is_const,
                    "has_return": fn.has_return,
                    "has_out_params": fn.has_out_params,
                    "specifiers": fn.specifiers,
                    "metadata": fn.metadata,
                    "params": [
                        {
                            "name": param.name,
                            "type_name": param.type_name,
                            "role": param.role,
                            "pass_kind": param.pass_kind,
                        }
                        for param in fn.params
                    ],
                }
                for fn in cls.functions
            ],
        }

    enum_entries: dict[str, Any] = {}
    for key in sorted(parsed_enums_by_key.keys()):
        enum_meta = parsed_enums_by_key[key]
        output_header_name = output_header_for_source.get(enum_meta.source_rel, source_output_name(enum_meta.source_rel, source_name_counts))
        output_cpp_name = output_cpp_for_source.get(enum_meta.source_rel, source_cpp_output_name(output_header_name))
        header_path = out_dir / output_header_name
        cpp_path = out_dir / output_cpp_name
        enum_entries[key] = {
            "namespace": enum_meta.namespace,
            "enum_name": enum_meta.enum_name,
            "decl_pos": enum_meta.decl_pos,
            "enum_hash": enum_meta.enum_hash,
            "content_hash": sha256_text(render_enum_registration_definition(enum_meta).__repr__()),
            "output_header": normalize_path(header_path),
            "output_source": normalize_path(cpp_path),
            "source_file": enum_meta.source_file,
            "source_include": enum_meta.source_include,
            "source_rel": enum_meta.source_rel,
            "scoped": enum_meta.scoped,
            "values": [{"name": v.name, "value_expr": v.value_expr} for v in enum_meta.values],
        }

    # Keep unmatched .gen.h files to avoid breaking temporary/manual includes.
    referenced_headers = {entry["output_header"] for entry in class_entries.values()}
    referenced_headers.update(entry["output_header"] for entry in enum_entries.values())
    referenced_sources = {entry["output_source"] for entry in class_entries.values()}
    referenced_sources.update(entry["output_source"] for entry in enum_entries.values())

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

    # Remove stale per-source generated cpp files so registration does not drift on deleted/renamed headers.
    for generated_cpp in out_dir.rglob("*.gen.cpp"):
        if normalize_path(generated_cpp) not in referenced_sources:
            generated_cpp.unlink()
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
        "sources": sorted(sources_with_reflection),
        "generated_headers": sorted(referenced_headers),
        "generated_sources": sorted(referenced_sources),
        "classes": sorted(parsed_classes_by_key.keys()),
        "enums": sorted(parsed_enums_by_key.keys()),
    }
    write_if_changed(out_dir / "index.reflection.json", json.dumps(index_payload, indent=2, ensure_ascii=False) + "\n")

    new_manifest = {
        "tool_cache_version": TOOL_CACHE_VERSION,
        "files": {k: asdict(v) for k, v in sorted(new_file_records.items())},
        "classes": class_entries,
        "enums": enum_entries,
    }
    save_manifest(manifest_path, new_manifest)

    print(
        f"[minEngine_header_tool] classes={len(parsed_classes_by_key)} enums={len(parsed_enums_by_key)} "
        f"generated={generated_count} removed={removed_count}"
    )
    print(f"[minEngine_header_tool] output={normalize_path(out_dir)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
