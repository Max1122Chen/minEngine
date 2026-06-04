# Modern RHI — GPU 工作模型教案 + 设计

## Meta

- **ID:** `RND-F02`
- **Type:** Refactor
- **Status:** Draft
- **Owner:** (maintainer)
- **Last updated:** 2026-06-01
- **Branch:** `render`
- **Code skeleton:** `minEngine/.../Render/RHI/`（见 **§B.7**）
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · `RND-F03`（Planned，另文档） · [RENDER_REFACTOR_PLAN](./RENDER_REFACTOR_PLAN.md)（Viewport/SceneDraw，正交）

## TL;DR

**问题：** 当前 `RHI` 是 OpenGL 全局状态 + 资源工厂的薄封装；Pass 内仍有 `glDraw*` / GL 强转，无法干净映射 Vulkan 等 API。

**方案：** 以 **GPU 如何工作** 为 normative 定义 Modern RHI；OpenGL 作为 **首个适配后端** 实现该语义；Vulkan 在 `RND-F03` 复用同一契约。

**状态：** 设计 Draft；**代码壳已落地**（空类型 + TODO）；实现按 **§6** 切片（PSO/RenderPass 优先，非空壳 Draw）。

**参考实现：** 本地 UE 源码 `D:\Dev\GitRepo\UnrealEngine\Engine\Source\Runtime\RHI\`（读源码对齐时优先于此文档措辞）。

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

- GPU 模型驱动的 RHI 公共契约（**`RHI` Dynamic 后端**、Resource/View、PSO、Binding、RenderPass、**`RHICommandList`**、Transition、Upload）
- OpenGL **Dynamic RHI** 实现上述契约（adapter，非定义源）
- Renderer 层迁移：Pass / `RenderPassBase` 仅通过 CommandList 提交；去掉 Pass 内 `gl*` 与 `OpenGL*` 强转
- 固定引擎 shader 路径先迁（Shadow、Present、Skybox、Post 等）；材质按名 uniform **后迁**（R3 之后或 R3 子切片）

### Out

- **RenderGraph**（`RND-F01`，Deferred）
- **Vulkan 实现**（`RND-F03`）
- 本阶段不要求 CSM/PBR/Material IR 的 binding 一次性迁完
- 不以「兼容 OpenGL 习惯」削弱 GPU 模型（例如 texture unit 写入核心 `RHITexture`）

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

**对 RHI：** 抽象类 **`RHI`**（Dynamic RHI）持有队列语义；`RHICommandList` 录制 draw 等命令并最终 **`Submit`** 到队列。OpenGL 可建模为 **单 Graphics 队列 + Immediate CommandList**。

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

**对 RHI：** `RHIRenderPassInfo` + `BeginRenderPass` / `EndRenderPass`。替代：散落 `FrameBuffer::Bind`、`Clear`、`SetDrawBuffer`。

## A.5 管线状态（PSO）

一次 Draw 需要 **一整包** 状态，现代 GPU 倾向 **不可随意拆分** 的配置：

- Shader stages（VS/PS/…）
- **Vertex input**（布局，非 VAO 对象本身）
- **Rasterizer**（cull、fill）
- **Depth/Stencil**（test、write、compare）
- **Blend**（attachments 的混合方程）
- **Pipeline layout**（与 Binding 关联）

**对 RHI：** `RHIGraphicsPipelineState`（handle）+ `RHIGraphicsPSOCreateInfo`（配置）+ `SetGraphicsPipeline`。替代：`EnableDepthTest` / `EnableBlend` / `glUseProgram` 分散调用。

## A.6 绑定（Binding / Descriptor）

Shader 从 **固定布局的 slot** 读取资源：

- **Layout：** slot 0 = PerFrame UBO，slot 1 = Lights，slot 8 = shadow map array…
- **Set / Table：** 本次 draw 绑定 **具体** buffer/texture/sampler。

按 **名字** 每 draw 查 location 是 GL/D3D11 **反射习惯**，不是 GPU 必需；Modern RHI 以 **slot + layout** 为主，名字可在 **编译/reflection 工具** 层映射。

## A.7 命令录制（Command List）

CPU 将操作录制成 **命令流**，再一次性提交：

典型顺序：`BeginRenderPass` → `SetPipeline` → `SetBindings` → `SetVertexBuffers` → `DrawIndexed` → `EndRenderPass`；另有 `Copy`、`Dispatch`、`Transition`。

**对 RHI：** Renderer **只** 通过 **`RHICommandList`** 录命令，**不** 调用 `glDraw*` / `vkCmdDraw*`。

## A.8 资源状态与屏障（Transition / Barrier）

资源在任意时刻处于 **一种访问状态**，例如：

`ShaderResource` | `RenderTarget` | `DepthWrite` | `CopyDest` | `Present` | …

读写角色变化时需 **Transition**（Vulkan barrier、D3D12 resource barrier；GL 常 no-op）。

**对 RHI：** `Transition(Resource, Before, After)`；OpenGL 后端可为空实现，接口必须存在。

## A.9 小结：RHI 语句 = GPU 语义

```text
RHI (Dynamic) — 创建 Resource / View / PSO；执行或接收 GPU 命令
  → RHICommandList: Pass → Pipeline → Bind → Draw/Dispatch/Copy
  → Transition（读写角色变化）
  → Submit + Fence
```

各 API 是上述语句的 **映射实现**；教案到此为止，下面落到 minEngine。

---

# Part B — minEngine Modern RHI 设计

## B.1 分层（UE 式：一个 Dynamic RHI + CommandList 转发）

```text
┌──────────────────────────────────────────────┐
│ RenderPipeline / *Pass / Material (Renderer)  │
│  · 组 MeshDrawCommand / Pass 逻辑             │
│  · 只拿 RHICommandList& 录 Pass / Draw        │
├──────────────────────────────────────────────┤
│ RHI 公共层（API 无关类型 + CommandList 包装）   │
│  RHICommandList · RHITexture · RHIBuffer · PSO │
├──────────────────────────────────────────────┤
│ RHI（abstract Dynamic RHI）← 仅此处 per-backend │
│  OpenGLRHI (F02) · VulkanRHI (F03)            │
└──────────────────────────────────────────────┘
```

**命名：** 抽象后端叫 **`RHI`**（与现有类名延续，语义等同 UE 的 `FDynamicRHI`），**不** 引入 `RHIDevice` / `IRHI*` 前缀。

**不变量：**

1. Renderer **不** `#include` backend 头文件（`OpenGL*.h`、`vulkan.h`）。
2. **仅 `RHI` 子类**（`OpenGLRHI`、`VulkanRHI`）实现 virtual / 后端逻辑；**`RHICommandList` 不做 per-backend 子类**。
3. **Native handle** 仅经 `GetNativeHandle()` 给 Platform/UI，不进 Pass。

### B.1.1 为何采用 UE 式「CommandList 转发」而非双虚表

| 方案 | 做法 | minEngine 结论 |
|------|------|----------------|
| **A. 双虚表** | `IRHIDevice` + `IRHICommandList` 各有一套 OpenGL/Vulkan 实现 | 类型翻倍，Pass 要依赖具体 CmdList 后端，**过度** |
| **B. UE 式（选用）** | **`RHI` 虚基类**承担 Create + RHICmd*；**`RHICommandList`  concrete**，内联转发 `GetRHI()->…` | 后端只写一份；Pass API 稳定；与 UE `GDynamicRHI` + `FRHICommandList` 同构 |

UE 里 Pass 常见写法：`FRHICommandListImmediate& RHICmdList`；`RHICmdList.DrawIndexedPrimitive(...)` 内部进入 RHI 命令队列或 **`GDynamicRHI`**。资源创建亦可在 CommandList 上暴露 `CreateUniformBuffer` 等，**实现仍是调 Dynamic RHI**，不是为了多一层虚函数。

minEngine 第一阶段：**Immediate CommandList**（命令立刻进当前 `RHI` 后端）。Deferred / Parallel 录制留接口位，不实现。

```text
Pass 调用                    RHICommandList（concrete）          RHI（virtual，每 API 一个子类）
─────────────────────────────────────────────────────────────────────────────────────────
CmdList.DrawIndexed(...)  →  m_RHI->RHICmdDrawIndexed(...)   →  OpenGL: glDrawElements
CmdList.CreateTexture(...)→  m_RHI->RHICreateTexture2D(...)  →  Vulkan: vkCreateImage …
CmdList.BeginRenderPass(..)→  m_RHI->RHICmdBeginRenderPass(..)
```

可选：与 UE 的 `RHICreateTexture2D` 一样，提供 **`RHICreate*`** 自由函数 → `GetRHI()->RHICreate*()`，供 **初始化 / 资源加载** 等无 CommandList 上下文处使用；Pass 内仍优先走 `CmdList.Create*` 以保持「经 RHI 层」的一致入口。

### B.1.2 与现有 `minEngine::RHI` 的演进

| 阶段 | 代码形态 |
|------|----------|
| 现状 | `class RHI` 混合 GL 状态 + Create* |
| F02 目标 | **`RHI` 升格为 Dynamic RHI**（纯虚 RHICreate* / RHICmd*）；GL 状态从 public API 删除 |
| | **`RHICommandList`** 新建；`RenderSystem` 持有 `RHI` + 帧内 `RHICommandListImmediate`（或 Pass Execute 时注入引用） |
| | **`OpenGLRHI : public RHI`** 实现全部 virtual |

---

## B.2 核心类型（目标契约）

### B.2.1 `RHI` — Dynamic RHI（**唯一 per-backend 虚表**）

```text
class RHI {
  // 生命周期
  virtual void Initialize() / Shutdown()

  // 资源创建（RHICreate* 命名对齐 UE 习惯，与 RHICmd* 区分）
  virtual RHITextureRef RHICreateTexture2D(RHITextureDesc, …)
  virtual RHIBufferRef  RHICreateBuffer(RHIBufferDesc, …)
  virtual RHIGraphicsPipelineStateRef RHICreateGraphicsPipelineState(const RHIGraphicsPSOCreateInfo&)
  virtual RHIShaderRef RHICreateShader(…)
  virtual RHIBindingLayoutRef RHICreateBindingLayout(…)
  virtual RHIBindingSetRef RHICreateBindingSet(…)

  // 命令执行（由 RHICommandList 转发；Immediate 模式下直接调用）
  virtual void RHICmdBeginRenderPass(const RHIRenderPassInfo&)
  virtual void RHICmdEndRenderPass()
  virtual void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState*)
  virtual void RHICmdSetBindingSet(uint32 setIndex, RHIBindingSet*)
  virtual void RHICmdSetVertexBuffer(…)
  virtual void RHICmdSetIndexBuffer(…)
  virtual void RHICmdSetViewport(…)
  virtual void RHICmdDrawIndexed(…)
  virtual void RHICmdDraw(…)
  virtual void RHICmdTransition(RHIResource*, StateBefore, StateAfter)
  virtual void RHICmdCopyTexture(…) / RHICmdUpdateBuffer(…)

  // 呈现 / 同步（F03 扩展）
  virtual void RHIWaitForIdle()  // 可选

  virtual RHICapabilities GetCapabilities() const
};

// 访问点（择一或组合，Implementation 定稿）
RHI* GetRHI();                    // 类似 GDynamicRHI
// RenderSystem::Get().GetRHI()
```

**说明：**

- **`RHICreate*`**：创建 GPU 对象，可在 **CommandList** 上包装暴露，也可 **`RHICreate*` 自由函数** 直调 `GetRHI()`（资源加载、SceneRenderTarget 初始化）。
- **`RHICmd*`**：录制/执行 GPU 命令；**只有后端子类实现**，CommandList 不 override。

### B.2.2 `RHICommandList` — concrete 转发层（**非 virtual 后端**）

```text
class RHICommandList {
  RHI* m_RHI;   // 构造注入，或 Immediate 单例持 RHI*

public:
  // —— 命令（inline 转发 RHICmd*）——
  void BeginRenderPass(const RHIRenderPassInfo& d) { m_RHI->RHICmdBeginRenderPass(d); }
  void EndRenderPass()                             { m_RHI->RHICmdEndRenderPass(); }
  void SetGraphicsPipelineState(RHIGraphicsPipelineState* pso) { m_RHI->RHICmdSetGraphicsPipelineState(pso); }
  void SetBindingSet(uint32 i, RHIBindingSet* s)   { m_RHI->RHICmdSetBindingSet(i, s); }
  void DrawIndexed(…)                              { m_RHI->RHICmdDrawIndexed(…); }
  void Draw(…)                                     { m_RHI->RHICmdDraw(…); }
  void Transition(…)                               { m_RHI->RHICmdTransition(…); }
  // …

  // —— 创建（inline 转发 RHICreate*，UE 同款便利 API）——
  RHITextureRef CreateTexture2D(…) { return m_RHI->RHICreateTexture2D(…); }
  RHIBufferRef  CreateBuffer(…)    { return m_RHI->RHICreateBuffer(…); }
  // …

  RHI* GetExecutingRHI() const { return m_RHI; }
};
```

**Immediate 变体（F02 默认）：** `RHICommandListImmediate` 或 typedef，构造时绑定当前帧 `RHI*`；`Execute()` 在 Immediate 下可为 no-op（命令已执行）。

**Pass 签名建议（迁移目标）：**

```cpp
void ShadowPass::Execute(RHICommandList& cmdList);
// 内部：cmdList.BeginRenderPass(...); cmdList.DrawIndexed(...);
```

帧入口（如 `RenderPipeline::Execute`）从 `RenderSystem` 取得 `RHICommandListImmediate`，传入各 Pass。

### B.2.3 Resources & Views（公共类型，非 backend 子类）

```text
RHIBuffer / RHITexture          — 统一资源（壳已声明；旧 RHITexture2D/Cube/Array 过渡）
RHIBufferView / RHITextureView  — SRV / RTV / DSV（待增）
RHIGraphicsPipelineState        — PSO handle（后端可继承）
RHIGraphicsPSOCreateInfo        — PSO 配置（≈ UE FGraphicsPipelineStateInitializer）
RHIGraphicsPSODescFallback      — 无原生 PSO 的后端 handle（≈ UE FRHIGraphicsPipelineStateFallBack）
RHIBindingLayout · RHIBindingSet · RHIShader
RHIVertexInputLayout            — 顶点布局（由 VertexDefinition 演进）
```

**删除/替换（公共 API）：**

- `RHITexture2D::GetID()` + `Bind(int unit)` → View + BindingSet
- `FrameBuffer` 作为 Renderer 中心概念 → **RenderPass + 附件纹理 View**

### B.2.4 Render Pass — `RHIRenderPassInfo`

（代码：`RHI/RHIRenderPass.h`）

```text
class RHIRenderPassInfo {
  // Color attachments: RHITexture* + ERenderTargetActions (Load/Store) — 对齐 UE FRHIRenderPassInfo
  // DepthStencil attachment + action + clear values
  // Extent / viewport 相关（或单独 RHICmdSetViewport）
};

cmdList.BeginRenderPass(info);
cmdList.EndRenderPass();
```

`SceneRenderTarget` 保留为 **Renderer 辅助类**，内部创建 `RHITexture` + View，组装 `RHIRenderPassInfo` 供 Pass 使用。

### B.2.5 Pipeline State — CreateInfo + Handle

（代码：`RHI/RHIGraphicsPipelineState.h`）

```text
class RHIGraphicsPSOCreateInfo {
  // BoundShaderState / RHIShader refs
  // Blend / Rasterizer / DepthStencil（可先 void* 或独立 RHIBlendState 壳，对齐 UE 分对象）
  // Render target formats + load/store（与 RenderPass / PSO 兼容）
  // Vertex input（VertexDefinition → RHIVertexInputLayout）
};

class RHIGraphicsPipelineState { /* handle；后端子类承载 native PSO */ };

class RHIGraphicsPSODescFallback : public RHIGraphicsPipelineState {
  // OpenGL 路径：存 RHIGraphicsPSOCreateInfo，绑定时拆 program + 固定功能状态
  // ≈ UE FRHIGraphicsPipelineStateFallBack
};

// 创建
RHIGraphicsPipelineState* pso = GetRHI()->RHICreateGraphicsPipelineState(createInfo);
cmdList.SetGraphicsPipelineState(pso);
```

**UE 对照：** `FGraphicsPipelineStateInitializer` → `RHIGraphicsPSOCreateInfo`；`FRHIGraphicsPipelineState` → `RHIGraphicsPipelineState`；`FD3D12GraphicsPipelineState` / `OpenGLGraphicsPipelineState` → 后端 private 子类（F03 前仅 OpenGL）。

### B.2.6 Binding

```text
RHIBindingLayout — 静态 slot 表
RHIBindingSet    — 具体资源

cmdList.SetBindingSet(setIndex, set);
```

**引擎固定 layout（初版）：**

| Set / Slot | 内容 | 备注 |
|------------|------|------|
| 0 | `PerFrameData` UBO | 对齐现有 binding 0 |
| 1 | `LightsData` UBO | binding 1 |
| 8–10 | Shadow maps + 相关 UBO | 对齐 Shadow/Base |
| Material | 动态 slot 区 | R3+；先 dual-path |

### B.2.7 能力查询

```text
GetRHI()->GetCapabilities()
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
| `GetID()` for ImGui | GL 泄漏 | `RHITextureView::GetNativeHandle()` 仅 UI 层 |

**主要改动文件：**

- `RHI/RHI.h` — 追加 `RHICreate*` / `RHICmd*`；Legacy API 过渡后删除
- `RHI/RHICommandList.h` — 转发
- `RHI/RHIRenderPass.h` · `RHIGraphicsPipelineState.h` · `RHITexture.h` · `RHIBuffers.h`
- `OpenGL/OpenGLRHI.cpp` — 实现 Create/Cmd
- `RenderPasses/*`、`RenderPassBase.cpp` — 去 GL
- `SceneRenderTarget.cpp` — 产出 `RHIRenderPassInfo`
- `Material.cpp` — **晚于** S4（Binding 迁移）

## B.7 代码壳现状（2026-06-01，与仓库对齐）

维护者已在 `render` 分支添加空类型；设计案 **以本节 + UE 源码** 为演进真源。

| 文件 | 已有 | 待填 / 待接 |
|------|------|-------------|
| `RHI.h` | Legacy `Enable*`、`CreateVertexBuffer`… + TODO | `RHICreate*`、`RHICmd*`；Legacy 标记删除 |
| `RHICommandList.h` | 空类（非 virtual，符合 UE 转发） | `m_RHI`、Begin/EndRenderPass、SetGraphicsPipelineState、Draw* |
| `RHIRenderPass.h` | 空 `RHIRenderPassInfo` | Color/Depth 附件、Load/Store、Clear |
| `RHIGraphicsPipelineState.h` | `RHIGraphicsPipelineState`、`RHIGraphicsPSOCreateInfo`、`RHIGraphicsPSODescFallback` | CreateInfo 字段；Fallback 存 CreateInfo；GL 原生 PSO 子类 |
| `RHITexture.h` | 空 `RHITexture`；Legacy `RHITexture2D/Cube/Array` + `Bind` | `RHITextureCreateDesc` 扩展；View 工厂；逐步迁调用方 |
| `RHIBuffers.h` | 空 `RHIBuffer`；Legacy `VertexBuffer`/`FrameBuffer`/… | 统一 buffer；FrameBuffer → RenderPass |
| `RHIShader.h` | Legacy `Use`/`UploadUniform*` + TODO | 长期进 PSO + Binding；短期保留 |

**演进策略：** Legacy 与 Modern **并行**直到 Present/Shadow 走通；不在壳阶段删旧 API。

## B.4 OpenGL 适配策略（backend 内部）

| GPU 概念 | OpenGL 实现要点 |
|----------|-----------------|
| CommandList Immediate | `RHICommandList` 转发 → `OpenGLRHI::RHICmd*` → `gl*` |
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

## C.1 GPU 模型 ↔ 各 API

| GPU 模型（normative） | Vulkan | D3D12 | minEngine 目标 |
|----------------------|--------|-------|----------------|
| Dynamic RHI | `VkDevice` + 队列 | `ID3D12Device` + queue | **`RHI`** |
| 命令录制 | `VkCommandBuffer` | `ID3D12GraphicsCommandList` | **`RHICommandList`** → `RHICmd*` |
| Resource | `VkImage` / `VkBuffer` | `ID3D12Resource` | `RHITexture` / `RHIBuffer` |
| View | `VkImageView` | descriptor view | `RHITextureView` 等 |
| Render Pass | `VkRenderPass` / dynamic | RTV/DSV + clear | **`RHIRenderPassInfo`** |
| PSO | `VkPipeline` | `ID3D12PipelineState` | **`RHIGraphicsPipelineState`** + **`RHIGraphicsPSOCreateInfo`** |
| Binding | `VkDescriptorSet` | root signature | `RHIBindingSet` |
| Barrier | `vkCmdPipelineBarrier` | `ResourceBarrier` | `RHICmdTransition` |

## C.2 UE RHI ↔ minEngine（学习用）

| UE | 角色 | minEngine F02 |
|----|------|----------------|
| `FDynamicRHI` / `GDynamicRHI` | **唯一** per-API 虚表 | **`RHI` / `GetRHI()`** |
| `FRHICommandList` / `Immediate` | Pass 用的录制 API；**转发** Dynamic RHI | **`RHICommandList`**（concrete，非 per-API 子类） |
| `FRHICommandList::Create*` | inline 调 `GDynamicRHI->Create*` | `CmdList.Create*` → `RHI->RHICreate*` |
| `DrawIndexedPrimitive` 等 | 进入 RHI 命令路径 | `CmdList.DrawIndexed` → `RHICmdDrawIndexed` |
| `RHICreateTexture2D` 自由函数 | 无 CmdList 上下文时创建 | 可选同名 free function → `GetRHI()` |
| `FRHITexture` / `FRHI*View` | 资源 + 视图 | `RHITexture` + View factory |
| `FRHIGraphicsPipelineState` | PSO handle | **`RHIGraphicsPipelineState`** |
| `FGraphicsPipelineStateInitializer` | PSO 配置 | **`RHIGraphicsPSOCreateInfo`** |
| `FRHIGraphicsPipelineStateFallBack` | 无原生 PSO | **`RHIGraphicsPSODescFallback`** |
| `FRHIRenderPassInfo` | Pass 附件描述 | **`RHIRenderPassInfo`** |
| `FRHIShader` + Shader Parameters | Binding | `RHIBindingLayout` / `Set`（S5） |

**刻意不照搬（规模）：** Parallel `FRHICommandList`、RDG、`FMeshPassProcessor` 全量、RHI 资源池与 deferred deletion 的完整 UE 实现。F02 用 Immediate + `shared_ptr`/简单 Ref 即可。

学习资源： [NVRHI Programming Guide](https://github.com/NVIDIA-RTX/NVRHI/blob/main/doc/ProgrammingGuide.md)；[UE Mesh Drawing Pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/mesh-drawing-pipeline-in-unreal-engine)。

## C.3 本地 UE 源码导读（实现时按需读）

路径根：`D:\Dev\GitRepo\UnrealEngine\Engine\Source\Runtime\RHI\`

| 主题 | UE 文件 | 读什么 |
|------|---------|--------|
| Dynamic RHI | `Public/DynamicRHI.h` | `FDynamicRHI`、`RHICreateGraphicsPipelineState`、`GDynamicRHI` |
| CommandList 转发 | `Public/RHICommandList.h` | `CreateTexture` → `GDynamicRHI`；`BeginRenderPass`；`SetGraphicsPipelineState` |
| PSO Initializer | `Public/RHIResources.h` ~4504 | `FGraphicsPipelineStateInitializer` 字段 |
| PSO handle | `Public/RHIResources.h` ~1088 | `FRHIGraphicsPipelineState`；~5047 `FallBack` |
| RenderPass | `Public/RHIResources.h` ~5198 | `FRHIRenderPassInfo`、`ERenderTargetActions` |
| D3D12 创建 PSO | `../D3D12RHI/Private/D3D12State.cpp` ~712 | Cache + `RHICreateGraphicsPipelineState` |
| PipelineStateCache | `Private/PipelineStateCache.cpp` | `FGraphicsPipelineState` 包装（F02 可后做简化 cache） |

**读法：** 先 **Initializer / RenderPassInfo 结构**，再 **CommandList 如何转发**，最后 **某后端 RHICreate* 实现**。不必先读 RDG（`RenderCore`）。

---

# §6 实现切片（F02）

> **顺序原则：** 先 **类型 + CreateInfo 字段 + RHICreate/RHICmd 声明**，再 **OpenGL 实现 Present/Shadow 路径**，最后 **CommandList 转发 + Pass 迁移**。避免「空壳 Draw 抽离」后再改 Pass/PSO。

| 切片 | 内容 | UE 锚点 | 验收 |
|------|------|---------|------|
| **S0** | 壳（**Done**）：`RHIRenderPassInfo`、`RHIGraphicsPSOCreateInfo`、`RHIGraphicsPipelineState`、`RHITexture`/`RHIBuffer` 空类 | — | 编译通过 |
| **S1** | 填 **`RHIRenderPassInfo`**（附件 + Load/Store + clear）；填 **`RHIGraphicsPSOCreateInfo`**（shader + depth/blend + RT formats）；`ERenderTargetLoadAction` 等枚举 | `FRHIRenderPassInfo`、`FGraphicsPipelineStateInitializer` | 编译；单元/静态断言可选 |
| **S2** | **`RHI` 追加** `RHICreateGraphicsPipelineState`、`RHICreateTexture`…与 `RHICmdBeginRenderPass`、`RHICmdSetGraphicsPipelineState`、`RHICmdDrawIndexed`（Legacy API **保留**） | `DynamicRHI.h` | 编译 |
| **S3** | **`OpenGLRHI`** 实现 S2：`RHIGraphicsPSODescFallback` + 可选 `OpenGLGraphicsPipelineState`；RenderPass → FBO/Clear；Present/Shadow 专用 PSO | `FRHIGraphicsPipelineStateFallBack`、GL 路径 | `verify.ps1`；可用 **临时** 测试入口 draw 一次 |
| **S4** | **`RHICommandList`** 转发 S2；`RenderPipeline` 注入 CmdList；迁 **PresentPass → ShadowPass**；去 Pass 内 `gl*` | `RHICommandList.h` | Editor 主视口 + 阴影目视无回归 |
| **S5** | **`RHIBindingLayout`/`Set`**（引擎固定 shader）；`GetNativeHandle`（ImGui）；`SceneRenderTarget` 产出 `RHIRenderPassInfo` | Shader Parameters | Inspector 纹理仍可用 |
| **S5+** | 材质 `BindForDraw` 迁 Binding；删 Legacy `Enable*`/`Bind(unit)` 公共 API | — | material-ir-test |

**F03（Vulkan）** 不在此表；契约稳定后以 S3 同级方式实现 `VulkanRHI::RHICreate*` / `RHICmd*`。

### §6.1 推荐推进节奏（你 + Agent）

| 周/阶段 | 你 | Agent / 协作 |
|---------|-----|----------------|
| 当前 | 继续填 S1 字段、命名微调 | 对齐设计、Review 与 UE diff |
| S2–S3 | 实现 OpenGL Create/Cmd（参考 UE + 现有 Shadow/Present） | 接线 review、`verify` |
| S4 | Pass 签名改为 `Execute(RHICommandList&)` | 协助去 `gl*`、SceneRenderTarget |
| S5+ | Binding / 材质 | 分切片，不一次迁完 |

---

# §7 验收标准（F02 Done）

- [ ] Part A 概念在代码中均有对应类型（不要求注释教学，但接口齐全）
- [ ] `RenderPasses/` 无 `gl*`、无 `#include glad`
- [ ] OpenGL 单后端：Editor 主场景、阴影、Present 与迁移前一致（维护者目视 + `verify.ps1`）
- [ ] `ACTIVE_WORK` 边界内无 RenderGraph、无 Vulkan 强依赖
- [ ] Design §6 切片 S0–S5 勾选完成（S5+ 材质可选单列 Done）

---

## §8 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 材质 binding 迁期过长 | dual-path 混乱 | S0–S4 不动 Material IR；S5 只迁引擎 shader |
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
| 2026-06-01 | 对齐代码壳（RHIRenderPassInfo、RHIGraphicsPSOCreateInfo、Fallback）；§6 改为 S0–S5（PSO/Pass 优先）；增 §B.7、§C.3 UE 导读 |
| 2026-06-02 | 细化：UE 式 `RHI` + `RHICommandList` 转发；去掉 `IRHI*`/`RHIDevice`；增 C.2 UE 对照 |
| 2026-06-01 | 初稿：Part A 教案 + Part B 契约/迁移/切片；`render` 分支 |
