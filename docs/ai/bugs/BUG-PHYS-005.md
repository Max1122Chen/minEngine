# BUG-PHYS-005 — Physics writeback MarkRenderStateDirty causes Cube flicker while dragging

## Meta
- **ID:** BUG-PHYS-005
- **Status:** Verified
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-10-25
- **Last updated:** 2026-10-25
- **Affects:** Physics writeback · Primitive Recreate · Editor Multi/Single viewport
- **Related Feature/Slice:** CORE-F45 frame roles (Apply-after-draw amplifies); CORE-F46 Editor Multi

## TL;DR
Dragging a Dynamic+Simulate Cube in the Editor makes the mesh strobe (disappear / reappear). Root: `SetTransformFromSimulation` marks **State** dirty → Recreate nulls `m_PrimitiveComponent` before Flush while ForwardRenderer skips null-component proxies during Draw.

---

## 症状
- Editor（含 `--frame-threads multi`）拖 Gizmo 移动带 RigidBody（Dynamic + Simulate）的 Cube。
- Cube **本体**亮灭 / 偶尔消失再出现，不是单纯与 Gizmo 错位。
- 停下后往往又能稳定显示。

## 期望
- 物理回写与 Gizmo 一样只更新 Transform（`UpdatePrimitiveTransform`），拖动时网格连续可见。

## 复现
1. 打开 MyMEProject，场景 `test`（Cube 带 `RigidBodyComponent` Simulate + Dynamic）。
2. 编辑态（非 PIE 亦可：Editor 场景 `TickPolicy=Gameplay`，物理仍会 Step）。
3. Translate Gizmo 拖动 Cube，观察 viewport 中网格是否频闪。

## 环境
- OS: Windows；worktree `feat/core` @ `minEngine-core`
- OpenGL Editor；`--frame-threads multi` 或 single 均可触发（Recreate 路径与帧序有关）

## 根因
1. `PhysicsWorld::SyncBodiesToScene` → `SetWorldTransformFromSimulation` → `MarkRenderStateDirty()`（应为 Transform dirty）。
2. EOF `RecreatePrimitive`：Game 上立刻 `oldProxy->m_PrimitiveComponent = nullptr`，Remove/Add 入队。
3. `TickRendererFrame`：**先 Draw 再 `FlushApply`**。Draw 时旧 Proxy 仍在列表，但 `ForwardRenderer` 因 `!m_PrimitiveComponent` 跳过 → 该帧不画。
4. 运动中每帧 Transform 变化都会 Recreate → 持续闪没；静止早退不 dirty → 又能看见。

## 修复
- `SetTransformFromSimulation`：改 `MarkRenderTransformDirty()`。
- `RecreatePrimitive`：Flush 前不要掏空仍在 RT 列表中的 Proxy 的 `m_PrimitiveComponent`（避免 Draw 窗口期被跳过）。

## 回归验证
- [x] Multi Editor 拖 Dynamic Cube：主人确认网格连续、无亮灭（2026-10-25）。
- [x] 静止显示正常（同次目视）。
- [ ] （可选）PIE 下落物体仍跟随物理，无整帧消失。

## 关联
- CORE-F45 Render：画 `World(N-1)` / Apply-after-draw
- `docs/ai/ACTIVE_WORK.md`（feat/core）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-25 | Open；根因确认；开始修复 |
| 2026-10-25 | Fixed：`MarkRenderTransformDirty` + Recreate 延迟清空 component 指针；待主人 Multi 拖 Cube 验收 |
| 2026-10-25 | Verified：主人确认拖 Cube 不再闪没 |
