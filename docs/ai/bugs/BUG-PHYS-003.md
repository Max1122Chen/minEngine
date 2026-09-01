# BUG-PHYS-003 — Intermittent crash when adding BoxColliderComponent in Editor

## Meta
- **ID:** BUG-PHYS-003
- **Status:** Open (intermittent; not reproduced after S02 visual acceptance)
- **Severity:** S2
- **Owner:**
- **Found:** 2026-09-01
- **Last updated:** 2026-09-01
- **Affects:** Editor Inspector → Add Component → `BoxColliderComponent`; `feat/debug-drawing` S02
- **Related Feature/Slice:** RND-F11-S02

## TL;DR
During S02 acceptance, adding `BoxColliderComponent` to a selected GameObject in the Scene editor caused an **intermittent** crash/assert. Could not reproduce reliably after retry; collider wireframe visualization works. Defer investigation unless it recurs.

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
- **Deferred.** If recurrence: capture call stack / `ME_ASSERT` site; check `PhysicsSystem::HasInstance` guard in viewport; verify `AddComponentCommand` raw pointer lifetime; log Jolt body creation failures.

## 回归验证
- [ ] Reproduce with fixed steps + stack trace
- [x] S02 wireframe visual acceptance (user, 2026-09-01)
- [x] `physics-smoke` / `physics-shapes` / `render-graph` pass

## 关联
- RND-F11-S02 Implementation Plan
- BUG-PHYS-002 (Inspector physics side effects — separate issue)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Filed from S02 acceptance; intermittent, not blocking S02 commit |
