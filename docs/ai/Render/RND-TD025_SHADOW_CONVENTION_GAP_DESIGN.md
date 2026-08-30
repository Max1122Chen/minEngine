# RND-TD025 — Shadow Convention Gap（OpenGL → Vulkan）

## Meta
- **ID:** TD-025（子文档；主设计见 [RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md](./RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md)）
- **Type:** Design Spec（Gap → 最小闭合方案）
- **Status:** In Progress (Step 1 ZO + slot gate landed, commit `3154700`; VK Spot ~OK, Dir/Point open)
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
| **B** | clip Y → 纹理行 | 无 flip；`flipY` 被忽略 | Shadow **写** 不 flip（scheme A）；Scene **呈现** 可 flip |
| **C** | NDC → 采样 UV/depth | `xy/z` 均 `*0.5+0.5` | XY：`*0.5+0.5`；**Z：直用 `ndc.z`** |

**铁律（Y）：** Shadow 写→读是**独立闭环**——写路径 viewport 若未 flip，读路径就按纯数学 `ndc→uv` 读，**不得**假设立过 flip。Main Pass 的 Scene viewport flip **只服务场景呈现**，与 Lit Pass 采样 shadow map **无关**。

**已废弃的错误思路（2026-08-29 共识）：**

- ~~因 Main Pass 在 VK 上 flip，就给 shadow **读**路径加 `uv.y = 1-y`~~
- ~~Scheme B：Shadow 写 flip + Back cull + 读侧 `uv.y` 补偿~~（把 Scene 约定误套进 shadow 链）
- ~~点光 cube 改用 `ShadowMap2D` 与 Dir/Spot 共用 flip~~（cube 采样不走 `ndc→uv`）

**当前约定（scheme A，代码与 `3154700` 一致）：** 全部 Shadow 写路径 `ViewportFlipY=false` + Front cull；读路径无 sample Y flip。

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

### Gap 2 — Y orientation（Dir/Spot 2D：**写读自洽即可**）

| Pass | VK viewport flip | 与 shadow 采样的关系 |
|------|------------------|----------------------|
| **Main / Scene** | `Scene` → `flipY=true` | 仅影响相机画面呈现；**不参与** shadow UV |
| **Shadow 写** | `ShadowMap2D` → `flipY=false` | 决定 depth RT 内 texel 布局 |
| **Shadow 读** | `uv = ndc.xy * 0.5 + 0.5`（无 `1-y`） | 必须与写路径一致（当前：写不 flip → 读不 flip） |

**原理：** NDC→RT 的 Y 映射在 shadow 链内由 **Shadow Pass 自己的 viewport** 决定。只要写、读同一套 `LightViewProj` 且读公式匹配写时的光栅约定，即闭合。**不应**用 Main Pass flip 推导读侧补偿。

**VK Spot 目视（2026-08-29）：** 在 scheme A + ZO depth read 下已基本正确 → 支持「写不 flip / 读不 flip」模型。

**若 Dir 仍错：** 优先查 CSM（cascade/layer/ortho）与多灯资源，而非回退 shadow viewport flip。

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

### Step 1 — 闭合 Gap 1（Layer C：Z only） — **Done 2026-08-29** (`3154700`)

在 Dir/Spot 采样路径引入 **单一** NDC→采样 helper `MinEngineShadowMapCoords`：

```glsl
vec3 ndc = clip.xyz / clip.w;
vec2 uv = ndc.xy * 0.5 + 0.5;
#if MINENGINE_CLIP_DEPTH_ZERO_TO_ONE
    float depth = ndc.z;
#else
    float depth = ndc.z * 0.5 + 0.5;
#endif
```

- `ShaderCompiler::InjectClipSpaceDefines`：仅注入 `MINENGINE_CLIP_DEPTH_ZERO_TO_ONE`（VK=1 / GL=0）
- **未**注入 `SAMPLE_FLIP_Y`，未改 Shadow viewport
- Point 路径未改
- 顺带恢复 pass-local OpenGL flat remap（`set=0` ShadowPass/Post/Sky；基线回退时误删）

**验收：** GL 回归无变；VK Spot 已基本正确；Dir/Point 仍 Open。

### Step 2 — Shadow viewport flip 试验 — **Reverted 2026-08-29**

曾试验 **scheme B**（`kVulkanShadowPass.ViewportFlipY=true` + Back cull + 读 `uv.y=1-y`）及点光 `ShadowMap2D`；**已回退**。结论：不应把 Main Pass 的 flip 逻辑套入 shadow 写读链。当前保持 scheme A。

### BUG-RENDER-012 — shadow index 门控 — **Fixed 2026-08-29** (`3154700`)

`MinEngineShadowMapSlot(Params.w)`：`Params.w < 0` 时不采样（修复 `int(-0.5)==0`）。GL 点光关阴影已验证。

---

## 8) 待查项 / 试验队列（2026-08-29 更新）

**FlipY / scheme B 试验已废弃。** Shadow 写读保持 scheme A（viewport 不 flip，读无 `uv.y` 补偿）。VK Spot 已基本正确。

| ID | 嫌疑 | 适用 | 状态 |
|----|------|------|------|
| ~~P0~~ | ~~写 flip + 读 `uv.y`~~ | Dir/Spot | **cancelled**（错误模型） |
| **P1** | CSM frustum→AABB / 级联混用（FORCE=0 多影→单影） | Dir | **CSM 路径相关**；固定盒子实验无效（双 RHI 无影） |
| **P2** | `SpotLightViewProj` 槽位 / 多灯交互 | Spot+Dir | open（单 Spot 已好转） |
| **P3** | `sampler2DArray` layer + shader `MinEngineShadowMapCoords` / Dir PCF | Dir | **open — 下一刀（GPU）** |
| **P4** | Point cube | Point | open |
| **P5** | bias / 深度比较 / SRV | 全类型 | 低优先 |
| **P6** | VK Dir 影与 Point Cast Shadow 耦合 / latch | Dir+Point | **BUG-RENDER-013** — **RDG 主因假设**（2026-08-30 修订） |
| **P7** | RDG：Scene 无 ShadowAtlas read edge；静态 fingerprint；enqueue vs bake 脱节 | Dir+VK | **BUG-RENDER-013** — 对齐 Granite `bake` / `setup_dependencies` |

### Dir 实验状态（2026-08-30，已回退）

| 实验 | 结果 |
|------|------|
| FORCE cascade 0 | 多影→单影；GL/VK 强制0 仍不对 |
| Fixed ortho box | **GL+VK 均无 Dir 影**（盒子未闭合，已回退） |

**当前：** `kDirShadowForceCascade=-1`（正常级联）；无固定盒子。Debug 开关仍可用（`DIR_SHADOW_DEBUG_MODE` / `DIR_SHADOW_FORCE_CASCADE` inject）。

### P7 — RDG 调查（2026-08-30，修订）

**结论（修订）：** Dir-only 隔离下 VK 正常 → TD-025 convention **基本排除**为主因。全类型 shadow 失败更符合 **Granite 式 RDG 缺口**（read edge、rebake、fingerprint、pass 序），而非持续堆 set1/描述符 patch。S1 工作区改动仅为薄层调度 + VK depth layout。详记 [BUG-RENDER-013](../bugs/BUG-RENDER-013.md)。

**隔离实验（仍有效）：** `MAX_*_SHADOW_MAPS=0` → **VK Dir 正常**（对照：问题随 shadow **图拓扑**出现，非 Dir 数学单路径）。

| 机制 | 行为 |
|------|------|
| RDG shadow 槽 | 编译期固定 `kMaxShadowGraphPasses = MAX_CASCADES + MAX_SPOT_SHADOW_MAPS + MAX_POINT_SHADOW_MAPS×6`（当前 **18**）；`BuildFrameRenderGraph` 全部 `ForceInclude` |
| 实际 GPU shadow pass | 仅 `ShadowGraphPass::NeedRenderPass()`（有 `ShadowDrawCommand` + 已 bind 纹理）为 true 的槽执行 `BeginRenderPass` |
| Scene 读 shadow | `BasePass` **未**声明 `DirShadowAtlas` 为 texture input → 无自动 barrier |
| set1 重建 | 点光 shadow map 指针 null→有效 会 `sceneSet1Dirty`；仅 Dir 时可能复用陈旧 VkDescriptorSet |

### Shadow pass 隔离实验矩阵

| 目标 | 场景层（推荐先试） | 编译期常量 | 影响 shadow **图**槽位 | 影响 shadow **GPU** pass |
|------|-------------------|------------|------------------------|---------------------------|
| 仅 Dir、无点/聚 shadow | 点/聚 `Cast Shadow` 关 | `MAX_POINT_SHADOW_MAPS=0`, `MAX_SPOT_SHADOW_MAPS=0` | 18→4（仅 cascade 槽） | 仅 Dir cascade 有 command 时 |
| 单级联 Dir | 同上 + `DIR_SHADOW_FORCE_CASCADE=0` | `MAX_CASCADES=1` | 18→1（若 spot/point map 也为 0） | 1 个 `Shadow.0` |
| 限制场景灯数量 | 删/关多余灯 | `MAX_POINT_LIGHTS` / `MAX_SPOT_LIGHTS` | **不变**（仍 18 槽） | 仅影响 `CollectShadowRequests` 是否生成 command |
| 完全无 shadow | `EnableShadows` 关 | — | 图仍 bake 18 槽* | 无 command → 全 skip |

\* `ForceInclude` 仍在 pass stack；空槽 `NeedRenderPass()==false`，不录 GPU render pass。

**注意：** `MaterialSceneShadows.glslinc` 内 `MAX_SPOT_SHADOW_MAPS` / `MAX_POINT_SHADOW_MAPS` 为 shader 宏（当前 2）；若只改 C++ `ShadowTypes.h` 为 0，需同步 shader 或接受 set1 槽位与 shader 循环不一致。

**推荐实验顺序（坐实 P6/P7）：**

1. ~~场景：Dir Cast Shadow on，点/聚 Cast Shadow off~~ → **引擎层已设 `MAX_*_SHADOW_MAPS=0`（2026-08-30）**；图槽 = `MAX_CASCADES`（4）；set1 spot/point 仍绑 dummy。
2. VK 复测：仅 Dir Cast Shadow → Dir 是否仍不可见（若是 → 支持 P0/P1，非点光 pass 污染）。
3. `MAX_CASCADES=1` → 单槽 Dir；配合 `DIR_SHADOW_DEBUG_MODE=2` 排除级联。
4. 修 P0–P3 后恢复 `MAX_*_SHADOW_MAPS=2` 作回归。

### Dir debug（`DIR_SHADOW_DEBUG_MODE`）

开关：`ShaderCompiler.cpp` 内 `kDirShadowDebugMode`（注入到材质 SPIR-V；改后需重编 Editor，材质会因源 hash 变化重编译）。

| Mode | 画面 | 看什么 |
|------|------|--------|
| **0** | 正常 lit | 关 debug |
| **1** | R/G/B/Y = cascade 0/1/2/3 | GL vs VK 条带是否一致（级联选择） |
| **2** | 灰度 = 单 tap 阴影（无 PCF、bias=0） | 是否仍错 → 排除 PCF |
| **3** | RG = shadow UV；品红 = OOB | UV 布局 / 越界 |
| **4** | 灰度 = currentDepth（读侧算出的 Z） | ZO 读是否在合理范围 |
| **5** | 灰度 = sampledDepth（shadow map） | 写路径内容是否合理 |
| **6** | 灰度 = `(current−sampled)*10+0.5` | 比较符号/尺度 |

**当前默认：`kDirShadowDebugMode = 0`（正常 lit）。** 固定盒子实验时保持 0；FORCE cascade 仍为 0。

### Point 专项（P4 展开）

**当前代码（无 flip）：**

| 项 | 值 |
|----|-----|
| Viewport | `CubeMapFace` → `ViewportFlipY=false`（与 `EnvMapCapture` 同源） |
| 写深度 | `gl_FragDepth = length(world-light)/far`（`UseLinearDepth=1`） |
| 读 | `normalize(fragPos-lightPos)` + 同线性 depth；**无** `MinEngineShadowMapCoords`、**无** sample Y flip |

**已做且已回退的 Point 试验：**

| 试验 | 内容 | 结果 |
|------|------|------|
| Step 2B 副产品 | 点光面 `CubeMapFace`→`ShadowMap2D` | 已随 scheme B 回退 |
| P4-A | 写 `CubeMapFace` + Front cull 专用 PSO | 有变化，四重鬼影仍在 → 回退 |
| P4-B | 读侧 `dir.y = -dir.y`（VK） | 无效 → 回退 |

**下一步（Point）：** debug 可视化 depth cube / 单 tap 比较；查 VK depth-only cube 每面 `EndRenderPass` layout 过渡；PCF 跨 cube 面泄漏 — **不是** shadow viewport flip。

### Step 3 — Point（Gap 4）

- 写：`CubeMapFace` + 线性 depth（保持）
- 读：方向 + 线性 depth（保持）
- 对照 `EnvMapCapture` 面朝向；**禁止**无依据改用 `ShadowMap2D` flip

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

- [x] Step 1 ZO depth read + slot gate committed (`3154700`)
- [x] Scheme A：Shadow viewport 不 flip；读无 sample Y flip
- [ ] VK `test`：Dir / Point 与 GL 一致（Spot ~OK）
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
| 2026-08-29 | §8 待查队列 P0–P5；P0 B+读侧 uv.y 补偿 |
| 2026-08-29 | Step 2B viewport flip；BUG-RENDER-012 Fixed |
| 2026-08-29 | **回退** scheme B / 读 `uv.y`；确立 shadow 写读独立闭环；commit `3154700`；VK Spot ~OK |
