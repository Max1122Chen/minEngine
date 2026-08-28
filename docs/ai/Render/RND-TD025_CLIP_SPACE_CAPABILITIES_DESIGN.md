# RND-TD025 — RHI Clip-Space Capabilities

## Meta
- **ID:** TD-025
- **Type:** Design Spec
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-28
- **Related:** ED-F01 · BUG-RENDER-010 · [Implementation](./RND-TD025_CLIP_SPACE_CAPABILITIES_IMPLEMENTATION.md)

## TL;DR

Vulkan Editor 阴影错误（三种光型、plane 假自阴影、采样随视角漂移）根因是 **clip/NDC/viewport/剔除策略碎片化**（散落 `IsVulkan()`），不是 PCF 参数。本设计抽出 **`RHIClipSpaceCapabilities`** + **`RHIClipSpace` 矩阵 helper** + **`RHIViewportConvention`**，闭合 shadow 写/读链。**Shadow 采用方案 A**（`flipY=false` + `Front` cull，与 `EnvMapCapture` 同源）。**不修改 CSM `lightView` 固定原点**（用户验证当前行为正确）。

## Scope

### In
- `RHIClipSpaceCapabilities.h/.cpp` — 平台真相表
- `RHIClipSpace.h/.cpp` — `MakePerspective` / `MakeOrthographic`
- `RHIViewportConvention` — Scene / ShadowMap2D / CubeMapFace
- ShadowPass、ForwardRenderer、RenderCamera、EnvMapCapture、ShaderCompiler、材质 shadow 采样迁移
- `EngineSceneBindingSets` — spot/point shadow 槽位清空（关 shadow 崩溃）
- 方向光 shadow index 门控（`Params.w < 0` 不采样）

### Out
- CSM `lightView` 中心化（§7 明确不做）
- DX12/Metal 后端实现（仅预留 Caps 字段）
- PCF/PCSS 算法调参

## 三层约定（闭合链）

```text
Layer A — CPU 矩阵（GLM + Caps 选 ortho/perspective RH_ZO）
Layer B — Viewport raster（Scene flipY / Shadow&Cube no-flip）
Layer C — Shader 采样（MinEngineShadowProject，depth 跟 Caps define）
```

**铁律：** A+B 写入的 texel 必须被 C 原样读回。

## 平台表（首版）

| 字段 | OpenGL | Vulkan |
|------|--------|--------|
| ClipDepthRange | N1 | Z0 |
| TextureOriginY | Bottom | Top |
| SceneViewportFlipY | false | true |
| Shadow.ViewportFlipY | false | false |
| Shadow.ReceiverFacingCullMode | Front | Front |
| CubeCapture.ViewportFlipY | false | false |

Shadow 有效剔除：`ViewportFlipY=false` → `Front`（GL/VK 相同）。

## Shadow 方案 A（选用）

| Pass | Viewport | Cull |
|------|----------|------|
| Directional / Spot 2D | `ShadowMap2D` (no flip) | Front |
| Point cube faces | `CubeMapFace` (no flip) | Front |
| Lit 采样 | `MinEngineShadowProject` | 无额外 Y flip |

## 相机滑动

用户确认 **不修改** `BuildDirectionalShadowDrawCommands` 中固定 `lightView`；滑动问题优先用 Caps 闭合链验收。若闭合后仍漂，另开 bug 调查 CSM snap。

## 验收

- [ ] VK `test`：plane cast on，无巨斑；方块影 world-anchored
- [ ] GL 回归无变
- [ ] 关 point/spot Cast Shadow 不崩溃
- [ ] `grep IsVulkan` clip 分支仅剩编译目标/窗口创建

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-28 | Draft；用户审阅通过（§7 CSM 不改） |
