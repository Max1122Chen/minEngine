# CORE-F47_FRAME_THREAD_CONTRACTS Design Spec

## Meta
- **ID:** `CORE-F47`
- **Type:** Refactor
- **Complexity:** L2
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-10-27
- **Revision:** v2 — 依据对 UE 源码的实测调查重写。v1 提出的三层补丁式机制（延迟释放队列 / present 边界销毁 / 资源所有权基类）被判定为"在源头之后打补丁"，目标改为对齐 UE 的三条硬约束。
- **Related:** [Feature Registry](../FEATURE_REGISTRY.md) · [CORE-F45](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md) · [CORE-F46](./CORE-F46_TASK_SYSTEM_UNIFIED_FACADE_DESIGN.md) · [System Review](./FRAME_THREADING_SYSTEM_REVIEW.md) · [BUG-RENDER-016](../bugs/BUG-RENDER-016.md)

## TL;DR

帧线程分离之后，引擎缺的是**三条硬约束**，不是三个机制：渲染侧不持游戏对象、代理删除两段式、GPU 资源的创建与销毁只在渲染侧。UE 用这三条把问题消灭在源头；我们此前用延迟队列、present 边界、引用计数补偿在源头之后打补丁，于是每修一处就长出新的一处。本设计按 UE 的顺序重建契约，并明确删除此前新增的补丁层。

## Problem

三条崩溃链都指向同一件事——所有权没有一次画清：

1. **渲染侧解引用游戏对象**：包围盒从组件现取，组件在游戏线程销毁后渲染线程读到已释放内存。
2. **渲染侧持有游戏对象的所有权**：代理持材质所有者（`shared_ptr<MaterialInterface>`），于是材质可能在渲染线程被销毁，只能再造一条"交还游戏线程"的队列兜底。
3. **GPU 资源没有线程归属**：`RHICreateBuffer` 在调用线程立即执行 GL；多线程下游戏线程没有当前上下文，得到的对象名字为 0，绘制时把非法状态交给驱动。同时 GL 的 buffer 与 VAO 从不删除（后端没有删除调用）。

## Design Source — UE 的做法（实测，非记忆）

| 事实 | 证据 |
|---|---|
| SceneProxy 由游戏线程创建（`FScene::AddPrimitive` 内调 `CreateSceneProxy`），由 `FPrimitiveSceneInfo` 拥有；渲染线程只做 `SetTransform` 与 `CreateRenderThreadResources` | `RendererScene.cpp:1376`、`PrimitiveSceneInfo.h:273` |
| SceneProxy **不拥有** GPU 缓冲：资源在资产的 render data，底层是引用计数句柄 `FBufferRHIRef = TRefCountPtr<FRHIBuffer>` | `StaticMeshSceneProxy.h:265`、`RenderResource.h:486`、`RHIFwd.h:104` |
| SceneProxy 的回指字段叫 `ComponentForDebuggingOnly`，注释明写渲染线程禁止解引用；渲染侧身份用 `FPrimitiveComponentId`；包围盒是自持快照 | `PrimitiveSceneProxy.h:1672-1677`、`:1615-1628` |
| proxy 删除两段式：移除命令只标记 `bPendingDelete` 并入队，本帧 scene update 末尾统一 `delete Proxy` | `RendererScene.cpp:5226-5231`、`:6427-6445` |
| 命令不带帧号：单队列 FIFO 定序，一批命令一个 task；背压来自帧末 fence，保证游戏线程不超前 N-1 帧；队列无界、不丢、不阻塞 | `RenderingThread.cpp:1522-1553`、`:2026-2158` |
| 场景改动在帧首应用（`OnRenderBegin` 内调 `FScene::Update`），早于可见性计算与录制 | `SceneRendering.cpp:3994` |
| RHI 资源引用计数在资源自身；归零只入队；唯一的 `delete` 在 RHI 线程、位于提交批次尾部；平台层再延迟一次 | `RHIResources.h:73-98`、`RHIResources.cpp:71`、`RHICommandList.cpp:1234-1254` |
| 资源创建只能是命令列表操作，线程保证 = 调用约定 + 断言（`IsTopOfPipe`、`IsInRenderingThread`） | `RHICommandList.h:767/797`、`RHICommandList.cpp:1284` |

## Hard Constraints（本设计的全部内容）

### H1 — 渲染侧不持游戏对象

- 场景代理只保存渲染侧自有数据：变换快照、世界与局部包围盒、可见性位、绘制范围、组件身份（数值）。
- 代理对组件的指针只允许做指针相等比较；命名与注释都比照 UE 的 `ComponentForDebuggingOnly`，写明渲染线程禁止解引用。
- 代理不持有任何 `MEObject`（材质、资产）的所有权。渲染侧需要的材质对象必须是渲染侧自有、引用计数的类型。
- 推论：不需要"把对象交还游戏线程销毁"的队列——那个队列的存在只是因为违反了本条。

### H2 — 代理删除两段式

- 移除命令在渲染角色只做三件事：置"待移除"标记、把代理从绘制列表摘掉、登记进本帧待删容器。
- 真正的 `delete` 发生在**本帧 scene update 结束处**，保证"由它产出的 draw command 不可能再被提交"。
- 场景销毁前必须先把所有 primitive 移除干净（比照 UE 的 `checkf(Primitives.Num() == 0)`），不做兜底批量删除。
- 推论：不需要"帧保活容器"这类补偿机制。

### H3 — GPU 资源的创建与销毁只在渲染侧

- 创建：所有 GPU 对象创建必须表达为渲染角色的操作；在非渲染角色上创建是**断言级错误**，不是静默失败。
- 销毁：句柄引用归零时只登记；真正的 `glDeleteBuffers` / `vkDestroyBuffer` 由 RHI 角色在提交批次尾部执行，更深的 GPU 使用期由后端自己的 fence 管理。
- 线程约束靠断言与调用约定钉死，不靠"软守卫跳过绘制"。

## Requirements

- 组件侧接口形态不变：仍是 `CreateSceneProxy` / `SyncSceneProxy` / `MarkRenderStateDirty`，不引入第二套注册路径。
- 单一实现路径：不保留"新的对 + 旧的在"双轨；被替换的机制要删除，删不掉的登记债务并写清移除条件。
- 可验证：每条约束都要有能在 headless 下跑的回归用例，且能构造出违反状态并证明会被抓住。
- 规模适中：不引入 UE 的 `FPrimitiveSceneInfo` 双层结构与 SoA 数组（那是为万级 primitive 服务），一层代理加一个所有者容器足够。

## Non-Goals

- 不复刻 UE 的 dispatch/translate/submit 三段任务链与并行 translate。
- 不引入 BYPASS 模式，不引入条件编译式的资源信息位域。
- 不引入无界命令队列加帧末全量阻塞（我们保留可观测的丢弃计数）。
- 不做 GPUScene 类的批量提交体系。

## Target Architecture

### 所有权域

| 域 | 所有者 | 内容 | 与其他域的交界 |
|---|---|---|---|
| 游戏对象域 | 游戏线程 | 组件、资产、材质对象 | 只把值快照与渲染侧对象交给渲染域 |
| 渲染域 | 渲染线程 | 场景代理、渲染侧材质对象、绘制列表 | 只持有渲染域自己的对象与 GPU 句柄 |
| RHI 域 | RHI 角色 | GPU 原生对象 | 只接受渲染域投递的创建与销毁请求 |

### 一帧的时序（目标）

| 阶段 | 游戏线程 | 渲染角色 | RHI 角色 |
|---|---|---|---|
| 1 | 组件变更，标记脏 | — | — |
| 2 | 投递命令包（顺序 FIFO，不带帧号） | — | — |
| 3 | 提交本帧 | **应用场景变更（早于录制）** | — |
| 4 | — | 剔除、排序、录制命令列表 | — |
| 5 | — | 提交命令列表 | 执行 |
| 6 | — | **帧末：删除待删代理** | 提交批次尾部：删除退休资源 |
| 7 | 帧末 fence：不超前 N-1 帧 | 发出完成信号 | — |

关键差异（相对现状）：**应用在录制之前**（现在在之后，导致变更晚一帧可见）；**代理删除在帧末**（现在在移除命令内就地析构）。

## What Changes

| 现状 | 目标动作 |
|---|---|
| 代理持 `shared_ptr<MaterialInterface>` | 删除；材质改为渲染侧引用计数对象，代理持其句柄 |
| `DeferredObjectRelease` 队列 | 删除（H1 落地后无存在理由） |
| `RHINativeResourceOwner` 基类 | 删除（队列归 RHI 全局，资源不背队列指针） |
| `RHIDeferredDestroy` 的固定帧延迟 | 保留骨架，判据改为提交批次尾部 |
| `RHICreateBuffer` 在调用线程立即执行 | 改为渲染角色执行，并在非渲染角色上断言 |
| 代理就地析构于移除命令 | 改为两段式：标记入队，帧末删除 |
| 帧保活容器 | 删除（H2 落地后无存在理由） |
| 场景变更在录制之后应用 | 改为录制之前应用 |
| 帧号所有权、小于等于 drain、过期丢弃 | 降级为诊断层：保留计数器与世代断言，但不再是正确性的承重结构 |
| 任务系统异常边界、关停顺序、启动失败传播 | 保留（纯增益） |
| 导出快照增量门禁 | 保留（与本设计无关） |

## Slices

| 编号 | 内容 | 验收 |
|---|---|---|
| **P-1** | 崩溃诊断可信化：修正符号化或接 minidump；两条 RHI 报错补"创建线程 + 创建调用栈" | 能把一次真实崩溃定位到引擎帧，或明确证明故障只在驱动线程 |
| **P1** | 应用时刻前移到录制之前；补帧末完成信号（游戏线程不超前 N-1 帧） | headless：变更当帧可见；超前会被 fence 挡住 |
| **P2** | 代理删除两段式；场景销毁前要求清空 | headless：移除命令执行后代理仍存活，帧末被删除；删除后不可能再提交它产出的命令 |
| **P3** | 材质渲染侧化：渲染代理成为渲染侧引用计数对象，代理不再持 `MEObject` | headless：材质资产在游戏线程释放后代理仍可安全绘制，且无跨线程销毁 |
| **P4** | GPU 资源线程约束：创建走渲染角色并断言；删除按提交批次尾部；删 `RHINativeResourceOwner` | headless：非渲染角色创建会断言；删除按批次发生；无泄漏 |
| **P5** | 可选：RT 侧按 (对象, 变更类型) 去重；唤醒改"空到非空一次" | 同帧同对象多命令只应用最后一条 |

### P4a 实现说明（创建侧）

P4 的创建侧先落地，销毁侧（批次尾部判据、删除 `RHINativeResourceOwner`）仍属 P4b。已确定的语义：

| 项 | 落地方式 |
|---|---|
| 队列位置 | `RHIDeferredCreateQueue` 是 `RHI` 实例的成员，与 `RHIDeferredDestroyQueue` 对称：同一后端的句柄不会由一个设备创建、由另一个设备销毁。类型与角色判定在 `Runtime/Function/Render/RHI/RHIDeferredCreate.h`。 |
| 入队判据 | `RHIGraphicsRole::IsCurrent()`：无 `FrameThreading` 实例（测试 / 离线 cook / headless 工具）视为已在图形角色；否则要求 `IsInRenderingThread() \|\| IsInRHIThread()`。Single 拓扑在初始化时把三个角色绑到同一线程，因此其游戏线程走立即分支，不引入额外延迟。 |
| drain 时机 | `Engine::TickRendererFrame` 的第一段代码、命令应用与 `RendererTick` 之前，入口是 `RHI::RHIDrainDeferredCreates()`（内部 `DrainOnGraphicsRole()`）。非图形角色调用被拒绝并计数，不尝试执行。帧序因此固定为"资源创建 → 命令应用 → 录制 → 帧末删除"：本帧录制读到的句柄一定是本帧已实体化的。 |
| 顺序 | FIFO。失败（本次帧首建不出原生对象）的请求留在队首原序，下一帧重试；它不阻塞排在它后面的请求。 |
| 未实体化的表达 | `RHIDeferredCreateTarget::IsReady()`（`RHIBuffer` / `RHIVertexInputLayout` / `RHITexture` 都有）。未就绪时原生名字为 0，`UpdateSubresource` 被拒绝并计数报错；绘制的守卫仍是既有的"名字 0 不画"。 |
| 初始数据 | 投递时复制。驱动对象在另一线程创建，调用方的内存不可达，句柄因此持有 CPU 副本直到实体化，之后释放。 |
| 关停 | 丢弃 + 计数 + Error 日志（`AbandonPending`，由 `~RHI` 调用）。队列里的请求已没有可用的帧，实体化只会立刻被销毁队列回收。留下的句柄仍然有效，只是永远 `IsReady() == false`。 |
| 不自锁 | 创建队列的 drain 不等待任何线程：它在本线程同步执行自己那张表的回调，且只在图形角色上运行。游戏线程从不阻塞等待它。因此 Single（同线程）与 Multi（另一线程）都不会互相等待。 |
| 范围 | 本片覆盖 buffer / vertex input layout / texture。shader / PSO / binding set 与 sampler 由 P4c 补齐（见下）。Vulkan 的设备调用不依赖"当前上下文"，因此保持立即创建，队列在那里没有要修的东西。 |

### P4b 实现说明（销毁侧）

销毁侧已落地：删除 `RHINativeResourceOwner`，判据从固定帧延迟改为提交批次尾部。已确定的语义：

| 项 | 落地方式 |
|---|---|
| 判据 | 每次投递记下"此刻已提交的批次数"（`RetiredBatch`）；批次尾部把该计数 +1，并销毁所有 `RetiredBatch` 更小的条目。等价于"这条句柄被某个批次带出去之后才删"：录制期间归零的句柄由**本帧**这个批次带出，就在本帧尾部删除；批次之间归零的等下一个批次。不再是"过了 N 帧"，因此帧率抖动与"一次录制跨多个 present"都不改变正确性。 |
| 提交点 | OpenGL：`RHIPresent` 内 `SwapBuffers` 之后（无窗口时该后端逐条下发命令，present 点即批界），入口 `RHI::RHISubmitBatchCommitted()`。Vulkan：`EndFrameRecordingAndSubmit()` 返回值表示 `vkQueueSubmit` 是否真的发生；为真才调 `RHISubmitBatchCommitted()`，为假调 `RHINoteFrameWithoutSubmit()`。Single / Multi 同一套语义：两条路径都只在"批次已交给平台"之后调用同一个入口。 |
| 无提交兜底 | 明确不发明边界。某帧没有提交（无窗口 / 最小化 / 提交失败）时只计数（`GetSkippedSubmitCount`）并触发滞留看门狗；真正的出口只有两个：下一个提交批次，或关停时 `DrainAll()`（`~RHI` 用 `AbandonPending` 报告泄漏）。 |
| 滞留可观测 | 队列长度 `GetPendingCount`、最久等待 `GetOldestPendingAgeMs`、累计投递 `GetRetiredCount`、累计销毁 `GetDestroyedCount`、拒收 `GetRefusedDrainCount`（非 RHI 角色调用）+ `GetPostedWithoutQueueCount`（无 RHI 时投递）、滞留告警次数 `GetStallWarnCount`。超过 `SetStallWarnIntervalMs`（默认 2000ms）时按间隔限频 Warn，不是每帧一条。 |
| 所有权图 | 资源只持 CPU 侧数据与 `RHIDeferredCreateTarget`；投递者 = 资源析构函数（任意线程，经 `PostNativeDestroy` 全局入口）；接收者 = 当前存活 `RHI` 实例注册到全局槽位的那条 `RHIDeferredDestroyQueue`；执行者 = RHI 角色在批次尾部。`RHI` 构造注册、析构先注销再 `AbandonPending`，因此"RHI 早于句柄死亡"仍然只报错计数、不跨线程 UB。 |
| 单设备不变量 | 资源不再知道自己是哪个设备的，因此全局槽位只保留一个活跃队列；同时存活两个 RHI 属于契约违反，注册时报 Error（不静默错投）。 |
| Vulkan 二次延迟 | 不变：队列回调把句柄交给 `VulkanRHI` 的 fence 有界 retired list，GPU 使用期由 fence 兜底。 |

### P4c 实现说明（材质编译产生的 GPU 对象）

材质编译在游戏线程同步进行，但它创建的 GPU 对象必须由图形角色创建：Multi 下游戏线程没有 GL 上下文，特化会直接失败（真实故障：精灵材质编译失败 → 屏幕 UI 不显示）。P4c 把这条路径并入 P4a/P4b 的既有通道，不新增第二套机制。

| 项 | 落地方式 |
|---|---|
| 走通道的对象 | `RHIShader` / `RHIGraphicsPipelineState` / `RHIShaderBindingSetLayout` / `RHIShaderBindingSet` / `RHISampler` 都继承 `RHIDeferredCreateTarget`，后端工厂一律经 `RHI::ScheduleNativeCreate`。OpenGL 侧真正带原生对象的是 shader（`glCreateShader` + `glSpecializeShader` + `glLinkProgram`）与 sampler（`glGenSamplers`）；PSO / binding set / layout 在 OpenGL 只是数据，`IsReady()` 恒真，通道因此直接放行而不是排一条帧首空转。 |
| 就绪判据 | `ScheduleNativeCreate` 先看 `IsReady()`：已就绪（刚在图形角色内联建成，或本就没有原生对象）不入队。未就绪且非图形角色才投递。Single 拓扑的调用线程即图形角色，走立即分支，零额外延迟。 |
| "编译完成但未实体化" | 三态可查：`IsReady() == false && HasNativeCreateFailed() == false` 为**已排队**；`IsReady() == true` 为**已创建**；`HasNativeCreateFailed() == true` 为**终态失败**。材质侧对应 `Material::HasCompiledProgram()`（编译完成）/ `IsProgramPendingMaterialization()` / `HasProgramCreateFailed()`，而 `IsCompiledForDraw()` 只有程序**确实存在**时才为真。发布快照里的 `bCompiledForDraw` 记的是"编译完成"这一编译期事实（快照在游戏角色取，看不到之后的帧首），程序是否已创建由句柄现问。 |
| 失败上报 | 只报一次，报在**看见驱动答复的那条线程**上（`OpenGLRHIShader::ReportCreateFailure`），带驱动原文。队列不再重试（`RHICreateOutcome::Failed`）也不重复记日志，只计数 `GetFailedCount()`。调用方：`MaterialCompiler::Compile` 对"已排队"返回成功（编译本身没失败）；`SpriteMaterialFactory` 在模板程序终态失败时补一条"该材质不可绘制、相关绘制被跳过"的解释，每个缓存模板一次。 |
| 调用方拿不到即时日志 | `RHICreateShader` 在未实体化时把 `outCompileLog` 填成明确的"已排队、日志稍后在句柄上可读"，而不是空串（空串读起来像成功）；`Material::GetShaderCompileLog()` 改为每次读取都从句柄刷新，所以稍后再问拿到的是驱动原文。 |
| 滞留告警 | 与删除队列同风格：最久等待超过 `SetStallWarnIntervalMs`（默认 2000ms）按间隔限频 Warn，计数 `GetStallWarnCount()`、`GetOldestPendingAgeMs()`。 |
| 帧序保证 | drain 在录制之前，因此**游戏 tick 内投递的对象在该帧录制前一定已实体化**——"未实体化"窗口对录制路径不可见，只对同一 tick 内的游戏侧消费者可见。唯一会把它latch住的消费者是屏幕 UI 代理的 `m_bVisible`（填充时算一次、不再重算），故改为判"编译完成"；真正的绘制守卫（`SceneMeshDrawUtils` 的 `IsCompiledForDraw()`）仍是严格判据。 |
| 消费者判据（绘制侧） | "未实体化"不是"加载失败"：`RHIDeferredCreate.h` 补三态查询 `GetDeferredUse()` 与每帧决策 `RHIDeferredConsumerGate`（首次跳过报一次、每 `kSkipReportInterval` 帧提醒一次、终态失败只报一次）。`SkyBoxPass` / `DebugDrawPass` 的 `Initialize` 只把"根本没有句柄"当加载失败；`Prepare` 用 gate 判此刻能否命名对象，未就绪则当帧跳过（不关 pass），`HasNativeCreateFailed()` 时 Error 一次并让 pass 永远关闭。`SkyBoxPass::IsReady()` 同步收紧成"这次绘制要用的对象此刻都在"，排队中的程序不再被当成可绘制。 |
| 同步捕获侧 | `EnvMapCapture` 的三处着色器判据同样三分解：无句柄 = 加载失败；`HasNativeCreateFailed()` = 驱动拒绝（带原文）；`!IsReady()` = 程序尚未创建——一次录制 + 回读的捕获不能等帧首，因此报"程序还没创建、需在图形角色创建后重跑"，而不是把它读成加载失败。 |
| 未纳入 | 着色器编译缓存（`EngineShaderUtils` 的程序缓存）按句柄缓存，已排队句柄同样命中；`RHIPipelineLayout` 在两个 OpenGL/Vulkan 后端的工厂里都未改（OpenGL 无原生对象，Vulkan 保持立即创建）；shader resource view 仍是纯 CPU 描述。 |

### P1 实现说明（应用时刻前移）

场景命令的应用从"录制之后"前移到"录制之前"，与 UE 的 `OnRenderBegin → FScene::Update` 早于 InitViews 一致。`Engine::TickRendererFrame` 的顺序即本帧的完整时序：

| 步 | 动作 | 线程 | 说明 |
|---|---|---|---|
| 1 | `RHI::RHIDrainDeferredCreates()` | 渲染角色 | 上一游戏 tick 投递的 GPU 对象在此实体化。 |
| 2 | 读 `GetCompletedGameFrame()`、`CheckPublishedGenerationForDrawnFrame` | 渲染角色 | 世代检查刻意放在 drain **之前**：drain 是移动 `m_DrawnFrame` 的那一步，放在之后比较的就不再是"上一帧画的世界"。 |
| 3 | `RenderCommandPipe::FlushApplyForFrameChecked(completedFrame)` | 渲染角色 | 应用本帧的场景变更。放在录制之前，因此"变更当帧可见"。 |
| 4 | `TakeViewIntents(completedFrame)` → `ReplacePendingDraws` | 渲染角色 | 本帧要画的视图。 |
| 5 | `RendererTick` → `RenderSystem::Tick` → `ForwardRenderer::Execute` | 渲染角色 | 剔除、排序、录制命令列表；Multi 把命令列表交给池，RHI 角色 replay。 |
| 6 | `RenderScene::FlushPendingProxyDeletes()` | 渲染角色 | 帧末删除第 3 步停车（park）的代理。 |
| 7 | present（OpenGL `SwapBuffers` / Vulkan `vkQueueSubmit`） | 渲染角色或 RHI 角色 | 提交批次；GPU 侧退休资源由批次尾部销毁。 |
| 8 | `SceneManager::FlushRetiredScenes()`（`Engine::CompleteFrameReleases`） | 游戏角色 | 退役场景的销毁点。 |

"应用阶段依赖录制结果"的耦合检查过：`FlushApplyForFrameChecked` 调用的每个 `Apply*_RT` 只写 `RenderScene` 的所有者与绘制列表、以及代理自身的快照字段；录制只读这些字段，没有任何 apply 路径读绘制队列、命令列表或视图意图。反过来录制依赖应用后的状态——这正是前移的目的。

删除点选在"录制结束、present 之前"而不是"present 之后"：`ForwardRenderer::Execute` 已经把本帧全部绘制翻译成命令列表（`MeshDrawCommand` 只持 GPU 句柄与材质渲染对象指针，不含场景代理指针），此后没有任何步骤再读场景代理；Multi 下 present 在 RHI 角色，渲染角色无法观察也不应等待它，而 GPU 侧生命期本就由 `RHIDeferredDestroyQueue` 的批次尾部兜底。因此"本帧 draw command 不会再生效"的判据是**录制已结束**，不是提交已发生——这样既不需要跨角色同步，也不依赖 present 的可观察性。

#### P2 实现说明（代理删除两段式）

移除命令不再就地析构代理，帧末删除因此成为唯一的删除点，两槽 keep-alive 补丁随之删除。

| 项 | 落地方式 |
|---|---|
| 移除命令 | 只做三件事：置 `m_bPendingRemove`、把代理从绘制列表摘掉、把 `unique_ptr` 从所有者容器 move 进本帧待删容器（按图元 / Widget / 灯光 / 天空盒各一条容器）。容器里的代理仍被拥有、仍存活，且不再可能产出一条新的 draw command。 |
| 登记与删除点 | 首次入容器时把场景登记进 `RenderScene` 的帧末列表。`Engine::TickRendererFrame` 在 `RendererTick` 返回之后调用 `RenderScene::FlushPendingProxyDeletes()`（见 P1 说明的帧序）：录制是本帧唯一读取场景代理的步骤，它已经结束，本帧的移除也在这一步之前的 drain 里应用完毕；pipe 只向前推进，游戏角色只能把新命令标到更晚的帧。因此此刻"没有任何命令还能命名这些代理"。 |
| 为何安全 | 删除点释放的只有 CPU 侧对象：场景代理持有的 GPU 缓冲与材质渲染对象的引用在这里 drop，原生对象经 `RHIDeferredDestroyQueue` 退休，由本帧这个提交批次的尾部销毁。录制期的 draw command 只借用 GPU 句柄（`MeshDrawCommand` 只有 `RHIBuffer*` / 顶点输入布局 / 材质渲染对象指针，不含场景代理指针），录制一结束就没有读者；GPU 侧的使用期由提交批次尾部兜底，因此"提交"在渲染角色不可观察（Multi 下 present 在 RHI 角色）不影响正确性。 |
| 待删容器的前置条件 | 帧末删除前断言：容器里的每个代理都带移除标记、且已不在任何绘制列表上。违反它意味着"仍可被本帧提交的 draw command 命名"，属于两段式被拆掉。 |
| 场景退役：移除先行 | `SceneManager::RetireScene` 在入队前对场景里每个图元 / 灯光 / Widget / 天空盒组件调用它自己的移除入口（`RenderScene::Remove*` + 组件侧 `DetachSceneProxy`），因此场景的销毁从"已经没有代理"开始，比照 UE 的 `checkf(Primitives.Num() == 0)`。retire 不是销毁点：`SceneManager::FlushRetiredScenes` 只在场景的 `RenderScene` 既无所有者也无待删代理（`HasPendingProxyWork()`）时才销毁它，否则留着等下一个帧边界——退役闸口因此多一个条件，而不是少一个。移除命令在**下一个帧首的 apply** 被应用，没有第二套 drain 入口。 |
| 关停时的 drain | 关停时（帧角色已停，本线程在此刻即渲染角色）`SceneManager::Shutdown` 先把队列里的移除按"命令能标到的最晚帧"应用掉，再走退役闸口销毁；因此 PIE 退出 / 场景卸载不再产生 `~RenderScene` 的"仍有 primitive"上报。线程不满足时只报 Error 并由 `~RenderScene` 如实上报，不跨角色删除。 |
| 角色 | `IsInRenderingThread` 之外调用被拒收：计数 + 限频 Error，代理继续挂在待删容器里，不跨角色删除。 |
| 同帧新增又移除 | `RecreatePrimitive` 的移除命令与新代理的 add 命令按 FIFO 应用：旧代理进待删容器，新代理进所有者容器与绘制列表。帧末只清空容器，因此新代理不会被误删；两者也不是同一对象，不会泄漏。 |
| 无绘制帧 | 删除点不依赖绘制：它挂在渲染帧末，无窗口 / 最小化 / 无绘制队列的帧同样走到。超出 2000ms 才 Warn 一次，说明帧没走到结尾（按等待时长判定，不按帧数）。 |
| 场景销毁 | `~RenderScene` 发现仍有未移除（或仍滞留）的代理时打 Error 并给出四类计数（比照 `checkf(Primitives.Num() == 0)`），不做兜底批量删除：剩余所有者随场景释放。 |
| 删除的补丁 | `m_RemovalKeepAlive` / `m_PreviousRemovalKeepAlive` 及 `CollectOrphanedSceneProxies` 里的轮转一并删除；该函数只剩绘制列表的清理，所有者只经移除命令离开。 |
| 导出 | 只新增 `RenderScene::FlushPendingProxyDeletes`（2997 → 2998），无符号消失。 |
| 场景析构：移除也不能交给组件 | `~Scene` 在拆 GameObject 之前自己调 `DetachSceneRenderState`。组件一旦失去所属 GameObject，`GetOwningRenderSceneIfPresent()` 就答不出它属于哪个渲染场景，于是只清自己的代理指针、不入移除命令，代理留在绘制列表上。真实日志：`BlockItemIconBake`（1 图元）在运行期被释放、PIE 世界（52 图元）在关停时被销毁，都是这条路。 |
| 最后一个强引用消失 | 所有者直接丢引用（不经过退役）时，`~Scene` 把仍持有代理的 `RenderScene` 交给帧末：`HandOverOrphanedRenderScene` 入登记表，`FlushPendingProxyDeletes` 在"应用已把它清空"之后释放它——移除先行的顺序不变，帧末仍是唯一删除点；还留着所有者（移除命令属于更晚的帧）就继续等，因为那条命令要写进这个场景。 |
| 关停时的同步 drain | 帧角色已停时没有 drain 会应用退役队列里的移除，也没有帧末跑删除。`SceneManager::Shutdown` 在销毁退役场景之前对每个场景调 `DiscardRenderSceneProxiesForTeardown`（标记 + 摘绘制列表 + 立即销毁，两步合并），并清空无主 `RenderScene` 表。不泄漏的理由：此刻没有帧在飞、pipe 之后只会被清空不再应用、设备仍存活，代理释放的 GPU 句柄直接进 RHI 退役队列，由 `RenderSystem::Shutdown` 在销毁设备前排空。 |
| 关停顺序：先释放渲染侧句柄 | `Engine::ShutdownSystems` 把 AssetManager / ObjectManager / GameplayTagManager 放到 `RenderSystem::Shutdown` 之前。材质、纹理等对象的代理析构把原生句柄投给 RHI 退役队列，投在设备之后就没有销毁者了（真实日志：`openGL vertex input layout ... no RHI was alive`）。 |
| 设备退场前排空队列 | `RenderSystem::Shutdown` 在 `m_RHI->Shutdown()` 之前调 `RHIDrainAllDeferredDestroys()` 并检查 `GetPendingCount()`；仍有残留时按 Error 上报，而不是让 `~RHI` 静默放弃。 |

### P3a 实现说明（数据自持）

P3 拆两片：P3a 只做"代理自持绘制数据"，不改所有权形态（`Material` / `MaterialInstance` 仍用
`unique_ptr` 拥有代理，场景代理仍持 `shared_ptr<MaterialInterface>`）；P3b 才换成渲染侧引用计数的
材质对象并删除延迟释放队列。P3a 已落地，语义如下：

| 项 | 落地方式 |
|---|---|
| 快照 | `MaterialRenderProxyData`：编译世代、四个绘制标志、shader / skinned shader / binding set layout 的引用计数句柄、标量表（名字 + 槽位 + 值）、纹理表（槽位 + `RHITextureRef` + 两个采样器候选）。不可变，不含任何 `MEObject`。 |
| 发布 | 游戏角色写自己的表并把代理标脏；`Engine::TickLogicalFrame` 末尾对全部存活代理做一次 `PublishPendingGameData`（`MaterialRenderProxyPublisher`），由它用一次 `std::atomic_store` 换入新快照。一次 tick 内的多次 `Set*` 合并成一次快照。编译与 `CreateRuntimeInstance` 例外：立即发布。 |
| 渲染侧读取 | 帧内每处读取做一次 `std::atomic_load`（`AcquirePublishedData`），读到的块与它命名的 GPU 句柄一起被引用住。渲染侧不再调用任何返回游戏对象的接口。 |
| 脏标记 | 跨线程裸 bool 已消除：渲染侧用"快照序号 + 编译世代"判断是否需要重建，两个计数都只由渲染角色读写。 |
| GPU 侧重建 | `BindForDraw` 按世代自愈：世代变化丢掉整套 GPU 状态并重建一次；快照只改标量时只重写 UBO；布局、纹理、采样器、UBO 指针都没变时沿用已建好的 binding set。 |
| 父链 | 实例代理持父**代理**的指针（不是父材质），实例发布时把父快照的槽位摊平并叠加自己的覆盖项；父快照序号变化会让实例重新发布，因此父材质重编译后实例照样解析正确。 |
| 纹理 | 参数发布时解析成 `RHITextureRef` 与采样器 desc 的值拷贝；每 tick 比较一次游戏侧纹理的当前指针对，纹理晚到或采样器变化都会触发重新发布。 |
| 场景代理 | `PrimitiveSceneProxy` / `WidgetSceneProxy` 在绑定材质时把代理指针解析好，绘制收集不再对材质做虚调用。 |

P3b 已落地（见下节）。仍未搬走的一次性 GPU 创建：材质编译里的 binding set layout 创建。

### P3b 实现说明（引用计数材质对象）

P3b 把最后一段游戏对象所有权移出渲染侧，`DeferredObjectRelease` 因此失去存在理由并被删除。语义如下：

| 项 | 落地方式 |
|---|---|
| 句柄 | `MaterialRenderProxyRef = std::shared_ptr<MaterialRenderProxy>`（声明在 `MaterialInterface.h`）；新增 `MaterialInterface::GetRenderProxyRef()` 供渲染侧取引用，`GetRenderProxy()` 仍返回裸指针，绘制侧一行未改。 |
| 持有者 | `Material` / `MaterialInstance` 各持一份（`EnsureRenderProxy` 经 `MakeMaterialRenderProxy` 创建并登记）；`PrimitiveSceneProxy` / `WidgetSceneProxy` 持 `m_MaterialSource` 与 `m_PreviousMaterialSource` 两份；`RenderScene` 的两槽 keep-alive 也改为持代理引用。 |
| 析构 | 最后一个引用归零时：离开发布登记表（一把互斥锁、弱引用表，任何角色都安全）、释放本对象持有的 GPU 句柄（各自投递 RHI 退役队列，按提交批次尾部销毁），**不触碰 `ObjectManager`、不读游戏对象**——代理不是 `MEObject`。 |
| 资产先死 | `~Material` / `~MaterialInstance` 调 `DetachFromOwner()`：代理断开 owner 指针并停止发布，保留最后一份已发布快照继续供绘制读取；否则下一次 tick 的批量发布会走已释放的表。 |
| 发布登记 | 登记表改存 `weak_ptr`：末次引用可能在渲染角色归零，发布扫描必须「先锁住再跳过已销毁条目」，不能对半析构对象做虚调用。 |
| 删除清单 | `Core/Object/DeferredObjectRelease.{h,cpp}`、`MaterialRenderProxyHandle.h`、`deferred-release` 测试套件；`Engine::CompleteFrameReleases` 与 `SceneManager::Shutdown` 只保留退役场景那一半。 |
| 导出 | 新句柄类型与虚函数全部内联实现，导出计数 2998 → 2997，只少 `GetDeferredObjectReleaseQueue`，无新增。 |
| 交给 P2 | `RenderScene` 两槽 keep-alive 曾在此保留，续命的是 draw command 里的裸代理指针；P2 落地后移除命令不再就地析构代理，两槽已随 P2 删除（见上节）。 |

## Risks

- P1 改变可见性时序，可能与依赖"晚一帧"的现有行为冲突（编辑器 UI、拾取）。
- P3 触及材质系统，影响面大；必须保证 `MaterialInstance` 的动态参数路径不受损。
- P2 改变 `RenderScene` 容器结构，移除路径与 `RecreatePrimitive` 的语义要同时收敛。
- 崩溃可能确有驱动侧诱因（三条记录的故障栈全在驱动工作线程内、无引擎帧），P-1 必须先证明我们能定位。

## Verification Strategy

- 每个切片都必须先写能构造违反状态的 headless 用例，再改实现。
- 断言路径必须可在测试中受控触发（返回诊断串或受控钩子），不依赖"跑一次看看崩不崩"。
- 真实编辑器验收：minCraft 单线程与多线程各跑不少于 30 秒，进出 PIE 数次，观察无崩溃且 chunk 正常绘制。

## Open Questions

- 崩溃是否完全由我们的非法 GL 状态引起，还是存在驱动侧独立缺陷——blocking：否，但影响 P-1 的验收表述。
- `MaterialInstance` 的动态参数更新是否要求渲染侧对象可被游戏线程寻址——blocking：是（P3 前置调研）。
- 帧末 fence 的深度（渲染线程完成 / RHI 提交 / 交换链）——blocking：否，先取"渲染线程完成"。

## Design Review

- **Verdict:** Pending（待维护者 review 本 v2）
- **Reviewer / date:** —

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Ownership and lifetime resolved（三条硬约束）
- [x] Alternatives considered（v1 补丁式 vs 本版约束式）
- [ ] Unresolved questions resolved or explicitly deferred

**Ready for implementation?** P-1 / P1 / P2 可以直接开始；P3 需先做 `MaterialInstance` 调研。