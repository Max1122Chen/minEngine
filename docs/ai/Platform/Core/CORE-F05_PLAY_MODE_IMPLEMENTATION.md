# CORE-F05 — Play Mode — Implementation Plan

## Meta
- **ID:** `CORE-F05`
- **Type:** Implementation Plan
- **Status:** **Done**（MVP；S05 Deferred）
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [Design Spec](./CORE-F05_PLAY_MODE_DESIGN.md) · [S06](./CORE-F05_S06_INSPECTING_CONTEXT.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)

## TL;DR
MVP 竖切已完成：S00–S04 + S06。**S05** Pause/Step Deferred。遗留：EnterPlay rollback（**TD-030**）、Binary PIE（**TD-028/029**）。

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| CORE-F05-S00 | `SceneComponent::m_AttachParent` → `ME_PROPERTY` GUID + load 重建 children | **Done** | 代码 + scene load |
| CORE-F05-S01 | `SceneDuplicator` + `SceneCloneContext` + Remap 单测 | **Done** | `scene-clone`（JSON wire / TD-029） |
| CORE-F05-S02b | `ESceneType`、`ESceneTickPolicy`、`SceneContext`、`SceneManager` API | **Done** | 编译 + Play 路径 |
| CORE-F05-S02 | `PlayInEditorSession` Enter/Stop、双 Scene、Mapping | **Done** | Play/Stop 目视；rollback → TD-030 |
| CORE-F05-S03 | Toolbar、View/Input、gizmo 隐藏 | **Done** | Editor 目视 |
| CORE-F05-S04 | Per-World Physics/Audio + TickPolicy 门控 | **Done** | `audio-smoke` |
| CORE-F05-S05 | Pause/Step | **Deferred** | — |
| CORE-F05-S06 | Inspecting Context（Hierarchy / Inspector / Command） | **Done** | 目视通过 |

---

## 2) 切片详情

### CORE-F05-S00 — Attach `ME_PROPERTY` 序列化 — **Done**

- **DoD:**
  - [x] 保存/加载保留 parent 关系
  - [x] Clone Remap 后 PIE Attach 正确

### CORE-F05-S01 — SceneDuplicator — **Done**

- **Wire:** in-memory **JSON**（**TD-029**）；Binary 恢复待 **TD-028**。
- **DoD:**
  - [x] PIE GUID ≠ Editor；Asset 共享
  - [x] Attach parent GUID 在 PIE 内正确
  - [x] 双 physics-mesh GO `scene-clone` 通过

### CORE-F05-S02b — SceneManager / SceneContext — **Done**

- **DoD:**
  - [x] `GetEditorScene` / `GetPIEScene` / `GetTickTargetScene`
  - [x] `TickScenes`：Editor @ Playing → `None`；PIE → `Gameplay`
  - [x] Physics tick 仅 Gameplay policy

### CORE-F05-S02 — PlayInEditorSession — **Done**（rollback 欠账）

- **DoD:**
  - [x] Enter：Duplicate → Register → `Playing`
  - [x] Stop：仅销毁 PIE
  - [ ] Enter 失败 rollback → **TD-030**
  - [x] 各 `SceneContext.TickPolicy`

### CORE-F05-S03 — View / Input / Toolbar — **Done**

- **DoD:** 见既有勾选；`IViewContextProvider` Deferred。

### CORE-F05-S04 — Per-World Systems — **Done**

- **DoD:** Audio/Physics hooks；Lua per-world → CORE-F08+。

### CORE-F05-S05 — Deferred

Pause/Step。

### CORE-F05-S06 — Inspecting Context — **Done**

- 见 [CORE-F05_S06_INSPECTING_CONTEXT.md](./CORE-F05_S06_INSPECTING_CONTEXT.md)。

---

## 3) 依赖顺序（完成态）

```text
S00 → S01 → S02b → S02 → S03 → S04 → S06   # MVP Done
S05 Deferred
```

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | 初稿 |
| 2026-09-03 | S01 JSON detour；S03/S04/S06 Done |
| 2026-09-03 | **Feature MVP Done**；对齐 S00–S02 状态；rollback → TD-030 |
