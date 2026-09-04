# CORE-F10 — JSON Disk Compat — Implementation Plan

## Meta
- **ID:** `CORE-F10`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:** [Design](./CORE-F10_JSON_DISK_COMPAT_DESIGN.md)

## TL;DR

选项接线 + Json 多余键 Warn + `$schemaVersion` + Loader 宽松 + 叶子类型不符 Skip+Warn 已落地。

## Slices

| Slice | Goal | Verify | Status |
|-------|------|--------|--------|
| **S01** | `SerializerOptions`：`writeSchemaVersion`/`schemaVersion`；Binary 强制 `strictTypeCheck=true` | 编译 | **Done** |
| **S02** | JsonReader：消费键跟踪；`EndObject` 对多余非 meta 键 Warn | JSON 单测 | **Done** |
| **S03** | 根写/读 `$schemaVersion`；缺省=0 | archive JSON round-trip | **Done** |
| **S04** | `strictTypeCheck=false` 时叶子 codec 失败 → Warn+Success | 代码路径 | **Done** |
| **S05** | 盘路径 Loader/AssetManager/Project/PathRegistry 默认宽松 | 加载路径 | **Done** |
| **S06** | 文档收口 + 测试 | `serialization-archive` · `scene-clone` · `verify.ps1` | **Done** |

## 关键文件（摘要）

- `SerializationTypes.h` — 选项语义 + schemaVersion
- `JsonArchive.*` — 多余键 Warn；`$schemaVersion` 读写
- `Serializer.cpp` — ToFile/Json 写 schema；叶子宽松；Binary 强制严格
- Loaders / PathRegistry / ProjectManager / AssetManager — `strictTypeCheck=false`，缺字段跳过

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | 初版切片 |
| 2026-09-04 | S01–S06 完成；Status → Done |
