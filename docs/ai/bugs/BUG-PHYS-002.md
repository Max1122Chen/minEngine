# BUG-PHYS-002 — Inspector Reflection Writes Bypass Physics Component Setters

## Meta
- **ID:** BUG-PHYS-002
- **Status:** Open (tactical mitigation landed on physics branch)
- **Severity:** S1
- **Owner:**
- **Found:** 2026-06-12
- **Last updated:** 2026-06-12
- **Affects:** Editor Inspector, `RigidBodyComponent`, `BoxColliderComponent`, reflection `ME_REFLECTION_ACCESSOR_FIELD`
- **Related Feature/Slice:** PHYS-F01-S01-e

## TL;DR
Inspector and `SetObjectProperty` deserialize directly into reflected fields, skipping `SetSimulatePhysics` / collider refresh side effects; physics state drifts until an authority transform edit wakes the body.

---

## 症状
- Toggle **Simulate Physics** in Inspector: field flips but body may stay inactive; object does not resume falling until Transform is edited.
- Change **BoxCollider** `HalfExtent` in Inspector: Jolt shape not rebuilt (setter only assigns vector; no `RefreshPhysicsBody` even when called from code).
- `SetSimulatePhysics(true)` from code / tests works (`OnRigidBodySimulatePhysicsChanged` runs).

## 期望
- Inspector edits invoke the same semantics as runtime setters (push/activate/deactivate, shape rebuild).
- `SyncBodiesFromScene` dirty rules remain valid after Inspector edits.

## 复现
1. Playground/Editor: dynamic box simulating.
2. Inspector: uncheck **Simulate Physics**, then check again.
3. Box stays frozen until Root Transform is nudged.

## 环境
- Branch: `physics`
- Editor Inspector (`SceneEditorInspectorSource`)

## 根因
- Reflection exposes `m_bSimulatePhysics`, `m_HalfExtent` via `ME_REFLECTION_ACCESSOR_FIELD` (direct member pointer).
- `DrawPrimitiveProperty` / `ApplySetObjectProperty` write fields without calling `SetSimulatePhysics` or shape refresh.
- `SyncBodiesFromScene` only activates on transform dirty or `OnRigidBodySimulatePhysicsChanged`; sim-on + clean transform → no activate.

**Note:** `SceneComponent` subclasses get generic `MarkRenderStateDirty` after Inspector edits; **non-SceneComponent** physics components do not.

## 修复
**Tactical (physics branch, 2026-06-12):** `ApplyPhysicsEditorSideEffects` from Inspector / `ApplySetObjectProperty`; `RigidBodyComponent::ApplySimulatePhysicsToWorld`; `BoxColliderComponent::SetHalfExtent` refreshes shape; `SceneComponent::ApplyEditorTransformEdit` for `m_Transform`. **Not** unified Assign — see BUG-CORE-001 (master).

## 回归验证
- [ ] Editor: simulate off → on without transform nudge → box falls same frame / next frame
- [ ] Editor: HalfExtent change updates collision
- [x] `physics-sync` + `physics-smoke` still pass

## 关联
- BUG-PHYS-001
- BUG-CORE-001 (umbrella: reflection bypasses setters)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-12 | Filed from SimulatePhysics Inspector investigation |
