# Collider Fixes & Hygiene — Placeholder

## Meta
- **ID:** `PHYS-F04`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../ACTIVE_WORK.md), [PHYS-F03](./PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md)
- **Branch:** `master`（小修复轨；不再依赖 `feat/physics` 长分支）

## TL;DR
Collider **Extent 轴 convention** 与 **Scale 对 extent 语义**（world half-extent vs local；与 Jolt 体创建、`PhysicsDebugDraw` 一致）。在 `PHYS-F03` 回调之前或并行推进。

## Scope
- **In:** Extent 轴、Scale→extent、形状同步、与 Scene/Transform 写回一致性、已知 collider bug
- **Out:** 玩法 Contact 派发（`PHYS-F03`）；新 collider 类型

## 依赖
- **Hard:** `PHYS-F01` / `PHYS-F02`
- **Soft:** `RND-F11` DebugDrawing（视口可视化）

## 验收（草案）
- [ ] Extent/Scale 语义与 debug draw、Jolt 体一致
- [ ] 已知问题列表逐项关闭或转 `BUG-PHYS-*`
- [ ] `test physics-*` 全绿
- [ ] GL Editor 场景可手动回归

## Status note（Planned）
| 字段 | 内容 |
|------|------|
| Why separate from F03 | 修复与派发解耦；可先修再 Broadcast |
| Branch | `master` |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry + ACTIVE_WORK 登记 |
| 2026-09-01 | 改轨 `master`；明确 Extent/Scale scope |
