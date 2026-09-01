# Vulkan 阴影诊断对照：minEngine vs Piccolo

## Meta

| 字段 | 值 |
|------|-----|
| **Status** | Reference — 诊断对照（非设计定稿） |
| **Last updated** | 2026-08-31 |
| **Purpose** | 汇总 BUG-RENDER-013 / RND-F13 会话中的实验结论、Vulkan RHI 只读审计、与 Piccolo 的差异，供修阴影时对照 |
| **Related** | [BUG-RENDER-013](../bugs/BUG-RENDER-013.md) · [RND-F13_MANUAL_RENDERER_DESIGN.md](./RND-F13_MANUAL_RENDERER_DESIGN.md) · [RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md](./RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md) |
| **External ref** | `D:\Dev\GitRepo\Piccolo`（教学向 VK 引擎，只读对照） |

---

## TL;DR

1. **RDG 非主因**：full-map 下 `ManualRenderer` 与 `ForwardRenderer`+RDG **同错影** → 问题在共享 ShadowPass / set1 / VK RHI。
2. **Dir-only 不能称「正确」**：仅 **错误程度较轻**（浅影、轻微错位）；与 GL / Piccolo 仍有差距。
3. **Piccolo 只有 2 种阴影**（Dir + Point），**无 Spot**、**无 CSM**；架构更简单。
4. **Pipeline 分离两边都有**；差别在 **Set1 高耦合**、**矩阵双轨**、**depth texture 采样** vs Piccolo 的 **R32 color shadow**。
5. 下一步优先：**RenderDoc 验证 shadow map 本体** → 再查 set1 / 矩阵 / layout。

---

## 1. 已坐实的实验矩阵

| 实验 | 结果 | 推论 |
|------|------|------|
| `MAX_*_SHADOW_MAPS=0`，`MAX_CASCADES=1`，`FORCE_CASCADE=0`，`--renderer manual` | VK dir **错误较轻**；Manual ≈ Forward | 写入链部分可用；**不能称 GL 正确** |
| 恢复 full map（Dir+Spot+Point，4 cascade） | Manual **==** Forward+RDG **同错影** | **RDG 调度非主因** |
| OpenGL 同场景 | 明显好于 VK | 问题类：**VK 资源/状态语义**，非纯 CSM 数学 |
| S2–S4 set1 pool 补丁 | 无持久修复 | 症状修补，非根因 |

**Dir-only 定性（user 2026-08-31）：**「能看」≠「正确」；文档与回归项应写 **less wrong / reduced severity**，不写 **normal / correct**。

---

## 2. Piccolo 阴影能力概览

### 2.1 有几种阴影？

| 类型 | 支持 | Pass | Shadow map |
|------|------|------|------------|
| **Directional** | ✅ | `DirectionalLightShadowPass` | 单张 `4096²` `R32_SFLOAT` **color** |
| **Point** | ✅ | `PointLightShadowPass` | `2048²` `R32` **2D Array**（每灯 2 layer，最多 15 灯） |
| **Spot** | ❌ | 无 | — |
| **CSM** | ❌ | 无 | 单 `light_proj_view` 拟合视锥 |

渲染顺序：`DirectionalLightShadow` → `PointLightShadow` → `MainCameraPass`。

### 2.2 关键文件（Piccolo）

| 主题 | 路径 |
|------|------|
| Dir shadow pass | `engine/source/runtime/function/render/passes/directional_light_pass.cpp` |
| Point shadow pass | `engine/source/runtime/function/render/passes/point_light_pass.cpp` |
| 管线顺序 | `engine/source/runtime/function/render/render_pipeline.cpp` |
| Dir 矩阵 | `engine/source/runtime/function/render/render_helper.cpp`（`CalculateDirectionalLightCamera`） |
| Shadow 写入 shader | `engine/shader/glsl/mesh_directional_light_shadow.frag`（`out_depth = gl_FragCoord.z`） |
| Shadow 采样 | `engine/shader/include/mesh_lighting.inl` |
| Main pass binding | `engine/shader/glsl/mesh.frag`，`main_camera_pass.cpp` |

---

## 3. minEngine 阴影能力概览

| 类型 | 支持 | 实现 |
|------|------|------|
| **Directional** | ✅ CSM | `Texture2DArray` depth，`MAX_CASCADES=4` |
| **Spot** | ✅ | `Texture2D` depth，`MAX_SPOT_SHADOW_MAPS=2` |
| **Point** | ✅ | `TextureCube` depth，`MAX_POINT_SHADOW_MAPS=2` |

| 主题 | 路径 |
|------|------|
| Shadow 绘制 | `minEngine/.../RenderPasses/ShadowPass.cpp` |
| Shadow shader | `minEngine/Shaders/ShadowPass.vert` / `.frag` |
| 采样 | `minEngine/Assets/.../MaterialSceneShadows.glslinc` |
| Set 布局 | `minEngine/.../EngineShaderBindings.h` |
| Set1 构建 | `minEngine/.../EngineSceneBindingSets.cpp` |
| Shadow pipeline layout | `minEngine/.../EnginePipelineLayouts.cpp` |
| 手动诊断路径 | `minEngine/.../ManualRenderer.cpp` |

---

## 4. 架构对比总表

| 维度 | Piccolo | minEngine |
|------|---------|-----------|
| Shadow 存储 | **R32 color** + transient depth | **DEPTH32 depth-only**（`ShaderResource` 标志） |
| Layout 过渡 | Render pass `finalLayout = SHADER_READ_ONLY`（color） | `EndRenderPass` + `RHICmdTransition` → `DEPTH_STENCIL_READ_ONLY` |
| Dir 级联 | 无（单 map） | 4 layer `Texture2DArray` |
| Spot | 无 | 2× `Texture2D` |
| Point | 2D array + **Geometry Shader** 球面展开 | Depth **cube** + 6 face passes |
| Shadow pass cull | **Back** | **Front**（TD-025 scheme A） |
| Depth bias | 关闭 | VK：slope=1.5，constant=0（GL constant=4） |
| Viewport | 静态（baked in PSO） | Dynamic viewport/scissor |
| 矩阵（dir） | **单块 SSBO**，写/采样同源 | **两套 UBO**：shadow 绘制单 `mat4` vs 采样 `mat4[4]` |
| 主场景 set 划分 | Set0 含灯光+IBL+**shadow 纹理** | Set0 灯光；Set1 **shadow+IBL**；Set2 材质 |

---

## 5. Descriptor / BindingSet 对照

### 5.1 Piccolo — Main Camera（`mesh.frag`）

**Set 0 — `_mesh_global`（8 bindings）**

| Binding | 类型 | 内容 |
|---------|------|------|
| 0 | `STORAGE_BUFFER_DYNAMIC` | Per-frame：相机、`point_light_num`、点光数组、`directional_light`、`directional_light_proj_view` |
| 1 | `STORAGE_BUFFER_DYNAMIC` | Per-drawcall instances |
| 2 | `STORAGE_BUFFER_DYNAMIC` | 骨骼 joint matrices |
| 3–5 | `COMBINED_IMAGE_SAMPLER` | BRDF LUT、Irradiance、Specular |
| **6** | `sampler2DArray` | **`point_lights_shadow`** |
| **7** | `sampler2D` | **`directional_light_shadow`** |

**Set 1**：per-mesh 蒙皮；**Set 2**：材质。

Shadow 纹理与 `directional_light_proj_view` **同在 set 0**，但 shadow pass **不绑定任何纹理**。

### 5.2 Piccolo — Shadow Pass（Dir / Point）

Pipeline layout = `{ shadow_global_set, per_mesh_set }`。

**Set 0（shadow 专用，仅 buffer）**

| Binding | Dir pass | Point pass |
|---------|----------|------------|
| 0 | `light_proj_view`（VS） | 点光位置/半径（GS+FS） |
| 1 | per-drawcall（VS） | per-drawcall（VS） |
| 2 | blending（VS） | blending（VS） |

Framebuffer → Main pass 的接线在 `render_pipeline.cpp` 初始化时完成（ImageView 指针），**非每帧重建 descriptor set**。

### 5.3 minEngine — Scene Mesh（Base Pass）

**Set 0**：`PerFrame` / `Lights` / `PerObject`（`EngineShaderBindings` kSet0）

**Set 1 — `BuildSceneSet1`（11 bindings）**

| Binding | 名称 | 类型 |
|---------|------|------|
| 0 | Dir shadow | `sampler2DArray` SRV |
| 1 | Dir light view proj | UBO `mat4[4]` |
| 2 | Cascade far planes | UBO |
| 3 | Spot light view proj | UBO |
| 4–5 | Spot shadow ×2 | `sampler2D` SRV |
| 6–7 | Point shadow ×2 | `samplerCube` SRV |
| 8–10 | IBL ×3 | SRV |

**Set 2**：材质。

Base pass 通过 `GetSceneSet1()` 整包绑定（`SceneMeshDrawUtils.cpp`）。

### 5.4 minEngine — Shadow Pass

Pipeline layout = **仅 set 0**（`GetShadowDepthPipelineLayout`）：

| Binding | 内容 |
|---------|------|
| 0 | `LightViewProj`（单 `mat4`） |
| 1 | `PerObject`（ring offset） |
| 2 | `ShadowPassParams` |

**不绑定** shadow map 纹理 ✅（与 Piccolo 写 pass 原则一致）。

---

## 6. 「Descriptor 分离」— 差异详解

### 6.1 三层含义

| 层次 | Piccolo | minEngine |
|------|---------|-----------|
| **Pipeline layout 分离** | Shadow / Main 不同 layout | ✅ 同上 |
| **写 pass 不绑采样纹理** | ✅ | ✅ |
| **生命周期 / 失效管理** | ImageView 一次接线；render pass 管 layout | 每帧/每代际 `BuildSceneSet1` + `InvalidateShadowTextureBindings` |

### 6.2 数据流对比（ASCII）

**Piccolo**

```
Dir/Point Shadow Pass          Main Camera Pass
Set0: buffers only      →      Set0 binding 6/7: 采样 shadow color
写 R32 FBO                     矩阵在 Set0 binding 0 同块 SSBO
```

**minEngine**

```
ShadowPass                     BuildSceneSet1 (先于 shadow 执行)
Set0: 3×UBO，写 depth   →      创建 Set1（含全部 shadow SRV + UBO + IBL）
                               ↓
                               BasePass: Set0 + Set1 + Set2
```

### 6.3 关键差别（与 BUG-013 相关）

| # | 差别 | 风险 |
|---|------|------|
| **A** | **Set1 大杂烩**：Dir/Spot/Point SRV + 三套矩阵 UBO + IBL 同一 descriptor set | 任一档位错误 → 多光源串槽；full-map 才明显 |
| **B** | **`BuildSceneSet1` 在 shadow 写入之前** | 允许，但依赖 EndRenderPass/Transition；invalidate 漏了 → latched SRV |
| **C** | **矩阵双轨**：`m_LightViewProjUniformBuffer`（画）vs `m_DirLightViewProjUniformBuffer`（采） | 不一致时「图有了、采样错了」 |
| **D** | **Per-object ring 共享**：shadow 与 base 同用 `WriteNextPerObjectModel` | 一般安全；shadow 每 mesh 新建 binding set，开销大 |
| **E** | **Depth texture 采样** vs Piccolo **R32 color** | layout 必须为 `DEPTH_STENCIL_READ_ONLY`；路径更脆 |

### 6.4 不是说「没做分离」

Shadow pass 与 lit pass 的 **pipeline / 纹理角色分离** 已做。BUG-013 更可疑的是 **Set1 聚合、双轨矩阵、depth 资源语义、invalidate 链**，而非缺少 `ShadowPass`。

---

## 7. Vulkan RHI 只读审计摘要（P0–P2）

静态代码结论（2026-08-31，未改代码）。Dir-only 仍不完美 → **P0-1 整链至少 layer 0 部分可用**，优先级相对下调。

| ID | 项 | 静态结论 |
|----|-----|----------|
| **P0-1** | `ArraySlice` → `baseArrayLayer` / framebuffer layer | ✅ `GetOrCreateAttachmentImageView` + `CreateImageView2DSubresource` 已接；需 RenderDoc 验证每层 |
| **P0-2** | Depth → `DEPTH_STENCIL_READ_ONLY` | ✅ `EndRenderPass` + Manual `Transition`；整图 barrier，偏粗 |
| **P0-3** | 每 cascade `UpdateSubresource` 单 UBO | ✅ 同 CB 顺序录制应安全；与采样 UBO 数组是两条线 |
| **P1** | Viewport / Scissor | ✅ `SetViewport` 内联 scissor；无独立 `SetDepthBias` API |
| **P1** | Cull / FrontFace / ShadowMap2D flip | ✅ scheme A 自洽 |
| **P1** | Depth compare | ✅ 默认 `LESS` |
| **P1** | Depth bias | ✅ 启用（slope=1.5）；与 GL 数值不同 → 浅影 |
| **P2** | Depth-only PSO | ✅ `colorAttachmentCount=0` |

---

## 8. 推荐排查顺序（RenderDoc / 运行配置）

```
1. 单 Dir，单 cascade（FORCE_CASCADE=0）→ 看 layer 0 depth 本体
2. 仅加 MAX_CASCADES=4，仍 FORCE_CASCADE=0 → layer 0 是否变坏
3. 看 cascade 0–3 四层是否分离
4. Base pass 采样前 layout 是否为 DEPTH_STENCIL_READ_ONLY
5. 对照 m_LightViewProjUniformBuffer vs m_DirLightViewProjUniformBuffer[cascade]
6. Set1 各 SRV 是否 null / 是否 stale（toggle point 后 generation）
7. 最后才扩到 spot/point / PCF / CSM 选择
```

| Shadow map 本体 | 下一步 |
|-----------------|--------|
| **错** | ArraySlice、viewport、cull、depth state、UBO、layout |
| **对** | set1、采样坐标、`MinEngineShadowMapCoords`、cascade index、spot/point 槽位 |

**调试 shader：** `DIR_SHADOW_DEBUG_MODE` 1–6（见 `MaterialSceneShadows.glslinc`）。

---

## 9. 可借鉴 Piccolo 的方向（非照搬 set 编号）

| 原则 | 说明 |
|------|------|
| 写 pass 不绑采样纹理 | 保持 |
| 采样接线稳定 | recreate 必须 `InvalidateShadowTextureBindings()` |
| **矩阵单源** | shadow 画与 lit 采共用 per-cascade `ViewProj` |
| 降低 Set1 耦合 | 按光源类型拆 set 或拆更新粒度 |
| **可选实验** | R32 color shadow + render pass `finalLayout`（Piccolo 验证路径） |

---

## 10. 与 BUG-010 的边界

| 现象 | 轨道 |
|------|------|
| 多 cascade **同一 mesh 多份影** | BUG-010 / CSM 选择重叠 |
| Point 开关 **影响 Dir**、latched、full-map 才炸 | **BUG-013** / set1·VK 资源 |
| Dir-only **浅影** | 质量轨（bias、cull）；与 013 可并行 |

---

## 11. 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 初版：实验矩阵、Piccolo 对照、Descriptor 分离、VK RHI 审计、排查顺序 |
| 2026-08-31 | 追加：[BUG-RENDER-013 执行方案](./BUG-RENDER-013_VK_SHADOW_FIX_EXECUTION_PLAN.md) 链接 |
