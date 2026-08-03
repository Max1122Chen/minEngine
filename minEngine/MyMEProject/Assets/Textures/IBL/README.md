# Project IBL textures (copied from EngineDefault)

**Do not** register or reference `Assets/EngineDefault/Textures/IBL` through AssetManager —
engine Default paths are rejected for registry keys. Copy seeds into this project folder.

## Seeded files

- `brdf_lut.png` — shared BRDF LUT (assign to `EnvironmentMap.m_BrdfLUT`)
- `citrus_orchard_puresky_1k.hdr` — HDR source for future Bake (RND-F10-S05)
- Face PNGs (`environment_*`, `irradiance_*`, `prefilter_*`) — optional until Bake; without them the runtime uses a validation cube

## Workflow

1. Copy / bake face sets into this directory.
2. Keep or edit `Assets/Environment/DefaultEnvironment.meenv` (`m_FaceDirectory` = `Textures/IBL`).
3. Assign that EnvironmentMap on `SkyBoxComponent` in the scene.
