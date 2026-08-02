# BUG-RENDER-003 — Directional Light CastShadow Ignored in Lighting Shaders

## Meta
- **ID:** BUG-RENDER-003
- **Status:** Open
- **Severity:** S2
- **Owner:**
- **Found:** 2026-06-12
- **Last updated:** 2026-06-12
- **Affects:** `DirectionalLightComponent`, `MaterialPhongLighting.glslinc`, `MaterialPBR.glslinc`, `MaterialSceneShadows.glslinc`
- **Related Feature/Slice:** Render lighting / shadow pass

## TL;DR
Toggling **Cast Shadow** on a directional light updates the component and proxy, but Phong/PBR directional lighting always samples cascaded shadow maps; CPU skips shadow pass when disabled, so the toggle has little or no visible effect.

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
- Branch: `physics` (render code shared with master)
- OpenGL Phong/PBR graph materials

## 根因
- CPU: `CollectShadowRequests` correctly gates on `dirLightProxy->m_CastsShadow`; `UpdateLight` copies `CastShadow()` to proxy; Inspector **does** call `MarkRenderStateDirty` for `LightComponent` (subclass of `SceneComponent`).
- GPU: `CalcDirLightGraph` / `CalcDirLightPBR` / `ComputeDirectionalShadowFactor` always call `SampleDirShadowPCF` — they do **not** check `DirectionalLight.Params.w` (shadow map index) unlike point/spot paths.
- Stale `DirLightViewProj` / shadow atlas can keep shading dark even when shadow pass skipped.

**Not the same root cause as BUG-PHYS-002** (setter bypass); proxy update path works; shader ignores cast-shadow gate.

## 修复
<!-- TBD: gate directional shadow on Params.w or uniform flag when shadowIndex < 0 -->

## 回归验证
- [ ] Dir light CastShadow off → no shadow darkening on lit meshes
- [ ] CastShadow on → shadows return
- [ ] Point/spot CastShadow regression check

## 关联
- BUG-PHYS-002 (similar symptom class, different mechanism)
- BUG-RENDER-001

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-12 | Filed; shader always samples dir shadows confirmed in code review |
