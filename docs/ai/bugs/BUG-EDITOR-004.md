# BUG-EDITOR-004 — PIE 时鼠标未限制在 Scene Viewport 内

## Meta
- **ID:** BUG-EDITOR-004
- **Status:** Fixed
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-24
- **Last updated:** 2026-09-24
- **Affects:** Play-In-Editor；Scene 视口指针 UX
- **Related Feature/Slice:** CORE-F05-S03 · Fix Design：[BUG-CORE-004 Fix Design](../Platform/Core/BUG-CORE-004_INPUT_GATE_DEVICE_CLEANUP_AND_PIE_POINTER_FIX_DESIGN.md) S02–S03
- **Related:** [BUG-CORE-004](./BUG-CORE-004.md)

## TL;DR
PIE 期间 Editor 关闭视口导航路由，也未提供「指针限制在 Scene Viewport / 相对鼠标」机制；玩法看向时鼠标易飞出视口，体验差。属能力缺口（F29 使玩法输入更「真」后更明显），非 scroll 同源。

---

## 症状
- PIE 时移动鼠标可离开 Scene 视口，点到其它 Editor 窗格
- 无 Cursor Disabled / Clip / warp-to-center 等约束

## 期望
- PIE 且 Scene 视口聚焦（或玩法请求指针锁）时：指针限制在该 viewport 矩形内，或使用相对鼠标（CURSOR_DISABLED）且位移只喂给游戏
- 离开 PIE / 失焦时恢复普通光标

## 复现
1. Play
2. 在 Scene 视口内大幅移动鼠标（若有鼠标看向绑定更明显）
3. 光标离开视口

## 环境
- Windows；PIE via MaximumEditor
- 相关：`EditorInputHub` PIE 早退（CORE-F05-S03，`4552c0f`）

## 根因
- PIE：`ProcessInput` 不跑 `ProcessViewportRouting` → 无 Edit 导航的 `SetCursorVisible(false)` / `GLFW_CURSOR_DISABLED`
- PlayMode / WindowSystem **无** viewport 矩形 ClipCursor / 相对指针 API 供游戏或 Editor 在 PIE 启用

## 修复
S02: PIE+Scene viewport focused -> \WindowSystem::SetCursorClipRect\ (Win32 ClipCursor) on ImageMin/Size; clear on unfocus/Stop/dtor.

S03 follow-up: while clipped, Win32 Raw Input feeds virtual Mouse2D so look intent continues at edges; GetMousePosition stays physical for ScreenUI.

## 回归验证
- [x] ClipCursor on focused PIE viewport image rect (Win32) — hand-verify
- [x] Clear clip on unfocus / Stop / viewport dtor
- [x] Edit navigate path unchanged (separate SetCursorVisible)
- [x] S03: look continues when cursor is pressed against viewport edge
- [x] S03: ScreenUI hit-test still follows physical cursor

## 关联
- BUG-CORE-004 · CORE-F05 Play Mode

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-24 | Open；与 CORE-004 同波 Fix Design |
| 2026-09-24 | Fixed S02 ClipCursor confine |
| 2026-09-24 | Reopened residual：边界 look 意图；S03 Raw Intent |
| 2026-09-24 | Fixed S03；手验 OK |
