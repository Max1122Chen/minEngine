# RND-F21 — Shadow Frustum / CSM Caster Culling

## Meta
- **ID:** RND-F21
- **Type:** Feature
- **Status:** Deferred
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - 前置可见性：[RND-F19](./RND-F19_CPU_MESH_VISIBILITY_DESIGN.md)（主视图锥；提供可复用 `Math::Geometry::Frustum`）
  - 阴影图：[RND-F08](./RND-F08_SHADOW_GRAPH_OWNERSHIP_DESIGN.md) 等（实现时再对齐 cascade 数据源）

## TL;DR

**占位 Feature：** 在 F19 的 `Frustum` 之上，对 ShadowDepth 候选做 **光视锥** 与/或 **每 cascade 体积** AABB 剔除，减少无效阴影绘制。

**本期（相对 F19）不做实现。** 登记以免遗忘；F19 主视图契约保持不变。

## Scope（未来）
- **In（规划）：** `Frustum::FromViewProjection(lightViewProj)` / cascade VP；过滤 `ShadowCasterQueue` 或 Collect 阴影支路
- **Out（相对 F19）：** 不改主视图 Opaque/Translucent 分流

## Status note
- **Reason：** F19 先落地主视图与数学类型；光/CSM 需对齐现有 cascade 构建数据。
- **Impact：** 身后/锥外 caster 在 F19 后仍可能偏多。
- **Options：** F19 Done 后开 Design 细化为 Planned。
- **Decision：** Deferred until F19 Done + 维护者排期。
- **Unblock：** F19 `Frustum` 合并；阴影 cascade 矩阵可查询。
- **Next check：** F19 收口后。

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Deferred 占位：复用 F19 `Frustum`；光锥/CSM caster 剔除 |
