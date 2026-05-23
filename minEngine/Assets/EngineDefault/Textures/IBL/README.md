# Engine default IBL textures (Phase 4–5)

Place cubemap face PNGs here. The loader expects **square** faces with **matching** width/height and channel count.

## Load order (startup)

```text
irradiance: irradiance_posx.png … negz.png
  else GPU convolution from environment (32×32)
  else alias environment cubemap

environment: environment_posx.png … negz.png (optional, with mips)
  else *.hdr → GPU equirect → cubemap 512×512 (+ mips)
  else 6-color validation cubemap (32×32)

prefilter: prefilter_*.png
  else GPU prefilter from environment (512, mips 0–7, 32 samples/texel)
  else alias environment cubemap

brdf_lut: brdf_lut.png
  else CPU integrated LUT — place brdf_lut.png to skip slow startup
```

## Irradiance cubemap (diffuse IBL)

File naming: `{prefix}_{face}.png` with OpenGL face order:

| Face | Filename |
|------|----------|
| +X | `irradiance_posx.png` |
| -X | `irradiance_negx.png` |
| +Y | `irradiance_posy.png` |
| -Y | `irradiance_negy.png` |
| +Z | `irradiance_posz.png` |
| -Z | `irradiance_negz.png` |

If missing and **environment** exists: `EnvMapCapture::ConvolveIrradiance` (shaders `irradiance_convolution.*`).

## Environment cubemap (specular source / future skybox)

HDR: `environment.hdr` or any `*.hdr` → `EquirectToCubemap` (512² + mips).

## Prefilter cubemap (specular IBL)

If `prefilter_*.png` missing and environment is RGB16F:

- Shaders: `prefilter.{vert,frag}`
- `EnvMapCapture::PrefilterEnvironment` — GGX importance sampling, **32** samples, mips **0–7** (matches `kMaterialPBRMaxReflectionLod` in `MaterialIBL.glslinc`)

## Optional offline assets

| Asset | Naming |
|-------|--------|
| Prefiltered env | `prefilter_posx.png` … `prefilter_negz.png` |
| BRDF LUT | `brdf_lut.png` (512×512) |

**Recommended BRDF LUT:** [4DA/brdfgen `brdfLUT.png`](https://github.com/4DA/brdfgen/blob/master/brdfLUT.png) → `brdf_lut.png`.

## Skybox (visible background)

When a scene has a **`SkyBoxComponent`** (at most one per scene), `SkyBoxPass` draws the **environment** cubemap as the viewport background before opaque geometry.

- Shaders: `EnvMap/background.{vert,frag}`
- Same HDR source as specular IBL (`GetEnvironment()`)
