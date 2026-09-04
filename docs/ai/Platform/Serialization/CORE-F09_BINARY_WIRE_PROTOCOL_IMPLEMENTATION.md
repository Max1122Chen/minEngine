# CORE-F09 — Binary Wire v2 — Implementation Plan

## Meta
- **ID:** `CORE-F09`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:** [Design](./CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md)

## TL;DR

Schema 表 + Writer/Reader v2 + Serializer 严格模式 + PIE Binary 已落地；TD-028/029 关闭。

## Slices

| Slice | Goal | Verify | Status |
|-------|------|--------|--------|
| **S01** | `TransientSchemaTable`：Finalize 分配 ClassId/FieldId；算 Fingerprint | Finalize 钩子 + 表查询 | **Done** |
| **S02** | `BinaryWriterArchive` / `BinaryReaderArchive` v2（header + object + tags）；删除 v1 EndObject 路径 | 手写 buffer round-trip | **Done** |
| **S03** | Serializer 接 Transient Ids；Buffer API 强制 `skipUnknownField=false` | `serialization-archive` | **Done** |
| **S04** | TD-028 复现（多 GO physics-mesh） | `scene-clone` physics-stack | **Done** |
| **S05** | `SceneDuplicator` → Binary v2；文档 TD-028/029 Done | `scene-clone` + `verify.ps1` | **Done** |

## 关键文件（摘要）

- `TransientSchemaTable.h/.cpp` — dense ClassId/FieldId + fingerprint
- `BinaryArchive.h/.cpp` — MEB2 header；`fieldCount`+`bodyLength`；无 wire `EndObject`
- `Archive.h/.cpp` — `BeginObject(MEClass*)` / `BeginObjectPtr(MEClass*)` 默认桥接
- `Serializer.cpp` — class 指针 Begin*；Buffer 路径强制严格
- `SceneDuplicator.cpp` — PIE 回 Binary buffer
- `Reflection.cpp` — Finalize 后 `BuildFromReflection()`
- Tests：`SerializationArchiveTest` / `SceneCloneTest`

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | 初版切片 |
| 2026-09-04 | S01–S05 完成；Status → Done |
