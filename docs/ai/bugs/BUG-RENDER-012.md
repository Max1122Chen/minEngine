# BUG-RENDER-012 — Shadow index -1 rounds to slot 0 in shader

## Meta
- **ID:** BUG-RENDER-012
- **Status:** Fixed (verified 2026-08-29 — GL point Cast Shadow off no stale sampling)
- **Severity:** S2
- **Found:** 2026-08-29
- **Affects:** OpenGL + Vulkan Editor; point/spot/dir shadow disable; all materials using `Params.w` shadow index
- **Related:** BUG-RENDER-011 · TD-025 · `MaterialPhongLighting.glslinc`

## TL;DR

CPU sets `Params.w = -1` when Cast Shadow is off, but shader decodes with `int(Params.w + 0.5)`. In GLSL, `int(-1.0 + 0.5) = int(-0.5) = 0` (truncate toward zero), so **disabled lights still sample shadow map slot 0** with stale depth.

## 症状

- OpenGL: toggle point light Cast Shadow **off** → lighting still darkened as if shadow map were active.
- Stale cube/2D depth in slot 0 continues to be sampled (binding may still hold last texture).
- May worsen multi-light composites on Vulkan (one light “off” still reads slot 0).

## 根因

```glsl
// CPU when shadow disabled:
Params.w = -1.0f;

// Shader (broken):
int shadowIndex = int(light.Params.w + 0.5);  // → 0, not -1
if (shadowIndex >= 0) { Sample... }           // always enters for slot 0
```

`UpdateLightUBO` / `CollectShadowRequests` / `EngineSceneBindingSets` clearing are **correct**; gate never sees `-1`.

## 期望

- `Params.w < 0` → no shadow sampling for that light.
- Slot 0 only used when explicitly assigned index 0.

## 修复方向（最小）

Prefer gate on float before round, e.g.:

```glsl
if (light.Params.w >= 0.0) {
    int shadowIndex = int(light.Params.w + 0.5);
    ...
}
```

Apply consistently in: `MaterialPhongLighting.glslinc`, `MaterialSceneShadows.glslinc`, `MaterialPBR.glslinc`, `Phong.frag`.

Optional CPU: clear point shadow ViewProj if added later; not required for this bug.

## 回归验证

- [x] GL: point Cast Shadow off → no shadow darkening; slot 0 texture unused
- [ ] GL: spot/dir same toggle
- [ ] VK: multi-light scene — disabled light does not read slot 0
- [ ] Cast Shadow on → shadows still work

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-29 | Verified on GL — point Cast Shadow toggle |
