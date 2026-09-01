# Shadow Pass UBO Lifetime — Implementation Plan

## Meta
- **ID:** `RND-F14`
- **Type:** Implementation Plan
- **Status:** **Done**
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Design:** [RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md](./RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md)

## Slices

### A0 — Shadow uniform scaffolding (Done)
- [x] Add `ShadowUniformBuffers` for Dir / Spot / Point / Params write-path UBOs
- [x] Extend `ShadowDrawCommand` with resolved uniform binding info
- [x] Wire `ForwardRenderer` / `ManualRenderer` / `ShadowPass` to the new uniform owner

### A1 — Directional binding single-source (Done)
- [x] Bind directional shadow draws to `DirLightViewProj[cascade]` by `BufferOffset`
- [x] Remove directional dependence on `m_LightViewProjUniformBuffer`
- [x] Keep base pass Set1 reading the same directional buffer

### A2 — Spot binding single-source (Done)
- [x] Bind spot shadow draws to `SpotLightViewProj[slot]`
- [x] Preserve current spot-only correctness

### A3 — Point + params ring (Done)
- [x] Add point shadow ViewProj ring writes per face
- [x] Add `ShadowPassParams` ring writes per command
- [x] Stop overwriting a shared params UBO during shadow pass recording

### A4 — Cleanup + regression (Done)
- [x] Delete dead single-mat4 shadow UBO path
- [x] Regress Manual + Forward on Vulkan (user verified full-map 2026-08-31)
- [x] GL regression accepted with batch close 2026-08-31

## Verify
- [x] `minEngine` Debug build
- [x] VK Editor Manual full-map: Dir / Spot / Point shadows independent (user 2026-08-31)
- [ ] `.\scripts\verify.ps1` — recommended before merge

## Notes
- Correctness goal met; bias / receiver acne tracked separately (next round: ShadowPass Front cull).
- Phase B–E remain backlog in Design §7.

## 变更记录
| 日期 | 说明 |
|------|------|
| 2026-08-31 | A0–A4 plan created; implementation started |
| 2026-08-31 | Phase A Done; BUG-RENDER-013 closed; user VK full-map verified |
