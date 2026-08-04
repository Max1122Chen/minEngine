# Contact Gameplay Dispatch — Placeholder

## Meta
- **ID:** `PHYS-F03`
- **Type:** Feature
- **Status:** Deferred
- **Owner:** project maintainer
- **Last updated:** 2026-08-04
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md), [TECH_DEBT.md](../TECH_DEBT.md) **TD-006**, [CORE-F04 Design](../Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)

## TL;DR
玩法侧接触通知应走 **委托（Delegate）** 订阅，而不是先落地一套 Collider 虚函数过渡 API。依赖 **CORE-F04 Native multicast**（TD-006）；故 **本 Feature 搁置**；完整 Design / Implementation **暂不编写**。

## 依赖与重开条件
- **Blocked on:** [CORE-F04](../Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) Native multicast 落地（TD-006）
- **Reopen when:** `AddMEObject` / `Broadcast` 可用且可挂到组件或等价玩法入口
- **Then:** 再写正式 Design（事件源仍为 F01 Contact 双缓冲；派发目标为本委托，而非虚函数方案）

## 现状可用（不依赖本 Feature）
- `PhysicsWorld::GetContactEvents()` 轮询 Begin/End（测试与临时玩法）

## Status note（Deferred）
| 字段 | 内容 |
|------|------|
| Why paused | 正确产品形态依赖委托；虚函数是过渡方案，避免先污染 API |
| What's done | F01/F02 已提供 Contact buffer 与形状/查询 |
| What's not | 玩法派发、委托绑定、正式 Design |
| Resume when | CORE-F04 Done — 可写正式 Design |
| Owner | project maintainer |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-01 | 曾起草 Collider 虚函数 Design（Review）；用户决定搁置，改为占位并依赖 TD-006 |
| 2026-08-04 | 依赖改为显式指向 CORE-F04 Native multicast Design |
| 2026-08-04 | CORE-F04 Done；可写正式 Design |
