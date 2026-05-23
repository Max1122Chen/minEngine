# Engine default IBL textures (Phase 4)

Place cubemap face PNGs here. The loader expects **square** faces with **matching** width/height and channel count.

## Load order (startup)

```text
1. irradiance_posx.png … negz.png
2. else *.hdr → GPU equirect → cubemap (+ mips)
3. else 6-color validation cubemap (32×32)
prefilter: prefilter_*.png, else same cubemap as environment (with mips if from HDR)
brdf_lut: brdf_lut.png, else CPU integrated LUT (slow) — recommend downloading a LUT (see below)
```

## Irradiance / environment cubemap

File naming: `{prefix}_{face}.png` with OpenGL face order:

| Face | Filename |
|------|----------|
| +X | `irradiance_posx.png` |
| -X | `irradiance_negx.png` |
| +Y | `irradiance_posy.png` |
| -Y | `irradiance_negy.png` |
| +Z | `irradiance_posz.png` |
| -Z | `irradiance_negz.png` |

If these files are missing, the engine tries an **HDR equirectangular** capture (LearnOpenGL-style):

1. Place any `*.hdr` in this folder (e.g. `citrus_orchard_puresky_1k.hdr`).
2. Shaders: `EngineDefault/Shaders/EnvMap/equirect_to_cubemap.{vert,frag}`.
3. GPU path: `ImageLoader::LoadHdr` → RGB16F 2D → `EnvMapCapture::EquirectToCubemap` (512×512 faces).

Preferred HDR name: `environment.hdr` (otherwise the first `*.hdr` found is used).

If no PNG faces and no HDR capture succeed, a **6-color validation cubemap** (32×32) is used so PBR draws still bind units 4–6.

## Optional offline assets (higher quality)

| Asset | Naming |
|-------|--------|
| Prefiltered env | `prefilter_posx.png` … `prefilter_negz.png` (mips if pre-baked) |
| BRDF LUT | `brdf_lut.png` (512×512, RG in R/G channels) |

If missing: HDR capture builds **mipmapped** environment cubemap for `textureLod` prefilter; CPU generates BRDF LUT at startup (place `brdf_lut.png` to skip).

**Recommended BRDF LUT:** [4DA/brdfgen `brdfLUT.png`](https://github.com/4DA/brdfgen/blob/master/brdfLUT.png) → rename to `brdf_lut.png` (512×512, R/G = scale/bias).

Shader: `MaterialIBL.glslinc` → `CalcIndirectPBR` (texture units 4–6, `u_EnvIntensity` default 1.0).

## Deferred (post–Phase 4)

- Irradiance convolution pass (diffuse-only low frequency)
- Dedicated prefilter filter pass
- Skybox background draw
