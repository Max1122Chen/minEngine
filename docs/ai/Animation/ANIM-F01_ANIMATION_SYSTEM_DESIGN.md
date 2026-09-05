# Animation System — Placeholder

## Meta
- **ID:** `ANIM-F01`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../ACTIVE_WORK.md), [ENGINE_DESIGN_PHILOSOPHY.md](../ENGINE_DESIGN_PHILOSOPHY.md), [ENGINE_CAPABILITY_ROADMAP.md](../ENGINE_CAPABILITY_ROADMAP.md)
- **Branch:** `feat/animation`（自 `master`；Design 确认后开）

## TL;DR
动画系统。正式 Design 待写。目标：可扩展的 **mechanism**（Skeletal Mesh / Asset / Instance / Pose / Skinning），而非一次落地完整 Anim Graph Framework。受 [ENGINE_DESIGN_PHILOSOPHY.md](../ENGINE_DESIGN_PHILOSOPHY.md) 约束；阶段位置见 [ENGINE_CAPABILITY_ROADMAP.md](../ENGINE_CAPABILITY_ROADMAP.md) M1。

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | Design / Implementation |
| Branch | `feat/animation`（自 `master`；CORE-F05 MVP 已满足 Play 验证） |
| Depends on | Reflection / Component / Render path（已有）；**非**完整 Async Asset / Prefab / GC |
| Next | 正式 Design Spec → Pre-flight → ACTIVE_WORK 确认焦点后开码 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry 占位登记 |
| 2026-09-01 | 分支改为 `feat/animation`；依赖 CORE-F05/F06 + merge 检查点 |
| 2026-09-03 | CORE-F05 MVP Done；前置放宽；对齐设计哲学 / Capability Roadmap M1 |
