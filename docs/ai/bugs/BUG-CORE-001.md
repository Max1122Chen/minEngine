# BUG-CORE-001 — Reflection Field Writes Bypass Component Setters (Editor)

## Meta
- **ID:** BUG-CORE-001
- **Status:** Fixed
- **Severity:** S2
- **Owner:**
- **Found:** 2026-06-12
- **Last updated:** 2026-09-05
- **Affects:** Reflection (`ME_REFLECTION_ACCESSOR_FIELD`), Editor Inspector, `ApplySetObjectProperty`, scene deserialization
- **Related Feature/Slice:** **CORE-F11** (AssignProperty + Getter/Setter thunks)

## TL;DR
Any `EditAnywhere` field with a hand-written `SetXxx` that performs side effects can be bypassed when Editor or serializer writes the field directly. **Fixed on `feat/core` via CORE-F11:** optional `meta=(Getter/Setter)` → native thunks + unified `AssignProperty`; `ApplyPhysicsEditorSideEffects` removed.

## 修复（CORE-F11）
- Codegen injects `ME_REFLECTION_PROPERTY_*_THUNK` macros; `AssignProperty` / `GetPropertyValue`.
- Serializer leaves + pending ObjectPtr resolve use Assign when Setter present (`m_Owner` → `SetOwner`).
- Editor: undo path Assign; live Inspector PostEdit for in-place writes; physics fields annotated with Setters.
- **Residual:** fields without Getter/Setter meta still direct-write; migrate side-effect fields as needed.

## 回归验证
- [x] `serialization-archive` · `scene-clone` · `reflection-function` assign · `physics-smoke`

## 变更记录
| 日期 | 说明 |
|------|------|
| 2026-06-12 | Open；physics tactical side-effects |
| 2026-09-05 | Fixed via CORE-F11 |
