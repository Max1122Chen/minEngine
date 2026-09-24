# BUG-CORE-004 — Edit 态 InputSystem gated Tick 跳过设备边角清理（滚轮粘滞 → 飞行极慢）

## Meta
- **ID:** BUG-CORE-004
- **Status:** Fixed
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-24
- **Last updated:** 2026-09-24
- **Affects:** Editor Edit 视口飞行；`InputSystem::Tick` + `SceneEditingViewportClient` 滚轮调速
- **Related Feature/Slice:** CORE-F29 · Fix Design：[BUG-CORE-004 Fix Design](../Platform/Core/BUG-CORE-004_INPUT_GATE_DEVICE_CLEANUP_AND_PIE_POINTER_FIX_DESIGN.md)
- **Related:** [BUG-EDITOR-004](./BUG-EDITOR-004.md)（同波 PIE 指针约束）

## TL;DR
CORE-F29 在 Edit 态对 `InputSystem::Tick` early-out 时跳过了 **MouseScroll 每帧清零**，滚轮 `Down` 粘滞，飞行 `m_MoveSpeed` 被每帧累加夹到 0.5/64。根因已定位；准备按 Fix Design S01 修。

---

## 症状
- Editor 右键飞行感觉突然极慢（或偶发极快）
- 与滚轮调速相关；非 `m_MoveSpeed` 默认值被改

## 期望
- Edit 态仍 gated 掉玩法 IMC
- 设备瞬时态（至少 MouseScroll）每帧仍按原契约清零
- 滚一次只调一次速度

## 复现
1. 打开 Editor，Scene 视口右键进入飞行
2. 滚轮向下（或向上）一次
3. 继续 WASD：速度迅速掉到约 0.5（或冲到 64）并卡住极端值

## 环境
- Windows；Debug `MaximumEditor`
- 引入提交：`99a299f`（CORE-F29）

## 根因
`IsGameplayInputEnabled()==false` 时：

```text
Tick → ClearGameplayActionEvents → return
```

落在原 Tick 末尾 **MouseScroll Idle/RawValue 清零**之前。  
`SceneEditingViewportClient` 每帧 `KeyDown(MouseScroll)` → `AdjustMoveSpeed` → 同一 `RawValue` 累加 `m_MoveSpeed`。

## 修复
S01: EndFrameDeviceEdgeCleanup on gated Tick path; test RunGatedTickStillClearsMouseScroll.

## 回归验证
- [x] gated Tick clears scroll (unit) — hand-check fly still recommended
- [ ] Edit：玩法 IMC 仍不派发（F29 契约）
- [x] PIE：玩法输入仍可用（闸门语义未改）
- [ ] 可选：`gameplay-input-gate` 仍绿

## 关联
- CORE-F29 Design/Impl
- BUG-EDITOR-004（PIE 指针，同 Fix Design S02）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-24 | 排查定位；Open + Fix Design Draft |
| 2026-09-24 | S01 implemented + gameplay-input-gate PASS |
| 2026-09-24 | Fixed (S01); S02 tracked on BUG-EDITOR-004 |
