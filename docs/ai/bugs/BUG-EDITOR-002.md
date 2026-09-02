# BUG-EDITOR-002 — Editor 冷启动偶发崩溃（Physics DebugDraw 阶段错位）

## Meta
- **ID:** BUG-EDITOR-002
- **Status:** Resolved
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-09-02
- **Resolved:** 2026-09-02
- **Last updated:** 2026-09-02
- **Affects:** Editor 冷启动首帧～数秒；**OpenGL / Vulkan 均曾高概率**（`0xC0000005`）
- **Related Feature/Slice:** `PhysicsDebugDraw`, `DebugDrawService`, `SceneEditingViewportClient`

## TL;DR
冷启动后数秒内偶发 `0xC0000005`。**根因已符号化确认：** `PhysicsDebugDraw::SubmitScene` 在 ImGui 阶段入队碰撞体 debug，但 `SubmitSceneDraw` 因首帧 viewport RT 未 publish 被跳过 → `DebugDrawPass` 不 `ClearFrameQueues` → 队列与懒初始化交织踩内存。**非** ImGui 字体 atlas / 显卡驱动主因。S04 修复 + 撤销 S03 过度 RT 守卫后，多次冷启动压测**不再复现**；viewport 场景 RT 正常显示。

---

## 症状（修复前）
- Editor 冷启动，项目与场景加载成功后，**数秒内偶发**无提示崩溃（窗口直接消失）。
- Windows 退出码：**`-1073741819` (`0xC0000005` ACCESS_VIOLATION)**。
- 日志末行常为 `EditorAppearance: UI font atlas rebuilt`（**误导性**——崩溃栈不在 atlas 路径）。

## 期望
- 冷启动稳定；viewport 场景 RT 首帧后正常显示；Physics debug 与 scene draw 生命周期一致。

## 复现（修复前）

### 自动化（本机 2026-09-02）
```powershell
cd minEngine\bin
$proj = (Resolve-Path "..\MyMEProject\MyMEProject.meproject").Path
1..50 | ForEach-Object { ... 15s ... }
# OpenGL：early=15，30% 崩溃 code=-1073741819
```

### 手动
`Editor.exe --rhi opengl --project <MyMEProject.meproject>`，多次冷启动。

## 环境
- OS: Windows 10/11
- Build: `Debug`（`minEngine/build`）
- 项目：`MyMEProject`，默认场景 `test`

## 根因（已确认 — 2026-09-02 符号化）

**崩溃点：** `libminEngined.dll+0x33CD4` → `DebugDraw::Box` → `DebugDrawService::EnqueueBox`（`DebugDraw.cpp:46`）

**调用链：**

```
EditorViewportWindow::OnDraw
  → SceneEditingViewportClient::EndFrame
    → PhysicsDebugDraw::SubmitScene        // EnableDebugDraw
      → SubmitCollider (BoxCollider)
        → DebugDraw::Box
          → DebugDrawService::EnqueueBox   // AV 读 0xFFFFFFFFFFFFFFFF
```

**机制：**

1. `EndFrame` 在 ImGui 绘制阶段调用 `PhysicsDebugDraw::SubmitScene`（**在** scene draw 之前）。
2. 首帧 viewport color RT 尚未 publish 时，若 scene draw 被跳过，debug 命令已入队但 `DebugDrawPass::Prepare` 不执行 `ClearFrameQueues`。
3. 下一帧再入队 + `DebugDrawService::Get()` 懒初始化 → 偶发 `vector::push_back` 访问违例。

**非主因（调查排除）：** ImGui 字体 atlas 重建、efsw 多线程竞态、显卡驱动。Jolt 使用 `JobSystemSingleThreaded`；efsw 回调不入队 `DebugDrawService`。

## 修复

| Slice | 内容 |
|-------|------|
| **S04** | `PhysicsDebugDraw` 与 `SubmitSceneDraw` 同条件（scene/camera/RT 有效且 RT 尺寸非零）；`Editor::Run` 帧头 `ClearFrameQueues()`；`Engine::Initialize` eager-init `DebugDrawService` |
| **S04b** | 撤销 S03 在 `ForwardRenderer`/`ManualRenderer`/`EndFrame` 上对 **已 publish color texture** 的守卫——该守卫阻止首帧 `SubmitSceneDraw`，导致 viewport 永久 "not ready" |
| **S01/S02** | ImGui atlas invalidate 顺序、首帧推迟 atlas、崩溃日志 `ed_crash.log`（辅助诊断，保留） |

### 正确门控逻辑

- **SubmitSceneDraw：** 仅检查 `Scene` / `Camera` / `RenderTarget` 及 RT **尺寸**（`GetWidth/Height`），**不**要求上一帧已 publish color texture。
- **PhysicsDebugDraw：** 与 `SubmitSceneDraw` 同帧、同条件调用（入队后立即 draw + clear）。

## 实施状态
- [x] S01：atlas invalidate 顺序 + Vulkan `DestroyDeviceObjects` + viewport pin registry
- [x] S02：首帧推迟 atlas + `ed_crash.log` + `kill_editor_processes.ps1`
- [x] S03：首帧跳过无 color RT 的 `SubmitSceneDraw`（**部分回滚**，见 S04b）
- [x] S04：PhysicsDebugDraw 生命周期 + 帧头 Clear + Engine init
- [x] S04b：恢复首帧 scene draw，修复 viewport RT 永久 not ready
- [x] 崩溃回归：多次冷启动不再复现（用户确认）

## 回归验证
- [x] OpenGL 冷启动多次无 `0xC0000005`（用户确认）
- [x] Viewport 场景 RT 正常显示（S04b 后）
- [ ] Vulkan 冷启动 30 次（建议补测）
- [ ] CJK glyphs 切换后 UI 正常（S01/S02 路径，建议补测）

## 关联
- [BUG-EDITOR-001](./BUG-EDITOR-001.md) — OpenGL atlas 重建（独立问题）
- [手动调试指南](./DEBUG_EDITOR_MANUAL.md) — GDB / `ed_crash.log` 符号化
- [ED-F03 Viewport Play Toolbar](../Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | 登记；初判 ImGui atlas / Vulkan |
| 2026-09-02 | **符号化确认：** `PhysicsDebugDraw` → `EnqueueBox`；S04 修复 |
| 2026-09-02 | S04b：撤销过度 RT 守卫；**Resolved**；用户多次压测通过 |
