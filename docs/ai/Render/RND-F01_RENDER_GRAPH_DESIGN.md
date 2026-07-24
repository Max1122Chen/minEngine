# RND-F01 — RenderGraph（帧图编排与 Pass 契约）

## Meta

| Field | Value |
|-------|--------|
| **Feature ID** | `RND-F01` |
| **Type** | Refactor + Architecture |
| **Status** | **Draft**（S0–S04 Done；**S05+ 暂停至 F06 Done**） |
| **Owner** | (maintainer) |
| **Last updated** | 2026-07-24 |
| **Depends on** | `RND-F02` **Done**；`RND-F04` **Done**；**S05+ 另依赖 `RND-F06` Done**（Renderer / Graph 职责分离） |
| **Blocks** | 图机制终态（Bake 等）；减轻 F05 Vulkan 迁移成本 |
| **Related** | [RND-F06](./RND-F06_FORWARD_RENDERER_DESIGN.md) · [RND-F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · Granite `renderer/render_graph.hpp` · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) |

## TL;DR

**问题：** F04 统一了 draw（`MeshDrawPacket`），但帧级编排仍是 OpenGL 式：手写顺序、资源读写隐式、无 Transition 叙事；现有 `RenderPasses/*` 与 RDG 的 **Pass** 不是同一概念。

**方案：** 引入 **RenderGraph**；图上的步骤叫 **`RenderPass`**（对齐 UE / Granite，**不必** `RGPass`）。**S01 Manual RenderGraph** = 手写 `PassExecutionOrder` + 统一 **Setup / PreparePass / BuildRenderPass** 纪律。

**命名：** 图步骤 = **`RenderPass`**；`RenderPasses/*` 实现类 ≈ Granite **`RenderPassInterface`**（P2）；逻辑纹理用 **字符串名**（P1，对齐 Granite `RenderTextureResource::name`）。Binding 词汇统一见 **§13 S0**。

**进度口径（2026-07-24）：** S0–S04 **Done**。复盘后发现图已挂主路径，但构图宿主仍是混杂的 `RenderPipeline`。**先完成 [RND-F06](./RND-F06_FORWARD_RENDERER_DESIGN.md)**（`ForwardRenderer` + 删除 Pipeline），再继续本 Feature：**S05 卫生 → S06 Bake → … → S08 调图形态**（接力真源见 F06 §1.3）。**不要在 F06 完成前实现 Bake。**

---

## Scope

### In

- RenderGraph 核心类型与 Pass 生命周期（§3、§10）
- **S0** RHI Binding 词汇统一（§13；与 S01 可并行，建议先定稿）
- Manual RenderGraph（S01–S02 详细设计）
- `RenderPasses/*` → `IRenderPass` 实现迁移路线（§3.1、§12）
- Shadow 拆为多 `RenderPass` 实例（S04 方向）

### Out（分阶段）

| 阶段 | Out |
|------|-----|
| S01–S02 | `Bake()`、自动拓扑、transient 别名、多队列 |
| S05+ | 全量 barrier 自动插入 |
| F05 | Vulkan 后端 |

---

## Reader quick start

1. **§3.1** — Pass 命名契约（必读）
2. **§13** — **S0 Binding 词汇统一**（F04 后首做）
3. **§10** — S01 Manual RenderGraph 详细设计
4. **§11** — S02 Post 链样板
5. **§6** — 全切片表

---

## 1) 背景与目标

F02/F04 已交付 RHI 词汇与完整 draw packet。F01 交付 **帧图编排**：谁读谁写、RHI RenderPass 边界在哪、Setup 与 GPU 录制如何分离。

**北极星：**

> `RenderGraph` 描述一帧依赖；每个 **`RenderPass`** 是图上的一个步骤；`BuildRenderPass` ≈ 今天 Legacy Pass `Execute` 里「只录 GPU 命令」的部分。

单线程 + 立即 `RHICommandList` 可长期保留。

---

## 2) 现状 Gap（摘要）

```text
RenderPipeline::Execute
├── CPU Setup 散落（队列、UBO、SceneBindings、指针注入）
├── Legacy ShadowPass.Execute     ← 自管 N× RHICmdBeginRenderPass
├── RHICmdBeginRenderPass(Scene)  ← 边界在 Pipeline
│     Legacy Sky / Base / Translucent / Post（方言不一）
├── RHICmdEndRenderPass
└── Legacy PresentPass.Execute
```

| RenderGraph 概念 | 现状 | Gap |
|------------------|------|-----|
| 统一 `RenderPass` 契约 | `RenderPassBase::Execute()` 仅虚函数名 | 高 |
| 逻辑资源 + 读写边 | `m_SceneColorTexture` 注入 | 高 |
| RHIRenderPass 边界 per Pass | Scene 大包 Post | 高 |
| `RHICmdTransition` 叙事 | 未使用 | 高 |
| Setup / BuildRenderPass 分离 | 混在 Execute | 中 |

**Legacy `RenderPassBase`：** mesh 工具可保留；**不是** 图 Pass 抽象根。

---

## 3) 命名契约

### 3.1 两套「Pass」—— 维护者拍板（D5、P2）

| 名称 | 含义 | 代码位置（今天 → 目标） |
|------|------|-------------------------|
| **`RenderPass`**（图） | RenderGraph 上的一个步骤；`AddPass` 的产物 | 新建 `Render/RenderGraph/` |
| **Pass 实现类** | 今天 `RenderPasses/*`：`BasePass`、`ShadowPass`… | ≈ Granite **`RenderPassInterface`**；实现 **`IRenderPass`**，**不** 与图 `RenderPass` 同名 |

**RDG 里的 Pass 直接叫 `RenderPass`，不必 `RGPass`。** 与实现类并存时，文档与 code review 用 **「图 Pass」vs「Pass 实现」** 区分；`RenderPasses/*` 目录 **短期保留**，类逐步 `public IRenderPass`，mesh 工具从 `RenderPassBase` 抽到共享 helper（S03）。

**Granite 对照：** `graph.add_pass("Post.FXAA", std::make_unique<FxaaPass>())` — 图槽位 + 实现对象分离；minEngine 同型。

**RHI 层：** 保留 `RHIRenderPassInfo` / `RHICmdBeginRenderPass` — 指 **GPU render pass 作用域**，与图 `RenderPass` 不同层；图 Pass 的 `BuildRenderPass` 内可调用 RHI Begin/End。

### 3.2 minEngine ↔ Granite ↔ UE 术语表

| minEngine（定稿） | Granite | UE RDG | 说明 |
|-------------------|---------|--------|------|
| `RenderGraph` | `RenderGraph` | `FRDGBuilder`（构图+执行） | 帧图根对象 |
| `RenderGraph::AddPass` | `add_pass` | `AddPass` | 注册图 Pass |
| `RenderPass` | `RenderPass` | Pass（AddPass 产物） | **不叫 Node** |
| `RenderPass::Setup` | `add_*_input/output` 于构图期 | Setup lambda | 声明读写、attachment 模板 |
| `RenderPass::SetupDependencies` | `setup_dependencies` | — | bake 前跨 Pass 接线 |
| `RenderPass::SetupPermanentResources` | `setup(device)` | — | bake 后一次性（可选） |
| `RenderPass::PreparePass` | `enqueue_prepare_render_pass` | Pass 参数填充 | 每帧 CPU，无 RHICmd |
| `RenderPass::BuildRenderPass` | `set_build_render_pass` / `build_render_pass` | Execute lambda | 每帧 GPU 录制 |
| `RenderGraph::SetupAttachments` | `setup_attachments` | — | 每帧物理资源绑定 |
| `RenderGraph::ExecuteGraph` | `enqueue_render_passes` | `Execute()` | 跑完整图 |
| `RenderGraph::Bake` | `bake` | 编译阶段 | **S06**（F06 后）；S01–S04 无 |
| `RDGTexture` / `RDGTextureRef` | `RenderTextureResource` | `FRDGTexture` | **逻辑**纹理 |
| `RDGBuffer` / `RDGBufferRef` | `RenderBufferResource` | `FRDGBuffer` | **逻辑**缓冲 |
| 逻辑资源名 `const char*` / `string_view` | `RenderTextureResource::name` | `FRDGTexture` 注册名 | **P1：字符串 id**，非 enum；便于演化真 RDG |
| `RenderGraphFrameResources` | `RenderGraph` 内资源表 | 外部/注册纹理 | 每帧句柄表 |
| `PassParameters` | Pass 捕获状态 | `*PassParameters` struct | 单 Pass 每帧输入 |
| `AddColorOutput` | `add_color_output` | RT 输出 | Setup 期 |
| `AddTextureInput` | `add_texture_input` | `UseTexture(Read)` | Setup 期 |
| `SetDepthStencilOutput` | `set_depth_stencil_output` | depth attachment | Setup 期 |
| `AddTransition` | `build_barriers`（自动）+ `cmd.barrier` | `AddTransition` | S01 显式调用 |
| `PassExecutionOrder` | 手写顺序 | — | Manual 阶段 |
| `IRenderPass` | `RenderPassInterface` | 复杂 Pass 类 | **P3：定稿**；`RenderPasses/*` 长期实现此接口 |

---

## 4) 目标架构（概念）

### 4.1 核心类型

```text
RenderGraph
  AddPass(name) → RenderPass&
  RegisterExternalTexture("SceneColor", RHITexture*)   // 字符串逻辑名
  Bake()                    // S06（F06 闸门之后）
  SetupAttachments()        // 每帧
  ExecuteGraph(cmdList, RenderGraphFrameResources&)

RenderPass
  Setup(RenderPassBuilder&)           // 构图：AddColorOutput / AddTextureInput …
  PreparePass(RenderGraphFrameResources&)   // 每帧 CPU
  BuildRenderPass(RHICommandList&, const PassParameters&)  // 每帧 GPU

RenderGraphFrameResources
  RDGTextureRef Get(const char* name)   // 或 Get(RDGTextureRef)
  RHITexture* GetRHI(const char* name)
  PassParameters& GetPassParameters(RenderPass&)
```

### 4.2 Pass 三阶段（纪律）

```text
Setup         — 图构建 / resize：声明 IO；禁止 RHICmd*
PreparePass   — 每帧 CPU：packet、ShaderBindingSet、UpdateSubresource；禁止 RHICmd*
BuildRenderPass — 每帧 GPU：AddTransition → RHICmdBeginRenderPass → Submit* → End
```

### 4.3 Manual vs Baked RenderGraph

| | Manual（S01–S04） | Baked（**S06+**，F06 后） |
|--|-------------------|---------------------------|
| 顺序 | `PassExecutionOrder[]` | `Bake()` 拓扑序 |
| Transition | Pass 内显式 `AddTransition` | 可自动插入 |
| 别名 | 物理纹理常驻 / 显式 ping-pong | transient 池 |

---

## 5) Shadow（方向，S04）

单体 Legacy `ShadowPass` 拆为多个图 **`RenderPass` 实例**（同实现类、不同 `AddPass("Shadow.Dir.C0")` 名）：

| 光型 | 粒度 | Setup 写 |
|------|------|----------|
| Directional | 每 cascade 一 Pass | `SetDepthStencilOutput(DirShadow[slice])` |
| Spot | 每盏灯一 Pass | `SetDepthStencilOutput(SpotShadow[i])` |
| Point | 每盏灯一 Pass | `SetDepthStencilOutput(PointShadow[i])` |

Scene Pass：`AddTextureInput(DirShadowAtlas)`。Legacy `ShadowPass::Render` switch 删除。

---

## 6) 执行切片

| Slice | 名称 | 验收 |
|-------|------|------|
| S00 | Design | 本文 §1–§9、§12–§13 |
| **S0** | **Binding 词汇统一** | §13；**Done**（`RHIShaderBinding*` / `RHIShaderBindingSetLayoutEntry`） |
| **S01** | **Manual RG 骨架** | §10；**Done** |
| **S02** | **Post 链样板** | §11；**Done** |
| **S03** | **全主帧 Pass 化** | verify + 黄金场景；**Done** |
| **S04** | **Shadow Pass 化** | 阴影 + 图边；**Done** |
| — | **（闸门）RND-F06 Done** | `ForwardRenderer` 取代 `RenderPipeline`；见 [F06](./RND-F06_FORWARD_RENDERER_DESIGN.md) |
| **S05** | **RDG 实现卫生** | 删空壳 TU、改名实不符、收敛过碎文件；**不**扩 Bake/transient；向 Granite 式紧凑靠拢 |
| **S06** | **Bake** | 依赖边 → 执行序；非法图失败（原「S05 Bake」口径迁此） |
| S07 | Transient（可选） | Deferred |
| **S08** | **Renderer 调图形态** | F06 之后、机制可用后：整理 `ForwardRenderer` 对 Graph 的装配 API（非再塞策略进 Graph） |

> **历史备注：** 2026-06 文档曾写「下一步 S05 Bake」。2026-07-24 起 Bake 改为 **S06**，并插入 **S05 卫生** 与 **F06 闸门**，避免在上帝对象上堆机制。

---

## 7) 风险

| 风险 | 缓解 |
|------|------|
| `RenderPass` 与 Legacy 类名混淆 | §3.1 强制区分；新代码在 `RenderGraph/` |
| S01 范围膨胀 | S01 **不接** Legacy 主路径，只骨架 + 可选 stub |
| 与 F03 并行 | Shadow 放 S04 |
| 未拆 Renderer 就 Bake | **F06 闸门**；ACTIVE_WORK 主线先 F06 |
| Manual 骨架过碎 / 空文件 | **S05 卫生** 专责收敛；不与 Bake 混做 |

---

## 8) Feature Done（草案）

**S0–S04 已满足（部分）：**

- [x] 主帧经 `RenderGraph::ExecuteGraph`（宿主曾为 `RenderPipeline`；F06 后改为 `ForwardRenderer`）
- [x] Post ping-pong + `AddTransition`（Manual）
- [x] Shadow 为多 `RenderPass` 实例

**仍待（F06 之后）：**

- [ ] RDG 实现卫生（S05）：无空壳 TU；helper 名实相符；未用占位不占目录噪音
- [ ] Bake（S06）+ 非法图失败
- [ ] 每图 Pass 可列出 Setup 声明的 IO（可审计）
- [ ] `RenderPassBase` 不再作为统一基类（可与 F06/S08 一并收）
- [ ] Renderer 调图形态稳定（S08）
- [ ] `verify.ps1` + 黄金场景（各切片回归）

---

## 9) 已拍板

| ID | 决定 |
|----|------|
| D1 | 术语 **RenderGraph** |
| D2 | **Manual RenderGraph** = 正确第一步 |
| D3 | 图 Pass 契约取代 `RenderPassBase::Execute` |
| D4 | Shadow 拆 per-map / per-cascade **RenderPass** |
| D5 | 图 Pass 类型名 **`RenderPass`**（非 RGPass）；与 Legacy Pipeline Pass 不同物 |
| D6 | GPU 录制阶段名 **`BuildRenderPass`**（对齐 Granite）；UE 读者见注释 = Execute lambda |
| D7 | 逻辑资源 **`RDGTexture` / `RDGBuffer`** |
| P1 / D8 | 逻辑纹理 **字符串名**（`"SceneColor"`），不用 `RDGTextureId` enum；对齐 Granite，便于真 RDG |
| P2 / D9 | `RenderPasses/*` = **Pass 实现类**（≈ `RenderPassInterface`）；实现 **`IRenderPass`**，目录短期保留 |
| P3 / D10 | 复杂 Pass 接口名 **`IRenderPass`** |
| D11 | RHI Binding 层统一 **ShaderBindingSet** 语境（§13）；与 `setIndex` / Vulkan descriptor 模型一致 |

---

## 10) S01 — Manual RenderGraph 详细设计

### 10.1 目标

交付 **可编译、可单测** 的 RenderGraph 骨架；**不要求** S01 结束切换 Editor 主路径（可与 S02 合并验收目视）。

S01 Done = 类型 + 执行器 + 一个 **NullPass** 或 **StubPass** 跑通 `Setup → PreparePass → BuildRenderPass` 两遍循环。

### 10.2 目录与文件（建议）

```text
Render/RenderGraph/
  RDGTexture.h              // RDGTextureRef, RDGTextureDesc（逻辑名 = string）
  RDGBuffer.h
  RenderPassBuilder.h       // AddColorOutput, AddTextureInput, …
  RenderPass.h              // 图 Pass（非 Legacy）
  IRenderPass.h             // 可选：多方法接口
  RenderGraphFrameResources.h
  PassParameters.h          // 基类 / type erasure 或 per-pass struct 容器
  RenderGraph.h / .cpp
  RenderGraphExecute.cpp    // ExecuteGraph, PassExecutionOrder
```

**不修改** `RenderPasses/*`（S01）；`RenderPipeline` 仅可加 **可选** 调试开关调用 `RenderGraph` stub。

### 10.3 逻辑纹理名（字符串，P1）

Setup / `RegisterExternal` 使用 **稳定字符串**（与 Pass 名同风格：`"Scene.Opaque"` vs `"SceneColor"` 分工：Pass 名 vs 资源名）。

```text
// 持久 / 外部（RegisterExternal）
"SceneColor"        // SceneRenderTarget color
"SceneDepth"
"Backbuffer"        // Present 目标（或空 = swapchain）

// S02 构图期 AddColorOutput
"PostBufferA"
"PostBufferB"

// S04
"DirShadowAtlas"
"SpotShadow0" … "SpotShadow1"
"PointShadow0" … "PointShadow1"
```

`RenderGraphFrameResources` 内部：`unordered_map<string, RDGTextureSlot>` 或 **frozen string pool**（构图后只读）。`RDGTextureRef` = 不透明句柄（可存 pool 下标），**Setup API 接受 `const char*`**。

**命名纪律：** 主帧槽位用 **PascalCase 字面量** 或 `constexpr const char*` 常量（如 `kRDGSceneColor = "SceneColor"`），避免 magic string 散落；**不**引入 enum id。

### 10.4 `RenderPassBuilder`（Setup 期 API）

对齐 Granite 动词；首版实现 **记录声明**，Manual 阶段 **不验证** 拓扑（S05 再验证）。

```cpp
// 概念签名（非最终实现）
class RenderPassBuilder {
public:
    RDGTextureRef AddColorOutput(const char* name, const RDGTextureDesc& desc);
    RDGTextureRef AddTextureInput(const char* name);
    RDGTextureRef SetDepthStencilOutput(const char* name, const RDGTextureDesc& desc);
    RDGTextureRef SetDepthStencilInput(const char* name);

    RDGTextureRef UseTexture(const char* name);  // 可选别名 AddTextureInput
};
```

内部：`RenderPass` 持有 `std::vector<PassResourceAccess>{ id, AccessType, Usage }`。

**AccessType（首版）：**

| 枚举 | 含义 | 后续 Transition |
|------|------|-----------------|
| `ColorOutput` | RT 颜色写 | → SRV 前需 Transition |
| `DepthStencilOutput` | depth 写 | → ShaderRead |
| `TextureInput` | shader 采样读 | 上游须已 Transition |
| `DepthStencilInput` | depth 读（可选手） | — |

### 10.5 `RenderPass` 类

两种注册方式（二选一或并存）：

**A. Lambda（简单 Pass，对齐 sandbox）**

```cpp
RenderPass& pass = graph.AddPass("Stub");
pass.SetSetup([](RenderPassBuilder& b) { ... });
pass.SetPreparePass([](RenderGraphFrameResources& fr) { ... });
pass.SetBuildRenderPass([](RHICommandList& cmd, const PassParameters& p) { ... });
```

**B. `IRenderPass`（复杂 Pass，S02+）**

```cpp
class IFXAARenderPass : public IRenderPass { ... };
graph.AddPass("Post.FXAA", std::make_unique<IFXAARenderPass>());
```

**`RenderPass` 存储：**

- `string m_Name`
- Setup / Prepare / Build 回调或 `IRenderPass*`
- `vector<PassResourceAccess> m_DeclaredAccess`（Setup 时填充）

### 10.6 `RenderGraph`

```cpp
class RenderGraph {
public:
    void Reset();  // resize / 场景重建

    RenderPass& AddPass(const char* name);
    void RegisterExternalTexture(const char* name, RHITexture* texture);

    // Manual 阶段：注册完成后由 RenderPipeline 或单测设置
    void SetPassExecutionOrder(std::span<const RenderPass* const> order);

    void SetupAttachments();  // S01：解析 external 指针，创建内部占位 RT（若已声明）
    void ExecuteGraph(RHICommandList& cmdList, RenderGraphFrameResources& frameResources);

    // S05
    // void Bake();
};
```

**`ExecuteGraph` 算法（S01）：**

```text
for (pass : PassExecutionOrder)
    pass.PreparePass(frameResources)

for (pass : PassExecutionOrder)
    pass.BuildRenderPass(cmdList, frameResources.GetPassParameters(*pass))
```

**禁止：** 在 `PreparePass` / `BuildRenderPass` 外对 Legacy 主路径做隐式 RHICmd（单测除外）。

### 10.7 `RenderGraphFrameResources`

```cpp
struct RDGTextureSlot {
    std::string Name;
    RHITexture* Texture = nullptr;       // 物理
    RHIShaderResourceViewRef SRV;        // flyweight，Prepare 时更新
    RDGTextureUsage LastKnownUsage;      // S01 手动；S05 自动
};

class RenderGraphFrameResources {
public:
    void RegisterExternal(const char* name, RHITexture* texture);
    RHITexture* GetRHI(const char* name) const;
    void SetSRV(const char* name, RHIShaderResourceViewRef srv);

    template<typename T> T& GetPassParameters(RenderPass& pass);

    // 由 RenderPipeline::BuildFrameSetup 填充（S03 接线）
    // SceneRenderContext*, queues, EngineSceneBindingSets*, …
};
```

### 10.8 `AddTransition`（S01 包装）

```cpp
// RHI 已有 RHITextureTransitionInfo
void AddTransition(
    RHICommandList& cmd,
    const char* textureName,
    RenderGraphFrameResources& fr,
    RDGTextureUsage before,
    RDGTextureUsage after);
```

S01：更新 `LastKnownUsage` + 调用 `cmd.Transition(...)`（GL no-op）。**表达意图** 为主。

**Usage 首版：** `RenderTarget` | `ShaderResource` | `DepthWrite` | `DepthRead`。

### 10.9 S01 验收

| # | 条件 |
|---|------|
| 1 | `minEngine` 编译通过；新 `.cpp` 编入 CMake |
| 2 | 单元或 dev 测试：`AddPass("Stub")` → Setup 声明 1 写 → Prepare 空 → Build 调 `RHICmdBeginRenderPass`/`End` |
| 3 | `ExecuteGraph` 按 `PassExecutionOrder` 调用两次循环 |
| 4 | 文档 §3 命名与代码一致 |
| 5 | **不破坏** 现有 `RenderPipeline::Execute` 目视（Legacy 路径不变） |

### 10.10 S01 明确不做

- `Bake()`、拓扑排序、资源别名池
- 迁移 Post / Present / Scene
- 删除 `RenderPassBase`
- 多队列 / `TaskComposer`

### 10.11 与 `RenderPipeline` 接线（S02 预览）

S01 不接线。S02 起：

```text
RenderPipeline::Execute
  BuildRenderGraphFrameResources(desc)   // 替代指针注入
  m_RenderGraph.SetupAttachments()
  m_RenderGraph.ExecuteGraph(cmdList, frameResources)   // 逐步替代中间 Legacy 段
```

过渡期：`ExecuteGraph` 只含 Post/Present Passes，前后仍调 Legacy Shadow/Scene。

---

## 11) S02 — Post 链样板（概要）

### 11.1 资源

```text
SceneColor     — RegisterExternal(SceneRenderTarget)
PostBufferA    — AddColorOutput（FXAA 写）
PostBufferB    — 可选；或 FXAA→SceneColor ping-pong
Backbuffer     — Present AddColorOutput 或空 attachment
```

### 11.2 Pass 顺序（`PassExecutionOrder`）

```text
1. Post.FXAA
     Setup: AddTextureInput(SceneColor), AddColorOutput(PostBufferA)
     PreparePass: 建 SRV(SceneColor)、Post params UBO、BindingSet、packet
     BuildRenderPass: Transition(SceneColor→SRV) → Begin(PostBufferA) → SubmitMeshDrawPacket → End

2. Post.Sharpen
     Setup: AddTextureInput(PostBufferA), AddColorOutput(SceneColor)
     PreparePass: …
     BuildRenderPass: Transition(PostBufferA→SRV) → Begin(SceneColor) → Submit → End

3. Present
     Setup: AddTextureInput(SceneColor)
     BuildRenderPass: Transition(SceneColor→SRV) → Begin(Backbuffer) → Submit → End
```

### 11.3 Legacy 拆除

- Post / Present **移出** Scene 的 `RHICmdBeginRenderPass` 块
- 删除 `m_SceneColorTexture` 指针注入
- `PostProcessPass` / `PresentPass` **实现 `IRenderPass`**（文件可暂留 `RenderPasses/`）

### 11.4 S02 验收

- 目视 FXAA + Sharpen + Present 与现等价
- grep：Post `BuildRenderPass` 内无 `CreateShaderBindingSet`（S0 后命名）
- 设计图可画出 SceneColor → PostA → SceneColor → Backbuffer 边

---

## 12) Pass 实现迁移对照（P2）

| Pass 实现类（`RenderPasses/*`） | 图 Pass 名 | 切片 | 备注 |
|--------------------------------|------------|------|------|
| `ShadowPass` | `Shadow.Dir.C*` / `Shadow.Spot.*` | S04 | 拆多实例；类可保留，实现 `IRenderPass` |
| `SkyBoxPass` | `Scene.Sky` | S03 | |
| `BasePass` | `Scene.Opaque` | S03 | |
| `TranslucencyPass` | `Scene.Translucent` | S03 | |
| `PostProcessPass` | `Post.FXAA` / `Post.Sharpen` | S02 | |
| `PresentPass` | `Present` | S02 | |

**Mesh 工具：** `PrepareMeshDrawPackets` / `SubmitSceneMeshDrawPackets` → `Scene.Opaque` 的 `PreparePass` 内调用；从 `RenderPassBase` 抽到共享 helper，**不**让实现类继承图 `RenderPass`。

---

## 13) S0 — RHI Binding 词汇统一

### 13.1 动机（F04 后小瑕疵）

F04 已打通 **PipelineLayout → ShaderBindingSet → MeshDrawPacket → `RHICmdSetShaderBindingSet`** 链条；S0 将原 **类型名**（`RHIBinding*`）与 **API 语境**（`setIndex`、`GetSetLayout`）统一为 Vulkan 对齐的 **`RHIShaderBinding*`** 词汇。

**目标：** RHI 公开类型与 `RHICommandList` / `RHI` 工厂方法使用 **同一套 ShaderBindingSet / Descriptor 语境**，与 `RHIPipelineLayout`（≈ `VkPipelineLayout`）并列可读；为 F05 Vulkan 后端减少二次翻译。

### 13.2 目标词汇（维护者意向，S0 实施前可微调措辞）

| Vulkan | 今天（F04） | S0 目标（意向） |
|--------|-------------|-----------------|
| `VkPipelineLayout` | `RHIPipelineLayout` | **保持** `RHIPipelineLayout` |
| `VkDescriptorSetLayout` | `RHIBindingLayout` | **`RHIShaderBindingSetLayout`** |
| `VkDescriptorSetLayoutBinding` | `RHIBindingLayoutEntry` | **`RHIShaderBindingSetLayoutEntry`** |
| `VkDescriptorSet` | `RHIBindingSet` | **`RHIShaderBindingSet`** |
| `VkWriteDescriptorSet` 单槽资源 | `RHIBindingResource` | **`RHIShaderBinding`**（实例侧「一个 binding 槽填什么」） |
| `VkDescriptorType` | `RHIBindingType` | **`RHIShaderBindingType`** |
| `vkCmdBindDescriptorSets` | `RHICmdSetBindingSet` / `SetBindingSet` | **`RHICmdSetShaderBindingSet`** / **`SetShaderBindingSet`** |
| `RHICreateBindingLayout` | 同名 | **`RHICreateShaderBindingSetLayout`** |
| `RHICreateBindingSet` | 同名 | **`RHICreateShaderBindingSet`** |
| `GetSetLayout(setIndex)` | 同名 | **`GetShaderBindingSetLayout(setIndex)`**（`setIndex` **保留**，即 Vulkan set #） |
| `kMaxPipelineDescriptorSets` | 同名 | **`kMaxShaderBindingSets`**（或保留旧名 + `using` 别名，二选一） |
| `MeshDrawPacket::BindingSets` | 同名 | **`ShaderBindingSets`** |

**文件：** `RHIBinding.h` → 可重命名为 **`RHIShaderBinding.h`**（或保留文件名、只改类型名 — S0 实施时选改动量小的一侧）。

**Engine 层（可选同期）：** `EngineSceneBindingSets` → `EngineSceneShaderBindingSets`；`CreateBindingSet` 调用点跟随 RHI 改名。**不**改业务语义，仅词汇。

**OpenGL 实现类：** `OpenGLRHIBindingLayout` → `OpenGLRHIShaderBindingSetLayout` 等，与 RHI 类型同步。

### 13.3 不在 S0 范围

- 改 GLSL `layout(binding=)` 编号策略
- 合并 Set0/Set1 重建策略（见 TD-013）
- Material 侧 layout 缓存结构重做
- 真 `VkDescriptorSet` 分配（F05）

### 13.4 S0 验收

| # | 条件 |
|---|------|
| 1 | `RHIBinding*` 公开类型名清零（或仅保留 deprecated typedef 一版，grep 无生产路径） |
| 2 | `RHICommandList` / `RHI` / `OpenGLRHI` / `MeshDrawPacket` / `EnginePipelineLayouts` / `EngineSceneBindingSets` 命名一致 |
| 3 | `verify.ps1` + 黄金场景目视无回归 |
| 4 | F04 设计案或本文 §13 与代码一致 |

### 13.5 与 S01 顺序

| 顺序 | 说明 |
|------|------|
| **推荐** | **先 S0 再 S01** — RenderGraph `PreparePass` 新代码直接写 `CreateShaderBindingSet` |
| 可接受 | 并行：S01 stub 不接 Binding；合并前必须 S0 Done |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-11 | 初稿 |
| 2026-06-11 | 命名契约：图 `RenderPass`；Granite/UE 对照；§10 S01 详细设计；§11 S02 概要 |
| 2026-06-11 | P1 字符串逻辑纹理；P2 Pass 实现 ≈ RenderPassInterface；P3 `IRenderPass`；§13 S0 Binding 词汇统一 |
| 2026-06-11 | **S0 Done**：`RHIBinding.h` → `RHIShaderBinding.h`；`RHIShaderBindingSetLayoutEntry` 等全量改名 |
| 2026-06-12 | **S01 Done**：`Render/RenderGraph/` 骨架 + `render-graph` smoke 测试 |
| 2026-07-24 | **口径**：S05+ 依赖 [F06](./RND-F06_FORWARD_RENDERER_DESIGN.md)；原「S05 Bake」→ **S06**；新增 **S05 卫生**、**S08 调图形态**；闸门写进 §6–§8 |
