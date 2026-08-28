# RND-TD025 — Clip-Space Capabilities Implementation

## Meta
- **ID:** TD-025
- **Type:** Implementation Plan
- **Status:** In Progress
- **Last updated:** 2026-08-28
- **Design:** [RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md](./RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md)

## Slices

| Slice | 内容 | Status |
|-------|------|--------|
| TD025-S01 | `RHIClipSpaceCapabilities` + `RHIClipSpace` helpers | Done |
| TD025-S02 | `RenderCamera`、CSM/frustum、`ForwardRenderer` 灯光矩阵 | Done |
| TD025-S03 | ShadowPass convention + cull；`MinEngineShadowProject` | Done |
| TD025-S04 | Point cube `CubeMapFace`；Spot/Directional `ShadowMap2D` | Done |
| TD025-S05 | ShaderCompiler defines；Editor ImGui UV；EnvMap | Done |
| TD025-S06 | `EngineSceneBindingSets` shadow 槽清空；dir shadow index 门控 | Done |
| TD025-S07 | 文档 + BUG-RENDER-010 更新；TD-025 → Done | Pending verify |

## 代码入口

- `minEngine/.../RHI/RHIClipSpaceCapabilities.h`
- `minEngine/.../RHI/RHIClipSpace.h`
- `ShadowPass.cpp` — `GetShadowPassCapabilities().GetEffectiveCullMode()`
- `MaterialSceneShadows.glslinc` — `MinEngineShadowProject`

## 测试

```powershell
cmake --build minEngine/build --target minEngine Editor
.\scripts\verify.ps1
```

手动：`Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject`，`test` 场景 A/B GL。

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-28 | 实现 S01–S06 |
