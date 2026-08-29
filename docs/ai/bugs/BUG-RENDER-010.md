# BUG-RENDER-010 — Vulkan directional shadow false self-shadow on large plane

## Meta
- **ID:** BUG-RENDER-010
- **Status:** Open
- **Severity:** S1
- **Owner:**
- **Found:** 2026-08-28
- **Last updated:** 2026-08-29
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

## 修复状态（TD-025 分层回退后）

**基础设施（保留）：** `RHIClipSpaceCapabilities` / `RHIClipSpace` / `RHIViewportConvention`；ShadowPass convention + Front cull；ForwardRenderer ZO 矩阵；`EngineSceneBindingSets` 槽清空；dir shadow index 门控。

**已回退（2026-08-29）：** `MinEngineShadowProject`、`ShaderCompiler` clip/flip define 注入、TD025-S08 shadow debug。Shader 采样回到 `bbdcdc` 语义（`projCoords*0.5+0.5`，无 ZO depth 分支）。

**待做：** 按 [handoff §6](../sessions/2026-08-29-vulkan-shadow-handoff.md) 重新闭合 Layer C（必要时 B），一次一层。

## 回归验证
- [ ] VK `test` scene visual parity
- [ ] GL regression
- [x] Build `minEngine` + `Editor`

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-28 | Filed; initial ZO patch (ineffective) |
| 2026-08-28 | TD-025 caps + shadow scheme A |
| 2026-08-29 | 分层回退 shader/flip；BUG 重开 Open；见 sessions handoff |
