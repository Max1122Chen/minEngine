# CORE-F10 — JSON Disk Compatibility & Schema Meta

## Meta
- **ID:** `CORE-F10`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/core`（建议在 CORE-F09 后或并行设计）
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../../ACTIVE_WORK.md) · [CORE-F09](./CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md)（Persistent 容错契约） · [SERIALIZATION_BINARY_AND_PROPERTY_API.md](./SERIALIZATION_BINARY_AND_PROPERTY_API.md)
- **Depends on:** 建议 `CORE-F09` Design 契约对齐后实施（可不依赖 Binary 实现完成）

## TL;DR

JSON 是当前 **存盘** 主路径（`.mescene` / `.memtl` 等）。将反序列化容错与 **CORE-F09 Persistent binding** 对齐：未知字段 Skip+Warn、缺字段用默认值、framing/类型损坏区分处理；并引入 **`$schemaVersion`**（及必要 meta），避免「加字段就完全无法加载旧资产」过硬。本期不改 Binary 存盘。

## Scope

### In（规划）
- 盘路径默认宽松策略（对齐 F09 §3.2 Persistent）
  - 未知 JSON 键（非 `$` 保留 meta）：Skip + Warn，不中断
  - 反射需要但文件缺失的字段：保留对象默认值（不 Fail）
  - 已知字段 JSON 类型与 codec 不符：Skip 字段 + Warn（默认；可与 F09 同步）
  - JSON 语法损坏 / 非 object 根等：Failure
- 根对象写入/读取 **`$schemaVersion`**（及可选 `$typeName` 现有行为梳理）
- `SerializerOptions` / Loader 默认：盘路径 `skipUnknownField=true` 语义升级为「完整宽松策略」；理清与 `strictTypeCheck` 关系
- 回归：旧 `.mescene` 在新加可选字段后仍可加载；新文件多未知键时旧编辑器/工具若只读子集不崩（同进程新代码视角）
- 测试 + 文档

### Out
- Binary 磁盘格式（仍 F09 Persistent 未来项）
- 完整字段迁移脚本 / rename redirect 表（可后续）
- TD-026
- 强制所有历史资产立刻重存（应能读旧文件；`$schemaVersion` 缺省视为 0/1）

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | 正式错误策略细则表、Implementation Plan、`$schemaVersion` 数值语义 |
| Why | 存盘 JSON 过严；与 F09 Persistent 契约应对齐 |
| Next | F09 Design 落地后展开本 Spec §方案；或与 F09 并行写细则 |
| Default disk load | 宽松；显式工具/校验模式可再开 strict |

## 与现状的差距（体检备忘）

| 现状 | 问题 |
|------|------|
| `skipUnknownField` | 主要影响「反射有、文件无」；文件有、反射无的键往往从未访问（静默忽略），行为不统一、少 Warn |
| 部分盘路径 `skipUnknownField=false`（如 Scene 另存） | 缺字段即 Fail，加字段演进痛 |
| 无 `$schemaVersion` | 无法区分协议代数；只能靠「能解就行」 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Registry 占位；对齐 F09 Persistent 容错方向 |
