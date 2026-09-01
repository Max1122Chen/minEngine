# BUG-RENDER-007 — Plane Texture Looks More Zoomed on Vulkan

## Meta
- **ID:** BUG-RENDER-007
- **Status:** Verified
- **Severity:** S2
- **Owner:**
- **Found:** 2026-08-26
- **Last updated:** 2026-08-26
- **Affects:** Vulkan Editor material sampling / UV appearance
- **Related Feature/Slice:** ED-F01 BF-S04

## TL;DR
Plane albedo looked more “zoomed” vs OpenGL. After Per-Object UBO ring fix, user confirmed texture density matches GL — treated as collateral of wrong model matrix, not a separate sampler bug.

---

## 症状
- Same `MaterialIRSmoke` / marble on plane; VK density differs from GL.

## 期望
- Acceptable parity or documented intentional difference.

## 复现
1. GL vs VK Editor, same camera on `test` plane.

## 根因
Same as BUG-RENDER-005: Cube draw used Plane’s matrix → large non-uniform scale warped UV density on what looked like the ground.

## 修复
No separate UV/sampler change; fixed by Per-Object UBO ring (BUG-RENDER-005/006).

## 回归验证
- [x] User confirms VK plane texture density matches GL (2026-08-26)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-26 | Filed; deferred code until post-UBO visual check |
| 2026-08-26 | Verified WontFix-as-separate — collateral of UBO hazard |
