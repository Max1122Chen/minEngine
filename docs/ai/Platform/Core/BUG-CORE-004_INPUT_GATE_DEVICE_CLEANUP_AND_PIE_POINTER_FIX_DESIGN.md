# BUG-CORE-004 / BUG-EDITOR-004 — Input gate 设备清理 + PIE 指针约束 — Fix Design

## Meta
- **ID:** BUG-CORE-004（主）· BUG-EDITOR-004（同波）
- **Type:** Bug fix design
- **Complexity:** L1（S01 L0；S02 L1；S03 L1）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-24
- **Related:**
  - [BUG-CORE-004](../../bugs/BUG-CORE-004.md)
  - [BUG-EDITOR-004](../../bugs/BUG-EDITOR-004.md)
  - [CORE-F29](./CORE-F29_PLAY_GATED_GAMEPLAY_INPUT_DESIGN.md)
  - CORE-F05-S03（PIE 抑制 Editor 视口路由）

## TL;DR

**S01：** F29 gated Tick 与设备边角清理解耦。  
**S02：** PIE 时 ClipCursor 到 Scene Viewport。  
**S03：** Clip 边界仍保留 look 意图（Raw Input → virtual Mouse2D；物理位留给 UI）。

---

## Scope

### In

| Slice | Goal | Done when |
|-------|------|-----------|
| **S01** | Gated Tick 仍跑设备边角清理 | **Done** — EndFrameDeviceEdgeCleanup + unit |
| **S02** | PIE 指针约束到 Scene viewport | **Done** — Win32 ClipCursor on ImageMin/Size |
| **S03** | Clip 边界保留 look 意图 | **Done** — Raw Input + virtual Mouse2D；手验 OK |

### Out

| 项 | 说明 |
|----|------|
| 重做 IMC / 改 F29 闸门语义 | 只修清理遗漏 |
| 完整 Raw Input / 多视口指针锁框架 | MVP 单 Scene Editing Viewport |
| 游戏侧「请求 Pointer Lock」通用 API | S02 可先 Editor/PIE 专用；后置通用化 |
| 改默认 `m_MoveSpeed` | 非根因 |

---

## S01 — 设备边角清理（BUG-CORE-004）

### 要求
1. `!IsGameplayInputEnabled()` 时：**仍禁止** IMC 累积 / InputComponent 派发（保持 F29）。
2. **必须**执行原 Tick 末尾的 MouseScroll 复位（及未来同类「事件粘滞」清理）。
3. 不引入「Edit 下偷偷跑半套 IMC」。

### 方案（推荐）

```text
InputSystem::Tick(dt)
  clear ActionsWithEventThisTick
  if (!IsGameplayInputEnabled())
      ClearGameplayActionEvents()
      EndFrameDeviceEdgeCleanup()   // NEW: scroll reset, etc.
      return
  ... existing IMC path ...
  EndFrameDeviceEdgeCleanup()       // shared tail
```

- 把现有 scroll reset 抽成 `InputSystem::EndFrameDeviceEdgeCleanup()`（或 private `ResetTransientDeviceState`）。
- 单测（可选加强）：Edit 闸门开着时，注入 scroll Down 后 Tick，断言 action Idle / RawValue 0。现有 `gameplay-input-gate` 保持绿。

### 备选（不采用）
- 在 `SceneEditingViewportClient::ApplyMoveSpeedFromScroll` 里自行清 scroll — 治标、其它消费者仍粘滞。

### 验证
- 手验飞行滚轮；`test gameplay-input-gate`

---

## S02 — PIE 指针约束（BUG-EDITOR-004）

### 要求
1. PIE **且** Scene Editing Viewport **聚焦**（建议再加 hovered 或「捕获中」）时约束指针。
2. Stop PIE、视口失焦、或离开 Play：恢复 `GLFW_CURSOR_NORMAL` / 取消 Clip。
3. 不恢复 PIE 下的 Editor 飞行导航（仍遵守 F05-S03：不 `ProcessViewportRouting`）。

### 方案（推荐默认）

**A. Viewport 矩形 ClipCursor（Win32）+ 可选中心附近软夹**

- Editor：PIE 时从 `SceneEditingViewportWindow` / Client 取 ImGui 视口屏幕矩形，经 `WindowSystem` 新 API `SetCursorClipRect(optional)`。
- GLFW 层：Win32 `ClipCursor`；其它平台先 no-op 或 CURSOR_DISABLED 降级。
- 每帧或 Focus 变化时更新矩形（dock 拖拽后）。

**B. 整窗 GLFW_CURSOR_DISABLED（相对鼠标）** — 实现快，但整 Editor 窗锁定，离开视口仍「看不见光标」，比飞出好、不如 A 精确。

**推荐：** S02 MVP 用 **A**；若 Win32 接线成本高，可先 **B 仅在 viewport focused**（DISABLED），再迭代 A。

### 挂点
- `PlayInEditorSession` Enter/Stop，或 `SceneEditingViewportClient::EndFrame` 在 `IsPlayModeViewActive()` 分支调用 `UpdatePlayPointerPolicy()`。
- 与现有 Edit 导航 `SetCursorVisible(!navigating)` 互斥：PIE 路径独立，Stop 时显式 Normal。

### 验证
- 手验 PIE 移鼠不出视口；Stop 后可点 Hierarchy；Edit 右键飞行仍 DISABLED。

---

## S03 — Clip 边界保留 look 意图（BUG-EDITOR-004 follow-up）

### 问题
`ClipCursor` 把物理光标卡在视口边后，GLFW 绝对坐标不再变化；依赖 `Mouse2D` 位置差分的 Look（及同类）在边界「推不动」。

### 要求
1. 物理光标仍约束在 viewport（ScreenUI `GetMousePosition` 用物理坐标）。
2. 在边界继续推动时，IMC/`Mouse2D` 的 look 意图仍累计（Raw 相对位移）。

### 方案
- Clip 开启时：Win32 Raw Input 相对位移 → `WindowSystem::ConsumeRelativeMouseDelta`。
- `InputSystem::SetPreserveMouseIntentBeyondClip(true)`：`Mouse2D` 走 virtual absolute（物理种子 + Raw 积分）；`GetMousePosition` 始终 physical。
- Clip 关闭：同步回 physical，注销 Raw Input / 还原 WndProc。

### 验证
- [ ] PIE：光标停在视口边时仍可持续左右/上下看
- [ ] ScreenUI 点击仍跟物理光标
- [ ] Stop/失焦后 clip + preserve 清除

---

## 实现顺序

1. **S01** 立刻做（小、确认回归）
2. **S02** 紧随（需 WindowSystem API + 手验）
3. **S03** Clip 边界 Raw Intent（本 follow-up）

## 风险

| 风险 | 缓解 |
|------|------|
| S01 抽函数漏其它瞬时键 | 先只搬 scroll；注释契约 |
| S02 ClipCursor 与 ImGui 抢光标 | 仅 PIE+viewport focus；Stop 必清理 |
| 多监控坐标 | 用 GLFW/ImGui 屏幕坐标一致路径 |
| S03 Raw + 绝对双计 | preserve 期间 Mouse2D 只用 Raw，不用 glfw 绝对 |

## Acceptance

### S01
- [x] Unit: gated Tick clears MouseScroll
- [x] F29 gate paths unchanged (early-out still clears IMC)
- [x] `gameplay-input-gate` PASS

### S02
- [x] Win32 ClipCursor when PIE viewport focused
- [x] Stop/失焦/dtor clear clip
- [x] Edit navigate SetCursorVisible unchanged

### S03
- [x] Raw Input relative while clipped; Mouse2D virtual; GetMousePosition physical
- [x] Hand-verify look continues at viewport edge

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-24 | Draft：S01 清理解耦；S02 Clip/DISABLED 方案 |
| 2026-09-24 | S01 EndFrameDeviceEdgeCleanup landed |
| 2026-09-24 | S02 ClipCursor landed |
| 2026-09-24 | S03：Clip 边界 Raw Intent / physical vs virtual Mouse2D |
| 2026-09-24 | S03 Done；手验 OK |
