# RND-TD025 — Shadow Convention Gap（OpenGL → Vulkan）

## Meta
- **ID:** TD-025（子文档；主设计见 [RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md](./RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md)）
- **Type:** Design Spec（Gap → 最小闭合方案）
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-08-29
- **Related:** BUG-RENDER-010 · ED-F01-S06 · [Implementation](./RND-TD025_CLIP_SPACE_CAPABILITIES_IMPLEMENTATION.md)

## TL;DR

OpenGL 阴影已正确；Vulkan Main Pass 已正确。阴影错误来自 **写→读坐标约定未闭合**，不是阴影算法本身。  
**不要重写 Shadow System**；只对齐 OpenGL → Vulkan 的 clip / viewport / sampling 语义差。

---

## 1) 问题陈述

阴影与普通深度渲染同构：

```text
World → LightView → LightProjection → Clip → NDC → Viewport → ShadowMap
                                                              ↓
Main Pass: World → 同一 LightViewProj → NDC → UV + CurrentDepth → 比较
```

| 事实 | 含义 |
|------|------|
| OpenGL 阴影正常 | 算法、PCF、bias、CSM 分割可作为 baseline |
| Vulkan Main Pass 正常 | 相机 ZO 投影 + Scene viewport flip 已闭合 |
| Vulkan 三种阴影异常 | Shadow 写/读链相对 baseline 有 convention gap |

**成功标准：** VK `test` 场景 Dir / Spot / Point 与 GL 目视一致；不改动已验证的 OpenGL 阴影算法语义。

---

## 2) 三层约定（闭合原理）

阴影链必须在三层上 **写什么就读什么**：

```text
Layer A — CPU 矩阵（投影 Z 范围）
Layer B — Viewport 栅格（Y 朝向、winding / cull）
Layer C — Shader 采样（NDC → UV / CurrentDepth）
```

| 层 | 管什么 | OpenGL | Vulkan（目标） |
|----|--------|--------|----------------|
| **A** | NDC Z 范围 | `[-1, 1]`（N1） | `[0, 1]`（ZO） |
| **B** | clip Y → 纹理行 | 无 flip；`flipY` 被忽略 | 与投影约定一致，且 **只处理一次** |
| **C** | NDC → 采样 UV/depth | `xy/z` 均 `*0.5+0.5` | XY：`*0.5+0.5`；**Z：直用 `ndc.z`** |

**铁律：** Y flip 只能出现在 Projection **或** Viewport **或** Sample 之一；FrontFace/Cull 随 Y flip 成对调整。

---

## 3) Gap 清单（相对 OpenGL baseline）

### Gap 1 — Clip-space Z（**确定，Dir/Spot**）

| | OpenGL | Vulkan（现状） |
|--|--------|----------------|
| LightProjection | `ortho` / `perspective` → NDC Z ∈ `[-1,1]` | `RHIClipSpace` ZO → NDC Z ∈ `[0,1]` ✅ |
| ShadowMap 写入 | `gl_FragCoord.z` ∈ `[0,1]` | 同左 ✅ |
| 采样 CurrentDepth | `ndc.z * 0.5 + 0.5` → `[0,1]` ✅ | **仍** `*0.5+0.5` → `[0.5,1]` ❌ |

**原理：** 硬件深度已是 `[0,1]`。GL 需把 N1 的 `ndc.z` 映射到 `[0,1]`；VK 的 `ndc.z` 已是 `[0,1]`，再乘半会系统性偏深，导致大面积假阴影。

**涉及：** `MaterialSceneShadows.glslinc` / `Phong.frag` 全部 Dir/Spot PCF 路径。

### Gap 2 — Y orientation（**可能，Dir/Spot；与 Gap 1 独立**）

| | Main Pass (VK ✅) | Shadow Pass (VK) |
|--|------------------|------------------|
| Viewport | `Scene` → `flipY=true`（负 height） | `ShadowMap2D` → `flipY=false` |
| 采样 UV | 不经手动 NDC→UV | `ndc.xy * 0.5 + 0.5`，无 `1-y` |

**原理：** Main Pass 用 viewport flip 对齐 GLM 风格投影与 Vulkan 纹理原点。Shadow 写路径未 flip，读路径按「纯数学 NDC→UV」走——若与写入 texel 的 Y 不一致，会出现上下颠倒采样。  
**注意：** 只修 Z 后若 UV 仍错，再单独闭合 Y；**禁止**同时改 viewport flip + sample flip。

### Gap 3 — Rasterization / Cull（**低优先，写路径对称**）

当前 scheme A：Shadow `ViewportFlipY=false` + `Front` cull；GL/VK 写路径对称。  
仅当为闭合 Gap 2 而打开 Shadow viewport flip 时，必须用 `GetEffectiveCullMode()` 补偿 winding。

### Gap 4 — Point 线性深度（**与 Gap 1 不同语义**）

| | Dir / Spot | Point |
|--|------------|-------|
| 写入 | projected Z（`gl_FragCoord.z`） | `length(pos-light)/far` |
| 采样 | LightViewProj → NDC → UV/depth | 方向向量 + 同一线性公式 |

**原理：** Point **不经** projected-Z 的 `*0.5+0.5`。若 Point 仍错，优先查 cube face viewport / 采样方向与 `TextureOriginY=Top` 的一致性，而不是 ZO Z 映射。可对照已工作的 `EnvMapCapture`（同源 `CubeMapFace` + ZO 投影）。

### 非 Gap（已对齐，勿当根因）

- `lookAt` / LightView 公式（GL/VK 共用）
- LightProjection 已走 `RHIClipSpace` ZO（与 `RenderCamera` 同源）
- Depth clear=1、D32、depth test/write、手动 `texture().r` 比较（无 compare-sampler 差异）
- PCF 核 / bias 参数（算法 baseline）

---

## 4) 解决方案（最小修改）

**目标形态：** OpenGL baseline 语义不变；Vulkan 只补 convention 差。

### Step 1 — 闭合 Gap 1（Layer C：Z only）

在 Dir/Spot 采样路径引入 **单一** NDC→采样 helper：

```glsl
// 伪代码
vec3 ndc = clip.xyz / clip.w;
vec2 uv = ndc.xy * 0.5 + 0.5;
#if MINENGINE_CLIP_DEPTH_ZERO_TO_ONE   // VK 编译期注入
    float depth = ndc.z;              // 已是 [0,1]
#else
    float depth = ndc.z * 0.5 + 0.5;  // GL N1 → [0,1]
#endif
```

- 恢复 **受限版** define 注入：仅 `MINENGINE_CLIP_DEPTH_ZERO_TO_ONE`（Vulkan）
- **本步不加** `SAMPLE_FLIP_Y`，不改 Shadow viewport
- Point 路径不动

**验收：** GL 回归无变；VK Dir/Spot 假阴影显著改善或消失。

### Step 2 — 若需闭合 Gap 2（Y，二选一）

| 选项 | 改哪里 | 代价 |
|------|--------|------|
| **B** | `GetShadowPassCapabilities().ViewportFlipY = true` + effective cull | 写路径与 Scene 对齐；改 caps |
| **C** | 采样 `uv.y = 1.0 - uv.y`（caps 已有 `GetShadowMapSampleFlipY`） | 只动读路径 |

**禁止：** B 与 C 同时开启。

**验收：** UV 与阴影轮廓方向与 GL 一致；orbit 相机稳定。

### Step 3 — Point（Gap 4）

在 Step 1/2 之后单独验证：

- 写：`UseLinearDepth=1` + `CubeMapFace` viewport
- 读：`SamplePointShadowPCF` 方向与线性 depth
- 对照 `EnvMapCapture` 面朝向 / up 向量

### 明确不做

- 不重写 ShadowPass / CSM / PCF 架构
- 不恢复 ShadowDebugPass，直到 Lit 采样基本正确
- 不修改已验证的 OpenGL 阴影算法公式（仅加 VK 分支）
- 不全仓回退丢掉 `RHIClipSpaceCapabilities`

---

## 5) 现状基线（实现前提）

**保留（Layer A/B 基础设施）：**

- `RHIClipSpaceCapabilities` / `RHIClipSpace` / `RHIViewportConvention`
- ForwardRenderer 灯光矩阵用 ZO helper
- ShadowPass convention + Front cull（scheme A）
- `dirShadowIndex` 门控、绑定槽清空

**已回退（干净 Layer C 起点）：**

- 无 `MinEngineShadowProject` / 无 flip define 注入
- 采样回到 `projCoords * 0.5 + 0.5`（与 Gap 1 一致的「未闭合」状态）

→ **已知张力：** A 已 ZO，C 仍按 N1 读 Z。Step 1 即闭合此张力。

---

## 6) 验收

- [ ] VK `test`：Dir / Spot / Point 与 GL 目视一致
- [ ] GL 回归无回归
- [ ] 关 plane Cast Shadow / 关 spot·point Cast Shadow 行为合理、不崩
- [ ] 一次只合一层；文档记录选了 Y 的 B 还是 C
- [ ] BUG-RENDER-010 → Fixed **仅在** 目视通过后

### 验证命令

```powershell
cmake --build minEngine/build --target Editor minEngineTests
.\minEngine\bin\Editor.exe --rhi opengl  --project ..\MyMEProject\MyMEProject.meproject
.\minEngine\bin\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject
```

---

## 7) 代码地图

| 职责 | 路径 |
|------|------|
| Caps / flip 表 | `RHI/RHIClipSpaceCapabilities.*` |
| ZO 矩阵 | `RHI/RHIClipSpace.*` |
| Light VP | `ForwardRenderer.cpp`（Dir/Spot/Point build） |
| 写 | `ShadowPass.cpp` + `Shaders/ShadowPass.{vert,frag}` |
| 读 | `MaterialSceneShadows.glslinc`、`MaterialPhongLighting.glslinc` |
| Define 注入 | `ShaderCompiler.*`（Step 1 受限恢复） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-29 | 初稿：问题 / 四类 gap / 分步最小闭合；供审阅 |
