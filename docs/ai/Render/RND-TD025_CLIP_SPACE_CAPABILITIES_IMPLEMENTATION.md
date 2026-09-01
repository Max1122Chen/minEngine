# RND-TD025 — Clip-Space Capabilities Implementation

## Meta
- **ID:** TD-025
- **Type:** Implementation Plan
- **Status:** In Progress
- **Last updated:** 2026-08-29
- **Design:** [RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md](./RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md)

## Slices

| Slice | 内容 | Status |
|-------|------|--------|
| TD025-S01 | `RHIClipSpaceCapabilities` + `RHIClipSpace` helpers | Done |
| TD025-S02 | `RenderCamera`、CSM/frustum、`ForwardRenderer` 灯光矩阵 | Done |
| TD025-S03 | ShadowPass convention + cull；shadow 采样 helper | **Partial** — Pass CPU 保留；`MinEngineShadowProject` 已回退 |
| TD025-S04 | Point cube `CubeMapFace`；Spot/Directional `ShadowMap2D` | Done |
| TD025-S05 | ShaderCompiler defines；Editor ImGui UV；EnvMap | **Partial** — ImGui UV 保留；define 注入已回退 |
| TD025-S06 | `EngineSceneBindingSets` shadow 槽清空；dir shadow index 门控 | Done |
| TD025-S07 | 文档 + BUG-RENDER-010 更新；TD-025 → Done | Pending — 待 VK 阴影闭合 |
| TD025-S08 | Shadow depth 可视化（`ShadowDebugPass`、Editor F9） | **Cancelled** — 已拆除 |

## 代码入口

- `minEngine/.../RHI/RHIClipSpaceCapabilities.h`
- `minEngine/.../RHI/RHIClipSpace.h`
- `ShadowPass.cpp` — `GetShadowPassCapabilities().GetEffectiveCullMode()`
- `MaterialSceneShadows.glslinc` — **bbdcdc 语义**（无 `MinEngineShadowProject`；待 Layer C 重闭合）
- **Handoff：** [2026-08-29-vulkan-shadow-handoff.md](../sessions/2026-08-29-vulkan-shadow-handoff.md)

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
| 2026-08-29 | 分层回退 shader/flip 注入；S08 取消；handoff 文档 |
