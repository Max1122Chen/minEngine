# CORE-F05 — Play Mode — Implementation Plan

## Meta
- **ID:** `CORE-F05`
- **Type:** Implementation Plan
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [Design Spec](./CORE-F05_PLAY_MODE_DESIGN.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)

## TL;DR
**S00** Attach 序列化 → **S01** Clone → **S02b** SceneContext/TickPolicy → **S02** `PlayInEditorSession` → **S03** View/Input → **S04** Systems → **S05** 增强。

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| CORE-F05-S00 | `SceneComponent::m_AttachParent` → `ME_PROPERTY` GUID + load 重建 children | Planned | `serialization-archive`；含 Attach 的 scene |
| CORE-F05-S01 | `SceneDuplicator` + `SceneCloneContext` + Remap 单测 | Planned | `scene-clone` test |
| CORE-F05-S02b | `ESceneType`、`ESceneTickPolicy`、`SceneContext`、`SceneManager` API | Planned | 编译 + smoke |
| CORE-F05-S02 | `PlayInEditorSession` Enter/Stop、双 Scene、Mapping | Planned | Play/Stop 手动 |
| CORE-F05-S03 | Toolbar、`ActiveSceneScope`、View/Input | **Done** | Editor 目视通过 |
| CORE-F05-S04 | Per-World Physics/Audio/Render/Lua + TickPolicy 门控 | **Done** | `audio-smoke`（手动目视 deferred） |
| CORE-F05-S05 | Pause/Step、PIE Inspector 只读 | Deferred | — |
| CORE-F05-S06 | Observing Context（Inspector / Debug Command 观察目标） | Planned | Play 时可观察 PIE |

---

## 2) 切片详情

### CORE-F05-S00 — Attach `ME_PROPERTY` 序列化

- **Goal:** Attach 纳入反射/磁盘/Clone 统一路径。
- **Touch:**
  - `SceneComponent.h/.cpp` — `m_AttachParent` 为 `ME_PROPERTY`；序列化 GUID
  - Load / `ResolvePendingObjectRefs` 后重建 `m_AttachChildren`
  - 现有 `.mescene` 兼容（无 parent 字段 = root）
- **DoD:**
  - [ ] 保存/加载保留 parent 关系
  - [ ] Clone Remap 后 PIE Attach 正确
- **Verify:** 含父子 Attach 的测试 scene；`serialization-archive`

---

### CORE-F05-S01 — SceneDuplicator

- **Goal:** `DuplicateForPIE`；Invariant A/D；依赖 S00。
- **Touch:** `SceneDuplicator.{h,cpp}`、`Serializer` clone 选项、`SceneCloneTest.cpp`
- **Wire:** in-memory **JSON** round-trip（与 `SceneLoader` 同 `SerializerOptions`）；**TD-029**；Binary 恢复待 **TD-028**。
- **DoD:**
  - [ ] PIE GUID ≠ Editor；Asset 共享
  - [ ] Attach parent GUID 在 PIE 内正确
  - [ ] 含双 physics-mesh GO 的 scene clone 通过（`scene-clone` physics-stack 用例）
- **Verify:** `minEngineTests.exe test scene-clone`

---

### CORE-F05-S02b — SceneManager / SceneContext

- **Goal:** 双 Scene 查询；`ESceneTickPolicy` 门控。
- **Touch:** `Scene.h`、`SceneManager.h/.cpp`
- **DoD:**
  - [ ] `GetEditorScene` / `GetPIEScene` / `GetTickTargetScene`
  - [ ] `TickScenes`：`Editor @ Playing` → `None`；`PIE` → `Gameplay`
  - [ ] Physics tick 仅 Gameplay policy
- **Verify:** 现有 `physics-smoke`（Editing 路径）

---

### CORE-F05-S02 — PlayInEditorSession

- **Goal:** Enter/Stop；Editor Scene 常驻。
- **Touch:** `Editor/PlayMode/PlayInEditorSession.{h,cpp}`、`PlayObjectMapping`、`IPlayModeService`
- **DoD:**
  - [ ] Enter：Duplicate → Register → `Playing`
  - [ ] Stop：仅销毁 PIE
  - [ ] Enter 失败 rollback
  - [ ] 设置各 `SceneContext.TickPolicy`
- **Verify:** 未保存编辑 Play/Stop 后仍在

---

### CORE-F05-S03 — View / Input / Toolbar

- **依赖：** [ED-F03 Editor Toolbar](../../Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md)（Chrome 可见性；**Review 待批**）
- **Touch:** `SceneEditingViewportClient`、`SceneEditingViewportWindow`、`SceneEditor::RouteViewportInput`、`EditorInputHub`、`ActiveSceneScope`
- **DoD:**
  - [x] Play 时 viewport 绑定 PIE `RenderScene` + 主相机跟随
  - [x] Play 时禁用 Editor 视口导航 / 选择操作
  - [x] Play 时**不绘制** ImGuizmo（仅禁操作不够）
  - [x] `RouteViewportInput` Play 时不路由到 SceneEditor
  - [x] Play 时 `EditorInputHub` 仅保留全局命令（如 Stop/F5）；跳过子模块编辑快捷键与 Undo/Redo
  - [ ] `IViewContextProvider` 抽象 — **Deferred**（现有 `GetViewTargetScene` / `IsPlayModeViewActive` 够用；避免过早抽象）
- **Verify:** Editor 目视 — Play 无 gizmo、相机跟随 PIE、Stop 后恢复 Editor 编辑 — **通过（2026-09-03）**

---

### CORE-F05-S04 — Per-World Systems

- **DoD:**
  - [x] Editor 静音；PIE Audio 正常
  - [x] 双 `PhysicsWorld` / 双 `RenderScene`（既有 per-Scene 实例 + tick 门控）
  - [x] `OnBeginPIE` / `OnEndPIE`（Audio + Physics；`UnregisterPIEScene` 仍负责 PIE teardown）
- **Verify:** `minEngineTests.exe test audio-smoke` PASS；`physics-smoke` 未在本切片重跑；Editor 目视 deferred

---

### CORE-F05-S05 — Deferred

Pause/Step；PIE Inspector 只读（与 S06 observing 可协同，但本切片仍 defer）。

---

### CORE-F05-S06 — Observing Context（后续切片）

- **问题：** Play 时 viewport 已看 PIE，但 **Inspector / Debug Console 命令**仍操作 Editor World（需求错位，非 bug）。
- **Goal:** 显式 Observing Context — 选择观察/命令目标为 Editor 或 PIE（含 Hierarchy 是否切 PIE 树、get/set 走哪套 GUID）。
- **DoD（草案）：**
  - [ ] Play 时 Inspector 可切到 PIE 对象（只读优先，与 S05 对齐）
  - [ ] Console / unified commands 可指定或跟随 Observing Context
  - [ ] Stop 后自动回到 Editor Context
- **Verify:** Play 中 inspect/get 命中 PIE；Stop 后恢复 Editor

---

## 3) 依赖顺序

```text
S00 → S01 → S02b → S02 → S03 → S04
                           ↘
                            S06（Observing；可后于 Feature MVP）
S05 Deferred
```

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | 初稿 |
| 2026-09-02 | 双 World；S00 Attach；命名修订；TickPolicy |
| 2026-09-03 | S01：PIE clone 改 in-memory JSON（TD-029）；Binary 待 TD-028 |
| 2026-09-03 | **S04 Done：** Audio PIE 门控 + `OnBeginPIE`/`OnEndPIE`；Physics hooks；`audio-smoke` PIE gating |
| 2026-09-03 | **S03 WIP：** viewport → PIE `RenderScene`；Play 时禁 Editor 视口输入；PIE 相机跟随 |
| 2026-09-03 | S03：Play 隐藏 gizmo；`EditorInputHub` PIE 输入门控；`IViewContextProvider` Deferred |
| 2026-09-03 | **S03 Done**（目视通过）；登记 **S06 Observing Context**（Inspector/Command 仍绑 Editor） |
