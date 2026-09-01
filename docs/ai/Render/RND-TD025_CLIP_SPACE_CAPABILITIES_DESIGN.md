# RND-TD025 — RHI Clip-Space Capabilities

## Meta
- **ID:** TD-025
- **Type:** Design Spec
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-29
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
- [ ] ~~Shadow depth 可视化（TD025-S08）~~ — **已取消**（2026-08-29 分层回退）

## BUG-RENDER-010 调查状态（2026-08-29）

**目视结论：** TD-025 S01–S06 落地后，VK 三种阴影仍错；GL 正常。BUG-RENDER-010 文档「Fixed」不可信，状态应为 **未闭合**。

### 强信号

| 观察 | 推论 |
|------|------|
| Dir + Spot + Point 全坏 | 共同读路径（`MinEngineShadowProject`）不可能是唯一根因；Point 不经该函数 |
| Dir+Point 同时开 → 多层平行四边形 | 更像错误 depth/坐标叠加，而非单纯 `uv.y` flip |
| 关 plane Cast Shadow → 大暗斑消失 | ShadowPass 写入链确实在影响画面 |

### 调查优先级（共识，暂停主查采样 flip）

```text
① Shadow Map 写出来对不对（depth 可视化）     ← P0
② Point Shadow vs EnvMapCapture（可复用字段）
③ ShadowPass VkViewport / depth-only RP / layer
④ ViewProj / clip → NDC 数据流
⑤ MinEngineShadowProject / inject / Y flip
⑥ PCF / bias
```

**策略（2026-08-29 更新）：** 已完成分层回退 — shader/flip 注入回到 `bbdcdc` 语义；从 **Layer C（ZO depth 读）** 重新开始，一次只动一层。见 [handoff](../sessions/2026-08-29-vulkan-shadow-handoff.md)。

### 写读链审计摘要

- **数据流正确：** 同帧同 `ViewProj`、同 world pos、同 RDG 纹理句柄；帧顺序 Shadow 写 → Base 读。
- **Dir/Spot：** 写硬件 ZO depth；读 `ndc.z`（须 inject `MINENGINE_CLIP_DEPTH_ZERO_TO_ONE`）。
- **Point：** 写/读均为 `length(pos-light)/far`，不经 `MinEngineShadowProject`。
- **VK 可疑断点（2026-08-29 更新）：** Shadow 写读为**独立闭环**——`Shadow flipY=false` 与 `Scene flipY=true` **不构成** shadow 采样 gap。曾试验 scheme B + 读 `uv.y` 已废弃。当前：ZO depth read + scheme A；VK Spot ~OK，Dir/Point 仍查 CSM / cube / 多灯。
- **Viewport 已核实：** `ShadowMap2D` / `CubeMapFace` → `GetViewportFlipY` → VK `y=0,height=+H`（shadow 写不 flip）。

### EnvMapCapture vs Point Shadow（可对齐 / 不可对齐）

| 可对比 | EnvMap | Point Shadow |
|--------|--------|----------------|
| 6 面 target/up、`CubeMapFace` viewport、`*RH_ZO` | ✅ 同源 | ✅ |
| Depth 附件 | 独立 **2D** depth（每面复用） | **Depth Cube** |
| FS 输出 | color | `gl_FragDepth` 线性 |
| Cull | 默认不剔除 | Front |

EnvMap 正常 **不能** 直接证明 depth cube 离屏写正确。

### 实现与设计张力（2026-08-29，`3154700`）

- `MinEngineShadowProject` 与 flip define 注入 **已移除**。
- Layer C：`MinEngineShadowMapCoords` + `MINENGINE_CLIP_DEPTH_ZERO_TO_ONE`（ZO depth read）**已落地**。
- Layer B：**scheme A**（shadow viewport 不 flip；读无 `uv.y` 补偿）。scheme B 试验 **已回退**。
- 下一步：**Dir** → CSM（P1/P3）；**Point** → cube 线性 depth / PCF / VK layout（P4）；非 shadow viewport flip。

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-28 | Draft；用户审阅通过（§7 CSM 不改） |
| 2026-08-29 | §BUG-RENDER-010 调查状态、优先级、写读审计 |
| 2026-08-29 | 分层回退；S08 取消；handoff 文档 |
