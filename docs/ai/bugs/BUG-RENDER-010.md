# BUG-RENDER-010 — Vulkan directional shadow false self-shadow on large plane

## Meta
- **ID:** BUG-RENDER-010
- **Status:** Fixed (pending user visual verify)
- **Severity:** S1
- **Owner:**
- **Found:** 2026-08-28
- **Last updated:** 2026-08-28
- **Affects:** Vulkan Editor / ForwardRenderer CSM, `test` scene plane receiver, ED-F01-S06
- **Related Feature/Slice:** ED-F01-S06 · TD-025

## TL;DR
Vulkan Editor: directional shadow on 100×100 plane appeared as huge false self-shadow; three light types incorrect on VK. **Root cause (revised):** fragmented clip/viewport/cull policy (`IsVulkan()` branches), not PCF. Plane cast off removed blob → caster/winding path. **Fix:** TD-025 `RHIClipSpaceCapabilities` + Shadow scheme A (`flipY=false`, Front cull). Prior ZO-only patch was ortho no-op.

---

## 症状
- `test` scene, `--rhi vulkan`: plane huge false shadow; cube shadow may slide with camera.
- All three shadow types wrong on VK; GL OK.
- Disabling plane Cast Shadow removes large blob.

## 期望
- No plane false self-shadow when plane casts.
- Cube shadow tracks transform; stable on orbit camera.
- Point/spot/dir shadows match GL.

## 修复（TD-025）
- `RHIClipSpaceCapabilities` / `RHIClipSpace` / `RHIViewportConvention`
- ShadowPass: `ShadowMap2D` + `CubeMapFace` viewport; `GetEffectiveCullMode()` → Front on VK
- `MinEngineShadowProject` + shader defines from caps
- Dir light shadow index gate; spot/point SRV slot clear (BUG-RENDER-011)

## 回归验证
- [ ] VK `test` scene visual parity
- [ ] GL regression
- [x] Build `minEngine` + `Editor`

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-28 | Filed; initial ZO patch (ineffective) |
| 2026-08-28 | TD-025 caps + shadow scheme A |
