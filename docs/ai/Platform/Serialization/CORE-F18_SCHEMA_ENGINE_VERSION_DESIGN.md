# CORE-F18 — Asset / Serialization Schema & Engine Version — Design Spec

## Meta
- **ID:** `CORE-F18`
- **Type:** Feature
- **Status:** Planned（Phase F2；**等 CORE-F17 合入后再开详设/实现**）
- **Owner:** project maintainer
- **Last updated:** 2026-09-11
- **Related:**
  - [ENGINE_0_1_0_ROADMAP.md](../../ENGINE_0_1_0_ROADMAP.md) Phase F2
  - Prior: [CORE-F10 JSON disk compat](../Serialization/CORE-F10_JSON_DISK_COMPAT_DESIGN.md)（已有 `$schemaVersion` 方向）
  - [CORE-F09 Binary wire](../Serialization/CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md)
- **Depends on:** 建议在 F1 Logger 之后（软）；不阻塞设计深化
- **Blocks:** Prefab A、稳定资产迁移、0.1.0 Demo 资产契约（D2）

## TL;DR

正式化 **资产 / 序列化 schema 版本 + 引擎版本注入**；不兼容时清晰失败（或最小迁移表）。本稿为占位；详设在 F1 后展开。

## Scope（预告）

### In
- 统一 meta 字段：`$schemaVersion`、引擎版本（semver 或 build id）
- 加载策略：未知/过旧 → 明确错误
- 样例工程关键资产过一遍

### Out
- 完整自动迁移框架全家桶（可后置）
- 存盘 Binary 全量（Transient wire 另议）

## Reader quick start

1. Roadmap §3 Phase F2  
2. CORE-F10 现状  
3. 本文件详设待补

## 1) 背景

0.1.0 多分支与 Prefab/Demo 需要可版本化资产契约。

## 2) 现状

CORE-F10 已引入 JSON 兼容与 `$schemaVersion` meta 方向；需盘点哪些资产类型已写、哪些仍缺引擎版本字段。

## 3) 方案

（待 F1 后补全：字段表、资产类型覆盖、失败 UX、与 Binary 关系。）

## 6) 验收（预告）

- [ ] D2：关键资产带 schema/引擎版本；不兼容有明确失败  
- [ ] 文档 + 样例升级说明  

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | Planned stub |
