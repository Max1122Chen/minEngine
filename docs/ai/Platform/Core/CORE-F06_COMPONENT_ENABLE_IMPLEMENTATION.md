# CORE-F06 — Component Activate — Implementation Plan

## Meta
- **ID:** `CORE-F06`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-02
- **Related:** [Design Spec](./CORE-F06_COMPONENT_ENABLE_DESIGN.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)

## TL;DR
**Public：** `SetActive` / `IsActive` / `OnActivate` / `OnDeactivate` / `m_bActive`。  
**Private：** `TryActivate` / `Deactivate` / `ApplyActivationToSystems` / `RemoveActivationFromSystems` / `m_bPendingActivation` / `m_bActivationApplied`。  
**Editor：** Undo via `m_bActive` property blob + `SyncActivationWithActiveFlag`。

---

## S01 — 基类契约与状态机

**任务：**
- [x] `m_bActive`、`m_bPendingActivation`、`m_bActivationApplied`
- [x] `SetActive`、`IsActive`、`CanApplyActivation`
- [x] `TryActivate`、`Deactivate`、`ResolvePendingActivation`
- [x] `ApplyActivationToSystems`、`RemoveActivationFromSystems` 骨架（protected virtual）
- [x] `OnActivate`、`OnDeactivate`（protected virtual）
- [x] `SetOwner` 挂接；`SyncActivationWithActiveFlag`（undo/反序列化）
- [x] 反射 + 序列化；旧场景默认 `m_bActive=true`

**Done when：** 不变量 §3.1 成立；无 public `SetEnabled`/`ApplyActivation`。

---

## S02 — Inspector

- [x] Header **Active** checkbox → `SetActive`
- [x] Undo（`m_bActive` blob + `SyncActivationWithActiveFlag`）；inactive 灰显

---

## S03 — Tick / Lua

- [x] `GameObject::Tick`：`!IsActive()` skip
- [x] `LuaComponent`：`IsActive() && m_ScriptEnabled`

---

## S04 — Physics

- [x] `ApplyActivation` → `RefreshPhysicsBody`
- [x] `RemoveActivation` → `DestroyPhysicsBody`
- [x] `SetOwner` 不经 bypass；`RebuildWorldBodies` 跳过 inactive

---

## S05 — Audio

- [x] `ApplyActivation` / `RemoveActivation` on Audio / Listener
- [x] `SetOwner` 经 `TryActivate` 路径

---

## S06 — Render + SkyBox

- [x] `RemoveActivation` 立即 Remove proxy；`ApplyActivation` → `MarkRenderStateDirty`
- [x] 删除 `SkyBoxComponent::m_Enabled`，改用 `m_bActive`；`DetachSceneProxy()`

---

## S07 — 收尾

- [x] Scene Load / CreateNewScene → `ResolvePendingActivationsForScene`（`SyncActivationWithActiveFlag` reconcile；TD-026 根治 deferred）
- [ ] 可选 `DeferredActivationRequest` 队列（**Defer** — 物理回调内 toggle 再议）
- [x] Registry、`PROGRESS_LOG`

**验证：** `physics-smoke` / `physics-shapes` / `audio-smoke` / `object-manager` / `serialization-archive` PASSED；`verify.ps1` smoke 中 `material-ir` 失败为既有问题（与 F06 无关）。Editor Active 勾选目视待确认。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 草稿 |
| 2026-09-01 | 命名：Active + ApplyActivation/RemoveActivation + PendingActivation |
| 2026-09-01 | S01–S07 实现（DeferredActivation 队列 Defer）；Inspector Undo 修复 |
