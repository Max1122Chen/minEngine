# Engine default IBL textures (Phase 4.1)

Place cubemap face PNGs here. The loader expects **square** faces with **matching** width/height and channel count.

## Irradiance cubemap (loaded at startup)

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

## Future (P4.2+)

- `prefilter_posx.png` … `prefilter_negz.png` (with mips when supported)
- `brdf_lut.png` (2D, typically 512×512 RG)

Prefilter and BRDF LUT are not loaded in P4.1; prefilter is aliased to irradiance at bind time.
