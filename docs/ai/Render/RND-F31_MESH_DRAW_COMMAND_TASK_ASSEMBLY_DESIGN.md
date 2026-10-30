# RND-F31 — MeshDrawCommand 组装的 Task 并行化

> 编号：`RND-F31`（F28 = Texture2DRenderTarget、F29 = Orthographic Camera、F30 = DebugDraw 索引线均已占用）。
> 正文统一用「本 Feature」自称。

## Meta
- **Type:** Feature（refactor + 并行化）
- **Complexity:** L2
- **Status:** Design / 待评审（未开工）
- **Owner:** project maintainer
- **Last updated:** 2026-10-30
- **Worktree / branch:** `D:/Dev/GitRepo/minEngine-core` · `feat/render`
- **Related（前置 · 只读）**
  - 帧角色三约束：[CORE-F47](../Platform/Core/CORE-F47_FRAME_THREAD_CONTRACTS_DESIGN.md)
  - 粗筛：[RND-F19](./RND-F19_CPU_MESH_VISIBILITY_DESIGN.md) · 子 mesh 剔除钩子：[RND-F26](./RND-F26_VIEW_AWARE_MESH_COLLECT_DESIGN.md)
  - Batch→MDC→Packet 契约：[RND-F18](./RND-F18_MESH_BATCH_PASS_MDC_REFACTOR_DESIGN.md)
  - 排序：[RND-F20](./RND-F20_MESH_DRAW_SORTING_DESIGN.md)（本文档不改排序语义，只保住它）
  - Task 系统：`Runtime/Core/Tasks/TaskSystem.h`（`ParallelFor` 只允许 Game/Render 编排）

---

## 1. 背景与目标

### 1.1 背景

今天每帧的 mesh 组装全部发生在**渲染角色单线程**上，且其中一大段（逐 primitive 的剔除 +
`GetMeshBatches` + `MeshPassProcessor` 出 `MeshDrawCommand`）是**纯 CPU、只读输入**的工作。
`ForwardRenderer::Execute` 与 `ManualRenderer::Execute` 各走一遍，多 viewport 时一遍不落。

游戏侧已经只负责把命令塞进 `RenderCommandPipe`（`Engine.cpp:449-460`）；渲染侧除了真正的 GPU
记录之外，还剩一整段可以摊到 worker 上的活。这段活是「渲染角色单线程」这条链上唯一还没被
缩短的部分。

### 1.2 目标

| # | 目标 | 判定 |
|---|------|------|
| G1 | 把**每帧 mesh 组装**中纯 CPU 的部分交给 `TaskSystem` 并行完成 | ≥1000 primitive 场景下装配耗时下降，且结果与串行**逐字节一致** |
| G2 | 守住 CORE-F47 三条硬约束（渲染侧不持游戏对象 / 代理两段式删除 / GPU 创建只在图形角色） | 不改这三条的任何语义；新增断言可证明没破 |
| G3 | 降级路径**永不静默**：任务系统不可用 / 单线程拓扑 / 角色未启动 / 调用线程非法时，必须走串行且产出完整队列 | 断言 + 计数 + 红灯 |
| G4 | 为「minCraft 地图生成 / chunk 网格构建也走 Task」打下**同一套**「只读输入 → 分片 → 确定性汇聚」骨架 | 骨架中与渲染无关的部分（分片、汇聚、降级、观测）是通用件，可复用 |
| G5 | 不引入第二个路径层 | 串行入口 = 分片入口的 `ChunkCount == 1` 特例，同一份实现 |

### 1.3 非目标

见 §13。最重要的一条：**不做** UE 式的 `FParallelCommandList`（worker 上录制 RHI 命令流）。
次重要的一条：**不涉及消费者侧** —— minCraft 的 chunk 网格构建 / 地图生成并行化不在本 Feature 范围内；
§14 只说明本 Feature 立下的契约为什么对它们同样成立，不产出任何消费者侧改动。

---

## 2. 现状与约束

### 2.1 每帧调用链（渲染角色，Multi 拓扑）

`Engine::TickRendererFrame`（`Runtime/Engine.cpp:463`）：

| 顺序 | 动作 | 位置 | 性质 |
|---|---|---|---|
| 1 | `RHIDrainDeferredCreates()` | `Engine.cpp:480` | **GPU**，必须串行 |
| 2 | `pipe.FlushApplyForFrameChecked(completedFrame)` | `Engine.cpp:516` | 应用场景命令，**改绘制列表**，必须串行且必须在装配之前 |
| 3 | `pipe.TakeViewIntents` → `ReplacePendingDraws` | `Engine.cpp:530-533` | 串行 |
| 4 | `RendererTick` → `RenderSystem::Tick` | `Engine.cpp:540` / `RenderSystem.cpp:221` | 逐 view 调 `Execute` |
| 5 | 逐 view `m_SceneRenderer->Execute(drawDesc)` | `RenderSystem.cpp:229-232` | 串行（每 view 一套记录窗口） |
| 6 | `RenderScene::FlushPendingProxyDeletes()` | `Engine.cpp:552` | **销毁已 park 的 proxy**，必须串行且必须在全部录制之后 |

单个 view 的 `ForwardRenderer::Execute`（`ForwardRenderer.cpp:625`）：

| 顺序 | 动作 | 位置 | 性质（详见 §3） |
|---|---|---|---|
| a | desc 校验 | `:627-651` | 串行 |
| b | `DebugDrawService::ApplyFrameSnapshot` | `:657` | 串行（全局状态） |
| c | `CollectOrphanedSceneProxies()` | `:667` | **串行**（改绘制列表） |
| d | `BuildRenderQueue(ctx)` | `:669` → `RenderQueueBuild.h:28` | **可并行**（本 Feature 主体） |
| e | `SortMeshDrawQueues(ctx)` | `:670` → `ForwardRenderer.cpp:1265` | 可并行（建议先串行，见 §9 S05） |
| f | `BuildScreenUIQueue` | `:671` → `RenderQueueBuild.h:110` | 串行（数据量小） |
| g | `m_ShadowPass.m_OpaqueQueue = ctx.ShadowCasterQueue` | `:673` | 串行（全量 copy） |
| h | `PrepareSceneEnvironment` | `:679` | 串行（资产加载 + 独立 immediate bake，**必须在窗口之外**） |
| i | `RHICommandListRecordingScope` 开窗 | `:701` | **窗口起点** |
| j | `UpdatePerFrameUBO` | `:703` | GPU 上传，串行 |
| k | `CollectShadowRequests` / `BuildShadowDrawCommands` | `:711-713` | GPU/ring，串行（灯光队列有全局上限计数，顺序有语义） |
| l | `SetupFrameRenderGraph` → `Bake` → 各 pass `Prepare` | `:727` | **GPU**（PSO 创建），串行 |
| m | `BindGraphShadowTextures` | `:731` | 串行 |
| n | `UpdateLightUBO` | `:738` | 串行 |
| o | `m_SceneBindings.BeginFrame` / `BuildSceneSet1` | `:742-755` | 串行 |
| p | 各 pass 命令交棒 | `:757-775` | 串行 |
| q | `EnqueueFrameRenderGraph` | `:777` | 串行 |
| r | `recordingScope.Finish()` + `Replay`/池提交 | `:781-790` | 串行 |

窗口内部（`l` 步）展开：

- `BasePass::Prepare`（`BasePass.cpp:48`）→ `PrepareSceneMeshDrawPackets`（`SceneMeshDrawUtils.cpp:16`）
  → `EnginePipelineLayouts::GetOrCreateSceneMeshGraphicsPipelineState`（`EnginePipelineLayouts.cpp:89`）
  → 首次命中时 `cmdList.CreateGraphicsPipelineState`：**GPU 创建**，且 PSO 缓存是裸 `std::unordered_map`。
- `BasePass::BuildRenderPass`（`BasePass.cpp:70`）→ `SubmitSceneMeshDrawPackets`（`SceneMeshDrawUtils.cpp:70`）
  → `EngineSceneBindingSets::BindNextPerObjectModel`（`EngineSceneBindingSets.cpp:216`，**全局 ring 游标**
  `m_PerObjectWriteIndex++` at `:210`）+ `MaterialRenderProxy::BindForDraw`（重建 material set）+
  `SubmitMeshDrawPacket`。这三步都**按提交顺序**产生语义，严格串行。

`ManualRenderer` 有同形路径（`ManualRenderer.cpp:328-330` 同 `c`/`d`/`e`，`:281-292` 同 `l`），
**必须**改走同一个入口，否则等于留下第二条路径。

### 2.2 三条硬约束在本 Feature 下的具体含义

| 约束 | 今天靠什么成立 | 并行化后的风险点 | 本 Feature 的守约方式 |
|---|---|---|---|
| **H1 渲染侧不持游戏对象** | 代理字段是游戏角色算好的快照（`PrimitiveSceneProxy.h:42-49` 的 `m_WorldBounds`）；`m_PrimitiveComponent` 只在 `RenderScene.cpp:309` 被**比较**，从不 deref | worker 上跑 `GetMeshBatches` 虚函数，若某 override 回头读组件即违约 | 三个 override 全是代理 POD 读（`PrimitiveSceneProxy.cpp:65`、`SkeletalMeshSceneProxy.cpp:7`、`SpriteSceneProxy.cpp:6`，见 §3）；新增「装配体不得 deref identity token」断言（§8.3） |
| **H2 代理两段式删除** | 移除命令置 `m_bPendingRemove` + 从 `m_PrimitiveSceneProxies` 摘除（`RenderScene.cpp:283-286`）；真正销毁只在 `FlushPendingProxyDeletes`（`RenderScene.cpp:1038`，由 `Engine.cpp:552` 调用） | 装配期间若有人销毁代理 → 悬空 | 扇出**同步 join 后返回**，整个并行区间内绘制列表的唯一 mutator（渲染角色自己）被阻塞；装配区间严格落在 `:516` 与 `:552` 之间（§4.4） |
| **GPU 只在图形角色** | 录制窗口按线程（`RHICommandList.h:157,223`）；离角色 immediate 调用被拒并计数（`RHICommandList.h:146`、`FrameWorkLossCounters::OffRoleRenderCommands`） | worker 上碰 RHI | **类型级**排除：并行装配入口签名里没有 `RHICommandList&`、没有 `ForwardRenderer&`（§4.1）；已有 off-role 拒绝计数作后备观测 |

### 2.3 Task 系统契约（必须尊重的部分）

引用自 `Runtime/Core/Tasks/TaskSystem.h`：

- workers 是**纯 CPU**：无 RHI、无 NewObject、无 AssetManager 写、无 Scene authority、无 Lua、无 ImGui、
  无 TaskSystem 控制面重入（`:150-151`）。
- `ParallelFor` 只允许 **Game 或 Render** 编排（`:202`，实现 `TaskSystem.cpp:982-986`）。
- `kMaxInFlightFrames = 3`（`:168`），端到端 in-flight 背压。
- **`ParallelFor` 拒绝时是「打一条 Error 日志然后 `return`」**（`TaskSystem.cpp:974-990`）：函数返回 void，
  调用方**看不出** body 没跑。这正是任务里说的「静默拒绝」问题，§7 给出结构性对策。
- `count == 1` 时直接 inline 执行（`TaskSystem.cpp:992-1007`）。
- `GetWorkerCount()` 最少为 1（`ResolveWorkerCount` at `TaskSystem.cpp:27-39`）。

---

## 3. 可并行与必须串行的边界

判据三条，缺一不可：**(P1)** 只读不可变输入；**(P2)** 不产生顺序语义的全局副作用；**(P3)** 不碰 RHI/GPU。

| # | 工作 | 调用点 | 判定 | 理由 |
|---|---|---|---|---|
| 1 | 逐 proxy 可见性粗筛（`Frustum::Intersect(AABB)`） | `RenderQueueBuild.h:79-82` | **可并行** | P1（读代理缓存 box）+ P2（只写本片输出）+ P3 |
| 2 | `PrimitiveSceneProxy::GetMeshBatches`（虚调用，出 `MeshBatch`） | `RenderQueueBuild.h:90-97` | **可并行** | 三个 override 全是代理 POD 读 + 缓存 bounds（§2.2 H1 行）；`MaterialRenderProxy::IsTranslucent()` 走 `std::atomic_load(&m_Published)`（`MaterialRenderProxy.cpp:118-121,148`），worker 安全 |
| 3 | `MeshPassProcessor::AddMeshBatch` → `MeshDrawCommand` | `MeshPassProcessor.cpp:39-83` | **可并行** | 纯结构体拷贝（`MakeDrawCommand` at `:21`） |
| 4 | `MeshBatchCollector::DispatchTo` | `MeshBatchCollector.cpp` | **可并行** | 本片 collector → 本片输出向量，无跨片依赖 |
| 5 | `BuildScreenUIDrawQueue`（widget 队列 + stable_sort） | `RenderQueueBuild.h:110-186` | **本期串行** | 数据量小；`m_WidgetSceneProxies` 与主队列无关，收益/风险比差 |
| 6 | 片内 `std::stable_sort`（不透明） | `MeshDrawCommandSort.cpp:46` | **可并行（延后）** | 见 §5；先保串行全局排序，S05 用「片内排序 + k 路归并」换 |
| 7 | 片间汇聚（按片序 append） | 新增 | **必须串行** | 顺序语义的唯一来源（§5.3） |
| 8 | `SortOpaqueMeshDraws` / `SortTranslucentMeshDraws` 全局排序 | `ForwardRenderer.cpp:1272-1277` | **必须串行**（本期） | 依赖完整输入；key 依赖指针折叠，见 §5.2 |
| 9 | `CollectOrphanedSceneProxies` | `ForwardRenderer.cpp:667`、`ManualRenderer.cpp:328` | **必须串行** | 改绘制列表；已离角色拒绝（`RenderScene.cpp:962-973`） |
| 10 | `PrepareSceneEnvironment` | `ForwardRenderer.cpp:679` | **必须串行** | 资产加载 + 自己的 immediate 命令缓冲，必须在窗口外 |
| 11 | `CollectShadowRequests` | `ForwardRenderer.cpp:711` | **必须串行** | 灯光队列有全局上限计数（`:981`、`:998`）与登记顺序语义 |
| 12 | `BuildShadowDrawCommands` | `ForwardRenderer.cpp:713` | **必须串行** | GPU/UBO |
| 13 | `PrepareSceneMeshDrawPackets`（PSO 解析） | `SceneMeshDrawUtils.cpp:16-67` | **必须串行** | `CreateGraphicsPipelineState`（`EnginePipelineLayouts.cpp:122` 起）+ 非线程安全 PSO map |
| 14 | `SubmitSceneMeshDrawPackets`（ring 槽位 + material set 重建 + 提交） | `SceneMeshDrawUtils.cpp:70-123` | **必须串行** | 全局 ring 游标（`EngineSceneBindingSets.cpp:210`）；`BindForDraw` 写 material GPU 状态；顺序即时序 |
| 15 | `UpdatePerFrameUBO` / `UpdateLightUBO` / `BeginFrame` / `BuildSceneSet1` | `ForwardRenderer.cpp:703,738,742,750` | **必须串行** | 上传 + 全局绑定集状态 |
| 16 | RDG `Bake` / pass `Prepare` / `BuildRenderPass` / `EnqueueRenderGraph` | `ForwardRenderer.cpp:727,777` | **必须串行** | 图形角色窗口内 |
| 17 | `FlushPendingProxyDeletes` | `Engine.cpp:552` | **必须串行** | 销毁；必须最后 |
| 18 | `DebugDrawService::ApplyFrameSnapshot` | `ForwardRenderer.cpp:657` | **必须串行** | 进程级全局状态 |

**结论：** 可并行的岛就是 §2.1 的 `d`（`BuildRenderQueue` 全体：粗筛 + Collect + Dispatch），
`e`（排序）为可选第二步。其余一律串行。这个界正好也是 UE 的界：**cull + collect 并行，packet 绑定与提交不并行**。

## 4. 数据与所有权

### 4.1 签名示意（只给签名，不给实现）

```cpp
// Runtime/Function/Render/RenderPipeline/MeshAssemblyChunk.h（新文件）

/**
 * 一片 primitive 的装配产物。
 * FirstProxy/ProxyCount 指向 RenderScene::m_PrimitiveSceneProxies 的连续区间。
 * Shadow 分两段：主视可见的 caster 与仅阴影的 caster 在今天的语义里是先后两批
 * （RenderQueueBuild.h:104-107），合并时必须保持这个先后。
 */
struct MeshAssemblyChunk
{
    uint32_t FirstProxy = 0;
    uint32_t ProxyCount = 0;

    std::vector<MeshDrawCommand> Opaque;
    std::vector<MeshDrawCommand> Translucent;
    std::vector<MeshDrawCommand> ShadowFromMainVisible;
    std::vector<MeshDrawCommand> ShadowFromHidden;

    /** 片内实际遍历过的 proxy 数；用于「片确实跑过」的校验（§7.2）。 */
    uint32_t VisitCount = 0;
};

/** 装配调度参数。ChunkCount <= 1 或 Tasks == nullptr 即串行。 */
struct MeshAssemblySchedule
{
    TaskSystem* Tasks = nullptr;
    int32_t ChunkCount = 1;
    uint32_t MinProxiesPerChunk = kMeshAssemblyMinProxiesPerChunk;
};

/**
 * 唯一实现入口。串行入口是它 ChunkCount==1 的特例。
 * 刻意不接收 RHICommandList / ForwardRenderer：worker 侧不持任何 GPU 入口是类型级保证。
 */
void BuildPrimitiveRenderQueueChunked(
    SceneRenderContext& ctx,
    const Math::Geometry::Frustum* mainFrustum,
    const MeshAssemblySchedule& schedule,
    std::vector<MeshAssemblyChunk>* outChunks);   // 非空：保留分片，供测试与排序复用
```

`BuildPrimitiveRenderQueue(ctx, frustum)`（`RenderQueueBuild.h:28`）保留签名，但**函数体改为**
`BuildPrimitiveRenderQueueChunked(ctx, frustum, {}, nullptr)`——即串行就是 `ChunkCount == 1`，
**不允许**出现两份遍历代码（AGENTS.md「refactor discipline：不留静默双路径」）。

放在 header 里而不是 `.cpp`，是因为现有约定：这两个 builder 之所以是 inline，是为了让 headless
测试能直接调用而不必新增导出（`RenderQueueBuild.h:17-25` 的注释）。新文件沿用同一约定。

### 4.2 输入（只读）

| 输入 | 归属 | worker 只读时安全的原因 |
|---|---|---|
| `RenderScene::m_PrimitiveSceneProxies`（`RenderScene.h:102`） | 场景 | 唯一 mutator 是渲染角色（`EnqueueRenderCommand` → `Apply*_RT`），而渲染角色在 join 中被阻塞（§4.4） |
| `PrimitiveSceneProxy` 字段 | 场景（`m_PrimitiveSceneProxyOwners`，`RenderScene.h:159`） | 写入分两类：① 注册前由游戏角色写完（`AddPrimitive` → `CreateSceneProxy` 在游戏角色上同步执行，`RenderScene.cpp:349`），经命令队列的锁建立 happens-before；② 注册后只由渲染角色的 `ApplyUpdatePrimitiveTransform_RT`（`RenderScene.cpp:264-274`）写。两者都不与装配重叠 |
| `PrimitiveSceneProxy::m_WorldBounds` | 同上 | `ResolveWorldBounds()` 只返回缓存（`PrimitiveSceneProxy.cpp:57-63`） |
| `MaterialRenderProxy` 的**已发布快照** | 材质代理（非游戏对象，`MaterialRenderProxy.h:161`） | `AcquirePublishedData()` = `std::atomic_load(&m_Published)`（`MaterialRenderProxy.cpp:118`）；`PublishGameData` 每次都**新建不可变块再 atomic_store**（`:202-207`），绝不原地改活块 |
| `Math::Geometry::Frustum` | 本帧栈变量（`ForwardRenderer.cpp:1255`） | 生命周期覆盖 join |
| `ctx.bEnableShadows` | 本帧上下文 | 装配前已定 |

**worker 侧禁止读**（写清是因为它们看起来无害）：

- `MaterialRenderProxy::GetPublishSerial()`：`m_PublishSerial` 是游戏角色上的裸 `++`（`MaterialRenderProxy.cpp:209`）。
- `PrimitiveSceneProxy::m_PrimitiveComponent`：只有身份意义（`PrimitiveSceneProxy.h:29-37`）。
- 任何 `RHIBuffer*` 的 desc 查询 / 非 const 成员函数：`GetDesc()` 属访问 GPU 对象。参照
  `StaticMeshComponent.cpp:64-69` 之所以在游戏角色上把 count 抓下来，就是同一个道理。

### 4.3 输出与汇聚（按 proxy 索引分片，片内按 pass 分桶）

- **分片维度 = proxy 索引区间**，不是 view，也不是 pass。理由：剔除/Collect 的循环体天然按 proxy
  遍历（`RenderQueueBuild.h:56`），按索引切只需要一个 `[first, first+count)`，无跨片依赖。
- **片内分桶 = pass**：四个向量（§4.1）。
- **汇聚（串行，在渲染角色）**：

```text
OpaqueQueue       = chunk[0].Opaque      ++ chunk[1].Opaque      ++ ...   // 片序
TranslucentQueue  = chunk[0].Translucent ++ chunk[1].Translucent ++ ...
ShadowCasterQueue = (chunk[0..n-1].ShadowFromMainVisible 依片序)
                 ++ (chunk[0..n-1].ShadowFromHidden      依片序)
```

  第三行 Shadow 的两段拼接是**必须**的：今天的顺序是 `mainCollector.DispatchTo(shadow)`
  然后 `shadowOnlyCollector.DispatchTo(shadow)`（`RenderQueueBuild.h:104-107`），两段之间的相对
  顺序是今天队列的一部分，不是实现细节。
- 汇聚用 `insert(end, ...)` + 提前 `reserve(总数)`；总数由各片 `size()` 求和（一次遍历）。
- 不为「按 view 分区」做任何设计：每个 view 有独立的 `SceneRenderContext`（`SceneRenderContext.h:20`）
  和独立的 `Execute` 调用，跨 view 合并没有消费者。多 view 的并行化是另一个决定（§12 未决问题 3）。

### 4.4 资源与寿命

| 被持物 | 今天 | 并行后 | 会不会变 |
|---|---|---|---|
| `MeshDrawPacket::PipelineState` | `RHIGraphicsPipelineStateRef`（owned） | 不变（装配阶段不产 packet） | 否 |
| `MeshDrawPacket::ShaderBindingSets` | `RHIShaderBindingSetRef`（owned，`MeshDrawPacket.h:19-30`） | 不变（在串行提交阶段填） | 否 |
| `MeshDrawCommand::m_VertexBufferRef` / `m_IndexBufferRef` / `m_VertexInputLayoutRef` | `RHIBufferRef` / `RHIVertexInputLayoutRef`（owned；同名裸指针降为帧视图） | 不变 | 否 |
| `MeshDrawCommand::m_BonePalette` | `const Matrix4*` 指向代理里的 `std::vector`（`SkeletalMeshSceneProxy.cpp:28-31`） | 仍然借用 | **否，但成为「命令不得跨帧」的强约束**（见 3） |

**借用为什么今天安全、并行后仍然安全：**

1. 几何沿 `MeshBatch → MeshDrawCommand → MeshDrawPacket → 录制流` 全程由 `RHIBufferRef` /
   `RHIVertexInputLayoutRef` 强持（`MeshDrawCommand.h`、`MeshDrawPacket.h` 的 ref 成员），裸指针只是
   帧视图；代理另持一份（`PrimitiveSceneProxy.h`）。所以装配 + 提交 + 录制这一段的借用**不依赖代理寿命**，
   worker 读到的句柄也不会因为代理被移除而失效。
2. `m_BonePalette` 指向代理 vector 的数据。它在注册**之前**由游戏角色写入
   （`SkeletalMeshComponent.cpp:363` 在 `SyncSceneProxy` 里，而 `SyncSceneProxy` 只由 `CreateSceneProxy`
   调用，`CreateSceneProxy` 只由 `RenderScene::AddPrimitive` 在游戏角色上调用，`RenderScene.cpp:349`），
   注册之后再没人写。所以 worker 读到的指针在整个帧内稳定。
   唯一的重绑路径是 `RecreatePrimitive`（`RenderScene.cpp:387-408`）：它建**新**代理并走 `AddPrimitive`，
   老代理被移除，不是原地改 vector。
3. `BindNextPerObjectModel` 是**立刻** `UpdateBuffer` 拷贝（`EngineSceneBindingSets.cpp:246,255`），
   所以「装配产物必须在同一帧内被消费」。并行化不改变这一点，但把它从「隐含」变成「明文」：
   **chunk 产物的寿命 = 本帧装配结束 → 本帧提交结束，不得缓存到下一帧。**（§13 第 6 条）
4. 任务期间代理被删（H2）：不可能与装配重叠。删除只在 `FlushPendingProxyDeletes`（`Engine.cpp:552`），
   它在所有 view 的 `Execute` 之后；且装配的 join 让渲染角色在整个并行区间内不可能执行任何移除命令。
   §8.3 提议一条断言把这个假设钉成会响的，而不是靠注释。

---

### 4.5 视图剔除的位置与粒度（对应 UE 的「每视图并行剔除 + 命令收集」）

**做，但粒度是「每视图内的 proxy 索引片」，不是「每视图片段任务」，也不是「每 proxy 一个任务」。**

| 候选粒度 | 评价 |
|---|---|
| **每 proxy 一个任务**（`ParallelFor(proxyCount, ...)`） | ❌ 否决：1000 proxy 就是 1000 个 `silent_async` 提交（`TaskSystem.cpp:1015-1037`），提交开销与 join 的原子计数成本会吃掉收益 |
| **每视图一个任务**（viewport 并行） | ⏸ 本 Feature 不做。它需要跨 view 的合并扇出与 `CollectOrphanedSceneProxies` 上移（§12.2-3），属于改帧结构的独立切片 |
| **每视图内的 proxy 索引片**（`ChunkCount` 个任务，`ChunkCount ≤ workerCount * 2`） | ✅ 采用。分片数可控、覆盖可验证（I1）、合并顺序天然确定（I2） |

**「位置」= 今天的 `BuildRenderQueue` 所在位置**（`ForwardRenderer.cpp:669` / `ManualRenderer.cpp:329`），
即每个 view 各做一次扇出，仍在录制窗口之前（§6）。剔除逻辑本身（`Frustum::Intersect`，
`RenderQueueBuild.h:79-82`）与 Collect（`:90-97`）都搬进片任务，**引擎侧不新增任何剔除层**：
子 mesh 剔除仍由 game 侧 proxy 在 `GetMeshBatches` 里决定（RND-F26 的契约），
本 Feature 只是把「已经存在的那个循环」并行化。

---

## 5. 顺序确定性

### 5.1 今天的顺序是怎么产生的

| 队列 | 排序 | 位置 | 最终顺序 = f(什么) |
|---|---|---|---|
| `OpaqueQueue` | `stable_sort` by `MeshDrawCommandSortKey::m_Key`（无符号 `<`） | `MeshDrawCommandSort.cpp:46-58` | **插入顺序** + key |
| `TranslucentQueue` | `stable_sort` by 视图距离**降序** | `MeshDrawCommandSort.cpp:76-90` | **插入顺序**（等距时）+ 距离 |
| `ShadowCasterQueue` | **完全不排序** | — | 纯插入顺序 |
| `ScreenUIQueue` | `stable_sort` by (CanvasSortOrder, StableOrder) | `RenderQueueBuild.h:175-185` | 插入顺序 + 两个显式键 |

不透明 key 的形状（`MeshDrawCommandSort.cpp:10-36`）：

```text
m_Key = (materialPtr[15:4] 折叠 24 bit)        << 40
      | (vertexInputLayoutPtr[15:4] 折叠 20 bit) << 20
      | (vertexBufferPtr[15:4] 折叠 19 bit)      << 1
      | skinned
```

### 5.2 关键结论：顺序完全由「插入顺序」承载

`std::stable_sort` + 元素自身 key ⇒ 相等 key 之间的先后**只由插入顺序决定**。
而 key 里有指针折叠，所以 key 相等时「两个命令谁先谁后」是**语义性的**（同一材质同一 layout 的两个
实例，其绘制顺序由场景登记顺序决定）。因此：

> **只要分片后的插入顺序与串行一致，最终顺序就逐字节一致。**
> 不需要引入新的排序键，也不需要引入 UE 的 priority/pass 键。

这条必须进评审结论，因为它决定了整个方案的风险面：**我们不重写排序，只保住插入顺序。**
（UE 给每个状态 bucket 一个稳定 key、并行合并时按 task index 归并；我们今天的 key 已经承担了
同样的职责，缺的只是「合并必须按片序」这一条纪律。）

### 5.3 两条不变量

- **I1（分片覆盖）**：所有 chunk 的 `[FirstProxy, FirstProxy + ProxyCount)` 两两不交，并集恰为
  `[0, m_PrimitiveSceneProxies.size())`。
- **I2（合并顺序）**：按 `FirstProxy` 升序逐片 append。

I1 + I2 ⇒ 合并后的 pre-sort 序列与串行遍历的 pre-sort 序列**逐元素相同** ⇒ 排序结果相同。

### 5.4 验证「顺序不变」的方法（可执行、可失败）

`MakeQueueDigest(const std::vector<MeshDrawCommand>&)`：FNV-1a 64，按顺序喂
`{VB, IB, VIL, IndexCount, VertexCount, MaterialProxy, ModelMatrix 的 16 个 float 位模式,
BoundingBox 6 个 float 位模式, CastShadow, bSkinned, BoneCount, SortKey}`。

- 同一进程内串行与并行看到同一批对象 ⇒ 指针值相同 ⇒ digest 可比。
- 判据：`digest(serial) == digest(parallel(k))`，`k ∈ {2, 3, 8, 17}`，同一帧同一 frustum。
- **这个测试必须能红**：故意把合并循环改成「按 chunk 逆序」或「按 chunk 大小排序」，digest 必须不同
  （§10 R3 的负例）。不能写一个永远绿的断言。
- 跨帧稳定性：场景不变时连续 60 帧 digest 恒等。这抓「分片数随 worker 数漂移」——
  分片数变化只能改变耗时，不得改变结果。

---

## 6. 与录制窗口的关系

### 6.1 选择：**本帧内、开窗之前装配完毕**，不做 N+1 流水

`d`（装配 + join）与 `e` 仍在 `RHICommandListRecordingScope`（`ForwardRenderer.cpp:701`）之前完成，
与今天位置完全一致。**不**做「本帧装配 N+1、录制 N」。

### 6.2 理由

1. **输入在帧内才最终**。绘制列表由 `FlushApplyForFrameChecked`（`Engine.cpp:516`）在**本帧开头**改写。
   想让第 N 帧的装配在 N-1 帧做，就必须把第 N 帧的命令提前应用——那等于推翻「应用先于录制」这条
   顺序保证（`Engine.cpp:509-515` 的注释）。
2. **没有可重叠的窗口**。装配本来就在窗口之外，前面没有东西可流水；真正的窗口内工作是 PSO 创建、
   ring 槽位写入、UBO 上传，那些不可并行（§3 #13-16）。N+1 流水只增加一帧视觉延迟，不增加吞吐。
3. **join 已经是最好的形态**。`ParallelFor` 是「扇出 + join」（`TaskSystem.cpp:1009-1040`），
   渲染角色在这段时间本来就是空闲的；跨帧装配会把 join 换成「下一帧再收」，于是要么引入跨帧持有
   （违反 §4.4-3 的寿命约束），要么引入 chunk 池的 double buffer（YAGNI）。
4. **背压已经存在**。`kMaxInFlightFrames = 3`（`TaskSystem.h:168`）已把游戏角色与 GPU 的漂移绑住；
   装配再加一帧只会让画面更旧。

### 6.3 一个必须写清的细节

窗口是**按线程**的（`RHICommandList.h:157,223`）。若 worker 上出现任何 `RHICommandList` 构造或驱动，
`IsOffRoleCaller`（`RHICommandList.h:198`）会拒绝并在 `OffRoleRenderCommands` 里可见。
本 Feature 的做法是**不让它有机会**：装配入口没有 RHI 参数（§4.1），所以「worker 在窗口内碰 RHI」
在类型上不成立，而不是靠运行期检查。

---

## 7. 降级与失败

### 7.1 能力判定放在调用方，先判定再选入口

```text
UseParallelAssembly(schedule) :=
      schedule.Tasks != nullptr
   && schedule.Tasks->IsRunning()                     // TaskSystem.h:273
   && schedule.Tasks->GetWorkerCount() > 1            // TaskSystem.cpp:1067
   && schedule.ChunkCount > 1
   && proxyCount >= schedule.MinProxiesPerChunk * 2   // 扇出成本下限
   && mainFrustum / ctx.Scene 非空（否则没有活可并行）
```

`schedule` 由**渲染角色**在 `Execute` 开头解析一次（每帧一次，不是每片一次），解析结果进 §8.3 的统计。
串行入口因此不是「另一条路」，而是「`ChunkCount == 1` 的同一条路」。

### 7.2 「不能静默不执行」的结构性对策

`ParallelFor` 拒绝时静默返回（`TaskSystem.cpp:974-990`），所以**不能**把「我叫了 ParallelFor」
当成「body 一定会跑」。对策是**效果校验，不是意图校验**：

```text
for i in chunks: chunk[i].VisitCount = 0
schedule.Tasks->ParallelFor(n, [&](i) { assemble(i); chunk[i].VisitCount += 1; });
for i in chunks where chunk[i].VisitCount == 0:      // 没跑过
    assemble_serial(i);                              // 就地补跑
    ++s_AssemblyChunksNotRun;                        // 计数（§8.1）
    记一条 Error 日志（带片序号）
```

要点：

- 计数器放在 body 的**最后一行**（返回前）。异常被 `ParallelFor` 吞掉（`TaskSystem.cpp:1018-1029`）时
  计数器不会加，于是「抛过异常的片」与「被拒绝的片」被同一条补跑逻辑抓住。
- 补跑是**就地**的，最终队列永远完整 —— 这才是「不能静默不执行」。
- 正常帧零成本（一次长度为 n 的 `VisitCount == 0` 扫描）。

### 7.3 具体退化场景

| 场景 | 表现 | 处理 |
|---|---|---|
| 无 `TaskSystem` 实例（headless / 宿主自驱） | `HasInstance() == false` → `schedule.Tasks == nullptr` | 串行，无日志（正常配置，不是异常） |
| `TaskSystem::Init` 未调 / `Shutdown` 后 | `IsRunning() == false` | 串行；每帧最多一条 Warn（去重），并计入统计 |
| Single 拓扑 | 渲染角色 = 游戏角色，`IsAllowedOrchestrator` 为 true，`ParallelFor` **可用** | 可以不退化。但 Single 下 `RHICommandList` 是 immediate（`RHICommandList.h:55-63`），对延迟更敏感：**默认仍允许**，开关可控，最终由 S05 数据定稿 |
| 渲染角色未启动（`AreFrameRolesRunning() == false`） | 仍在 Single 语义下运行 | 串行，不进入 `TaskSystem` 的帧角色路径 |
| 调用线程非法（宿主绕过角色直接调 `Execute`） | `ParallelFor` 静默拒绝 | §7.2 抓出，记 Error + 计数 + 串行补跑 |
| worker 数 = 1 | `GetWorkerCount() <= 1` | 串行，无日志 |
| primitive 少 | `proxyCount < 2 * MinProxiesPerChunk` | 串行，无日志 |
| 装配 body 抛异常 | `ParallelFor` 吞掉并记日志 | §7.2 补跑 |

### 7.4 阈值不是拍脑袋

```text
chunkCount = clamp(ceil(proxyCount / MinProxiesPerChunk), 1, workerCount * 2)
```

- `MinProxiesPerChunk` 先给保守初值（建议 128），**由 S05 测量定稿**，并把测得值写进文档，
  不留成注释里的魔法数。
- 上界用 `workerCount * 2` 而不是 `workerCount`：长尾（某片特别重）时仍有均衡余量。
- 分片数**必须不影响结果**（§5.4 的跨帧 digest 恒等就是这条的测试）。

## 8. 可观测性

### 8.1 进 / 不进 `FrameWorkLossCounters` 的分界

`FrameWorkLossCounters`（`TaskSystem.h:25-64`）的语义被注释钉死为「**帧角色事件 = 工作丢失或契约被违约**」
（`:15-24`）。据此：

| 观测项 | 进 `FrameWorkLossCounters`？ | 理由 / 落点 |
|---|---|---|
| **worker 上碰了 GPU** | ❌ 不需要新增 | 已有 `OffRoleRenderCommands`（`TaskSystem.h:47`，来源 `RHICommandList.h:146`）就是它的度量。类型级排除（§4.1）让它在装配阶段恒为 0，因此**它的增量本身就是失败信号**：装配改造后这个数一旦上升，说明有人往 worker 里塞了 RHI |
| **装配片未跑**（并行被拒 / body 抛异常） | ✅ **新增 1 条** `MeshAssemblyChunksNotRun` | 它是「工作丢失」的同族：片没跑就等于这一片的 draw 曾丢失（补跑救回了正确性，但并行性与帧时间丢了）。放进这个结构符合其定义，且能进现有的每帧一行摘要 |
| **装配耗时** | ❌ | 每帧都在变，进 delta 行会变成噪声并掩盖真正的丢失。落点：`ME_PROFILE_SCOPE("Render.MeshAssembly")` 包住整个扇出 + join（在渲染角色上，一个 scope，不逐片）。串行/并行用**同一个 scope 名**才能直接对比 |
| **任务数 / 并行度 / 分片数** | ❌ | 同上。落点：`MeshAssemblyStats`（§8.3），测试可读 + Debug 级日志 |
| **装配降级原因** | ❌ | 落点：`MeshAssemblyStats::LastFallback` + 每帧最多一条 Warn（去重） |

### 8.2 `FrameWorkLossCounters` 的具体改动点（4 处，缺一不可）

1. `TaskSystem.h:26-49`：加字段 `uint64_t MeshAssemblyChunksNotRun = 0;`，注释说明
   「片没跑 = 该片 draw 曾丢失，已就地串行补跑」。
2. `TaskSystem.h:50-61`：`operator==` 补一项。
3. `TaskSystem.h:74-139`：`DescribeFrameWorkLossDelta` 里算 delta +
   `append(chunkDelta, "mesh assembly chunk(s) did not run")`。
4. `Runtime/Engine.cpp:333-368` `ReportFrameWorkLoss`：从新的 provider 读**进程级累计**
   （与 `OffRoleCoreServiceCalls` / `OffRoleRenderCommands` 同样处理，`:357-360`）。

### 8.3 断言 / 契约（新增）

| 断言 | 位置 | 抓什么 |
|---|---|---|
| `ME_ENSURE(chunk.VisitCount == 1)` | 汇聚前 | 同一片被跑两次（分片区间重叠，I1 破了） |
| `ME_ENSURE(coveredProxyCount == proxyCount)` | 汇聚前 | 片集覆盖不全（漏画） |
| 「装配区间内不得发生 `FlushPendingProxyDeletes`」：记录该函数最后一次调用的帧序号并与装配区间比对 | `RenderScene::FlushPendingProxyDeletes` + 装配入口 | 把 §4.4-4 的假设钉成会响的断言，而不是靠注释 |
| `ME_ENSURE(!RHICommandList::IsRecordingWindowOpenOnThisThread())` | 装配入口 | 有人把装配挪进窗口（§6） |
| `ME_ENSURE(FrameThreading 角色是 Game 或 Render)` | 装配入口 | 非法编排线程（§7.3 表内第三行） |

`MeshAssemblyStats`（只读、测试可见）：

```cpp
struct MeshAssemblyStats
{
    uint64_t Frames = 0;              // 调用次数
    uint64_t ParallelFrames = 0;      // 走了并行的次数
    uint32_t LastProxyCount = 0;
    uint32_t LastChunkCount = 0;
    uint32_t LastWorkerCount = 0;
    const char* LastFallback = nullptr;   // nullptr 表示没退化
    uint64_t ChunksNotRun = 0;            // 与 FrameWorkLossCounters 同源
};
```

### 8.4 现有探针的复用（不新增重复设施）

- `ME_PROFILE_SCOPE("Render.BuildRenderQueue")`（`ForwardRenderer.cpp:1253`）保留；新增的
  `Render.MeshAssembly` 套在扇出 + join 上。两层分别是「总时长」与「并行段时长」。
- `RHICommandList::GetRecordedBindingSetHoldCount()`（`RHICommandList.h:167`）继续作「录制流没被回收」
  的哨兵。并行化不改提交路径，这条不该动；它开始涨说明有人把 packet 填到了错误的阶段。
- `TaskSystem::GetInFlightFrameCount()`（`TaskSystem.h:257`）在装配前后各读一次，Debug 断言两次相等：
  装配不得改变 in-flight 计数（它不产生帧）。

---

## 9. 切片计划

每片都必须能独立验证、独立回滚。**S01 必须在一天内完成并验证。**

### S01 — 抽出分片骨架（纯串行，零行为变化）

- **改什么**
  - 新增 `RenderPipeline/MeshAssemblyChunk.h`：`MeshAssemblyChunk` / `MeshAssemblySchedule` /
    `BuildPrimitiveRenderQueueChunked(...)`。
  - `RenderQueueBuild.h:28` 的 `BuildPrimitiveRenderQueue` 函数体改为
    `BuildPrimitiveRenderQueueChunked(ctx, frustum, {}, nullptr)`（**只剩一份遍历代码**）。
  - `ForwardRenderer.cpp:669` / `ManualRenderer.cpp:329` 不动（入口签名不变）。
- **红灯（先写，必红）**
  1. 新套件 `MeshAssemblyDeterminismTest`：构造 200 个混合 proxy（不透明 / 半透明 / 仅阴影 caster /
     隐藏 / `m_bPendingRemove`），同一 frustum 调 `BuildPrimitiveRenderQueue`，把
     `OpaqueQueue` / `TranslucentQueue` / `ShadowCasterQueue` 的 `MakeQueueDigest` 与**期望常量**比对。
     第一步先写 `CHECK(digest == 0)` 让它红，再用真实值填上 —— 这个常量就是「今天的行为」的固化。
  2. `CHECK(ShadowCasterQueue 的前半段全是主视可见 caster、后半段全是仅阴影 caster)`
     —— 红在「两段顺序被写反」。
- **判定没退化**：`SceneProxyRenderReadTest` / `SceneProxyResourceLifetimeTest` / `MaterialRenderProxyDataTest`
  全绿（它们直接调 `BuildPrimitiveRenderQueue`，见 `SceneProxyRenderReadTest.cpp:207,224`）；
  digest 常量不变；`test smoke` 全绿。
- **估时**：0.5–1 天。

### S02 — 扇出 + 确定性合并（真并行，只在测试里启用）

- **改什么**：实现 `ChunkCount > 1` 分支（`ParallelFor` + 片序合并）；`MakeQueueDigest` 进测试支撑。
- **红灯**
  1. `digest(serial) == digest(parallel(k))`，`k ∈ {2,3,8,17}`，1000 proxy 合成场景。
     先让 `parallel` 走**逆序合并**，必须红（证明测试能失败）。
  2. I1 测试：`ChunkCount > proxyCount` 时不得出现空片 / 重叠片。
- **判定没退化**：上条 + 连续 60 帧 digest 恒等 + S01 的 digest 常量不变。

### S03 — 接线到两条管线 + 降级与观测

- **改什么**
  - `ForwardRenderer::Execute` 在 `d` 之前解析一次 `MeshAssemblySchedule`（读 `TaskSystem`、worker 数、
    proxy 数，算 `ChunkCount`）。
  - `ManualRenderer::Execute` 同样接线（**同一个**解析函数，不允许复制判定逻辑）。
  - §7.2 的 `VisitCount` 补跑 + `MeshAssemblyChunksNotRun`；§8.2 四处 `FrameWorkLossCounters` 改动；
    `MeshAssemblyStats`。
- **红灯**
  1. **非法线程不静默**：在未持有 Game/Render 角色的线程上调用装配入口（用 `FrameThreading` 绑定其它
     kind，或干脆不绑定），断言队列**完整**（`OpaqueQueue.size()` 等于串行结果）且
     `MeshAssemblyChunksNotRun > 0`。修复前必须红——它会红在「队列为空」。
  2. **任务系统未启动**：`TaskSystem` 未 `Init`（`IsRunning() == false`），断言走串行、队列完整、
     无 Error 日志。
  3. **worker 数 = 1**：同上。
- **判定没退化**：`TaskSystemTest` / `FrameThreadingTest` / `ContractViolationsTest` 全绿；
  正常帧 `MeshAssemblyChunksNotRun` 不增；`OffRoleRenderCommands` 不增。

### S04 — 阴影两段 + 边界清理

- **改什么**：把 `ShadowFromMainVisible` / `ShadowFromHidden` 两段合并写实（S02 可先用单段占位）；
  `m_ShadowPass.m_OpaqueQueue = ctx.ShadowCasterQueue`（`ForwardRenderer.cpp:673`）改成移动，
  或让装配直接产出到 shadow pass 的目标（省掉今天的全量 copy）。
- **红灯**：仅阴影 caster 在两段中的相对位置测试（S01 红灯 2 的并行版本）；
  `bEnableShadows == false` 时不得产生任何 `ShadowFromHidden`（对应 `RenderQueueBuild.h:84` 的 `bWantShadowCollect`）。
- **判定没退化**：digest 常量不变；阴影相关既有测试全绿（RND-F21 的 caster 集合语义不变）。

### S05 — 测量、阈值定稿、（可选）排序并行

- **改什么**：填 `MinProxiesPerChunk` 的实测值；决定 Single 拓扑默认值；
  **可选**：片内 `stable_sort` + 按片归并替换全局 `stable_sort`（`ForwardRenderer.cpp:1272`）。
- **红灯**：排序并行化前后 `OpaqueQueue` digest 恒等（k 路归并的 tie-break 必须与 `std::stable_sort`
  完全一致：等键按片序、片内按插入序）。
- **判定没退化**：≥1000 proxy 场景帧时间有可复现改善（写明测量方法与数值）；否则**不做**排序并行化
  并把原因记进未决问题。

### 依赖

```text
S01 ──► S02 ──► S03 ──► S04
                   └──► S05（需要 S02 的 digest 与 S03 的 stats）
```

---

## 10. 验证计划（含红灯）

| # | 红灯（必须先红） | 红在哪 | 转绿后保留为回归 |
|---|---|---|---|
| R1 | 串行 digest 常量 | 初始 `CHECK(digest == 0)` | ✅ 固化了今天的顺序 |
| R2 | 并行 digest == 串行 digest | 逆序合并 → 不等 | ✅ `k ∈ {2,3,8,17}` |
| R3 | 「测试能失败」的元验证 | 故意破坏合并顺序必须让 R2 红 | ✅（用一份顺序被刻意打乱的 chunk 列表） |
| R4 | 非法线程仍产出完整队列 | 修复前队列为空 | ✅ + `ChunksNotRun > 0` |
| R5 | 任务系统不可用仍产出完整队列 | 修复前直接返回空 | ✅ |
| R6 | 分片数不影响结果（60 帧 digest 恒等） | 若分片数进了 key 或进了顺序 | ✅ |
| R7 | 装配区间内不得发生 `FlushPendingProxyDeletes` | 把 flush 挪进 `Execute` 后必须红 | ✅ |
| R8 | 阴影 caster 两段顺序 | 两段写反 | ✅ |

**非自动化验证（人工 / 编辑器手验）**

1. `minCraft` 默认场景 + 编辑器 4 viewport 同时开，跑 ≥5 分钟：无崩溃、`MeshAssemblyChunksNotRun` 不增、
   `OffRoleRenderCommands` 不增、`GetRecordedBindingSetHoldCount()` 不持续上涨。
2. 场景里持续增删 mesh（编辑器里复制/删除 1000 次）：验证 H2 没有被装配窗口撕开。
3. 用 `Render.MeshAssembly` 的 profile 数据出「串行 vs 并行」对比表（≥1000 primitive，写明机器与场景）。

**门禁**：`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1` 必须全绿
（它跑 `minEngineTests.exe test smoke`）。新测试套件按 `TestSuiteRegistration.cpp` 的 `TypedTestSuite`
模式登记，参照 `DrawPacketLifetimeTestSuiteTraits`（`TestSuiteRegistration.cpp:1622`）。

---

## 11. UE 对照

> **说明：以下是对 UE 设计的记忆性描述，不是逐行源码引用。** 不提供 UE 源码行号，也不引用具体版本。
> 落地前应对照手上的引擎源码复核。

### 11.1 我们参考的部分

| UE 的机制（记忆） | 它解决什么 | 我们取哪一半 |
|---|---|---|
| `FPrimitiveSceneInfo::CachedMeshDrawCommands` + `CacheMeshDrawCommands()`：**每 primitive 缓存** mesh draw command，只在 mesh / material / VIL / skinning 变化时重建；可见性变化**只改「可见命令引用列表」**，不重建命令本身 | 把「命令构建」与「每帧可见性」解耦：前者是昂贵的静态工作，后者是廉价的动态工作 | **取概念**（分离两件事），**不取实现**（不建每 primitive 缓存，理由见 11.2） |
| 每视图的并行剔除 + 命令收集：把 primitive 范围切成 chunk，每 chunk 在 worker 上剔除并收集自己的命令，然后按 chunk 序合并进该 pass 的命令列表 | 剔除/收集是纯 CPU 且可扩展 | **取**：按 proxy 索引分片（§4.3）+ 片序合并（§5.3） |
| 命令列表的**确定性合并**：并行构建写入 per-task 数组，合并严格按 task index 而非完成顺序 | 并行后仍可复现 | **取**：I2（§5.3）；并发现我们不需要新排序键 |
| **worker 不碰 RHI**：渲染侧只读快照 + 把「真正提交」推迟到 RHI 线程 | 图形 API 单线程亲和 | **取**：类型级排除（§4.1）+ 已有 off-role 拒绝计数（§8.1） |
| 按 state bucket（pipeline / shader / 绑定集布局）分组排序，bucket 内保持稳定序 | 减少状态切换 | **今天的 key 已经是这件事**（`MeshDrawCommandSort.cpp`：material 24bit / VIL 20bit / VB 19bit），所以本 Feature **不碰排序** |

### 11.2 刻意不参考的部分及原因

| UE 的机制 | 为什么不取 |
|---|---|
| **每 primitive 的缓存 MDC + 脏列表** | 我们的 `MeshDrawCommand` 只是从代理 POD 拷出的结构体（`MeshPassProcessor.cpp:21-37`），构建成本 ≈ 一次拷贝。缓存会引入「代理字段变了但缓存没刷新」这类新的一致性缺陷，而当前瓶颈（尚未测量）不在这里。**先测量，再决定**（S05）。 |
| **`FParallelMeshDrawCommandPass` 整套 pass 级框架**（relevance / state bucket / 多 pass 共享剔除结果） | 我们只有一个前向不透明 + 一个半透明 + 阴影，pass 级框架的分层收益小于一致性成本。 |
| **`FParallelCommandList` / 延迟上下文：worker 录制一份待执行命令流，RHI 线程统一执行** | 这是**最危险的误移植**。我们的 `RHICommandList` 是 `std::vector<std::function<void()>>` + **每线程一个开着的录制窗口**（`RHICommandList.h:214-224`、`IsRecordingWindowOpenOnThisThread` at `:157`），池按 view 顺序租借（`ForwardRenderer.cpp:685-696`），而提交路径写全局状态：ring 游标 `m_PerObjectWriteIndex++`（`EngineSceneBindingSets.cpp:210`）与 `BindForDraw` 的 material set 重建。要在 worker 上录制，得同时把窗口、池、scene 绑定集全改成可并发 —— 那是另一个 Feature，且在这个规模上没有收益。 |
| **GPU Scene / Nanite 式 GPU 驱动剔除** | 见 §13。 |
| **层级 / 实例化剔除（HISM 式）** | `RND-F26` 已把子 mesh 剔除留给 game 侧 proxy 自己决定；本 Feature 只并行化「已经存在的那个循环」，不新增剔除层。 |
| **多线程资产解析 / 材质编译（shader compilation manager）** | 超出范围；且 GPU 创建必须留在图形角色（`Engine.cpp:480` 的 `RHIDrainDeferredCreates`）。 |

### 11.3 一句话的对应关系

> UE：**并行剔除/收集 → 稳定合并 → RHI 线程录制**。
> 我们：**并行剔除/收集 → 片序合并 → 同一渲染角色串行绑定/提交**（因为我们的提交写全局 ring 状态）。
> 差别只在最后一段，而最后一段本来就不是瓶颈。

### 11.4 逐条对照：UE 的并行剔除 / 收集是怎么组织的（记忆性描述）

> **本节全部是记忆性描述**：不引用 UE 源码路径、不引用 UE 行号、不引用具体版本。
> §11.3 是结论，这一节把它拆开：UE 到底怎么组织这件事，我们取它的哪一半、依据是哪一行本仓代码。
> 凡出现「我们据此做了某个决定」，依据一律是本仓 `file:line`；UE 侧的叙述只作设计参照。

#### 11.4.1 任务粒度：一个视图一个主任务，视图内再按 primitive 区间细分

记忆中的组织方式：

- **顶层单位是视图**：每帧每个视图（view / view family 成员）各自持有一份可见性数据，剔除 + 命令收集
  以视图为顶层单位启动；不同视图之间不共享这次工作。（记忆里后来还有「跨视图共享剔除结果」的优化层，
  那是加在这个形状之上的东西，不是形状本身。）
- **视图内再按 primitive 区间切任务**：把该视图要处理的 primitive 索引区间切成若干段，一段一个任务；
  每个任务只写自己那一段的输出，任务之间没有共享写，因此不需要锁。
- **有一个「每任务最多处理多少」的上限参数**决定切多少段。它的作用不是「多大算大」，而是**在两种成本
  之间取值**：
  - 上限小 → 段多 → 长尾（最重的那段决定墙钟）被压下去，但**每任务的固定开销**（提交、join 计数、
    每任务自己的临时容器）摊不下来；
  - 上限大 → 段少 → 固定开销低，但某个段可能明显重于其它段，并行度被它拖住。
- 关键一点：**这个上限与 worker 数解耦**。worker 数只给并行度一个上界，段数由上限和元素数决定。这样
  同一份输入在 4 核和 32 核机器上跑出来的段数不同、但**结果相同**。

我们对应的选择（**取形状，不取它的参数体系**）：

| UE 的做法（记忆） | 我们的落点 | 依据（file:line） |
|---|---|---|
| 顶层单位 = 视图 | 每个 view 各自扇出一次，仍在自己那次 `Execute` 里 | `ForwardRenderer.cpp:669`、`ManualRenderer.cpp:329`；跨 view 合并列为 §12.2-3 |
| 视图内按 primitive 区间切 | 分片 = `[FirstProxy, FirstProxy + ProxyCount)` | §4.3、§4.1 的 `MeshAssemblyChunk`、遍历点为 `RenderQueueBuild.h:56` |
| 「每任务最多处理多少」上限 → 段数 | `MinProxiesPerChunk`（初值 128）+ `clamp(ceil(proxy / Min), 1, workerCount*2)` | §7.4 |
| 段数不由 worker 数决定（worker 数只封顶） | 上界 `workerCount * 2`、下界由 proxy 数决定；段数**不影响结果** | §5.4 的 60 帧 digest 恒等（R6） |

**没取的部分：** UE 那个上限的量纲是字节 / 命令条数，且与它自己的命令缓存结构配套；我们的输入是
proxy 遍历，用「每片 proxy 数」已经够用，而且能用 §5.4 的 digest 证明段数不进结果——参数体系换了，
**可证明性**不能丢。

#### 11.4.2 双阶段：先「剔除 + 记录可见命令索引」，再「按 state bucket 收成实际命令列表」

记忆中的两阶段，以及它们为什么并行度不同：

- **阶段一（每视图、并行）**：对 primitive 做视锥 / 遮挡剔除，对通过者收集它的 mesh 元素，得到
  「这个视图、这个 pass 下哪些 draw command 可见」。它写出的是**索引 / 引用**——指向已缓存的命令，
  或指向本帧动态命令数组里的条目——**不是命令本体**。
- **阶段二（按 pass）**：把阶段一的索引按 state bucket（PSO / shader / 绑定集布局）归组、稳定排序，
  合成该 pass 实际要执行的命令列表，再为每条命令做 packet 准备。

两阶段并行度不同，原因只有一个：**输入完整性**。

| 阶段 | 需要的输入 | 并行度 | 为什么 |
|---|---|---|---|
| 一 | 视图 + primitive 数组，分片即可 | 高（按 primitive 区间切） | 每片只看自己那段，输出区间不相交 |
| 二 | **阶段一的全量结果** | 低；只能在 pass 之间切 | 排序与 bucket 归组要看整组，看不见全体就无法定序 |

我们对应的选择：

| UE 的阶段（记忆） | 我们 | 依据（file:line） |
|---|---|---|
| 阶段一产出可见命令**索引** | 不产索引：片直接产 `MeshDrawCommand` 向量（片内已按 pass 分桶） | `MeshPassProcessor.cpp:21-37`（命令 = 一次结构体拷贝）；§4.3 的四个分桶 |
| 阶段二按 bucket 归组 + 稳定排序 | `MeshDrawCommandSortKey` 已经是 bucket 键；`stable_sort` 仍在**全局、串行**阶段 | `MeshDrawCommandSort.cpp:10-36`、`:46-58`、`ForwardRenderer.cpp:1272-1277` |
| 两个阶段不重叠 | 一样：汇聚（串行）→ 排序（串行）→ 提交（串行） | §3 #7 / #8 / #13-14 |

结论：**结构相同，切分点不同。** UE 用「索引」把两个阶段解耦；我们把这层去掉，代价是阶段二看见的已经
是命令向量而不是索引，换来的是不引入索引容器与它的二级寿命问题（`MeshDrawCommand` 自己持 ref，
索引方案还要再管「索引指向哪份存储」）。这条取舍有明确的可回退点：§12.2-5 —— 若 S05 测出
`GetMeshBatches` 与结构体拷贝占大头，那时「按代理缓存命令 + 只产索引」才值得做。

#### 11.4.3 命令缓存与失效：可见性变化只动引用列表，不动命令本身

记忆中的规则：

- **重建条件**是「命令的内容真的变了」：mesh / LOD 换了、material 或它的 uniform 布局变了、
  vertex input layout / vertex factory 换了、skinning 状态（是否蒙皮、bone 数）变了、影响 PSO 的渲染
  状态标志变了。这些由「渲染状态脏」标记驱动，重建是一次性、批量的。
- **可见性变化不触发重建**：它只重写该视图下的可见命令列表（每 pass 一份）。于是每帧重复的工作只是
  廉价的列表写入，昂贵部分只在脏时发生。这就是 §11.1 第一行「取概念」的那一半。

我们**不取**这条缓存，理由与「如果哪天要取，先得补什么」：

| 问题 | 本仓事实 | 依据（file:line） |
|---|---|---|
| 命令本体有多贵？ | `MakeDrawCommand` 只从 `MeshBatch` 拷字段；`MeshDrawCommand` 不是编译产物 | `MeshPassProcessor.cpp:21-37` |
| 失效要盯哪些脏源？ | material **有**不可变发布快照（每次新建块再 atomic_store），但**代理字段没有版本号** | `MaterialRenderProxy.cpp:118-121`、`:202-207`；`PrimitiveSceneProxy.h:39-49`（`m_Transform` / `m_CastShadow` / `m_WorldBounds` 都是裸字段） |
| 没有 per-proxy 序列号意味着什么？ | 缓存无法自证「我看到的还是同一版输入」，只能靠每个改字段的调用点记得失效——这正是 §11.2 拒绝的那类新一致性缺陷 | §11.2 第 1 行、§12.2-5 的「先测再定」 |

一句话：**我们取的是「静态工作与每帧工作分离」这个洞察，不取它的载体**；在命令成本 ≈ 一次拷贝的前提
下，分离的收益小于它带来的失效管理成本。

#### 11.4.4 线程规则：渲染线程生成命令、RHI 线程执行、worker 只碰纯 CPU 数据

记忆中的分工：

- **game 线程**：场景增删改的唯一 owner，变更以命令形式交给渲染侧。
- **渲染线程**：消费命令、生成 draw command（11.4.2 的两个阶段），**自己不直接驱动图形 API**。
- **RHI 线程**：把已经录好的命令流提交给驱动；图形 API 调用只在这里发生。
- **worker**：只做纯 CPU 计算（剔除、收集、排序），读渲染侧发布好的快照，写自己的输出。

**为什么这条规则让 UE 的并行命令列表（`FParallelCommandList` 一类「每个任务录一份待执行命令流、
RHI 线程统一 replay」）不能直接搬到我们这里：**

| 环节 | UE 的前提（记忆） | 本仓现状 | 依据（file:line） |
|---|---|---|---|
| 录制流的所有权 | 每个并行任务拿一份**属于任务**的命令列表 | `RHICommandList` 的窗口是**每线程一个**，不是每任务一个；同一线程再开窗即违约 | `RHICommandList.h:148-157`、`:223`；§6.3 |
| 列表从哪来 | 任务自建 | 列表由**池**按 view 顺序租借、帧末统一回收 | `ForwardRenderer.cpp:685-696`；`RHICommandList.h:159-167` |
| 提交是不是纯 append | 命令流 append 进全局命令表即可 | 提交路径写**全局**状态：per-object ring 游标 `m_PerObjectWriteIndex++`、`BindForDraw` 重建 material 绑定集 | `EngineSceneBindingSets.cpp:210`、`SceneMeshDrawUtils.cpp:70-123` |
| 顺序由什么决定 | 由 task index 拼接决定 | 由**提交顺序**决定（ring 槽位即语义） | §2.1 的 `l` 步、§3 #14 |

结论：**规则照搬，载体不照搬。**「worker 纯 CPU、GPU 只在图形角色」在我们这里是同一条规则
（`TaskSystem.h:150-151` 的 worker 契约、§4.1 的类型级排除）；但「一份命令流属于一个任务」这个前提
在我们的 `RHICommandList` 上不成立，而要把窗口、池、ring 游标一起改成可并发，那是另一个 Feature
（§11.2 第三行、§13-1）。

#### 11.4.5 四列收尾：UE 概念 → 本仓对应物 → 取 / 不取 → 依据

| UE 概念（记忆性描述） | 本仓对应物 | 取 / 不取 | 依据（file:line） |
|---|---|---|---|
| 每视图一份可见性数据，视图是顶层任务单位 | 每 view 一次装配扇出，仍在各自 `Execute` 内 | 取（形状） | `ForwardRenderer.cpp:669`、`ManualRenderer.cpp:329` |
| primitive 区间切任务、每任务只写自己那段 | `MeshAssemblyChunk` 的 `[FirstProxy, FirstProxy + ProxyCount)` | 取 | `RenderQueueBuild.h:56`、§4.3 |
| 「每任务最多处理多少」的上限参数 | `MinProxiesPerChunk` + `workerCount * 2` 上界 | 取（概念）；数值由 S05 测 | §7.4 |
| 阶段一产「可见命令索引」、阶段二归组 | 片直接产 `MeshDrawCommand`，无索引层 | 不取索引层 | `MeshPassProcessor.cpp:21-37`、§4.3 |
| 按 state bucket 稳定排序 | `MeshDrawCommandSortKey`（material / VIL / VB 折叠）+ `stable_sort` | 已等价，本 Feature 不动 | `MeshDrawCommandSort.cpp:10-36`、`:46-58` |
| 并行合并严格按 task index | I2：按 `FirstProxy` 升序逐片 append | 取 | §5.3、§5.4 |
| 每 primitive 缓存命令 + 脏标记失效 | 无缓存；命令 = 一次结构体拷贝 | 不取（先测再定） | `MeshPassProcessor.cpp:21-37`、§12.2-5 |
| 可见性变化只改引用列表 | 每帧重建命令向量，但重建成本 = 拷贝 | 目标等价、成本结构不同 | `MeshDrawCommandSort.cpp:46-58`、§5.1 |
| 渲染线程生成命令、RHI 线程执行 | 渲染角色装配 + 同一角色窗口内提交 | 取规则，不取 RHI 线程分层 | §2.1、§6.1 |
| `FParallelCommandList`（worker 录命令流） | `RHICommandList` 每线程录制窗口 + 池 + 全局 ring 游标 | **不取** | `RHICommandList.h:148-157,223`、`EngineSceneBindingSets.cpp:210` |
| worker 只碰纯 CPU 数据 | 装配入口签名里没有 `RHICommandList&`、没有 renderer | 取（类型级，不靠运行期检查） | §4.1、`TaskSystem.h:150-151` |

---

## 12. 风险与未决问题

### 12.1 风险（每条一行，含「最可能先炸的地方」）

| # | 风险 | 最可能先炸的地方 |
|---|---|---|
| K1 | 合并顺序写错（逆序 / 按完成序 / 按片大小）→ 同 key 命令换位 → 偶发闪烁 | `BuildPrimitiveRenderQueueChunked` 的合并循环；靠 R2/R3 与 60 帧 digest 抓 |
| K2 | 有人把装配挪进录制窗口 | 未来某次「把 `BuildRenderQueue` 提到更靠近提交处」的重构；靠 §8.3 的 `IsRecordingWindowOpenOnThisThread` 断言抓 |
| K3 | 某天有人给 `GetMeshBatches` 的 override 加上「回头问组件」的读取 | minCraft 的 `ChunkMeshSceneProxy`（外部仓）；靠 §8.3 的 identity-token 断言 + review |
| K4 | 分片数随 worker 数 / 机器漂移，导致同一场景在不同机器上 digest 不同 | 分片数泄漏进 key 或顺序；靠 R6 |
| K5 | `ParallelFor` 静默拒绝被当成成功 | 编辑器宿主自驱帧循环绕过角色；靠 R4（§7.2 补跑） |
| K6 | 小场景变慢（扇出 + join 的固定成本） | 200–500 primitive 场景；靠 `MinProxiesPerChunk` 下限 + S05 测量 |
| K7 | 阴影两段顺序被压成一段 | `ShadowCasterQueue` 的合并写法；靠 R8 |
| K8 | 「每 proxy 恰好走一条分支」的假设被打破（`RenderQueueBuild.h:84-97` 是 if/else 二选一） | 未来加「主视可见 + 强制入阴影」双路时；分片必须保持每 proxy 只走一条 |
| K9 | ~~编号冲突扩散~~ **已解决**：登记为 `RND-F31`，`FEATURE_REGISTRY.md` 已录入 | — |

### 12.2 未决问题（需决策）

1. ~~编号~~ **已确定**：`RND-F31`，文件名为 `RND-F31_MESH_DRAW_COMMAND_TASK_ASSEMBLY_DESIGN.md`，
   `FEATURE_REGISTRY.md` 已录入（状态 `Design`）。
2. **Single 拓扑默认是否并行**：Single 下 `RHICommandList` 是 immediate，帧延迟更敏感，但并行装配
   仍是纯 CPU。推荐：**默认允许**，开关默认 On 且可关，用 S05 的测量定稿。
3. **多 viewport 是否做「跨 view 一次扇出」**：`RenderSystem::Tick` 手上已有全部 `m_PendingDraws`
   （`RenderSystem.cpp:229`），可以在任何 view 开窗之前把所有 view 的装配合并成**一次**扇出
   （4 个 viewport 时省 3 次 join，总工作量不变）。代价：`CollectOrphanedSceneProxies` 的调用点要从
   `Execute` 提到 `Tick`（每 scene 一次），且 `Execute` 要接受「已装配好的 ctx」。
   推荐：**本 Feature 不做**，作为独立切片 S06 评估 —— 它是编辑器多 viewport 的真正大头，但会改帧结构。
4. **是否把 `BuildScreenUIDrawQueue` 也纳入**：推荐不做（数据量小）。
5. **每 primitive 缓存的触发条件**：等 S05 的 profile 数据。若装配耗时中 >50% 在 `GetMeshBatches`
   的虚调用与结构体拷贝，则「按代理缓存 `MeshDrawCommand`」比并行更值；若在剔除数学，则并行是正解。
   **先测再定。**
6. **`m_ShadowPass.m_OpaqueQueue = ctx.ShadowCasterQueue`（`ForwardRenderer.cpp:673`）这次全量 copy**
   是否在 S04 一并去掉，需要确认 shadow pass 的生命周期不会因此缩短。

---

## 13. 明确不做的事

1. **不做 `FParallelCommandList` 式「worker 上录制 RHI 命令流」**（§11.2）。
2. **不做每 primitive 的缓存 mesh draw command + 脏列表**（§11.2、§12.2-5 待测）。
3. **不做 GPU 驱动剔除 / Nanite / GPU Scene**。
4. **不新增剔除层级**（层级 / 实例化 / HISM 式）；子 mesh 剔除仍由 game 侧 proxy 决定（`RND-F26` 契约）。
5. **不改排序语义**、不新增排序键、不把 `stable_sort` 换成不稳定排序。
6. **不把 chunk 产物跨帧缓存**：`m_BonePalette` 是借用（§4.4），产物寿命 = 本帧。几何句柄与布局句柄虽由命令自己持有（跨帧不悬垂），但 per-object 绑定集是立刻写入的，所以"本帧消费"这条约束照旧。
7. **不做「本帧装配 N+1、录制 N」的流水**（§6.2）。
8. **不并行化** `CollectOrphanedSceneProxies` / `FlushPendingProxyDeletes` / PSO 创建 / ring 槽位写入 /
   UBO 上传 / RDG `Bake` / 任何 `RHICommandList` 调用。
9. **不引入第二个串行路径**：串行 = `ChunkCount == 1`（G5、§4.1）。
10. **不为并行给 `RenderScene` / `PrimitiveSceneProxy` 加锁**：锁会把「唯一 mutator 是渲染角色」这个
    已经成立的事实换成每帧争用，并掩盖真正需要断言的假设（§8.3）。
11. **不动 minCraft**（外部仓，只读）。
12. **本切片不写实现代码**：本文档只有签名级示意。
13. **不为消费者侧做任何改动**：minCraft 的 chunk 网格构建 / 地图生成并行化不在本 Feature 内，
    本节之后的 §14 只做契约可复用性说明与前置条件推导，真正的消费者切片另行登记（见 §14.4）。

---

## 14. 同一套契约怎么用到消费者侧（minCraft 的 chunk 网格构建 / 地图生成）

> 本节的地位：**设计推断与约束推导，不是承诺。** 不写 minCraft 代码、不改本仓代码，也不改 §1–§13 的
> 任何结论。它要回答一个问题：本 Feature 立下的几条不变量，到底是不是「渲染专用件」，还是任何
> 「只读输入 → 分片 → 确定性汇聚」的 CPU 工作都适用。
> 依据一律是本仓 `file:line`；minCraft 内部结构不在本文引用（外部仓只读，见 §13-11）。

### 14.1 为什么这些不变量对消费者同样适用

| 不变量 | 本 Feature 的依据 | 换到 chunk 网格构建 / 地图生成为什么仍然必须 |
|---|---|---|
| **worker 只碰纯 CPU 数据**（无 GPU、无资产写、无场景 authority） | `TaskSystem.h:150-151` 的 worker 契约；§4.2 的「worker 禁止读」清单 | chunk 构建要读地形数据、要产出几何缓冲，正好压在「资产写」与「GPU 创建」两条线上。这条规则不因换了消费者而放松，因为放松它的代价（后台线程驱动图形 API）与消费者无关 |
| **产物必须是可交接的快照**，不是指向活对象的借用 | §4.4-1 / §4.4-3：几何由 ref 强持才算可跨阶段交接；`m_BonePalette` 这种借用被明文禁止出帧（§13-6） | chunk 网格在 worker 上产出后要经过「排队 → 上传 → 绑定」才被消费。产物若借用某个地形块的内部指针，块卸载时它就悬垂；POD 自持是唯一能跨这几步的形态 |
| **GPU 创建只在图形角色** | `RHIDeferredCreateQueue::Post` 是「任意线程提交、图形角色执行」（`RHIDeferredCreate.h:252-256`），drain 越角色会被拒并计数（`:283-299`），调用点是 `Engine.cpp:480` | chunk 的 VB / IB 上传不能在最深的那个 worker 里做，只能走这条既有延迟创建通道；消费者不需要、也不应该发明第二条 |
| **失败不得静默** | `ParallelFor` 被拒时只打一条日志就返回（`TaskSystem.cpp:974-990`）；对策是**效果校验**而非意图校验（§7.2 的 `VisitCount` 补跑） | 渲染漏一条 draw 是画面少一块；chunk 漏一格是地形空洞，而且要等玩家走过去才被发现。同一套「跑过的证据 + 就地补跑 + 计数」是唯一不靠人眼的结构性对策 |
| **结果顺序确定** | I1（分片覆盖）+ I2（按片序合并），§5.3 | chunk 的合并顺序会进空间索引、碰撞体列表或渲染批，顺序变了就是可测差异；没有 I1/I2 的同构物，连「并行与串行等价」这句话都写不出来 |
| **降级必须走同一条实现** | G5：串行 = `ChunkCount == 1`（§4.1） | 地图生成在编辑器里可能跑在单线程宿主上。若为此维护第二份生成器，两条路径会各自演化出不同的产物——§4.1 拒绝的正是这件事 |

### 14.2 一个 chunk 网格构建并行化的形状建议（只给形状，不给实现）

复用本 Feature 的三段式：**只读快照 → 扇出 → 串行交接**。四步如下：

| 步骤 | 输入（只读） | 产出 | 谁执行 | 与本 Feature 的对应 |
|---|---|---|---|---|
| ① 快照 | 一个 chunk 的地形 / 方块数据副本；相邻 chunk 的边界数据（跨块面需要） | 交给任务的只读视图 + 一个版本号或「本帧不变」的证明 | 编排线程（Game） | §4.2 的「只读输入」表；抓取手法同 `PrimitiveSceneProxy.h:42-49`（创建时抓、之后只读缓存） |
| ② 扇出 | 上述快照 | 每 chunk 一份 `{顶点数组, 索引数组, 包围盒, 非空标志}`——**自持 POD** | worker（`ParallelFor`，一片一 chunk 或一片若干 chunk） | §4.1 的 `MeshAssemblyChunk` 的同构物 |
| ③ 串行交接 | 各 chunk 的 POD 产物 | ① 上传：`RHIDeferredCreateQueue::Post`（`RHIDeferredCreate.h:252`），drain 在图形角色（`Engine.cpp:480`）② 绑定 / 命令：沿用既有持有链（录制流自己持 ref，`RHICommandList.h:224-234`） | 串行（编排线程或图形角色） | §4.4 的寿命结论 |
| ④ 收尾 | — | 卸载旧网格、出统计 | 串行 | §3 #17 的同构物（销毁必须最后） |

**两条纪律（与 §5 同源）：**

1. **切点按 chunk 索引切，不按「顶点数」切**。切点必须是稳定、可枚举的区间（I1 的同构物），否则
   「并行与串行等价」没有可比对的锚点，digest 也无从写起。
2. **合并按 chunk 索引升序**（I2 的同构物）。哪一片先算完不影响合并顺序——这与 §5.2 的结论同构：
   顺序由输入顺序承载，不由完成顺序承载。

**需要新增的观测点（都落在既有设施上，不新建体系）：**

| 观测点 | 落在哪 | 抓什么 |
|---|---|---|
| 片没跑（被拒 / body 抛异常） | 与 §8.2 同族的一条 loss 计数，但**是消费者自己的计数器**，不进渲染的 `FrameWorkLossCounters` | 静默漏 chunk |
| 延迟创建队列深度 | `RHIDeferredCreateQueue::GetPendingCount()`（`RHIDeferredCreate.h:405`）+ 既有 stall 告警（`:619` 报「排队数与最老等待毫秒」） | 上传供给跟不上构建速度——内存峰值的前兆 |
| 队列顺序有没有被破坏 | 队列本身保证「新的不越过旧的」（`:349-355`），所以顺序问题应当表现为**断言**而不是观测 | 有人改成乱序插入 |
| 构建耗时 | `ME_PROFILE_SCOPE`（与 `Render.MeshAssembly` 同样的用法，但**不同 scope 名**，否则串并行对比会把两件事混在一起） | 串行 vs 并行的收益 |

### 14.3 风险与前置条件

| # | 风险 / 前置条件 | 为什么在消费者侧更尖锐 | 依据（file:line） |
|---|---|---|---|
| C1 | **并行产出的顺序影响 chunk 合并顺序** | 渲染侧的顺序只影响同 key 命令的先后（多数情况肉眼不可见）；chunk 合并顺序一旦进了空间结构或碰撞体表，就是可见、可测的差异 | §5.3 I1 / I2 的同构要求 |
| C2 | **上传时机与帧边界** | chunk 构建不在渲染帧内、结果要跨若干帧才成形，所以「产物必须本帧消费」这条渲染约束**不能照搬**；能照搬的是「GPU 创建只在图形角色、并且有一个明确的 drain 点」 | `RHIDeferredCreate.h:283`、`Engine.cpp:480`；§4.4-3 只是本 Feature 的约束 |
| C3 | **内存峰值（多个 chunk 同时在飞）** | 渲染装配的中间产物在一帧内就释放；chunk 的在飞产物要跨帧存活，而 `kMaxInFlightFrames = 3` 约束的是帧，不约束消费者自己的队列深度 | `TaskSystem.h:168`、`:257`；`RHIDeferredCreate.h:405` 的队列深度 |
| C4 | **读地形数据与「数据可变」的冲突** | 渲染侧靠「唯一 mutator 是渲染角色 + 同步 join」（§4.4-4）成立；地图数据可能同时被游戏角色或编辑器改，所以消费者必须**先做快照或版本号**，不能沿用「join 期间没人写」这个假设 | §4.4-4 的前提；抓取手法同 `PrimitiveSceneProxy.h:42-49` |
| C5 | **卸载与在飞构建的竞态** | 与 H2 同构：chunk 被卸载时可能正有 worker 在算它的网格；渲染侧靠「装配区间内不得 flush」（§8.3）化解，消费者需要自己的等价断言 | §2.2 的 H2 行、§8.3 |
| C6 | **降级不是「就不并行了」，而是「必须仍产出完整结果」** | 消费者侧漏 chunk = 地形空洞，用户直接看见；比渲染漏 draw 更晚被发现 | §7.2 |

**前置条件（缺一条就不要开工）：**

1. **快照成本先测过**：如果复制地形数据的成本已经接近或超过构建本身，并行拿不到收益——这与 §12.2-5
   「先测再定」是同一条纪律。
2. **上传路径确认走 `RHIDeferredCreateQueue`**，不得新建第二条 GPU 创建通道（否则 §13-1 的教训会在
   消费者侧重演：多一条「听起来更快」的路，就多一处没人维护的语义分叉）。
3. **顺序契约有可执行判据**：digest 或等价物，且必须能红（§10 R1 / R3 的做法）。
4. **卸载路径与在飞构建的关系有断言**，而不是靠注释（§8.3 的做法）。

### 14.4 边界声明（这一段是本节的结论）

- **本 Feature 不为消费者侧做任何改动。** 不改 minCraft（外部仓只读，§13-11），不改本仓 Task 系统、
  `RHIDeferredCreateQueue`、也不改任何 chunk 相关代码。§14 只说明契约可复用，不产出实现。
- **真正的消费者切片另行登记**：chunk 网格构建 / 地图生成并行化会拿到自己的 Feature ID，走自己的
  Design → 评审 → 切片流程。它复用本节的**形状与纪律**，不继承本 Feature 的 slice 计划（§9）。
- **若消费者切片发现某条契约不通用**（最可能是「产物必须本帧消费」，见 C2），修正应落在**契约的表述**
  上——改本文件相应段落并在消费者文档里写明差异——而不是在消费者侧悄悄绕过。

---

*（本文档至此结束：§1–§13 为本 Feature 的设计，§14 为契约可复用性说明，不含任何实现承诺。）*


