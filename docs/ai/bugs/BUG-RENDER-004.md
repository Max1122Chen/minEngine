# BUG-RENDER-004 — Directional CSM Self-Shadow Acne on Large Ground Planes

## Meta
- **ID:** BUG-RENDER-004
- **Status:** Fixed
- **Severity:** S2
- **Owner:**
- **Found:** 2026-08-04
- **Last updated:** 2026-08-04
- **Affects:** Directional cascaded shadow maps, `ShadowPass`, `ForwardRenderer::ExpandCascadeZForShadowCasters`, `MaterialSceneShadows.glslinc`, OpenGL rasterizer depth bias
- **Related Feature/Slice:** CSM / lighting (orthogonal to RND-F05; discovered while validating render branch)

## TL;DR
Large scaled ground planes showed strong texture-following **vertical/banding stripes** under directional light; stripes vanished when the directional shadow pass was disabled. Root cause: CSM self-shadow acne (inflated cascade Z + no front-face cull / depth bias + weak receiver bias). **Fixed** 2026-08-04.

---

## 症状
- Ground plane (`scale ~100×1×100`, PBR material with albedo/normal maps) shows obvious dark/light stripes aligned with texture grain under directional light.
- Disabling the directional light (or skipping directional shadow map generation while keeping dir lighting) removes the stripes.
- Point-light-only lighting looks clean.
- Post-process / Sharpen / ShadowPass GLSL-vs-SPIR-V were ruled out; issue existed before RND-F05 commits.

## 期望
- Directional CSM darkens only true occluder shadows.
- Flat/large receivers should not show dense self-shadow acne / Moire-like bands.

## 复现
1. Open `MyMEProject` scene `test` with directional light CastShadow on, large plane casting+receiving shadows.
2. Observe banding on the plane with marble material.
3. Set `CollectShadowRequests` to skip directional (or disable dir light) → banding gone.

## 环境
- Branch: `feat/render` (also present at post-rebase `c565e82`)
- OpenGL Editor, Debug
- Shadow map resolution was 512; cascade Z expand used bounding-sphere radius

## 根因
1. **Self-shadow write:** Shadow depth pass did not cull light-facing faces; large ground wrote its own depth into the cascade and then sampled it → classic acne amplified by normal maps.
2. **Cascade Z over-expansion:** `ExpandCascadeZForShadowCasters` used mesh **bounding-sphere radius**, which for a 100×100 plane (~70+ radius) hugely inflated light-space Z and destroyed depth precision.
3. **Weak receiver bias:** Face-on `NdotL` used ~`0.0005` depth bias; no polygon offset on the depth pass.
4. Related: directional shadow sampling ignored `Params.w` (see BUG-RENDER-003); fixed in the same change set.

## 修复
- Shadow PSO: **front-face cull** + `DepthBiasSlopeScale`/`DepthBiasConstant` via RHI → OpenGL `glPolygonOffset`.
- Cascade Z: expand with light-space **AABB corners** (`Math::Geometry::Transform`), not sphere radius.
- Receiver: cascade-scaled bias + light-direction sample offset; gate sampling on `DirectionalLight.Params.w >= 0`.
- Shadow map resolution **512 → 1024**.
- Hygiene: tangent transform uses model matrix (not inverse-transpose) under non-uniform scale (TBN correctness; not the stripe root cause).

## 回归验证
- [x] Editor: dir light + CSM on → ground stripes gone (user visual 2026-08-04)
- [x] A/B: disable dir shadow pass → stripes gone (confirmed root class)
- [ ] CastShadow off on dir light → no dir shadow term (`Params.w` gate; also BUG-RENDER-003)
- [ ] Thin single-sided casters still cast reasonably (front-cull trade-off)

## 关联
- BUG-RENDER-003 (dir CastShadow / `Params.w` gate — fixed together)
- Known pitfall: large flat casters + sphere Z expand → CSM precision collapse

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-04 | Filed + Fixed after A/B (disable dir shadow pass) and CSM acne mitigations |
