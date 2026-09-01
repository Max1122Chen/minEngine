# BUG-RENDER-003 — Directional Light CastShadow Ignored in Lighting Shaders

## Meta
- **ID:** BUG-RENDER-003
- **Status:** Fixed
- **Severity:** S2
- **Owner:**
- **Found:** 2026-06-12
- **Last updated:** 2026-08-04
- **Affects:** `DirectionalLightComponent`, `MaterialPhongLighting.glslinc`, `MaterialPBR.glslinc`, `MaterialSceneShadows.glslinc`
- **Related Feature/Slice:** Render lighting / shadow pass

## TL;DR
Toggling **Cast Shadow** on a directional light updated the component and proxy, but Phong/PBR always sampled cascaded shadow maps. **Fixed** 2026-08-04: `ComputeMaterialSceneShadowVisibility` gates on `DirectionalLight.Params.w >= 0` (same pattern as point/spot).

---

## 症状
- Inspector **Cast Shadow** on directional light appears to do nothing (or not immediately): scene keeps directional shadows.
- Long-standing Editor observation; unlike physics, this is **not** fixed by nudging Transform.

## 期望
- `CastShadow == false` → no directional shadow term in lighting (same frame after EOF update).
- `CastShadow == true` → shadow pass + shaded result.

## 复现
1. Editor scene with directional light + shadow-casting meshes.
2. Toggle **Cast Shadow** on directional light in Inspector.
3. Observe shadowing on meshes unchanged.

## 环境
- Branch: `physics` (render code shared with master); verified fix on `feat/render`
- OpenGL Phong/PBR graph materials

## 根因
- CPU: `CollectShadowRequests` correctly gates on `dirLightProxy->m_CastsShadow`; `UpdateLight` copies `CastShadow()` to proxy.
- GPU: `ComputeMaterialSceneShadowVisibility` always called `ComputeDirectionalShadowFactor` when direction was non-zero — it did **not** check `DirectionalLight.Params.w` (shadow map index) unlike point/spot paths.

## 修复
- Gate directional shadow application on `int(DirectionalLight.Params.w + 0.5) >= 0` in `MaterialSceneShadows.glslinc`.
- Landed with BUG-RENDER-004 CSM acne work (same commit batch).

## 回归验证
- [x] Dir light CastShadow off / no dir shadow request → `Params.w < 0` → no dir shadow darkening (A/B during BUG-RENDER-004)
- [ ] Inspector CastShadow toggle alone → shadows disappear/return without restart
- [ ] Point/spot CastShadow regression check

## 关联
- BUG-RENDER-004 (CSM self-shadow acne)
- BUG-PHYS-002 (similar symptom class, different mechanism)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-12 | Filed; shader always samples dir shadows confirmed in code review |
| 2026-08-04 | Fixed: `Params.w` gate in `ComputeMaterialSceneShadowVisibility` |
