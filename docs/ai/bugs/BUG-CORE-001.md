# BUG-CORE-001 — Reflection Field Writes Bypass Component Setters (Editor)

## Meta
- **ID:** BUG-CORE-001
- **Status:** Open
- **Severity:** S2
- **Owner:**
- **Found:** 2026-06-12
- **Last updated:** 2026-06-12
- **Affects:** Reflection (`ME_REFLECTION_ACCESSOR_FIELD`), Editor Inspector, `ApplySetObjectProperty`, scene deserialization
- **Related Feature/Slice:** TD-005 (reflection docs); **architecture fix deferred to `master`**

## TL;DR
Any `EditAnywhere` field with a hand-written `SetXxx` that performs side effects can be bypassed when Editor or serializer writes the field directly; physics is the first impacted subsystem. **Physics branch uses tactical `ApplyPhysicsEditorSideEffects` until unified `AssignProperty` lands on master.**

---

## 症状
- Inspector / undo / deserialize mutates reflected **member** storage, not setter API.
- Components with side-effect setters (`SetSimulatePhysics`, `SetCastShadow`, mesh setters, etc.) behave differently from code call sites.
- `SceneComponent` subclasses get a **partial** mitigation: Inspector calls `MarkRenderStateDirty` after edits, but this does not cover non-`SceneComponent` types or transform→physics dirty (`MarkTransformDirty`).

## 期望
- Single authoritative mutation path per property (setter or `OnPropertyChanged`).
- Editor, undo, and deserialization all trigger the same hooks.

## 复现
See BUG-PHYS-002, BUG-PHYS-001; directional CastShadow is **not** explained by this alone (see BUG-RENDER-003).

## 根因
- `ME_REFLECTION_ACCESSOR_FIELD` exposes raw member address.
- No code-gen or runtime dispatch from property name to `SetXxx`.
- Deserialization `ResolvePendingObjectRefs` assigns raw pointers without `SetOwner`.

## 修复
**Deferred to `master`:** unified property mutation (`AssignProperty` / codegen Setter binding / `PostEditChangeProperty` + optional `PostLoad`). Do not expand tactical per-component Editor switches on physics branch except for PHYS-F01 blockers.

## 回归验证
- [ ] Representative matrix: physics simulate, collider extent, light color, mesh assignment

## 关联
- BUG-PHYS-001, BUG-PHYS-002, BUG-RENDER-003

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-12 | Umbrella filed alongside PHYS-F01 Editor findings |
| 2026-06-12 | Tactical physics mitigations on branch; unified fix deferred to master |
