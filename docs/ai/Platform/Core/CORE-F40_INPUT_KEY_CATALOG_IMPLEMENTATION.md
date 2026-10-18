# CORE-F40 Input Key Catalog — Implementation Plan

## Meta
- **ID:** `CORE-F40`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-18`
- **Related:** [Design](./CORE-F40_INPUT_KEY_CATALOG_DESIGN.md)
- **Design readiness:** `Ready with deferred items`（全量键表已决）

## Goal

全量扩展 `InputKeys` + GLFW 接线；合成修饰键；canonical Gamepad poll；未知码忽略。

## Implementation Slices

| Slice ID | Summary | Verification | Status |
|----------|---------|--------------|--------|
| CORE-F40-S01 | 键盘/鼠全量目录 + 映射 + 停写 AnyKey + 合成 Shift/Ctrl/Alt/Super | `test input-key-catalog` | Done |
| CORE-F40-S02 | Gamepad canonical + Tick poll + PS 同名别名 | 同 suite（无垫 Idle） | Done |

## Verification

```text
minEngine/bin/minEngineTests.exe test input-key-catalog
minEngine/bin/minEngineTests.exe test input-trigger-down
```
