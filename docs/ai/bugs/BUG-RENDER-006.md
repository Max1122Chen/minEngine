# BUG-RENDER-006 — Plane Y-Scale Appears to Add Thickness (Vulkan)

## Meta
- **ID:** BUG-RENDER-006
- **Status:** Verified
- **Severity:** S1
- **Owner:**
- **Found:** 2026-08-26
- **Last updated:** 2026-08-26
- **Affects:** Vulkan Editor mesh transforms / Per-Object UBO
- **Related Feature/Slice:** ED-F01 BF-S02

## TL;DR
Changing plane `scale.y` looked like it thickened a Y=0 sheet. Likely the Cube mesh was drawn with the plane's non-uniform scale (shared UBO hazard). Fixed with Per-Object UBO ring (same as BUG-RENDER-005).

---

## 症状
- Vulkan: editing plane Y-scale elongates a visible “slab”.
- `plane.obj` vertices are Y=0 — scale.y cannot create geometric thickness by itself.

## 期望
- Thin plane stays thin; Cube keeps its own transform.

## 复现
1. Vulkan Editor, `test` scene.
2. Change plane Scale.Y; observe thickness vs OpenGL.

## 根因
Same as BUG-RENDER-005: last Per-Object UBO write applied to earlier draws (Cube → plane matrix).

## 修复
Per-Object UBO ring (BF-S02).

## 回归验证
- [x] VK: plane Y-scale no longer creates a thick slab; Cube separate (user 2026-08-26)

## 关联
- BUG-RENDER-005

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-26 | Open → Fixed with 005 |
| 2026-08-26 | User verified → Verified |
