# CORE-F09 — Binary Wire Protocol Redesign

## Meta
- **ID:** `CORE-F09`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/core`（自 `master`；**依赖** `CORE-F08` API 整理）
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../../ACTIVE_WORK.md) · [SERIALIZATION_BINARY_AND_PROPERTY_API.md](./SERIALIZATION_BINARY_AND_PROPERTY_API.md) · [CORE-F08](./CORE-F08_SERIALIZATION_CLEANUP_DESIGN.md)
- **Tech debt:** TD-028（wire 歧义）、TD-029（PIE JSON 绕道）

## TL;DR

重新设计 **BinaryArchive wire 协议**（无歧义 framing、stable `ClassId` / `FieldId`、版本头），替换当前 v1 字段流；验收后恢复 PIE `SceneDuplicator` Binary 路径并关闭 TD-028/029。**磁盘 JSON 资产格式不变**（本阶段）。

## Scope
- **In:**
  - Wire v2 Design Spec（tag 表、object body framing、class/field 标识、schema version）
  - `MEClass` stable id 注册（消费 `CORE-F08` 已稳定的 `MEClass*` 热路径）
  - 新 `BinaryWriterArchive` / `BinaryReaderArchive` 实现或 v2 后端
  - `SerializationArchiveTest` 扩展 + TD-028 复现场景
  - PIE clone 从 JSON 改回 Binary（TD-029）
- **Out:**
  - 磁盘 `.mescene` 默认改 Binary
  - 网络复制 / 压缩 / 增量 diff
  - GC / Object Lifetime（刻意延后）
  - TD-026（与 F08 相同：随 Setter 验证另开）

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | 正式 Wire Spec 正文、Implementation Plan |
| Depends on | `CORE-F08` Done（MEClass* API + 死代码清理） |
| Next | **等 F08 收口后** Wire Spec 设计评审 → Pre-flight → 实现 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 登记；占位 Design |
| 2026-09-04 | 对齐 F08 拍板；明确依赖 F08 Done |
