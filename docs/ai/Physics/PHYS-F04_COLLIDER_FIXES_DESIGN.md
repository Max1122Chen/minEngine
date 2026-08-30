# Collider Fixes & Hygiene — Placeholder

## Meta
- **ID:** `PHYS-F04`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../ACTIVE_WORK.md), [PHYS-F03](./PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md)
- **Branch / worktree:** `feat/physics` · `D:/Dev/GitRepo/minEngine-physics`（重开前 merge `master`）

## TL;DR
**碰撞体行为修复与整理**（形状同步、trigger、layer、已知 Jolt 集成问题）。在 `PHYS-F03` 回调之前或并行推进；不依赖 VK。

## Scope
- **In:** 维护者已发现的 collider bug 列表、回归测试、与 Scene/Transform 写回一致性
- **Out:** 玩法 Contact 派发（`PHYS-F03`）；新 collider 类型

## 依赖
- **Hard:** `PHYS-F01` / `PHYS-F02`
- **Soft:** `RND-F11` DebugDrawing（视口可视化）

## 验收（草案）
- [ ] 已知问题列表逐项关闭或转 `BUG-PHYS-*`
- [ ] `test physics-*` 全绿
- [ ] GL Editor 场景可手动回归

## Status note（Planned）
| 字段 | 内容 |
|------|------|
| Why separate from F03 | 修复与派发解耦；可先修再 Broadcast |
| Resume when | `feat/physics` worktree merge `master` 后开 S01 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry + ACTIVE_WORK 登记；physics 轨解冻 |
