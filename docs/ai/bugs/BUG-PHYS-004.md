# BUG-PHYS-004 — Collider disable/remove does not unregister physics shape

## Meta
- **ID:** BUG-PHYS-004
- **Status:** Open
- **Severity:** S2
- **Owner:**
- **Found:** 2026-09-02
- **Last updated:** 2026-09-02
- **Affects:** Physics — `ColliderComponent` hierarchy; Editor Inspector remove/disable; `RigidBodyComponent` + Jolt world
- **Related Feature/Slice:** `CORE-F06` (Component Activate); `PHYS-F04`

## TL;DR
Disabling or deleting a collider component does not remove its shape from the physics world; the owning rigid body (or ghost collider) still collides using the previous geometry until a full rebuild or other workaround.

---

## 症状
- Inspector: disable **Active** on a `BoxColliderComponent` / `SphereColliderComponent` / etc., or **Remove Component** on the collider.
- GameObject with `RigidBodyComponent` continues to collide with other bodies as if the collider were still present.
- Observed during `CORE-F06` Component Activate acceptance (Editor, `test.mescene`).

## 期望
- Disable collider (`SetActive(false)` or component removed): Jolt shape/body attachment for that collider is torn down immediately (or on EOF), same frame or next physics step.
- Re-enable or re-add collider: shape is recreated from current collider properties.
- Deleting collider while `RigidBodyComponent` remains: body is destroyed or rebuilt without the removed shape (policy TBD — at minimum no stale collision).

## 复现
1. Open `test.mescene` (or any GO with `RigidBodyComponent` + collider).
2. Play / simulate in Editor viewport (or run physics smoke with equivalent setup).
3. Disable collider **Active** or remove collider component.
4. Observe collisions unchanged (e.g. cube still rests on plane with same contact).

## 环境
- Branch: `master`
- Editor + OpenGL viewport
- Post `CORE-F06` Component Activate

## 根因
- **Suspected (not fixed):** Collider components do not participate in `ApplyActivationToSystems` / `RemoveActivationFromSystems`; `RigidBodyComponent::RefreshPhysicsBody` is not notified when sibling collider is disabled or destroyed. `PhysicsWorld` may retain stale Jolt shape refs until explicit `Unregister` / `RebuildWorldBodies`.

## 修复
- **Deferred.** Likely paths:
  - Collider `OnDeactivate` / destructor → notify owning `RigidBodyComponent::RefreshPhysicsBody` or `PhysicsWorld::RebuildBody`.
  - `RemoveComponentCommand` / `GameObject::RemoveComponent` physics side-effects (see `PhysicsEditorSideEffects`).
  - Optional: collider-only bodies (no rigid body) need their own activation hooks.

## 回归验证
- [ ] Disable collider Active → no collision with that shape (mesh may still render).
- [ ] Remove collider → rigid body falls through or body removed per design.
- [ ] Re-enable collider → collision restored.
- [ ] `physics-smoke` / `physics-shapes` still pass.

## 关联
- [CORE-F06 Component Activate Design](../Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md)
- [TD-026](../TECH_DEBT.md) — deserialize `m_Owner` bypass (separate issue)
- [BUG-PHYS-003](./BUG-PHYS-003.md) — intermittent add collider crash

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | Registered during CORE-F06 acceptance |
