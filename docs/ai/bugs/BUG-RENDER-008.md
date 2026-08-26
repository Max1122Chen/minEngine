# BUG-RENDER-008 — Vulkan Skybox ±Y Faces Split / Wrong Orientation

## Meta
- **ID:** BUG-RENDER-008
- **Status:** Verified
- **Severity:** S1
- **Owner:**
- **Found:** 2026-08-26
- **Last updated:** 2026-08-26
- **Affects:** `EnvMapCapture` HDR→cubemap bake, Vulkan viewport Y-flip
- **Related Feature/Slice:** ED-F01 BF-S03

## TL;DR
HDR sky side faces looked plausible; **±Y (top/bottom) looked split/wrong**. Bake used the same negative-height viewport Y-flip as the scene. **Fixed** by baking cube faces with an unflipped viewport.

---

## 症状
- Vulkan sky: polar faces discontinuous / rotated vs OpenGL.

## 期望
- Cubemap continuity on all six faces, close to GL.

## 复现
1. Vulkan Editor with HDR env (citrus); inspect zenith/nadir.

## 根因
`RHICmdSetViewport` always flips Y for GLM/GL-style projection. Equirect→cube face raster + lookAt ups for ±Y interact badly with that flip.

## 修复
EnvMap bake `SetViewport(..., flipY=false)`. Scene mesh path keeps flip.

## 回归验证
- [x] VK sky ±Y continuous (user 2026-08-26)
- [x] GL bake unchanged

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-26 | Open → Fixed (bake unflipped viewport) |
| 2026-08-26 | User verified → Verified |
