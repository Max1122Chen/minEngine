# Modern RHI — GPU 工作模型教案 + 设计

## Meta

- **ID:** `RND-F02`
- **Type:** Refactor
- **Status:** Draft
- **Owner:** (maintainer)
- **Last updated:** 2026-06-01
- **Branch:** `render`
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · `RND-F03`（Planned，另文档） · [RENDER_REFACTOR_PLAN](./RENDER_REFACTOR_PLAN.md)（Viewport/SceneDraw，正交）

## TL;DR

**问题：** 当前 `RHI` 是 OpenGL 全局状态 + 资源工厂的薄封装；Pass 内仍有 `glDraw*` / GL 强转，无法干净映射 Vulkan 等 API。

**方案：** 以 **GPU 如何工作** 为 normative 定义 Modern RHI；OpenGL 作为 **首个适配后端** 实现该语义；Vulkan 在 `RND-F03` 复用同一契约。

**状态：** 设计 Draft；实现按 R0→R3 切片，均在 `render` 分支。

---

## 文档怎么用（教案 + 设计）

| 章节 | 性质 | 读者目标 |
|------|------|----------|
| **Part A — GPU 执行模型** | **教案（normative）** | 理解「RHI 在抽象什么」；与 API 无关 |
| **Part B — minEngine 设计** | **工程 design** | 接口、迁移、切片、验收 |
| **Part C — API 对照** | **参考（non-normative）** | 实现/学习时查 Vulkan、UE、NVRHI 用词 |

> **原则：** 先掌握 Part A，再读 Part B。Part C 不得反向定义 Part B 的接口形状。

---

## Scope

### In

- GPU 模型驱动的 RHI 公共契约（Device、Resource/View、PSO、Binding、RenderPass、CommandList、Transition、Upload）
- OpenGL **Dynamic RHI** 实现上述契约（adapter，非定义源）
- Renderer 层迁移：Pass / `RenderPassBase` 仅通过 CommandList 提交；去掉 Pass 内 `gl*` 与 `OpenGL*` 强转
- 固定引擎 shader 路径先迁（Shadow、Present、Skybox、Post 等）；材质按名 uniform **后迁**（R3 之后或 R3 子切片）

### Out

- **RenderGraph**（`RND-F01`，Deferred）
- **Vulkan 实现**（`RND-F03`）
- 本阶段不要求 CSM/PBR/Material IR 的 binding 一次性迁完
- 不以「兼容 OpenGL 习惯」削弱 GPU 模型（例如 texture unit 写入核心 `IRHITexture`）

---

## Reader quick start

1. 读 **Part A**（1–2 小时量级）→ 对照自己 Vulkan 练习经验。
2. 读 **Part B §3–§5** → 知道要改哪些文件、验收是什么。
3. 实现时打开 **Part C** 与 `minEngine/.../Render/RHI/` 对照。
4. 切片计划见 **§6**；完成后勾选 **§7 验收**。

**代码入口（现状）：**

- `minEngine/minEngine/src/Runtime/Function/Render/RHI/`
- `minEngine/minEngine/src/Runtime/Function/Render/OpenGL/`
- `minEngine/minEngine/src/Runtime/Function/Render/RenderPipeline/RenderPasses/`

---

# Part A — GPU 执行模型（教案）

现代图形 API（Vulkan、D3D12、Metal）与旧 API（OpenGL、D3D11）的差异，很大程度是 **「GPU 能力是否显式暴露」**，而不是 GPU 在做不同的事。RHI 应抽象 **后者**。

## A.1 并行处理器与队列

- GPU 是 **异步并行** 设备：CPU 提交 **工作包**，GPU 稍后执行。
- 工作通过 **队列（Queue）** 提交：常见有 **Graphics**、**Compute**、**Copy/Transfer**。
- CPU 与 GPU、队列与队列之间需要 **Fence / Semaphore** 等同步。

**对 RHI：** `IRHIDevice` 持有队列；`ExecuteCommandList` 将录制好的命令提交到指定队列。OpenGL 可建模为 **单 Graphics 队列 + Immediate CommandList**。

## A.2 内存与资源（Resource）

- GPU 使用 **Buffer**（顶点、索引、uniform、staging）与 **Image/Texture**（2D、Cube、Array）。
- 资源有：**尺寸、格式（Format）、用途（Usage）**（是否可作 RT、深度、采样、UAV 等）。
- CPU 写入 GPU 资源通常：**Staging Buffer → Copy** → GPU 资源（非「构造函数里顺手上传」）。

**对 RHI：** `CreateBuffer` / `CreateTexture` 描述符含 `Format`、`Usage`；`Upload` / `WriteBuffer` / `WriteTexture` 显式表达上传路径。

## A.3 视图（View）— 同一内存，多种用法

同一块 GPU 内存可因 **用法不同** 而需要不同 **视图**：

| 概念 | 典型用途 |
|------|----------|
| **Shader Resource (SRV)** | Pixel/Vertex/Compute **读** 纹理或 buffer |
| **Render Target (RTV)** | **写** 颜色附件 |
| **Depth Stencil (DSV)** | **写/测** 深度（+ stencil） |
| **Unordered Access (UAV)** | 可随机读写的纹理/buffer（compute、部分 RT） |

状态转换不当时会产生 **hazard**（未定义行为）。

**对 RHI：** `RHITexture` / `RHIBuffer` 为资源；`CreateShaderResourceView`、`CreateRenderTargetView` 等为 **View 工厂**。禁止「一张 `RHITexture2D` 既 Bind(unit) 又当 FBO 附件」的单一类包办。

## A.4 光栅 Pass（Render Pass）

一段 **光栅化工作** 的边界：

- **附件（Attachments）：** 若干 color + 可选 depth/stencil。
- **Load/Store：** 开始时 Clear / Load / DontCare；结束时 Store / DontCare（影响带宽与 tile GPU）。
- **Viewport / Scissor** 在该 Pass 内有效。
- Pass 结束常是 **同步自然点**（附件从 RT 变为 Sample 等需 **Transition**）。

**对 RHI：** `RHIRenderPassDesc` + `BeginRenderPass` / `EndRenderPass`。替代：散落 `FrameBuffer::Bind`、`Clear`、`SetDrawBuffer`。

## A.5 管线状态（PSO）

一次 Draw 需要 **一整包** 状态，现代 GPU 倾向 **不可随意拆分** 的配置：

- Shader stages（VS/PS/…）
- **Vertex input**（布局，非 VAO 对象本身）
- **Rasterizer**（cull、fill）
- **Depth/Stencil**（test、write、compare）
- **Blend**（attachments 的混合方程）
- **Pipeline layout**（与 Binding 关联）

**对 RHI：** `RHIGraphicsPipelineState` + `SetGraphicsPipeline`。替代：`EnableDepthTest` / `EnableBlend` / `glUseProgram` 分散调用。

## A.6 绑定（Binding / Descriptor）

Shader 从 **固定布局的 slot** 读取资源：

- **Layout：** slot 0 = PerFrame UBO，slot 1 = Lights，slot 8 = shadow map array…
- **Set / Table：** 本次 draw 绑定 **具体** buffer/texture/sampler。

按 **名字** 每 draw 查 location 是 GL/D3D11 **反射习惯**，不是 GPU 必需；Modern RHI 以 **slot + layout** 为主，名字可在 **编译/reflection 工具** 层映射。

## A.7 命令录制（Command List）

CPU 将操作录制成 **命令流**，再一次性提交：

典型顺序：`BeginRenderPass` → `SetPipeline` → `SetBindings` → `SetVertexBuffers` → `DrawIndexed` → `EndRenderPass`；另有 `Copy`、`Dispatch`、`Transition`。

**对 RHI：** Renderer **只** 持有 `IRHICommandList*`，**不** 调用 `glDraw*` / `vkCmdDraw*`。

## A.8 资源状态与屏障（Transition / Barrier）

资源在任意时刻处于 **一种访问状态**，例如：

`ShaderResource` | `RenderTarget` | `DepthWrite` | `CopyDest` | `Present` | …

读写角色变化时需 **Transition**（Vulkan barrier、D3D12 resource barrier；GL 常 no-op）。

**对 RHI：** `Transition(Resource, Before, After)`；OpenGL 后端可为空实现，接口必须存在。

## A.9 小结：RHI 语句 = GPU 语义

```text
Device / Queue
  → 创建 Resource + View
  → 创建 PSO（含 layout）
  → CommandList: Pass → Pipeline → Bind → Draw/Dispatch/Copy
  → Transition（读写角色变化）
  → Submit + Fence
```

各 API 是上述语句的 **映射实现**；教案到此为止，下面落到 minEngine。

---

# Part B — minEngine Modern RHI 设计

## B.1 分层

```text
┌──────────────────────────────────────────────┐
│ RenderPipeline / *Pass / Material (Renderer)  │
│  · 组 MeshDrawCommand / Pass 逻辑             │
│  · 只调用 IRHICommandList + 已建 PSO/Binding  │
├──────────────────────────────────────────────┤
│ Modern RHI (API-agnostic, GPU model)          │
│  IRHIDevice · IRHICommandList · IRHI*Resource │
├──────────────────────────────────────────────┤
│ Dynamic RHI Backend                           │
│  OpenGLRHI (F02) · VulkanRHI (F03)            │
└──────────────────────────────────────────────┘
```

**不变量：**

1. Renderer **不** `#include` backend 头文件（`OpenGL*.h`、`vulkan.h`）。
2. Backend **不** 定义 Renderer 可见的新概念（仅实现 GPU 模型已有概念）。
3. **Native handle**（如 GL texture id、VkImage）仅经 `GetNativeHandle()` 给 **Platform/UI**，不进 Pass。

## B.2 核心类型（目标契约，命名可微调）

### Device / Queue

```text
IRHIDevice
  Initialize(InitParams) / Shutdown()
  GetGraphicsQueue()
  CreateCommandList(Type: Immediate | Deferred)
  CreateBuffer(desc) / CreateTexture(desc)
  CreateGraphicsPSO(desc)
  CreateBindingLayout(desc) / CreateBindingSet(layout, bindings)
  CreateShaderModule(bytecode | stage sources per backend policy)
```

### Resources & Views

```text
IRHIBuffer          — GPU buffer
IRHITexture         — 2D / Cube / 2DArray（维度在 desc）
IRHIBufferView      — VB / IB / SRV / UAV
IRHITextureView     — SRV / RTV / DSV / UAV
```

**删除/替换（公共 API）：**

- `RHITexture2D::GetID()` + `Bind(int unit)` → View + BindingSet
- `FrameBuffer` 作为 Renderer 中心概念 → **RenderPass + 附件纹理 View**

### Render Pass

```text
struct RHIRenderPassDesc {
  attachments[]  // RTV/DSV + LoadAction + StoreAction + clear
  extent         // width/height
};

CommandList::BeginRenderPass(desc)
CommandList::EndRenderPass()
```

`SceneRenderTarget` 保留为 **Renderer 辅助类**，内部持有 color/depth **纹理 + View**，向 Pass 提供 `RHIRenderPassDesc`。

### Pipeline State

```text
struct RHIGraphicsPSODesc {
  shader modules (VS/PS minimum)
  vertexInputLayout
  rasterizer / depthStencil / blend
  bindingLayout
  renderPassCompatibility  // format/layout 兼容键
};

CommandList::SetGraphicsPipeline(pso)
```

### Binding

```text
BindingLayout — 静态 slot 表（UBO、Texture、Sampler）
BindingSet    — 某 layout 上具体资源

CommandList::SetBindingSet(setIndex, set)
```

**引擎固定 layout（初版建议）：**

| Set / Slot | 内容 | 备注 |
|------------|------|------|
| 0 | `PerFrameData` UBO | 对齐现有 binding 0 |
| 1 | `LightsData` UBO | binding 1 |
| 8–10 | Shadow maps + 相关 UBO | 对齐 Shadow/Base 现有 unit/binding |
| Material | 动态 slot 区 | R3+ 迁；先保留 GL 路径或 dual-path |

### Command List

```text
IRHICommandList
  BeginRenderPass / EndRenderPass
  SetGraphicsPipeline
  SetBindingSet
  SetVertexBuffers / SetIndexBuffer
  SetViewport / SetScissor
  DrawIndexed(indexCount, ...)
  Draw(vertexCount, ...)
  Dispatch(...)                    // 预留
  Transition(texture|buffer, from, to)
  CopyTexture / UpdateBuffer       // upload
```

### 能力查询（可选，R2+）

```text
IRHIDevice::GetCapabilities()
  — maxTextureUnits / maxUBOSize / supportsDynamicRendering / ...
```

用于 Editor/UI，**不** 用于 Pass 内 `#if`。

## B.3 现状与迁移映射

| 现状 | 问题 | 目标 |
|------|------|------|
| `RHI::EnableDepthTest` 等 | GL 全局状态 | PSO + RenderPass |
| `RHITexture2D::Bind(unit)` | GL texture unit | BindingSet + SRV |
| `FrameBuffer::Bind` | 与 Pass 语义混在一起 | `BeginRenderPass` |
| `RHIShader::Use` + `UploadUniform*` | 按名 + 即时 | PSO + BindingSet；材质后迁 |
| `VertexDefinition` = VAO | API 对象泄漏 | `RHIVertexInputLayout`（GL 内用 VAO 实现） |
| `CreateVertexBuffer(float*...)` | 隐含 upload | `CreateBuffer` + `Upload` |
| `RenderPassBase::DrawMeshCommand` 内 `glDraw*` | 击穿 RHI | `CommandList::DrawIndexed` |
| `ShadowPass` 内 `glDrawElements` | 同上 | 同上 |
| `PresentPass` 内 `glDrawArrays` | 同上 | 同上 |
| `GetID()` for ImGui | GL 泄漏 | `IRHITextureView::GetNativeHandle()` 仅 UI 层 |

**主要改动文件（Implementation 时会扩表）：**

- `RHI/*` — 新接口
- `OpenGL/*` — 适配实现
- `RenderPasses/*`、`RenderPassBase.cpp` — 去 GL
- `SceneRenderTarget.cpp` — Pass desc 产出
- `Material.cpp` — **晚于** R2（Binding 迁移）

## B.4 OpenGL 适配策略（backend 内部）

| GPU 概念 | OpenGL 实现要点 |
|----------|-----------------|
| CommandList Immediate | 单线程；命令立即转 `gl*` |
| RenderPass | `glBindFramebuffer` + `glClear` + `glViewport`；MRT 用 `drawBuffers` |
| PSO | 缓存 `(program, VAO, depth/blend/cull)` 或 program + 显式状态 |
| BindingSet | `glActiveTexture` + `glBindTexture` + `glBindBufferBase`（UBO） |
| Transition | no-op 或 `glMemoryBarrier`（需要时） |
| Upload | `glBufferSubData` / `glTexSubImage`；staging 可选简化 |
| NativeHandle | `GLuint` texture / FBO |

**禁止：** 为省事在 `RHI.h` 保留 `EnableBlend`「给 GL 用」。

## B.5 与 `RENDER_REFACTOR_PLAN` 的关系

- Viewport / `SceneDrawDesc` / `SceneRenderTarget`：**已存在**，继续作为 Renderer 输入。
- F02 **替换** 「如何绑定 RT 与 draw」；**不** 重做 Scene 逻辑。
- 多视口 = 多个 `RHIRenderPassDesc` / 多个 CommandList 录制段，不引入 RDG。

---

## B.6 删除列表（契约切换完成后）

- `RHI` 上 GL 式 `Enable*` / `SetDrawBuffer` / `SetReadBuffer`（公共接口）
- Pass 与 `Material` 路径上的 `glad` include（材质路径最后清）
- `RHITexture2D::Bind` / `GetID` 作为 **引擎通用** API
- `static_cast<OpenGL*>` in `RenderPasses/`

---

# Part C — API 对照（参考，non-normative）

| GPU 模型（normative） | Vulkan | D3D12 | UE（近似） |
|----------------------|--------|-------|------------|
| Queue | `VkQueue` | `ID3D12CommandQueue` | `FRHICommandList` 提交目标 |
| Resource | `VkImage` / `VkBuffer` | `ID3D12Resource` | `FRHITexture` / `FRHIBuffer` |
| View | `VkImageView` | descriptor 中 view | `FRHI*View` |
| Render Pass | `VkRenderPass` / dynamic rendering | RTV/DSV + clear | `BeginRenderPass` |
| PSO | `VkPipeline` | `ID3D12PipelineState` | `FRHIGraphicsPipelineState` |
| Binding | `VkDescriptorSet` | root signature + table | shader parameters |
| Command list | `VkCommandBuffer` | `ID3D12GraphicsCommandList` | `FRHICommandList` |
| Barrier | `vkCmdPipelineBarrier` | `ResourceBarrier` | `Transition` |

学习资源： [NVRHI Programming Guide](https://github.com/NVIDIA-RTX/NVRHI/blob/main/doc/ProgrammingGuide.md)（Device + CommandList + 自动 barrier 思路）；UE Mesh Drawing / RDG 文档（CommandList 在 Pass 中的角色）。

---

# §6 实现切片（F02）

| 切片 | 内容 | 验收 |
|------|------|------|
| **R0** | `IRHICommandList::DrawIndexed/Draw`；`OpenGLCommandList`；`RenderPassBase` / `ShadowPass` / `PresentPass` 无 `gl*` / 无 `OpenGL*` 强转 | 编译通过；`verify.ps1`；Editor 主视口目视无回归 |
| **R1** | `RHIRenderPassDesc` + `Begin/EndRenderPass`；`SceneRenderTarget`、Shadow FBO 迁此模型；去掉 Pass 内 `EnableDepth*` 等（进 PSO 默认或 pass 级默认 PSO） | 同上 |
| **R2** | `RHIGraphicsPSO` + `RHIVertexInputLayout`；Shadow / Present / Skybox 用固定 PSO | 同上 |
| **R3** | `BindingLayout` + `BindingSet`；引擎固定 shader 迁 binding；`GetNativeHandle` 给 ImGui | Inspector/视口纹理仍可用；材质 IR **可仍 dual-path** |
| **R3+** | `Material::BindForDraw` 迁 Binding；删 `UploadUniform*` 热路径 | 材质测试 / material-ir-test |

**F03（Vulkan）** 不在此表执行；以 F02 契约为输入，里程碑：Present → Shadow → 简化 Base（见 `RND-F03` 待建文档）。

---

# §7 验收标准（F02 Done）

- [ ] Part A 概念在代码中均有对应类型（不要求注释教学，但接口齐全）
- [ ] `RenderPasses/` 无 `gl*`、无 `#include glad`
- [ ] OpenGL 单后端：Editor 主场景、阴影、Present 与迁移前一致（维护者目视 + `verify.ps1`）
- [ ] `ACTIVE_WORK` 边界内无 RenderGraph、无 Vulkan 强依赖
- [ ] Design §6 切片 R0–R3 勾选完成（R3+ 材质可选单列 Done）

---

## §8 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 材质 binding 迁期过长 | dual-path 混乱 | R0–R2 不动 Material IR；R3 只迁引擎 shader |
| PSO 组合爆炸 | GL 状态缓存复杂 | 先少量固定 PSO；按 pass 缓存 |
| ImGui 断图 | `GetID` 移除 | 尽早提供 `GetNativeHandle` + UI 桥 |
| 范围蔓延 | 推迟 F03 | ACTIVE_WORK 边界表；切片 DoD |

---

## §9 Status note

（无 — Draft）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：Part A 教案 + Part B 契约/迁移/切片；`render` 分支 |
