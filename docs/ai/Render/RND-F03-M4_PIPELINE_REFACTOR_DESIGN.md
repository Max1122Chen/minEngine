# F03-M4：渲染管线复盘（现状 · 建议 · 参考）

## Meta

| 字段 | 值 |
|------|-----|
| Feature | RND-F03（M4 相关） |
| Status | **Reference** + **§9 已采纳简化方案**（维护者 2026-06-02 拍板） |
| Last updated | 2026-06-02 |
| 父文档 | [RND-F03_LEGACY_RHI_REMOVAL_DESIGN](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) §16 |
| 性质 | §1–§7 为复盘与参考；**§9 为当前 agreed 的简化实施方向**（不抄 UE cache/helper） |

---

## 1. 背景：M1/M2 之后为什么还觉得「不对」

F03-M1/M2（commit `5bada4e`）完成了：

- 公共 Legacy RHI API 删除；Pass 侧 BindingSet + UBO 迁移；
- Material 走 `RHIShader` + Set2；引擎 Pass shader 统一 GLSL 420。

但维护者在代码走读与 UE 对照后发现：**调用面干净了，编排心智仍是 OpenGL 即时状态**。典型感受：

- PSO 类型存在，但 **draw 时并不完全信 PSO**（layout、部分固定功能可旁路或缺失）。
- Shadow / Base / Present **各写各的 draw 循环**，没有统一的「录制 vs 提交」分界。
- BindingSet 名字现代，行为上仍像 **每 draw 绑纹理**。

因此 §16 把 M4 标为「管线心智模型」工作项；**本文是对该问题的复盘材料**，不是已定稿的重构蓝图。

---

## 2. 现状盘点（以代码为准）

### 2.1 一帧在做什么（`RenderPipeline::Execute`）

```text
[CPU，无 RenderPass]
  BuildRenderQueue
  CollectShadowRequests / BuildShadowDrawCommands（若开阴影）

[RHI — Shadow，嵌套多次]
  ShadowPass.Execute(cmdList)
    每种光：BeginRenderPass(depth) → SetViewport → SetPSO → 更新 UBO → DrawOpaqueMeshes → EndRenderPass

[CPU]
  UpdatePerFrameUBO / UpdateLightUBO
  EngineSceneBindingSets::BuildSceneSet0 / BuildSceneSet1（CreateBindingSet）

[RHI — 主场景，单次 RenderPass]
  BeginRenderPass(scene color + depth)
  SkyBoxPass / BasePass / TranslucentPass / PostProcessPass（各自内部 SetPSO、Bind、Draw）
  EndRenderPass

[RHI — Present，独立 Pass]
  PresentPass.Execute(cmdList) → BeginRenderPass(default) → … → EndRenderPass
```

**观察：**

- 整帧 **共用一个** `RHICommandList`，方向合理。
- **Pass 边界不统一**：Shadow 每层 map 有完整 Begin/End；Scene 一个大 Pass 包住多个子 Pass 类；Present 在外。
- **UBO 更新** 多在 CPU 阶段；**BindingSet 重建** 有的在帧初（Set0/1），有的在 draw 内（Material）。

### 2.2 关键类型实际职责

| 组件 | 文件 | 实际行为 |
|------|------|----------|
| `RHICommandList` | `RHI/RHICommandList.h` | 转发 `RHICmd*`；无 Pass 作用域校验；**无** `SetVertexInputLayout` |
| `RHIGraphicsPSODesc` | `RHIGraphicsPipelineState.h` | 字段含 layout、blend、depth、raster、RT 格式；**材质 compile 时常未填 layout** |
| `RHIGraphicsPSOStateFallback` | 同上 | 仅 desc 容器 |
| `OpenGLRHI::ApplyGraphicsPipelineState` | `OpenGL/OpenGLRHI.cpp` | 主要：`glUseProgram`、可选 VAO、粗粒度 depth/blend；**raster/cull 基本未接** |
| `OpenGLRHI::RHICmdSetBindingSet` | 同上 | 立即 `glBindTexture` / `glBindBufferBase`；**`setIndex` 参数被忽略** |
| `MeshDrawCommand` | `DrawCommands/MeshDrawCommand.h` | VB/IB/layout/material/model；**不含 PSO、不含 binding set 句柄** |
| `RenderPassBase::DrawMeshCommand` | `RenderPassBase.cpp` | 只 SetVB/IB + Draw；**不 SetPSO**（由调用方负责） |
| `Material::CommitCompileResult` | `Material.cpp` | `CreateGraphicsPipelineState(BuildMaterialPSODesc(shader, blend))`，**无 VertexInputLayout** |
| `Material::BindForDraw` | `Material.cpp` | 每 draw：`UpdateSubresource` scalar UBO + **`CreateBindingSet` + `SetBindingSet`** |
| `EngineSceneBindingSets` | `EngineSceneBindingSets.cpp` | Initialize 建 layout；每帧 Build Set0/1；SRV 仍 **`make_shared<OpenGLRHIShaderResourceView>`** |
| `EngineShaderBindings` | `EngineShaderBindings.h` | 冻结的 set/binding 表；主场景三 set + pass-local set |

### 2.3 各 Pass 差异（编排不统一）

| Pass | PSO 含 layout? | Binding 方式 | Draw 路径 |
|------|----------------|--------------|-----------|
| ShadowPass | Initialize 时 **无** layout | pass 级 `EnsureShadowBindingSet`，draw 内 Set + 手写 Draw | `DrawOpaqueMeshes` 重复 VB/IB 逻辑 |
| BasePass / Translucent | 依赖 Material PSO（无 layout） | `BindSceneDrawResources` + `Material::BindForDraw` | `RenderPassBase::DrawMeshCommand` |
| PresentPass | **有** layout（Initialize） | 每帧 `CreateBindingSet` | 仍调用 **`SetVertexInputLayout`**（API 已不存在 → **编译断裂**） |
| SkyBoxPass | 类似 Present | 自有 binding | 同上 **`SetVertexInputLayout`** |
| PostProcessPass | 各有实现 | pass-local set | 与 Present 类似的全屏路径 |
| EnvMapCapture | 独立体系 | 引擎层 GL 泄漏集中地 | 与主 `RenderPipeline` 不一致 |

### 2.4 M3 后端（若在进行中）

§15 记录的方向：内联 `OpenGLShader` / `OpenGLTexture*` 进 `OpenGLRHITexture` / `OpenGLRHIShader`。与 M4 编排 **正交**，但若同文件并发修改需先保证 **可编译**。

### 2.5 已知断裂 / 技术债（grep 可验证）

- `PresentPass.cpp`、`SkyBoxPass.cpp`：`cmdList.SetVertexInputLayout(...)` — `RHICommandList` 已无此接口。
- `Material` PSO 与 mesh 的 `m_VertexInputLayout` **脱节**：谁负责 VAO 不明确。
- `ShadowTypes::TextureUnit` 与 `RHITextureRef` **并存**（灯光 UBO 里仍写 unit 索引）。
- `RenderPipeline::Initialize` 仍 `rhi->SetClearColor`（Pass 外全局状态）。

---

## 3. 问题归纳（模式层，非方案）

1. **PSO 非权威**  
   Desc 很全，应用很窄；layout 可旁路（或调用已删 API）；材质 PSO 与 mesh layout 分裂。根因之一见 **§4**（PSO 不应归属 Material）。

2. **Binding 仍是「立即绑定」思维**  
   `BindingSet` 像 descriptor 名字，行为像每 draw `glBind*`；`setIndex` 未实现；Material 每 draw 重建 set。

3. **缺少「Setup vs Execute」分界**  
   UE 在 pass 外/初段录 `FMeshDrawCommand`，pass 内只做 submit + state cache。我们 **队列在 CPU 有**，但 **packet 不含 GPU 状态**，execute 时现凑。

4. **Pass 无共同模板**  
   Shadow 嵌套 pass 较规范；Scene 子 Pass 各自为政；Fullscreen 类 Pass 互抄但不共用 helper。

5. **EnvMap 集中反模式**  
   §16.5 共识：不宜与主路径并行修；**停用或绕开** 比「边迁边修」更省事（维护者已倾向后者）。

6. **文档目标（§12.3）与实现差距**  
   §12.3 为 Done 方向；**§9** 为已拍板的简化达成路径（不抄 UE cache/helper）。

---

## 4. PSO 放在 Material：F03 的错误理解与合理分工

### 4.1 我们当时做了什么

F03-M1/M2 在 `Material::CommitCompileResult` 中：

```cpp
m_PipelineState = CreateGraphicsPipelineState(
    BuildMaterialPSODesc(m_GPUShader.get(), m_BlendMode));
```

`BasePass` / `TranslucentPass` draw 时调用 `material->GetPipelineState()` 再 `SetGraphicsPipelineState`。同时 **mesh 的** `m_VertexInputLayout` 留在 `MeshDrawCommand` 上，未写入 PSO desc。

当时动机可以理解：Legacy 时代「材质 ≈ shader program」；迁现代 RHI 时自然把 **「可提交的管线对象」** 挂在 Material 上，以为 compile 完就能 draw。

### 4.2 为何这是错误理解

**Graphics PSO 描述的是一次 draw 在特定 Pass 下的一整套 GPU 固定状态**，至少包括：

| 输入 | 典型来源 | Material 单独能否提供 |
|------|----------|------------------------|
| VS / PS | Material 编译 | 部分（Pass 可能换 shader 变体，如 ShadowDepth） |
| Vertex input layout | Mesh / VertexFactory | **否** |
| Depth / stencil / blend | **Pass 规则** ± 材质 | **否**（Pass 常覆盖材质默认） |
| Raster（cull、fill） | Mesh + 材质 | 部分 |
| RT 格式 / load-store | 当前 `BeginRenderPass` | **否** |

因此 **Material 只能提供 PSO 的若干字段**，不是完整 initializer 的拥有者。把 `m_PipelineState` 放在 Material 会导致：

- layout 在 mesh、PSO 在 material → **权威分裂**（谁决定 VAO / input assembly）。
- 同一材质进 BasePass vs TranslucentPass vs Shadow → **应不同固定功能**，却共用一个 Material PSO。
- `PresentPass` 等引擎 Pass 的 PSO 在 Pass 内建、Scene mesh 在 Material 内建 → **两套心智**。

这与 §3 中「PSO 非权威」是同一根因，不是单纯「忘了填 layout」的实现疏漏。

### 4.3 UE 何时决定 PSO（对照）

UE **不在** `FMaterial` 内创建最终 `FGraphicsPipelineState`。时间线大致为：

1. **录制 `FMeshDrawCommand` 时**（`FMeshPassProcessor::BuildMeshDrawCommands`）  
   由 **MeshPassProcessor** 合并：Material 选的 shader、`VertexFactory` 的 declaration、Pass 的 `DrawRenderState`（depth/blend）、mesh 的 fill/cull → `FGraphicsMinimalPipelineStateInitializer` → 存入 `CachedPipelineId`。  
   静态 mesh 可在 AddToScene 缓存；动态 mesh 在每帧 pass setup 录制。

2. **提交 draw 时**（`FMeshDrawCommand::SubmitDrawBegin`）  
   `AsGraphicsPipelineStateInitializer()` + **`ApplyCachedRenderTargets`**（当前 Pass 的 RT）→ `SetGraphicsPipelineStateCheckApply` → `PipelineStateCache` 取/建 GPU 对象。

3. **预热（可选）**  
   `CollectPSOInitializers` 等在加载期枚举 **Material × VertexFactory × Pass** 组合，仍非 Material 私有 API。

要点：**PSO = f(Material, MeshLayout, Pass, [RenderTarget])**；组装点在 **Pass 的 mesh 录制层**（UE 即 `FMeshPassProcessor`），不是 Asset 层。

### 4.4 合理分工（方向性，供后续设计参考）

不照搬 UE 类名，但职责宜对齐：

```text
Material（资产 / 编译产物）
  ├── RHIShader（VS+PS 程序）
  ├── 材质 BindingLayout + 缓存的 MaterialBindingSet（Set2）
  ├── 材质参数 UBO、纹理槽位
  └── 材质侧状态 *输入*：blend mode、shading model 等（供上层拼 desc，非最终 PSO）

Mesh / StaticMeshSceneProxy
  ├── VertexInputLayout、VB/IB
  └── 可选：与材质无关的几何 raster 提示（wireframe 等）

Pass（BasePass、TranslucentPass、ShadowPass、Fullscreen…）
  ├── 本 Pass 默认 depth/stencil/blend（FMeshPassProcessorRenderState 等价物）
  ├── Pass 专用 shader（Shadow depth 等）
  └── Pass 级 BindingSet / UBO

Draw 录制（**各 Pass::PrepareDrawCommands**，见 §9.5；不在 BuildRenderQueue 统一建 PSO）
  └── 本 Pass 遍历 queue：Material(shader) + Command(layout) + Pass 固定状态 → PSO → 写入 MeshDrawCommand

Draw 提交（Pass 内 RHICmd*）
  └── SetGraphicsPipelineState(录制的 PSO) → SetBindingSet* → SetVB/IB → Draw
      若未来 RT 进 PSO：在 BeginRenderPass 之后、draw 前合并 RT 信息（对标 ApplyCachedRenderTargets）
```

**Material 不应再持有 `m_PipelineState` 作为 draw 权威**；可保留「仅 shader + blend 的 desc 片段」供录制层查表，或完全移到 Pass 录制代码。

**Fullscreen / 引擎 Pass**（Present、Post、Sky、Shadow）：PSO 本来就该在 **Pass::Initialize + 该 Pass 的 layout** 建，这与 mesh 路径一致——都是 **「Pass + 几何 + shader」** 拼出来，只是 fullscreen 的 layout 固定为 screen quad。

### 4.5 与 F03-M1/M2 文档的关系

- M1/M2 **调用面迁移目标仍成立**（BindingSet、删 Legacy API）。
- **`Material::GetPipelineState()` 作为长期模型不成立**；属迁移过程中的 **概念错位**，应在 M4 或后续切片纠正，而非在 Material 上补 layout 打补丁了事。
- §12.3「权威 PSO」的正确读法：**PSO 在 draw 录制点完整确定**，而不是「Material compile 时确定」。

---

## 5. UE 参考阅读笔记（本地源码）

路径：`D:\Dev\GitRepo\UnrealEngine\Engine\Source\Runtime\`

### 5.1 建议先读的 5 个锚点

| 优先级 | 文件 | 看什么 |
|--------|------|--------|
| 1 | `Renderer/Public/MeshPassProcessor.h` | `FMeshDrawCommand` 注释；`FMeshDrawCommandStateCache` |
| 2 | `Renderer/Private/MeshPassProcessor.cpp` | `SubmitDrawBegin` / `SetOnCommandList`；PSO 变才 `SetGraphicsPipelineState` |
| 3 | `Renderer/Public/MeshPassProcessor.inl` | `BuildMeshDrawCommands`：PSO 在 **录制** 时含 `VertexDeclaration` |
| 4 | `Renderer/Private/ShadowDepthRendering.cpp` | RDG pass + pass UB + `ShadowDepthPass.Draw` |
| 5 | `RHI/Public/RHIResources.h` | `FGraphicsPipelineStateInitializer`、`FBoundShaderStateInput` |

官方概述：[Mesh Drawing Pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/mesh-drawing-pipeline-in-unreal-engine)

### 5.2 UE 做法摘要（对照用，非照抄清单）

- **录制在上、提交在下**：`FMeshDrawCommand` 含 PSO id、shader bindings、vertex streams；draw 时 state cache 去重。
- **PSO 含 shader + vertex declaration + 固定功能**；`SetGraphicsPipelineStateCheckApply` 走 cache。
- **Pass 级 UB**：RDG `CreateUniformBuffer` / `SetStaticUniformBuffers`；cached draw 用 **`RHIUpdateUniformBuffer`** 换每帧数据而不改 binding。
- **RenderPass 边界**：RDG raster pass prologue 调 `BeginRenderPass`（`RenderGraphBuilder.cpp`）。
- **我们不一定要 RDG**：immediate `RHICommandList` 也可保持 **相同语义顺序**（§16.3 已写）。

### 5.3 概念映射（松散）

| UE | minEngine 现状 | 备注 |
|----|----------------|------|
| `FMeshDrawCommand` | `MeshDrawCommand` | 我们缺 PSO/bindings 录制 |
| `SubmitDrawBegin` | 分散在各 Pass | 无统一 cache |
| `FBoundShaderStateInput` | `RHIGraphicsPSODesc` | 字段有，用法不全 |
| `FRDGBuilder::AddPass` | `RenderPipeline::Execute` 手写 | F01 再议 |
| `RHIUpdateUniformBuffer` | `RHIBuffer::UpdateSubresource` | 概念接近 |

---

## 6. 早期松散建议（已由 §9 取代）

§6 初稿中的方向性想法已收敛为 **§9 维护者拍板方案**。阅读时以 §9 为准。

---

## 7. 与 F03 其它章节的关系

| 章节 | 关系 |
|------|------|
| §12.1 M1–M2 | 已完成；本复盘 **不否定** 其成果 |
| §12.2 M3 | 后端内绞杀；**§9 将 M3 与 P0/P1 并行优先** |
| §12.3 M4 | **目标陈述**；达成路径见 **§9** |
| §15 | 后端过渡态清单；与 §3.2–3.5 互参 |
| §16 | 共识摘要（CommandList、停 EnvMap）；本文 **展开现状与参考** |
| F02 | GPU 模型教案；编排层读 UE 补 F02 未覆盖部分 |
| F04 | Vulkan、descriptor 池化、PSO 缓存策略等 **推迟** |

---

## 9. 已采纳：简单正确的管线方案（维护者 2026-06-02）

> **原则：** 逻辑通顺、一层薄封装；**不**引入 UE 式 MeshPassProcessor / draw state cache / RDG。  
> **优先级：** 先 **摘掉 EnvMap**（否则持续阻塞主路径），再统一 draw、再挪 PSO、再收 Material binding。

### 9.1 维护者决策表

| ID | 议题 | 决定 |
|----|------|------|
| D1 | EnvMap / IBL 怎么摘 | **A**：不调用 `EngineIBLEnvironment::Initialize` / `EnvMapCapture`；`BuildSceneSet1(nullptr)`；PBR 不绑 IBL |
| D2 | SkyBox | **A**：保留 Sky；**不**依赖 IBL environment（Pass 内自给 sky 资源或简单着色） |
| D3 | PBR 目视 | **A**：仅直接光 + 简单 ambient；黄金场景 **不验 IBL** |
| D4 | 数据结构 | **A**：扩展 `MeshDrawCommand`（`m_PipelineState`、`m_MaterialBindingSet`） |
| D5 | 统一 draw | **A**：单一 `SubmitDraw`（`RenderPassBase` 或 `DrawSubmission.h`），Shadow/Base/Present 共用 |
| D6 | PSO 何时建 | **各 Pass 开头**：拿本 Pass 的 command 列表，用 **Pass 固定状态 + Material + layout** 拼 desc 并 `CreateGraphicsPipelineState`；**不在** `BuildRenderQueue` 统一建 |
| D7 | Scene RenderPass | **A**：保持一个大 `BeginRenderPass`（Sky + Base + Translucent + Post） |
| D8 | 见 §9.2 | 分项拍板 |

### 9.2 D8 分项（与「推迟」列表不同）

| 项 | 内容 | 决定 | 含义 |
|----|------|------|------|
| D8-1 | `setIndex` 真语义 | **本阶段不做** | `RHICmdSetBindingSet` 继续忽略 `setIndex`；GL 下 binding 数字已错开可用；**Vulkan（F04）前再严做** |
| D8-2 | `RHICreateShaderResourceView` | **优先做** | 引擎层去掉 `make_shared<OpenGLRHIShaderResourceView>`；与改 Set1/场景绑定时一并收 |
| D8-3 | M3 后端内绞杀（§15） | **优先做** | `OpenGLShader` / `OpenGLTexture*` 内联进 RHI 资源类；与管线重构 **并行**，减少双层包装 |
| D8-4 | 「只断调用、保留 EnvMap 源码」 | **不采纳** | P0 **清理 EnvMap 出主路径**：断链 + 从构建/链接剔除或删除 `EnvMapCapture` 等（**不**长期留一套从不走的 Capture 代码碍眼） |

### 9.3 三条硬规则

1. **PSO 权威**  
   Layout 只进 `RHIGraphicsPSODesc`；禁止 `SetVertexInputLayout` 旁路。Material **不**持 draw 用 `m_PipelineState`。

2. **每次 draw 固定四步**（唯一实现：`SubmitDraw`）  
   `SetGraphicsPipelineState` → `SetBindingSet(s)` → `SetVertexBuffer` / `SetIndexBuffer` → `Draw` / `DrawIndexed`。

3. **改 GPU 配置的 `RHICmd*` 只在 `BeginRenderPass` … `EndRenderPass` 内**  
   `UpdateSubresource`、`BuildSceneSet*` 可在 Pass 外；`RHICreate*` 可在初始化/加载时。

### 9.4 职责（一张表）

| 谁 | 管什么 |
|----|--------|
| `Material` | `RHIShader`、材质 BindingLayout/Set（compile 时建）、scalar UBO；**不管**最终 PSO |
| `MeshDrawCommand` | VB/IB、layout、material、model；**本 Pass 准备好的** `m_PipelineState`、`m_MaterialBindingSet` |
| `Pass` | 默认 depth/blend、pass shader（Shadow）、fullscreen PSO；**`PrepareDrawCommands(queue)`** |
| `RenderPipeline` | `BuildRenderQueue`（**不填 PSO**）、UBO 更新、Scene Set0/1、Pass 顺序 |
| `SubmitDraw` | 全引擎唯一的四步 draw |

**不引入：** PSO 全局 cache 类、draw state cache、并行 CommandList、RDG。

### 9.5 PSO：`Pass::PrepareDrawCommands`

```text
BuildRenderQueue
  └── 只写：几何、material 指针、layout、model、排序键 …（无 PSO）

BasePass::Execute（draw 之前）
  └── PrepareDrawCommands(m_DrawCommands, PassKind::Opaque)
        for cmd in commands:
          desc.VertexShader / PixelShader = material->GetShader()
          desc.VertexInputLayout       = cmd.m_VertexInputLayout
          desc.Blend/Depth/…           = BasePass 固定（写 depth、无 blend…）
          cmd.m_PipelineState          = CreateGraphicsPipelineState(desc)

TranslucentPass::Execute
  └── 同上，PassKind::Translucent（blend on 等）

ShadowPass
  └── 继续 **Pass 级单一 depth PSO**（Initialize 时含 position-only layout）；caster 不走材质 PSO

Present / Post / Sky
  └── Initialize 时建好 **含 layout** 的 PSO，存 Pass 成员
```

Opaque / Translucent **已是两个队列**，各 Pass 各准备各的 PSO，避免 Base/Translucent 固定功能冲突。

同一帧重复 `(shader, layout, pass固定状态)` 可先 **不** 做 map；网格量小时每 command 一次 `Create` 可接受；需要时 Pass 内加小型 `unordered_map` 即可，**非**全局系统。

### 9.6 `SubmitDraw` 与场景绑定

```text
ScenePass（BeginRenderPass 之后、mesh draw 循环之前，一次）:
  SetBindingSet(kSetSceneObject, set0)
  SetBindingSet(kSetShadowIBL, set1)   // 无 IBL 时 shadow 槽仍有效，IBL 槽 null/dummy

每条 mesh（SubmitDraw）:
  UpdateSubresource(perObject, model)   // 或 beforeDraw 回调
  SetGraphicsPipelineState(cmd.m_PipelineState)
  SetBindingSet(kSetMaterial, cmd.m_MaterialBindingSet)
  SetVertexBuffer / SetIndexBuffer
  DrawIndexed / Draw
```

`Material::BindForDraw`：**删除** draw 内 `CreateBindingSet`；compile / 改纹理时更新 `m_MaterialBindingSet` 指针。

### 9.7 目标帧结构（与现形相近）

```text
Setup (CPU)
  BuildRenderQueue
  UpdatePerFrameUBO / UpdateLightUBO
  BuildSceneSet0 / BuildSceneSet1（无 IBL）

Execute (一个 RHICommandList)
  ShadowPass …（嵌套 Begin/End，SubmitDraw 或等价）
  BeginRenderPass(scene)
    绑 Set0/1
    SkyBoxPass
    BasePass.Prepare + Execute
    TranslucentPass.Prepare + Execute
    PostProcessPass（fullscreen SubmitDraw）
  EndRenderPass
  PresentPass
```

### 9.8 实施顺序（建议）

| 阶段 | 内容 | 备注 |
|------|------|------|
| **P0** | **摘 EnvMap（D1–D3、D8-4）** | 不 `IBLEnvironment::Initialize`；清理/剔除 `EnvMapCapture` 出构建；PBR `bBindPBRIBL=false`；Sky 不读 IBL；必要时 shader 常量 ambient |
| **P0′** | **D8-3 M3** + **D8-2 RHICreateSRV** | 与 P0/P1 **并行优先**；改 binding 时 SRV 走 RHI；后端去掉 `OpenGLShader`/`OpenGLTexture*` 外壳 |
| **P1** | `SubmitDraw` + Present/Sky layout 进 PSO | 修编译；Shadow/Present 改调 `SubmitDraw` |
| **P2** | `PrepareDrawCommands`；Material 去掉 `m_PipelineState` | 扩展 `MeshDrawCommand` |
| **P3** | Material BindingSet 仅 compile/改参时建 | grep：draw 内无 `CreateBindingSet` |

**明确不做（本阶段）：** D8-1 `setIndex` 真语义。

### 9.9 轻量验收

- [ ] 主路径无 `EnvMapCapture` / `IBLEnvironment::Initialize` 调用；PBR 无 IBL 不崩
- [ ] 无 `SetVertexInputLayout`；无 `Material::GetPipelineState` 作 draw 权威
- [ ] Shadow + 主视口 + 透明 + Post + Present 目视 OK
- [ ] 引擎生产路径无 `new/make_shared<OpenGLRHIShaderResourceView>`（D8-2）
- [ ] grep：`OpenGLShader`、`OpenGLTexture2D` 等为 0（D8-3，§12.2）
- [ ] `verify.ps1` 通过

---

## 10. 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：完整重构设计（已废止） |
| 2026-06-01 | **改为复盘**：仅现状、问题归纳、UE 参考、松散建议；Status → Reference |
| 2026-06-02 | **§4**：PSO 放 Material 为 F03 错误理解；合理分工与 UE 对照 |
| 2026-06-02 | **§9**：维护者拍板 — 简单方案、EnvMap 先行、D6 Pass 内建 PSO、D8 分项 |
