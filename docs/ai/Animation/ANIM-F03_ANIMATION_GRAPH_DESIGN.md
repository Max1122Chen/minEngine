# ANIM-F03 — Animation Graph MVP — Placeholder

## Meta
- **ID:** `ANIM-F03`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · [ANIM-F01](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md) · [ANIM-F02](./ANIM-F02_CLIP_PLAYBACK_DESIGN.md)
- **Branch:** `feat/animation`
- **Depends on:** `ANIM-F02`（Clip + Player）

## TL;DR
轻量 **Animation Graph / Animator**：Parameters + State Machine + Transition（含 Pose Blend）。定位接近精简 Animator Controller，**不是**完整 UE AnimBP 节点 VM。正式 Design 待 F02 后写。

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | Design 正文 / Implementation / 图编辑器 |
| In（预期） | bool/int/float/trigger 参数；状态绑 Clip；条件过渡；过渡期双 Pose 混合 |
| Out（预期） | 完整节点图编辑器、Blend Tree、Layer/Additive/Mask、Montage、IK、Root Motion、Retarget |
| 后续可选 | 通用节点图 Runtime + 编辑器（另开 Feature，不阻塞本 MVP） |
| Unblock | `ANIM-F02` 可稳定播 Clip |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 占位；MVP = SM + Params + Transition Blend |
