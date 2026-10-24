# ASSET-F11 — Async JSON Asset Load Generalization Design Spec

## Meta
- **ID:** `ASSET-F11`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `Draft`
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`
- **Related:**
  - [ASSET-F09](./ASSET-F09_ASYNC_DATAASSET_LOAD_DESIGN.md)（Done；DataAsset MVP）
  - [CORE-F44](../Platform/Serialization/CORE-F44_SERIALIZER_FILE_JSON_OBJECT_SPLIT_DESIGN.md)（Done）
  - [CORE-F38](../Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md)（Done）
- **Depends on:** ASSET-F09 Done
- **Branch:** `feat/asset`（勿用 ID）
- **Priority note:** **不升 Primary**；线程分离（CORE-F45）优先期间本 Feature 仅挂号与草案。

## TL;DR

把 F09 的「Worker→Json / 游戏线程 FromJson+同步 Resolve / in-flight coalesce」从 **DataAsset 特判** 收成 **JSON 磁盘资产** 可扩展白名单机制；第二客户候选为 **Prefab 资产 Load**（仍同步 Instantiate）。**Scene 整图异步、GPU 资产、异步依赖图** 明确后置。

## Problem

1. `m_InFlightDataAssetLoads` 与 `AssetType == "DataAsset"` 特判使推广成本高。
2. Prefab/其它 JSON `.me*` 与 DataAsset 同构（File→Json→Object），重复造轮子浪费。
3. 若过早把 Scene 整图塞进同一 API，会与入世/Editor 契约缠死。

## Intent playback

| 项 | 内容 |
|----|------|
| 目标 | L1 异步装载机制可扩展到多种 JSON 资产，不绑死 DataAsset |
| 成功 | 白名单类型走同一 in-flight；Prefab 资产 AsyncLoad 可测；Instantiate 仍同步 |
| 非目标 | Scene 世界切换异步；Texture/Mesh GPU；异步递归引用图 |
| 约束 | 嵌套引用默认同步 Resolve；同步 Load 禁为合流 Drain |

## Requirements（草案）

### Must

1. in-flight 表与组装路径按「可异步 JSON 资产」通用化（命名去 DataAsset 专属）。
2. 白名单显式配置（代码表）；MVP 至少：`DataAsset` + `Prefab`（资产 Load）。
3. Prefab：**AsyncLoad 只到资产实例**；`Instantiate` / 入 Scene 仍同步、另路径。
4. 行为对齐 F09：coalesce、缓存命中入队 Drain、sync Wait join。

### Must not

5. Scene 整图 AsyncLoad 本 Feature（另开）。
6. 运行时 Sync|Async 递归开关。

## Non-Goals

- minCraft 迁移（消费方自理）。
- 改 F10 RuntimeClassName 规则。

## Open Questions

| # | Question | Default | Blocking? |
|---|----------|---------|-----------|
| Q1 | Prefab 是否本 Feature 第二客户？ | **是** | No |
| Q2 | Scene 是否列入白名单？ | **否（后置）** | No |

## Decisions

| # | 决定 |
|---|------|
| 1 | 本 Feature **Draft 挂号**；实现排在 CORE-F45 线程分离调研/设计之后或并行支线 |
| 2 | 推广范围 = JSON 文本资产 Load，不是「所有 AssetType」 |
| 3 | 嵌套引用保持同步递归（继承 F09） |

## Design Review

- **Verdict:** `Pending`
- **Ready for implementation?** `no`

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | Draft 挂号：通用化方向；Prefab 候选；Scene/GPU 后置；不挡 CORE-F45 |
