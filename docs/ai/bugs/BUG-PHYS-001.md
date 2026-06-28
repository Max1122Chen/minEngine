# BUG-PHYS-001 — Scene Load Does Not Rebuild Jolt Bodies

## Meta
- **ID:** BUG-PHYS-001
- **Status:** Open
- **Severity:** S1
- **Owner:**
- **Found:** 2026-06-12
- **Last updated:** 2026-06-12
- **Affects:** `SceneManager::LoadSceneByPath`, `SceneLoader`, `RigidBodyComponent` / `BoxColliderComponent`, Editor + headless scene load
- **Related Feature/Slice:** PHYS-F01-S01

## TL;DR
Opening a saved scene creates an empty `PhysicsWorld` but never registers deserialized rigid bodies; simulation appears dead until a runtime path triggers `RefreshPhysicsBody`.

---

## 症状
- Scene asset already contains `RigidBodyComponent` + `BoxColliderComponent` with `m_bSimulatePhysics = true`.
- After **Open Scene**, objects do not fall / collide.
- Runtime **Add Component** path works; `physics-smoke` / `physics-sync` pass (they use `AddComponent` → `SetOwner`).

## 期望
- Load scene → all valid rigid-body pairs register in the scene's `PhysicsWorld` before the next `SimulateActiveScene`.
- Unload → bodies destroyed (already works via `DestroyWorld`).

## 复现
1. Save a scene with a dynamic box (Root + RigidBody + BoxCollider).
2. Close / reload scene in Editor (or `LoadSceneByPath`).
3. Observe object stays frozen while simulate flags look correct in Inspector.

## 环境
- Branch: `physics`
- OS: Windows (Editor)

## 根因
- `LoadSceneByPath` only calls `GetOrCreateWorld` (empty world).
- Jolt registration is hooked to `RigidBodyComponent::SetOwner` / `BoxColliderComponent::SetOwner` → `RefreshPhysicsBody`.
- Deserialization resolves `m_Owner` via GUID → **raw pointer write**, never calls `SetOwner`.
- `m_PhysicsBodyId` in scene file is stale (`0`); no post-load rebuild pass.

## 修复
**Tactical (physics branch, 2026-06-12):** `PhysicsSystem::RebuildWorldBodies` after `LoadSceneByPath` / `CreateNewScene`. Full `PostLoad` deferred — see BUG-CORE-001.

## 回归验证
- [x] `minEngineTests.exe test physics-load`
- [ ] Editor: open saved scene → dynamic box falls

## 关联
- BUG-PHYS-002 (Inspector bypasses physics setters)
- BUG-CORE-001 (unified property mutation — **deferred to master**)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-12 | Filed from Editor scene-open investigation |
