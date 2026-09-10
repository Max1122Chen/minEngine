# CORE-F08 — Serialization Cleanup — Implementation Plan

## Meta
- **ID:** `CORE-F08`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:** [Design](./CORE-F08_SERIALIZATION_CLEANUP_DESIGN.md)

## TL;DR

S01–S04 已落地于 `feat/core`。下一 Feature：`CORE-F09` Binary wire v2。

## Slices

| Slice | Goal | Status | Verify |
|-------|------|--------|--------|
| **S01** | 删除 `allowObjectPtrSerialization`、`m_IsHandlingPtr`；清过时 ObjectPtr TODO | **Done** | 编译 |
| **S02** | `MEClass*` 根 API + 模板便捷；`string` 重载委托 | **Done** | `serialization-archive` |
| **S03** | `ForEachPropertyInHierarchy(MEClass*)`；`MEObject::StaticClass()`；合并查找；Deserialize `static_cast` | **Done** | 同上 + `scene-clone` |
| **S04** | 调用点迁移（Editor / Duplicator / Loaders / AssetManager / Tests）；文档收口 | **Done** | `verify.ps1` |

## DoD（Feature）

- [x] Design §6 验收项
- [x] `PROGRESS_LOG` 追加收口条目
- [x] `ACTIVE_WORK` / Registry 更新
- [ ] 准备 commit（等维护者批准）

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 占位切片 |
| 2026-09-04 | 对齐拍板：S01 先删死代码；去掉 TD-026 slice；四片 P0+P1 |
| 2026-09-04 | **Done** — 全切片合入工作区 |
