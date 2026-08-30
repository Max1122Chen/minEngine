# Contact Gameplay Dispatch — Placeholder

## Meta
- **ID:** `PHYS-F03`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md), [TECH_DEBT.md](../TECH_DEBT.md) **TD-006**, [CORE-F04 Design](../Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)

## TL;DR
玩法侧接触通知应走 **委托（Delegate）** 订阅。**CORE-F04** 已 Done；完整 Design / Implementation **待写**。

## 依赖
- **Hard:** [CORE-F04](../Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) Native multicast（Done）
- **Soft:** [RND-F11](../FEATURE_REGISTRY.md) DebugDrawing — Editor 视口可视化 Contact/Collider（可用 `test physics-contact` 先推进）
- **Branch:** `feat/physics` · 建议 **PHYS-F04** 修复后再或并行 F03

## 现状可用（不依赖本 Feature）
- `PhysicsWorld::GetContactEvents()` 轮询 Begin/End（测试与临时玩法）

## Status note（Planned）
| 字段 | 内容 |
|------|------|
| What's done | F01/F02 Contact buffer；CORE-F04 Delegates |
| What's not | 玩法派发、委托绑定、正式 Design |
| Resume when | `feat/physics` merge `master` 后写 Design S01 |
| Owner | project maintainer |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-01 | 曾起草 Collider 虚函数 Design（Review）；用户决定搁置，改为占位并依赖 TD-006 |
| 2026-08-04 | 依赖改为显式指向 CORE-F04 Native multicast Design |
| 2026-08-04 | CORE-F04 Done；可写正式 Design |
| 2026-08-31 | Deferred → Planned；physics 轨解冻；F11 降为软依赖 |
