# BUG-PHYS-003 — Intermittent crash when adding BoxColliderComponent in Editor

## Meta
- **ID:** BUG-PHYS-003
- **Status:** Fixed
- **Severity:** S2
- **Owner:**
- **Found:** 2026-09-01
- **Last updated:** 2026-09-02
- **Affects:** Editor Inspector → Add Component → `BoxColliderComponent`; `feat/debug-drawing` S02
- **Related Feature/Slice:** RND-F11-S02

## TL;DR
During S02 acceptance, adding `BoxColliderComponent` caused an **intermittent** crash once; **not reproduced** after retry. Treated as resolved after subsequent physics/editor work (2026-09-02).

---

## 症状
- Inspector: select GO (e.g. Cube with `RigidBodyComponent` in `test` scene) → Add Component → `BoxColliderComponent`.
- Editor crashes or hits debugger break **once**; subsequent attempts did not reproduce.
- User later confirmed wireframe draws correctly; issue treated as flaky for now.

## 期望
- Adding any collider component is stable; no crash on first add or on Inspector refresh.

## 复现
- **Not reliably reproducible** (2026-09-01).
- Suspected context: `test` scene, GO with `StaticMeshComponent` + `RigidBodyComponent`, first `BoxColliderComponent` add during S02 smoke.
- Possible paths (unverified): `RigidBodyComponent::RefreshPhysicsBody` → `RegisterRigidBody`; `PhysicsDebugDraw::SubmitScene` on same frame; Inspector property draw for new component.

## 环境
- Branch: `feat/debug-drawing`
- Editor: `SceneEditingViewportClient` + `EnableDebugDraw`
- RHI: OpenGL (user session)

## 根因
- **Unknown** — no stack trace captured. Intermittent nature suggests timing/order (physics body register vs debug submit vs Inspector) or one-shot bad state, not a deterministic logic bug in the S02 path.

## 修复
- **Fixed (2026-09-02):** No recurrence after physics collider lifecycle / CORE-F06 activation work and related Editor paths. Original root cause not isolated; closed as non-reproducible.

## 回归验证
- [x] No recurrence in Editor add-collider smoke (2026-09-02, user)
- [x] S02 wireframe visual acceptance (2026-09-01)
- [x] `physics-smoke` / `physics-shapes` / `render-graph` pass

## 关联
- RND-F11-S02 Implementation Plan
- BUG-PHYS-002 (Inspector physics side effects — separate issue)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Filed from S02 acceptance; intermittent, not blocking S02 commit |
