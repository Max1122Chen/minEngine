## 2026-10-29 — 帧角色身份表原子化 + 作用域真正恢复（`ScopedFrameThreadKind` 收口）

- **问题：** `ScopedFrameThreadKind` 析构里只有 `(void)` 两行：构造时把当前线程临时绑成某角色，**离开作用域永不恢复**，此后该线程永久通过该角色的所有断言；身份表 `m_ThreadIds[]` 与 `m_ThreadBound[]` 是两个字段，由角色线程写、任意线程读，无原子无 fence → 可以读到"已绑定 + 旧 id"的组合。
- **新身份模型：** 每个角色一个 `std::atomic<std::thread::id>`。默认构造的 id 对任何运行线程的 id 都不相等，所以它本身就表示"无人持有"——持有者与持有状态是**一个**值，一次 acquire 读到的要么是完整的持有者，要么是无人持有，不存在撕裂态。绑定 release 存、查询 acquire 读、认领与交还走 acq_rel 的 compare-exchange。
- **作用域契约：** 只认领**无人持有**的角色（先 load 看，再 CAS 认领：两线程抢同一空角色只有一个赢）；退出时 CAS(自己 → 无人持有) 把角色交还。嵌套同一角色是 no-op（由外层释放），嵌套不同角色各自独立、按层退出。**抢占**别的线程持有的角色 → `ME_ENSURE` 计数并拒绝，角色留在原处；作用域活过授予它的实例、或交还时角色已易主 → 同样计数并跳过交还（绝不写回把它抢回来）。
- **`StopFrameRoles` 之后仍把调用线程绑成三角色（保留，判断与理由）：** join 已证明旧持有者不再运行，此时进程回到启动时的单线程拓扑，编辑器 teardown 在调用线程上释放 RHI 资源必须能过角色门禁；改成"不绑"只会把合法的关停期内联图形工作变成拒绝 + 资源泄漏。代价是关停后调用线程的角色断言恒真，所以把它显式化：停帧日志现在写明调用线程已持有 Game/Render/RHI，代码处也写清这是 handover 而不是抢占。
- **红灯：** 先写 4 例（作用域结束恢复、两层嵌套、并发认领/查询、跨线程抢占计数）。旧实现下"—离开作用域后 `GetThreadId(Render)` 仍是调用线程"首例即红（`CHECK( 0xdc == thread::id of a non-executing thread )`）。用例运行器带 `abort-after=1`，一次只暴露首个失败断言，嵌套例与抢占计数例由同一处缺失（析构空实现、认领路径无计数）决定；并发例在旧实现下不会失败（写序恰好是"先 id 后 bound"），它钉的是新契约的不变量。
- **验证：** `minEngineTests` / `Editor` 构建 exit 0；`frame-threading` 13 例 / 85 断言（原 9 例 / 49 断言）、`task-system` 23/163、`contract-violations` 7/46 全绿（在 `minEngine/bin` 下跑）；`test smoke` exit 0（24 套件）；导出 3003 → 3003（增量 0，无符号消失）。
- **残留：** 身份表只存 tid，**分不出"持有者已经退出"和"持有者还活着"**——角色线程退出却不交还角色时，之后的临时认领会按抢占被拒（`TaskSystem::StopFrameRoles` 正常路径会交还，生产路径不触发）；`BindCurrentThread` 仍是无条件占用（角色线程用它声明所有权，不做仲裁）；"Multi 下可以临时认领空闲角色"这条契约写在头文件里，没有任何编译期或运行期手段强制调用方说明理由。

## 2026-10-29 — 录制期上传成为命令：`RHICommandList::UpdateBuffer`（TD-045 收口）

- **根因：** `RHIBuffer::UpdateSubresource` 在**调用线程立即**执行 `glBufferSubData`，不进命令流；Multi 整帧先录制后 replay，于是任何录制期上传都排在"回放时才被读"的 draw 之前。屏幕空间 per-frame 块那次（世界消失）只是这块地基上最先塌的一处，机制本身仍在。
- **接口：** `RHICommandList::UpdateBuffer(buffer, data, offset, size)`。录制期把 payload `memcpy` 进命令流，replay 时才在正确的顺序位置调用真正的上传原语；立即模式（Single / 非录制列表）原样直通——不拷贝、不排队，行为与改前逐字节相同。
- **副本归属：** 命令列表自持 `std::vector<std::vector<uint8_t>>` 副本块（一次录制里一个块对应一次上传，块只增不减），`Reset/BeginRecording` 只把游标归零 → 池把同一个列表发给下一帧时块被复用，稳态帧不再分配，也没有每帧一次的堆申请。
- **防回归：** 新增"录制窗口"（thread_local 计数，`BeginRecording` 进入、`Replay` 开头关闭、`Reset` 兜底退出）。后端原语（OpenGL/Vulkan `UpdateSubresource`）在窗口内被直接调用时计入 `RHICommandList::GetImmediateUploadWhileRecordingCount()` 并报一条 Error，指向 `UpdateBuffer`。之所以用计数+日志而不是 `__debugbreak`：断言宏只有硬陷一种档位（TD-042），一次无害的旧调用会直接杀掉进程。
- **迁移：** per-frame / 灯光块（`ForwardRenderer::UpdatePerFrameUBO/UpdateLightUBO`）、屏幕 UI 块、后处理参数块、天空盒帧块（graph 与 Execute 两条路径）、debug 顶点流、`EngineSceneBindingSets` 的 per-object 与骨骼 ring（`WriteNextPerObjectModel` 补 `cmdList` 形参，`ShadowPass` 调用点跟进）、材质 scalar 参数块。**未迁移**：`ForwardRenderer::BuildShadowDrawCommands` 与 `ShadowUniformBuffers` ring 的写入——它们在 `BeginRecording` 之前执行，不在录制窗口内，此刻命令列表还是 Idle，没有可排的位置。
- **红灯：** 先按旧写法（录制期直接 `UpdateSubresource`）写"上传 A → draw → 上传 B"，`rhi-command-list` 首例即失败：`CHECK( buffer.Text() == std::string(4u, '\0') ) is NOT correct! values: CHECK( BBBB == )` —— 录制还没结束，字节已经是 B，draw 只能读到 B。改到 `UpdateBuffer` 后 GREEN。
- **测试：** `rhi-command-list` 4 例（顺序、payload 副本语义、立即模式不变、旁路计数），其中旁路计数一例用真实的 `OpenGLRHI` + 头显 GL 缓冲跑真后端原语，能看到窗口内一次 Error 且 replay 的那次上传不计。
- **Verification:** `minEngineTests` / `Editor` 构建 exit 0；`rhi-command-list` 7 例 / 45 断言、`screen-ui-per-frame-ubo` 2/17、`material-proxy-data` 8/149、`scene-proxy-lifetime` 11/177、`scene-proxy-render-read` 3/59、`render-graph` 11/382 全绿；`test smoke` exit 0；导出 2998 → 3000（+2：`UpdateBuffer` 与 `GetImmediateUploadWhileRecordingCount`，snapshot 已重生成），无符号消失。
- **残留：** 录制期直接调原语的检测只在后端原语处生效（Vulkan 的两个编译分支都接上了，但本机只有 OpenGL 配置实跑过）；录制命令捕获的是裸 `RHIBuffer*`，缓冲必须活到 replay（与既有 `SetVertexBuffer` 等命令同一约定）；帧录制开始前的阴影 UBO 预热仍是立即写，Multi 下与上一帧 replay 的跨帧窗口未处理。

## 2026-10-29 — 着色器延迟创建被消费者读成"加载失败"（两条真实错误）

- **现象（真实日志，Multi 拓扑）：** `DebugDrawPass: failed to load DebugDraw shader.` / `SkyBoxPass: failed to load background SPIR-V shader.` 各一条 Error，天空盒与 debug 绘制整条被禁用，但程序在下一个帧首其实建成了。
- **根因：** 两个 pass 在 `Initialize`（非图形角色）里把"当刻 `IsValid()`"当加载失败判据。延迟创建通道下"已排队未实体化"的句柄 `IsValid() == false`，正常状态因此被判成终态失败并关掉整条 pass。
- **修法：** `RHIDeferredCreate.h` 补三态查询 `GetDeferredUse()` 与每帧决策 `RHIDeferredConsumerGate`（首次跳过报一次、每 120 帧提醒一次、终态失败只报一次）。`Initialize` 只把"根本没有句柄"（读文件 / 编译失败）当加载失败；`Prepare` 用 gate 判此刻能否命名对象，未就绪当帧跳过（不关 pass），`HasNativeCreateFailed()` 时 Error 一次并让 pass 永远关闭。`SkyBoxPass::IsReady()` 收紧成"这次绘制要用的对象此刻都在"，排队中的程序不再被当成可绘制。
- **同类审计：** `EnvMapCapture` 三处同步捕获同样三分解（无句柄 / 驱动拒绝 / 尚未创建）——一次录制 + 回读的捕获不能等帧首，报"需在图形角色创建后重跑"而不是"加载失败"；Vulkan 无延迟创建通道（`ScheduleNativeCreate` 未使用）、材质与精灵侧早已三分解、OpenGL 后端内联创建处的 `IsValid()` 本就是终态判据，均无需改。
- **测试：** `rhi-deferred-create` 新增 3 例（排队 = Pending 且不报错、帧首 drain 后 = Usable 并恢复绘制、驱动拒绝 = Unusable 且只报一次）。红灯复现：把 `GetDeferredUse` 换回旧判据（未就绪即 Unusable）重编译，首例即 `CHECK( decision.Use == RHIDeferredUse::Pending )` 失败（`values: CHECK( 2 == 1 )`）。
- **Verification:** `minEngineTests` / `Editor` 构建 exit 0；`rhi-deferred-create` 18 例 / 154 断言（新增 3 例 / 31 断言）、`screen-ui-per-frame-ubo` 2/17、`material-proxy-data` 8/149、`scene-proxy-lifetime` 11/177、`scene-proxy-render-read` 3/59、`scene-retirement` 10/243 全绿；`test smoke` exit 0（23 套件）；导出 2998 → 2998（增量 0，无符号消失）。
- **残留：** pass 侧的接线（`Prepare` 里那次 gate 调用）只有编译 + smoke 覆盖，判据本身由单测钉住；真实编辑器 Multi PIE 手工验收未执行；帧首 drain 被拒 / 停摆时的跳过与提醒只有在真实停滞下才会出现，未做人为注入演练。

## 2026-10-28 — 屏幕空间 per-frame 块独立（多视口 PIE 世界消失的修复）

- **根因：** `ForwardRenderer` 只有一个 per-frame UBO，ScreenUI pass 在**录制期**把像素正交写进去；Multi 整帧先录制后 replay，早先录下的 3D draw 到 replay 时读到正交矩阵 → 世界消失（Single 恰好不受影响，因为 UI 写在 3D 已 replay 之后）。
- **修复：** 屏幕空间拿到自己的块（`ForwardRenderer::m_ScreenUIPerFrameUniformBuffer`）+ 自己的 set0（`EngineSceneBindingSets::SetScreenSpacePerFrame` 只在该 pass 录制期间路由 per-frame 绑定，set0 按角色分缓存）。`PerFrameData` 移到 `RenderPipeline/ScenePerFrameData.h`；正交块由 `ScreenUIPass::MakeScreenSpacePerFrameData` 产出（纯数据，可直接断言）。
- **Single 不变：** UI 读到的块内容逐字节等于旧写法（identity View + `ScreenUICoords::MakePixelOrthoProjection`，绑定仍是整块），像素结果不变。
- **测试：** 新套件 `screen-ui-per-frame-ubo`（2 case / 17 断言）钉住正交契约与"块不共享存储"；`Tests/Fixtures/ScreenUIPerFrameUboProbe` 编译真实 `ScreenUIPass.cpp` + `EngineSceneBindingSets.cpp`，修复前 **RED（3 项失败）**、修复后 **GREEN（8/8，含 set0 绑定路由）**。
- **遗留：** 录制期 `UpdateSubresource` 仍绕过命令列表 → `TECH_DEBT` TD-045，站点清单见 probe README。

## 2026-10-28 — CORE-F47 收尾：退出阶段两条真实错误（场景销毁仍有图元 / 设备早于渲染侧句柄）

- **现象（真实日志，minCraft-render 16:55–16:58）：** `RenderScene destroyed with 1 primitive`（`BlockItemIconBake`，运行期被释放的工装场景）、`RenderScene destroyed with 52 primitive`（关停时被销毁的 PIE 世界）、`openGL vertex input layout: last reference dropped while no RHI was alive`。
- **第一条错误的路径：** `~Scene` 先 `m_GameObjects.clear()`。组件析构时已经问不到所属渲染场景（`GetOwningRenderSceneIfPresent()` 走 GameObject→Scene 链，链正在拆），于是只清自己的代理指针、不入移除命令；代理留在绘制列表上，接着 `m_RenderScene.reset()` 直接带着它们析构。补法：`~Scene` 在拆 GameObject 之前自己调 `DetachSceneRenderState`（从 SceneManager.cpp 的文件内静态挪成可共享声明，导出零增长）。
- **第二条错误的路径：** 关停时帧角色已停，退役队列里的移除没有 drain 会应用、也没有帧末跑删除；`FlushRetiredScenes` 于是把场景留在队列里，最后由 SceneManager 析构释放——52 个代理在这时随场景一起释放。补法：`SceneManager::Shutdown` 在销毁退役场景前对每个场景调 `DiscardRenderSceneProxiesForTeardown`（标记 + 摘绘制列表 + 立即销毁），并清空无主 `RenderScene` 表。
- **最后一个强引用消失的出口：** `~Scene` 把仍持有代理的 `RenderScene` 交给帧末（`HandOverOrphanedRenderScene` → `FlushPendingProxyDeletes`），移除先行不变，帧末仍是唯一删除点；还留着所有者就继续等。
- **关停顺序：** AssetManager / ObjectManager / GameplayTagManager 移到 `RenderSystem::Shutdown` 之前；`RenderSystem::Shutdown` 在 `m_RHI->Shutdown()` 之前调 `RHIDrainAllDeferredDestroys()` 并检查 `GetPendingCount()`，残留按 Error 上报。
- **Verification:** `minEngineTests` / `Editor` 构建通过；`test smoke` exit 0（23 套件）；`scene-retirement` 10 例 / 243 断言（新增 2 例）、`scene-proxy-lifetime` 11/177、`rhi-deferred-destroy` 14/297（新增 1 例）全绿；导出 2998 → 2998（增量 0，无符号消失，consumer coverage PASS）。
- **残留：** 渲染角色在运行期被停的顺序未改（任务边界外）；真实编辑器 Single PIE + 关停手工复跑待执行；无主 `RenderScene` 若出现"没有移除命令可等"的代理，只有关停 drain 会清它。

## 2026-10-27 — CORE-F47 P4c：着色器 / PSO / 绑定集 / sampler 的创建并入延迟创建通道

- **起因（真实日志）：** Multi 下 PIE 起来但 UI 完全不显示，312 条 `Vertex SPIR-V specialize failed`。材质编译在游戏线程，而 `glCreateShader` + `glSpecializeShader` 仍在调用线程执行；P4a 只搬了 buffer / VAO / texture，shader / PSO / binding set 明确留作残留。
- **机制（不是新机制）：** `RHIShader` / `RHIGraphicsPipelineState` / `RHIShaderBindingSetLayout` / `RHIShaderBindingSet` / `RHISampler` 继承 `RHIDeferredCreateTarget`，OpenGL 工厂一律经 `RHI::ScheduleNativeCreate`；`IsReady()` 已就绪者直接放行（本就没有原生对象的 OpenGL PSO / binding set / layout 因此不排空转），未就绪且非图形角色才投递。shader 与 sampler 拆出 `DeferredNativeCreate` 构造函数：只复制 SPIR-V / 采样描述，特化、链接与 `glGenSamplers` 交给图形角色。
- **中间态：** `IsReady()` / `IsValid()` / `HasNativeCreateFailed()` 三态；材质侧 `HasCompiledProgram()` / `IsProgramPendingMaterialization()` / `HasProgramCreateFailed()`，`IsCompiledForDraw()` 仅在程序确实存在时为真。发布快照的 `bCompiledForDraw` 记编译期事实，程序是否已创建由句柄现问——快照因此不必在帧首之后重发就能从不可绘制转为可绘制。
- **失败不再静默：** 失败只上报一次，报在看见驱动答复的线程上并带原文；`RHICreateOutcome::Failed` 让请求离队不再重试，队列只计数。`RHICreateShader` 的 `outCompileLog` 在未实体化时给出明确的"已排队、稍后可在句柄上读到"；`Material::GetShaderCompileLog()` 每次读取从句柄刷新。`SpriteMaterialFactory` 在模板程序终态失败时补一条"相关绘制被跳过"的解释（每模板一次）。
- **调用方语义：** `MaterialCompiler::Compile` 对"已排队"返回成功（编译没失败）；`Material::CreateRuntimeInstance` 与 `CompileSkinnedVariant` 改判"编译完成"，因此一次编译后的派生/变体请求不会因为程序晚一帧而失败。屏幕 UI 代理的 `m_bVisible` 与两个组件的 `EnsureRuntimeMaterial` 从"可绘制"改判"编译完成"：该标志填充一次后被 latch，判严格可绘制会把"程序还没创建"永久冻成不可见。绘制守卫（`SceneMeshDrawUtils`）仍是严格判据。
- **滞留可观测：** 与删除队列同风格——最久等待超过 `SetStallWarnIntervalMs`（默认 2000ms）限频 Warn，计数 `GetStallWarnCount()` / `GetOldestPendingAgeMs()`。
- **Verification:** `minEngineTests` / `Editor` / `SampleEditor` 构建通过；`test smoke` exit 0（23 套件全绿）；`rhi-deferred-create` 15 例 / 123 断言（新增 6 例）、`material-proxy-data` 8 例 / 149 断言（新增 1 例）、`material-ir` 5/79、`scene-proxy-lifetime` 11/177、`screen-ui-text` 1/2、`sprite-translucency` 2/7、`scene-proxy-render-read` 3/59、`material-compile-cache` 8 例、`texture-sampler` 7 例、`rhi-command-list` 3 例、`sprite-runtime-consume` 3 例、`rhi-deferred-destroy` 13/287、`render-command-pipe` 9/6914 全绿；导出 2998 → 2998（增量 0，无符号消失），导出门禁（growth / 移除基线 / 三方桶 / consumer coverage）、GL 函数表装载守卫、渲染线程状态审计、编辑器模块边界审计全部 PASS。顺带修掉 GL 装载守卫指出的既有漏判 `OpenGLRHIBuffer::CreateNativeBuffer`（同一后端文件，与本轮 shader 改动相邻）。
- **残留：** `RHIPipelineLayout` 与 shader resource view 未走通道（前者两个后端都无 OpenGL 原生对象、Vulkan 保持立即创建）；屏幕 UI 的 latch 修复只有单测覆盖到判据、`FillSceneProxy` 那一行的接线为静态审阅；真实编辑器 Multi PIE 手工验收待执行。
## 2026-10-27 — CORE-F47 P1：应用提前到录制之前 + 代理删除点移到帧末 + 场景退役移除先行

- **帧序（`Engine::TickRendererFrame`）：** 资源创建（`RHIDrainDeferredCreates`）→ 世代检查 → **命令应用**（`FlushApplyForFrameChecked`）→ 视图意图 → **录制**（`RendererTick`）→ **帧末删除**（`RenderScene::FlushPendingProxyDeletes`）。场景变更因此当帧可见，不再晚一帧。
- **删除点：** 从"应用之后"移到"录制之后"。判据是**录制已结束**而不是 present 已发生：`ForwardRenderer::Execute` 结束即已把绘制翻译成命令列表（`MeshDrawCommand` 只持 GPU 句柄与材质渲染对象指针，不含场景代理指针），Multi 下 present 在 RHI 角色不可观察，GPU 侧生命期由 `RHIDeferredDestroyQueue` 的批次尾部兜底，因此不新增跨角色同步。
- **断言：** 帧末删除前断言容器内每个代理都带移除标记且已不在任何绘制列表上（"仍可被本帧提交的 draw command 命名"即两段式被拆掉）。
- **场景退役：** `SceneManager::RetireScene` 先对场景内图元 / 灯光 / Widget / 天空盒走各自的移除入口并清组件侧句柄，再入退役队列；`FlushRetiredScenes` 只在场景的 `RenderScene` 既无所有者也无待删代理（`HasPendingProxyWork()`）时销毁，否则等下一个帧边界——退役闸口多一个条件而不是少一个。关停时（帧角色已停、本线程即渲染角色）先 drain 队列再走闸口，PIE 退出 / 场景卸载不再产生 `~RenderScene` 的"仍有 primitive"上报。
- **Verification:** `minEngineTests` / `Editor` 构建通过；`test smoke` exit 0（31 套件）；新增用例：`scene-proxy-lifetime` 11 例 / 177 断言、`scene-retirement` 8 例 / 213 断言；`render-command-pipe` 9/17298、`rhi-deferred-create` 9/63、`rhi-deferred-destroy` 13/287、`material-proxy-data` 7/125 全绿；导出 2998 → 2998（增量 0，无符号消失）。
- **Next:** 帧末完成信号（游戏线程不超前 N-1 帧）仍属 P1 剩余项；真实编辑器 PIE 手工验收。

## 2026-10-27 — CORE-F47 P3b：材质渲染代理改为渲染侧引用计数对象（删延迟释放队列）

- **Scope:** 代理所有权从 `Material` 内的 `unique_ptr` + 场景代理的 `shared_ptr<MaterialInterface>` 改为 `MaterialRenderProxyRef`；删除 `DeferredObjectRelease` 与 `MaterialRenderProxyHandle`
- **Completed:**
  - `MaterialInterface::GetRenderProxyRef()`（新增，虚函数内联）；`Material` / `MaterialInstance` 各持一份代理引用
  - `PrimitiveSceneProxy` / `WidgetSceneProxy` 持 `m_MaterialSource` + `m_PreviousMaterialSource` 两份代理引用；`RenderScene` 两槽 keep-alive 同样改为持代理引用
  - 代理析构：离开发布登记表（弱引用表 + 互斥锁）+ 释放 GPU 句柄（RHI 退役队列），不碰 `ObjectManager`
  - `~Material` / `~MaterialInstance` 调 `DetachFromOwner()`：资产先死时代理冻结在最后一份快照，不再发布
  - 删除 `Core/Object/DeferredObjectRelease.{h,cpp}`、`MaterialRenderProxyHandle.h`、`deferred-release` 套件；`Engine::CompleteFrameReleases` 只留退役场景
- **Verification:** `minEngineTests` / `Editor` 构建通过；`test smoke` exit 0；`material-proxy-data` 7/125、`scene-proxy-lifetime` 5/79、`scene-retirement` 5/162、`scene-proxy-render-read` 3/59、`sprite-translucency` 2/7、`screen-ui-text` 1/2、`material-ir` 5/79 全绿；导出 2998 → 2997（只少 `GetDeferredObjectReleaseQueue`，无新增）
- **Next:** P2 代理删除两段式（落地后可删 `RenderScene` 两槽 keep-alive 与 `MeshDrawCommand` 里的裸代理指针）

## 2026-10-27 — CORE-F47 帧线程契约：按 UE 对齐重做（P-1 / P4a / P4b / P3a / P3b / P2 / P1）

- **起因：** 多线程升级后连续出现三类真实崩溃（包围盒回指已销毁组件、材质渲染代理悬垂、驱动内空指针），复盘结论是"缺三条硬约束，不是缺三个机制"。
- **取经：** 实测 UE 源码（Component↔SceneProxy、RHI 资源生灭、命令与帧边界），确认三条约束：渲染侧不持游戏对象、代理删除两段式、GPU 资源创建/销毁只在渲染侧。
- **P-1 诊断可信化：** 崩溃记录补实际装载模块基址 + timestamp + 故障线程身份/创建位置 + minidump；新增 `scripts/symbolize_crash.py`（含已知符号自检，三镜像 outside=0）。原"符号化给出合理但错误答案"的真因是崩溃记录按错误模块名查基址，永远拿不到基址。
- **P4a 创建：** GPU 对象创建走 `RHIDeferredCreateQueue`，图形角色立即执行、否则帧首实体化；未就绪状态可检查。
- **P4b 销毁：** 判据从"固定 2 帧"改为"提交批次尾部"；删除 `RHINativeResourceOwner`，队列归 RHI 全局。
- **P3a/P3b 材质：** 渲染代理自持不可变快照（探针实测渲染路径对游戏对象读取 0 次）；改为渲染侧引用计数对象，删除 `DeferredObjectRelease` 与 `MaterialRenderProxyHandle`。
- **P2 两段式：** 移除命令只 park，帧末统一删除；删除帧保活两槽补丁。
- **P1 帧序：** 资源创建 → 命令应用 → 录制 → 帧末删除（与 UE 的 OnRenderBegin→Scene::Update 一致）；场景退役先移除 primitive 再销毁。
- **验证：** 构建（minEngineTests / Editor）、`test smoke` exit 0、导出门禁 growth 0（2998）。真机 Single/Multi 验收待执行。
- **删除清单：** `DeferredObjectRelease`、`RHINativeResourceOwner`、`MaterialRenderProxyHandle`、帧保活两槽、固定帧延迟常量——本轮是净删除。

## 2026-10-25 — BUG-RENDER-016 收口 + 帧线程契约四片落地（CORE-F47 S01–S04）

- **范围 A（BUG-RENDER-016 直接原因）：** 渲染代理改为强引用其绘制所需的 GPU 缓冲 / 顶点布局 / 材质所有者；绘制范围从网格 section 元数据贯通到 packet；提交路径删除两条 `GetDesc()` 兜底。新套件 `scene-proxy-lifetime` 3 例 / 38 断言。
- **门禁修正：** `frame-threading` 用例改为断言真实契约（Multi 初始化只绑 Game，Render/RHI 由角色线程认领）并加单例作用域守卫；导出基线按评审流程删除 3 条早已失效的符号（`RenderScene::UpdateLight`/`UpdatePrimitive` 由 `330f1b3d` 删除，`RenderGraph::SetupAttachments` 旧签名由 `1eb90492` 替换）。
- **S01 止血：** 角色线程与命令 Flush 加异常边界（失败仍结算完成信号并计数）；列表池借用改 RAII；命令队列过期条目丢弃 + 有界等待；关停顺序改为先停角色再拆世界；`TryStartMultiFrameRoles` 传播启动失败。
- **S02 帧号所有权：** 逻辑帧号归游戏角色，删除 `AdvanceLogicFrame` / `GetLogicFrameIndex`；Drain 改「小于等于」；世代断言改为比较两个独立写入者（游戏发布的世代 vs 渲染已 drain 的帧）；计数器与帧号原子化。
- **S03 退役闸口：** 场景销毁改到「该帧 RHI 完成」且回到游戏角色；`m_RetiredScenes` 加锁；PIE 退出 / 切场景 / 卸载统一走 `RetireScene`；新增延迟释放队列。
- **S04 接线与背压：** 代理销毁路径把材质交接给延迟释放队列（`TD-039` 的真实路径）；背压改为端到端在飞帧上界；每帧最多一条工作丢失汇总；`scene-retirement` / `deferred-release` / `scene-proxy-lifetime` 纳入 smoke。
- **验证：** `minEngineTests` / `Editor` 构建通过；`test smoke` exit 0（21 套件）；导出 2998/3000。`verify.ps1` 一路绿灯，**停在外部消费者审计**：兄弟仓库 `minCraft*` 的过期二进制仍导入引擎已删除的符号——需重建外部宿主后整门转绿。
- **未完成：** 真实编辑器 PIE 手工验收；`TD-035/036`（渲染线程仍解引用代理回指的组件）需「渲染线程不回指」结构性切片。

## 2026-10-25 — BUG-RENDER-016：PIE SubmitDrawBuffers dangling IndexBuffer（非 PointerPolicy）

- **Scope:** minCraft 反馈 PIE Shift+F1/recapture AV；本机 gdb 复现与符号化
- **Evidence:** `SubmitDrawBuffers` L225 `IndexBuffer->GetDesc()`；packet `IndexCount=0`，vtable 首字=`0x1`；`drawIndex=1/39`；F5 后即崩（不必 recapture）
- **Docs:** [BUG-RENDER-016](./bugs/BUG-RENDER-016.md)；crash 诊断增强（RSP dump）已改源码、重建曾被残留进程锁 DLL 打断
- **Next:** 追 IndexBuffer UAF 来源（PIE/proxy/mesh 重建）；再修 submit 路径

## 2026-10-25 — merge: feat/core + feat/render → master

- **Scope:** 主工作树合入 core（F38/F42/F44–F46 等）与 render（F26–F28）
- **Conflicts:** ACTIVE_WORK / PROGRESS_LOG 手工合并；代码侧 ForwardRenderer 自动合并
- **Next:** minCraft ENGINE_ROOT → 主树；重建后手验 ShowUi / 可选 RT bake

## 2026-10-25 — CORE-F42 S06：UI 集成收口 · Feature Done

- **Scope:** S06；不引入 UISystem 自动 Confined（硬边界）
- **Completed:**
  - 验收：容器拥有者 RAII `Pointer::ShowUi` / `ConfineUiTo`；视口提示已是 Shift+F1
  - O7：`test screen-ui-pointer` + `gameplay-input-gate` 全绿；`pointer-policy` 11/105 绿
  - Design / Registry / ACTIVE_WORK → **Done**
- **Next:** 合入 `master`；minCraft `ApplyPointerMode` → `ShowUi()`（`ENGINE_ROOT` 需含 F42）

## 2026-10-25 — CORE-F42 S04：Shift+F1 释放 + 轻量位置采样

- **Scope:** S04 键位；BeginFrame 位置窗口/视口采样
- **Completed:**
  - `IsReleaseRequestedThisFrame`：Shift+F1 主路径；`bKeepLeftAltAlias` 默认开
  - SEVC / 视口提示改绑；`PointerPolicySettings` 挂服务
  - `BeginFrame` 填 WindowPx / ViewportPointPx；ReferencePx 复用上一帧 UI 样本
- **Verification:** `test pointer-policy` + `gameplay-input-gate`；Editor 已链
- **Next:** S06 或合 master + minCraft `ShowUi`

## 2026-10-25 — CORE-F42 S02/S03/S05：PointerPolicyService + Editor 迁移 + 游戏 facade

- **Scope:** 指针策略服务竖切（开背包可显标）；S04 Shift+F1 / S06 / 位置采样后置
- **Completed:**
  - `PointerPolicyService` + `MapModeToWindowCommand` + `IPointerModeSink` + RAII `PointerModeRequest`
  - Engine 接线；`WindowSystem::ApplyPointerWindowCommand`；SEVC/PlaySession/Editor 迁出直调
  - PIE Captured 挂 **Game** tier（UI `ShowUi` 可盖住）；`Pointer::*` facade
  - suite 增 6 例（tier / refcount / UI 盖 Captured / focus suspend / map）
- **Verification:** `minEngineTests.exe test pointer-policy` 11 PASS / 105 asserts；`Editor` 已链
- **Deviation:** PIE Captured 用 Game 非 Play（否则 UI 压不过 Play）；位置采样未接
- **Next:** minCraft `ApplyPointerMode` → `Pointer::ShowUi()`；可选 S04；合 master

## 2026-10-25 — CORE-F46 S03：backpressure / Profile / Multi smoke 收口

- **Scope:** S03 Done；Feature Done；顺带修 Wait→Drain 竞态
- **Completed:**
  - `WaitForRenderQueueSpaceLocked` 统一背压等待；`SubmitRenderFrameTask` / `WaitForRenderQueueSpace` 共用
  - Profile：`Frame.Game.SubmitRenderFrame` / `WaitRenderQueue` / `WaitFrameComplete`；`Frame.Graphics|Render.Tick`；`Frame.RHI.Replay`
  - suite：async multi-frame + backpressure（队列满时 Game 阻塞）
  - `FinishRecord`：先 PublishCompletion 再唤醒 Wait（避免 Drain 漏回调）
  - docs：Design/Registry/ACTIVE_WORK/BOOTSTRAP；F45 标 Done（S04 已吸收）
- **Verification:** `minEngineTests.exe test task-system` 19 PASS（`MINENGINE_SKIP_TASK_PERF=1`）@ minEngine-core bin
- **Next:** 主人验收后合入 master；点选下一 Primary

## 2026-10-24 — CORE-F46 S02：帧角色进 TaskSystem + Engine/Editor Multi

- **Scope:** 吸收 FrameThreadScheduler；StartFrameRoles / SubmitRenderFrameTask；OpenGL combine；接线 Engine/Editor
- **Completed:**
  - TaskSystem：Render/RHI（或 combined graphics）OS 线程；有界队列；waitForCompletion
  - 删除公共 FrameThreadScheduler；RHICommandListPool 保留给 Recording Replay
  - Engine::StartMultiFrameRoles / Run Multi（异步 Submit）；Editor Multi（ImGui+Present 在 graphics 角色，wait=true）
  - GLFW MakeGraphicsContextCurrent / Clear；suite 增 combined/split frame-role 用例
- **Verification:** `minEngineTests.exe test task-system` 17 PASS；`MaximumEditor.exe --project MyMEProject --frame-threads multi` 短跑成功（日志：combined graphics frame role started）
- **Next:** S03 Profile/backpressure 收口；主人验收后准备 commit

## 2026-10-25 — BUG-PHYS-005：物理回写 StateDirty 导致拖 Cube 闪没

- **Scope:** 登记并修复 Editor 拖 Dynamic+Simulate Cube 网格频闪
- **Completed:**
  - 登记 [BUG-PHYS-005](./bugs/BUG-PHYS-005.md)
  - `SetTransformFromSimulation` → `MarkRenderTransformDirty`
  - `RecreatePrimitive`：Flush 前不再掏空旧 Proxy 的 `m_PrimitiveComponent`
- **Verification:** Editor 已重建；待主人 `--frame-threads multi` 拖 Cube 目视
- **Next:** 主人验收后勾回归项

## 2026-10-25 — Editor Multi：ImGui Platform/Renderer 分线程

- **Scope:** Editor 支持 `--frame-threads multi`；避免 GLFW 进 graphics 死锁
- **Completed:**
  - `EditorImGuiBackend`：`PlatformNewFrame` / `RendererNewFrame` / `BuildDrawData` / `SubmitDrawData`
  - Game：GLFW + UI CPU + `BuildDrawData`；graphics：`RendererNewFrame` + `TickRendererFrame` + `SubmitDrawData` + Present；`wait=true`
  - 启动前在 Game 上做 font atlas + 一次 `RendererNewFrame`（上下文移交前）
- **Verification:** `MaximumEditor --frame-threads multi` Responding=True；日志 `combined graphics frame role started`
- **Next:** S03；可选 wait=false 重叠（需更严 ImDrawData 生命周期）

## 2026-10-24 — CORE-F46 S01/S01b：ParallelFor + SubmitFlow

- **Scope:** Design v3 Job→Task + 自审 Ready；实现扇出与小图
- **Completed:**
  - Design：`SubmitRenderFrameTask` / `RenderFrameTask` 命名；Status In Progress；自审 Ready
  - `ParallelFor` / `RunAndWait`（Game|Render）；`TaskFlowBuilder` + `SubmitFlow`（环拒绝；菱形）
  - suite `task-system` 15 cases PASS（含 Render ParallelFor / cycle / diamond）
  - 顺手：`ManualRenderer` 补 `RHICommandListPool.h`（S04 spike 编译缺口）
- **Verification:** `minEngineTests.exe test task-system` exit 0 @ minEngine-core bin
- **Next:** S02 吸收帧角色（`StartFrameRoles` / `SubmitRenderFrameTask`；去公共 Scheduler）

## 2026-10-24 — CORE-F46 Draft v2：API / 数据结构 / 小图

- **Scope:** 据讨论修订 Design：Join vs Drain、小 DAG、代码示意
- **Completed:**
  - [Design v2](./Platform/Core/CORE-F46_TASK_SYSTEM_UNIFIED_FACADE_DESIGN.md)：`TaskFlowBuilder`/`SubmitFlow`；`ParallelFor`；帧角色 API；用法 A–E
  - 锁定 D8/D9：RHI Create≠Task 回调；Render 剔除=Join
  - Non-Goal：Named 泵与跨角色节点（小公共 DAG **做**）
- **Verification:** docs only
- **Next:** 主人审阅 v2 → Design Review → S01 ParallelFor

## 2026-10-24 — CORE-F46 Draft：TaskSystem 统一门面

- **Scope:** 登记执行模型演进；统一门面 + Named 帧角色线程 + Worker ParallelFor；调整 F45 S04 优先级
- **Completed:**
  - 登记 `CORE-F46`；[Design Draft](./Platform/Core/CORE-F46_TASK_SYSTEM_UNIFIED_FACADE_DESIGN.md)
  - 锁定：Render/RHI 仍为常驻有名 OS 线程；不仿完整 TaskGraph；`FrameThreadScheduler` = spike 待吸收
  - F45 Meta / ACTIVE_WORK：S04 暂停，Primary → F46 审阅
- **Verification:** docs only
- **Next:** 主人审阅 F46；Design Review 后再实现（建议 S01 ParallelFor → S02 吸收帧角色）

## 2026-10-24 — CORE-F45 S03（RHICommandList Immediate / Recording）

- **Scope:** List 状态机；Single 默认 Immediate；Recording 可测；Pass 调用约定不变
- **Completed:**
  - RHICommandList：BeginRecording / FinishRecording / Replay / Reset；SetImmediateOverride
  - Immediate：命令仍直调 RHICmd*；Begin/Finish/Replay no-op
  - Recording：入队 std::function，Replay 回放；Create* 仍同步
  - Forward/Manual：主路径 Begin→Passes→Finish→Replay（Single 行为≈今日）
  - suite 
hi-command-list（smoke）
- **Verification:** minEngineTests.exe test rhi-command-list；	est smoke exit 0 @ minEngine-core（cwd=minEngine/bin）
- **Next:** S04 Multi OS 线程 + Finished-List 池 / Wait 区段

## 2026-10-24 — CORE-F45 S01/S02（显式 Proxy 推送 + ViewIntent）

- **Scope:** ACK 后按 UE 显式 API 重写 Game→Render 推送；ViewIntent + 先画后 Apply
- **Completed:**
  - `RenderCommandPipe`：Enqueue / FlushApply / ViewIntent / publishedWorldGen
  - `RenderScene`：Add / UpdateTransform / UpdateColor(Light) / Recreate / Remove；删除全家桶 Snapshot
  - Dirty：`MarkRenderTransformDirty` / `MarkRenderColorDirty` / `MarkRenderStateDirty`；EOF 分流
  - Engine RenderRole：Take ViewIntent → Record → FlushApply(N) → publishedWorldGen=N → Advance
  - suite `render-command-pipe`（smoke）
- **Verification:** `minEngineTests.exe test render-command-pipe`；`test frame-threading`；`test smoke` exit 0 @ `minEngine-core`（cwd=`minEngine/bin`）
- **Next:** Multi OS 线程 / RHI List（按 Render/RHI 子 Design）

## 2026-10-24 — CORE-F45 S00 落地（身份 + Mode）

- **Scope:** `FrameThreading` 角色身份 / `FrameThreadMode`；CLI + Engine 接入；**不改 Proxy 热路径**
- **Completed:**
  - `Runtime/Core/Threading/FrameThreading.{h,cpp}`：Kind/Mode、Bind、Assert、Resolve（CLI → env → Single）
  - `--frame-threads single|multi`；`MINENGINE_FRAME_THREADS`；Multi 本期仍单 OS 线程 + Warn
  - Engine：Init/Shutdown；`TickLogicalFrame`/`TickRendererFrame` 断言；`Frame.Game.Logical` / `Frame.Render.Record` scope
  - suite `frame-threading`（smoke 列表已挂）
- **Verification:** `minEngineTests.exe test frame-threading`；`test smoke`（含 frame-threading）exit 0 @ `minEngine-core`
- **Next:** S01 四家族 Enqueue + Apply*_RT

## 2026-10-24 — CORE-F45 Mode A 收口 → Ready（待 ACK）

- **Scope:** 子 agent 中断后主会话 Mode A；锁开工歧义；不改帧循环代码
- **Completed:**
  - Decisions **D12–D18**（入队+Flush、可调用体载体、Mode 优先级与默认 Single、Immediate/Recording、Present@RHIRole、Recreate MVP、队列常量）
  - 修 Single/Multi 工序与「同步 Enqueue」矛盾；综述 **Ready for implementation = yes**
  - 终审清单写入综述 Design Review 节
- **Verification:** docs only
- **Next:** 维护者 ACK → S00（身份+Mode only）

## 2026-10-24 — CORE-F45 Proxy 推送纠偏（对齐 UE 家族 Enqueue）

- **Scope:** 统一 POD + Light/Sky 遗漏；对照 UE 家族 Enqueue
- **Completed:** 撤回巨石 payload；四家族；Recreate vs UpdateFast
- **Verification:** docs only
- **Next:** （已完成）Mode A 收口

## 2026-10-24 — CORE-F45 加厚：双模式 + 子文口径

- **Scope:** 工序/世代/Mode/Profile；Single/Multi 一等公民
- **Completed:** 综述 Mode；Game/Render/RHI 加厚（其后 Proxy 口径见上条纠偏）
- **Verification:** docs only
- **Next:** （已完成）Proxy 家族纠偏

## 2026-10-24 — CORE-F45 文档拆分：综述 + 三线程子 Design

- **Scope:** 避免三角色细节堆一篇；修正 CS5「旁路 List」误述
- **Completed:** 综述 + Game/Render/RHI 初版拆分；List 录制化（非另起类型）
- **Verification:** docs only
- **Next:** （已完成）加厚 + FrameThreadMode

## 2026-10-24 — CORE-F45 Design 修订：Mode A + UE 源码锚点

- **Scope:** 按 design-review blocking 项锁契约；对照本机 UE（只读）
- **Completed:**
  - Design Status → **Review**；锁 CS2=UE 风格 Enqueue；Game=窗口线程；Intent(N)↔World(N−1)；队列满阻塞；后澄清为 List 录制化（非旁路新类型）
  - UE 锚点：`RenderingThread.h`；`RendererScene.cpp` AddPrimitive；`RHICommandList.h` FinishRecording / Dispatch
- **Verification:** docs only
- **Next:** （已完成）拆分为综述 + 三子文

## 2026-10-24 — CORE-F45 Design Draft：临界区 + 接触面

- **Scope:** Survey 证据 → Design Spec；不改帧循环代码
- **Completed:**
  - [Design Draft](./Platform/Core/CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)：CS1–CS8；Proxy Publish；RenderQueue；ViewIntent；切片 S00–S06
  - 代码锚点：`RenderScene::UpdatePrimitive` MT TODO；即时 `RHICommandList`；`SubmitSceneDraw`→同帧 Execute；Vulkan 录制 / GL 上下文
  - Survey → **Reference**；Registry / ACTIVE_WORK 指向 Design
- **Verification:** docs only
- **Next:** （已完成）Mode A → 本上条修订

## 2026-10-24 — CORE-F45 Survey + ASSET-F11 挂号；TaskSystem 契约注释

- **Scope:** 帧线程分离升 Primary（Survey）；AsyncLoad 通用化仅挂号；TaskSystem 头注释 + game-module 风格示例测试
- **Completed:** [CORE-F45 Survey](./Platform/Core/CORE-F45_FRAME_THREAD_SEPARATION_SURVEY.md)；[ASSET-F11 Draft](./Asset/ASSET-F11_ASYNC_JSON_ASSET_LOAD_GENERALIZATION_DESIGN.md)；`TaskSystem.h` 用法契约；`task-system: game-module style submit apply on drain`
- **Verification:** docs + 待跑 `test task-system`（示例用例）
- **Next:** （已完成）三线程 Design Draft → design-review

## 2026-10-24 — ASSET-F09 Done：异步 DataAsset + TaskSystem 单例

- **Scope:** `AsyncLoadAssetByPath/GUID/Meta`；Worker `LoadJsonFromFile`；Drain 上 `FromJson`+同步递归引用；coalesce；sync Wait join（禁 Drain）；TaskSystem `Get`/`SetInstance`
- **Completed:** `AssetManager_AsyncLoad.cpp`；Engine 接线；`data-asset` async smoke；去掉 `BindTaskSystem`
- **Verification:** `bin/minEngineTests.exe test data-asset` / `task-system` / `serialization-archive` — PASS
- **Next:** 可选一并 commit（F44+F09）

## 2026-10-24 — CORE-F44 Done：Serializer File/Json/Object 分段

- **Scope:** `LoadJsonFromFile` + `FromJson`；`FromFile`(JSON) 组合；Helper `ReadJsonFromFile` / `ReadObjectFromJson`
- **Completed:** Serializer/Helper API；`serialization-archive` 等价用例；Design/Registry → **Done**
- **Verification:** `serialization-archive` / `data-asset` / `prefab` — PASS
- **Next:** 见上条 F09 Done

## 2026-10-24 — CORE-F44 + ASSET-F09：讨论结论落档（待审批）

- **Scope:** 序列化 File/Json/Object 分段前置；F09 嵌套引用 MVP=同步递归、不做双模式开关
- **Completed:** F44/F09 Design Draft + Registry；维护者已批准并开工
- **Verification:** docs only（当时）
- **Next:** 见上条 F44 Done

## 2026-10-24 — ASSET-F09 Design Draft：异步 DataAsset

- **Scope:** minCraft 批量 Catalog / `.medata` 加载动机；F38 后第一个真实客户
- **Completed:** [Design Draft](./Asset/ASSET-F09_ASYNC_DATAASSET_LOAD_DESIGN.md) — Worker→Json；游戏线程装盘；同步 Load 可 Wait Worker 禁止合流 Drain；coalesce；仅 DataAsset。Registry **Draft**
- **Verification:** docs only
- **Next:** 见上条（F44 前置 + 嵌套引用锁定）

## 2026-10-24 — CORE-F38 Done：TaskSystem S00–S02

- **Scope:** TaskFlow vendor + `TaskSystem` Submit/Wait/Drain + Engine 挂接 + 合成证明
- **Completed:**
  - Vendor TaskFlow v3.10.0 → `Third-Party/taskflow`
  - `Runtime/Core/Tasks`；Engine Init/Shutdown；LogicalTick Drain（Input 后、Fixed 前）
  - Suite `task-system` + smoke 序；功能/异常/Shutdown/非游戏线程拒绝；1 vs N perf（可 `MINENGINE_SKIP_TASK_PERF`）
  - Design/Impl/Registry/ACTIVE_WORK → **Done**
- **Verification:** `bin/minEngineTests.exe test task-system` — 8 passed / 36 assert（speedup ~6.3 本机）；`test smoke` from `bin` — EXIT 0（含 task-system）
- **Next:** 一并 commit（docs+code）；真实客户另开 `ASSET-F09`

## 2026-10-24 — CORE-F38 批准开工：Design Planned + Impl Plan

- **Scope:** 吸收开工前 design-review（游戏线程=`Init` thread id；回调无结果载荷；`MINENGINE_API`）；维护者批准
- **Completed:** Design Status → **Planned**；[Impl Plan](./Platform/Core/CORE-F38_TASK_SYSTEM_IMPLEMENTATION.md) S00–S02；Registry / ACTIVE_WORK / Digest 同步；`ASSET-F09` 仍 Deferred
- **Verification:** docs only
- **Next:** `feat/core` 对齐 `master` → S00（TaskFlow vendor + Init/Shutdown）

## 2026-10-25 — RND-F27 / RND-F28 实现（RegisterExternal + Texture2DRenderTarget）

- **Scope:** F27 S01：`RegisterExternalTexture` / Unregister / Clear；`SetupAttachments(RHI&)`；Unregister 后强制重建 graph-owned 槽。F28 S01–S02：`Texture2DRenderTarget`（`NewObject`）+ `MakeTexture2DRenderTargetDesc`；`SceneDrawDesc.ExternalSceneColor`；Forward Register；Offscreen `SetColorTarget`；`CreateFromRGBA8`/`CreateSolidRGBA` → `NewObject`；`GetRHITextureShared`
- **Verification:** `minEngineTests.exe test render-graph` — 11 passed / 59 assert（工作树；minCraft extra suite 因 ABI 未链入，用 ExtraTestStub 链接）
- **Completed:** Design/Registry/ACTIVE_WORK → Done；S02 Depth、F28 S03/S04 可选未做
- **Next:** commit；minCraft 消费块图标 bake

## 2026-10-25 — RND-F27 / RND-F28 Design Review + 中文稿

- **Scope:** Mode A 设计评审两篇；全文改中文；锁定：F27 失败不得静默 create、R5 收窄为图+单测；F28 `RenderOnce` 保持 `RHITextureRef&`（Thumbnail）、SceneDrawDesc.ExternalSceneColor、make_shared、`GetRHITextureShared`
- **Verdict:** 均为 `Ready with deferred items`；F28 渲入编码仍等 F27 S01
- **Completed:** Design Status → Review；Registry/ACTIVE_WORK 同步
- **Next:** 主人确认 → Planned；开 F27 S01（已完成，见上条）

## 2026-10-25 — RND-F27 / RND-F28 Design Draft（Offscreen → Texture2DRenderTarget）

- **Scope:** 登记并起草两篇 Design（`feat/render`）：`RND-F27` RDG `RegisterExternal`；`RND-F28` `Texture2DRenderTarget` + Offscreen SetColorTarget（依赖 F27）
- **Note:** F26 已占用 → External=F27、RT=F28
- **Completed:** Draft + 登记（随后已 Review + 中文）
- **Next:** （见上条）

## 2026-10-22 — BUG-EDITOR-009：scene.set_property 空串 string 崩溃

- **Scope:** `WritePropertyValue` 去掉对未构造 raw buffer 的 `GetProperty`/`SetProperty`；对齐 Graph 域 blob + `CommitObjectPropertyBlobs`；新增 suite `scene-property-string`（smoke）
- **Completed:** 修复；[BUG-EDITOR-009](./bugs/BUG-EDITOR-009.md) → Fixed；smoke 序加入 `scene-property-string`
- **Verification:** `minEngine/bin/minEngineTests.exe test scene-property-string` — 3 passed / 36 assert
- **Next:** 主人验收后提交；可选 MCP 手验 `m_Name` ← `""`

## 2026-10-22 — BUILD-F03 刀2 终：默认 `explicit` + 清冗余 linker 旗标

- **Scope:** `MinEngineExports.cmake` 默认 `MINENGINE_EXPORT_MODE=explicit`；whitelist/explicit 路径只留 `--exclude-all-symbols`（`--exclude-libs` / `--exclude-modules-for-implib` 仅 `auto`）
- **Verification:** configure 显示 `mode=explicit`；四类 target 绿；`--verify-coverage` missing=0；budget≤3000 / forbid-buckets PASS；`bin` 下 `smoke` / `game-module-stub` 全绿
- **Next:** 刀2 主链收口；外仓 live coverage 可选再跑；Primary 回 ASSET-F10

## 2026-10-22 — BUILD-F03 刀2 批5：Render/Resource/UI/Audio/Physics/Prefab/Animation · `.def`→0

- **Scope:** 附录 C-A/B/C 类级/点级 `MINENGINE_API`（Asset*/Material*/Sprite*/Physics*/Prefab*/Animation*/UI*/RHI*/Test* 等）；缺 `EngineAPI.h` 的头补 include；`WindowSystem::HasInstance` 出线；`NativeGameplayTag` 全局 `ME_DECLARE` 带导出；嵌套 `ScreenUICoords::LetterboxMapping` / `AssetManager::AssetRegistryBroadcastBatchScope` 补标；`.def` 烧光 → **0**（LEGACY 仅注释）
- **Verification:** 四类 target 绿；`--verify-coverage` missing=0；forbid-buckets PASS；`bin` 下 `smoke` / `game-module-stub` / `gameplay-tags` 全绿；DLL NumberOfFunctions≈2720
- **Next:** 主人验收 → 切默认 `MINENGINE_EXPORT_MODE=explicit` + 清冗余 linker 旗标

## 2026-10-22 — BUILD-F03 刀2 批4：Object / Scene / Components / GameplayTag / Engine

- **Scope:** 类级 `MINENGINE_API`：`MEObject`/`ObjectManager`/`ObjectCloneContext`/`GameObject`/`Component`/`SceneComponent`/`PrimitiveComponent`/`Scene`/`SceneManager`/`SceneDuplicator`/`GameInstance`/`Subsystem*`/`SubsystemCollectionBase`/`GameplayTag*`/`NativeGameplayTag*`/`Engine`/`ProjectManager`；`Engine`/`ProjectManager::HasInstance` 改 `.cpp`（避免消费者 import `s_Instance`）；按 Itanium 归属类烧 `.def` −156 → **498**
- **Verification:** 四类 target 绿；`--verify-coverage` missing=0；budget/forbid-buckets PASS；`bin` 下 `smoke` / `game-module-stub` / `gameplay-tags` / `object-manager` 全绿
- **Next:** 主人验收 → 批5 Render/Resource/UI/Audio/Physics/Prefab/Animation
- **Note（顺手排查，未修）：** DataAsset 加载后 meta/medata Guid 不对称 — 规范身份在 `.meta`（`ApplyMetaIdentity`）；`m_Guid` 仍写入 `.medata`（Invisible≠Transient）；`RegisterAsset` 零 Guid/碰撞只改 `.meta`；加载+Save 会把 medata 改写成 meta 侧 Guid

## 2026-10-22 — BUILD-F03 刀2 批3：Reflection codegen MODULE_API + 烧 `.def`

- **Scope:** `ME_REFLECTION_MODULE_API`（默认 `MINENGINE_API`）；`MinEngineReflection.cmake` 增加 `MODULE_API`；`GameModuleStub` 用 `GAMEMODULESTUB_API`；宏/`StaticClass`/`GetClassName` 跟随归属模块；从 `.def` 烧掉 Reflection 命名空间与 `*::StaticClass` 行（约 −127 → **654**）；签名里仅提及 `MEClass` 的行（如 `GetSubsystem`/`PostEditChangeProperty`）误删后已补回
- **Verification:** `minEngineTests`/`Editor`/`SampleEditor`/`GameModuleStub` 绿；`--verify-coverage` missing=0；budget/forbid-buckets PASS；`bin` 下 `smoke` / `game-module-stub` / `reflection-function` / `reflection-display-names` 全绿
- **Next:** 主人验收 → 批4 Object / Scene / Components / GameplayTag*

## 2026-10-22 — BUILD-F03 刀2 批2：Math / Input + HasInstance out-of-line

- **Scope:** 标 `InputSystem` / `InputTrigger*` / `Quaternion` / `Color` / `LinearColor` / Geometry `AABB`·`Frustum` / `PointerConstraintState`·Os；`InputSystem::HasInstance()` 改 `.cpp`（避免消费者 import `s_Instance`）；`.def` −118（约 781 余；组件 SetColor 等含 LinearColor 签名的行留给批4/5）
- **Verification:** 构建绿；`--verify-coverage` missing=0；baseline 删除守卫 PASS；`input-key-catalog` / `input-trigger-down` / `bin` 下 `smoke` 全绿
- **Next:** 主人验收 → 批3 Reflection + codegen `MODULE_API`

## 2026-10-22 — BUILD-F03 刀2 批1：Log / CLI / Path / Serialization 离 `.def`

- **Scope:** 补齐 `PathRegistry`、`ApplicationCommandLine` 类级 `MINENGINE_API`；Log/Serialization 本已有宏，从 `.def` 烧掉对应约 98 行（余 899；`TAG_Channel_Default` 留批4）
- **Completed:** 头文件 2 处；`.def` 燃烧；Design 批表 / Registry / ACTIVE_WORK → 刀2 In Progress
- **Verification:**
  - `cmake --build … minEngineTests Editor SampleEditor` 绿
  - `--verify-coverage` missing=0；删除守卫 baseline 1282 全在（类级宏额外导出 PathRegistry/`s_Instance` 等，属预期膨胀）
  - `minEngine/bin` 下 `test smoke` 全绿；`logging-channels` / `serialization-archive` PASSED
- **Note:** 从 repo 根跑 smoke 时 `material-ir` 会因 EngineConfig cwd 失败——与本批无关；验收以 `bin` 为 cwd
- **Next:** 主人验收本批 → 批2 Math/Input（含 inline→`s_Instance`）

## 2026-10-22 — BUILD-F03 v3：刀2 设计收束（待审批）

- **Scope:** 文档 only — 对齐「烧光 `.def`、只靠 `*_API`、静态身份跨 DLL 单份」；不改头文件 / codegen
- **Completed:**
  - F03 Meta → **Review** v3：刀1/F04 进度；刀2 简化叙事；Q4 InputKeys → 导出方案（① inline 作废）；附录 C；批序；「待主人审批」闸；实施可顺带清死 include
  - F02：进度表 + TL;DR/Q 表与落地对齐；剩余指向 F03 刀2
  - FEATURE_REGISTRY / ACTIVE_WORK 挂刀2 Review（批前不大批改头）
- **Verification:** docs only
- **Next:** 主人批 F03「待主人审批」表后再开工；Primary 仍为 ASSET-F10

## 2026-10-22 — GP-F03 修订：GameplayTag 名身份（对齐 UE）

- **Scope:** `operator==` / hash / `<` 按 `TagName`；从 `GameplayTag` 移除 `m_Manager*`；`m_Index` 仅 Bind 缓存；`Matches` 经 `GameplayTagManager::Get()`
- **Completed:**
  - 未 Bind 同名可作 `unordered_map` / `map` 键；`IsValid`/`Matches` 仍要求已注册
  - 导出表去掉 out-of-line `operator==`（改为头文件内联）；baseline 同步收缩
  - Design / Registry / ACTIVE_WORK → Done
- **Verification:** `minEngineTests.exe test gameplay-tags`（3 passed / 48 assert）；`test gameplay-events`（2 passed / 18 assert）
- **Next:** 提交本切片（勿混入 Input/Texture WIP）；minCraft 可直接用 Tag 作 map 键

## 2026-10-19 — MCP-F05：Prefab 作者化自动化 + 属性引用解析

- **Scope:** 新增 `prefab.create_from_game_object` / `open` / `save` / `close`；`scene.set_property` 支持 `$assetRef` / `$guid` / `$objectRef` 与 `properties[]` 批量；会话接线（Scene/Prefab → Stage 场景）
- **Completed:**（见 [Impl](./Editor/MCP/MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_IMPLEMENTATION.md)）
  - create 不改场景；overwrite 保资产 GUID + 备份还原
  - save 走 `SaveStage(key)` = `WriteStageTreeToPrefab` → `SavePrefabAsset` → `RestoreTemplateObjects`
  - 属性引用：phase A 全解析才写；phase B 失败报 `PartialWrite` + `applied[]`
  - 顺带修复 `TryRestoreBlob` 对指针类属性 undo/restore 丢引用
- **Verification:** `scripts/verify.ps1` `VERIFY_EXIT=0`（13 suites）；`ui_prefab_smoke.ts` **55/55**（两次独立跑通）
- **Known issue:** Stage save+close 循环偶发 AV → [BUG-EDITOR-007](./bugs/BUG-EDITOR-007.md)；主路径已用 `create_from_game_object` 规避
- **Next:** 修 BUG-EDITOR-007（Runtime）；再把 smoke 接 CI；提交前无 WIP 混入

## 2026-10-19 — CORE-F41 Review：会话日志文件与读回已落地

- **Scope:** 磁盘会话日志、项目指针、`editor.get_log`、Console Copy Visible / Copy Line / Ctrl+C / Show Log File
- **Completed:**
  - `LogSystem`：`<exe>/Logs/minEngine-…-pid.log`；Warn 立即 flush + 1s 周期；保留最近 20 个会话文件；`PublishSessionPointer` → `Saved/Logs/latest-session-<stem>.txt`
  - 环：`IsTruncated` / `Tail`；Automation `editor.get_log`（经现有 `editor_query`）
  - Console：Copy Visible、右键 Copy Line、Ctrl+C、Show Log File
  - Design → **Review**；无独立 Impl 文档（切片在 Design Must）
- **Verification:** `minEngineTests.exe test logging-channels` → 6 passed / 37 assertions
- **Next:** 手工点验 Console 剪贴板 → Done；再提交（勿混入无关 WIP）

## 2026-10-19 — CORE-F41 Draft：会话日志文件与读回

- **Scope:** 磁盘会话日志、项目指针、`editor.get_log`、Console 单条复制
- **Completed:** 登记 `CORE-F41`；[Design Draft](./Platform/Core/CORE-F41_SESSION_LOG_FILE_AND_READBACK_DESIGN.md)。结论：Copy 按钮已用换行拼接过滤结果；缺的是 Copy Line / Ctrl+C。MCP 走现有 `editor_query`，不新增 TS tool
- **Verification:** docs only
- **Next:** design-review → 实现

## 2026-10-18 — UI-F08 Review：Widget 多键指针交互

- **Scope:** ScreenUI 多键状态机 + 修饰键；Widget Enter/Leave/Down/Up/Click；Button 糖；Drag `InitiatingButton`
- **Completed:**
  - 类型：`UIPointerEvent` / `UIKeyModifiers` / `EUIPointerButton`（无 F 前缀）
  - `UISystem` 跟踪 Left/Right/Middle；采样 CORE-F40 合成修饰键
  - Widget / Button 委托；无参 `OnClicked` 仅 Left
  - Drag session 记发起键；suite `screen-ui-pointer`；button/drag 回归绿
- **Verification:** `test screen-ui-pointer` / `screen-ui-button` / `screen-ui-drag` PASSED
- **Next:** 维护者验收 → Done；与 UI docs 一并提交

## 2026-10-18 — CORE-F40 Done：全量键表 + 合成修饰 + Gamepad

- **Scope:** 全量扩展 InputKeys / GLFW 映射；合成 Shift/Ctrl/Alt/Super；canonical Gamepad poll；未知码忽略（不再写 AnyKey）
- **Completed:**
  - `InputKeys`：导航/标点/KP/F13–24/Mouse_4–8/Super/Menu/锁键 + Gamepad 键轴；PS 四键同 Name 别名
  - `GLFWWindowSystem::TryConvertGLFWKeyToInputKey`；未映射 return false
  - `InputSystem`：侧键更新后刷新合成修饰；Tick 内 poll 第一只 gamepad（不受 gameplay gate）
  - suite `input-key-catalog`；回归 `input-trigger-down`；Status → **Done**
- **Verification:** `minEngineTests.exe test input-key-catalog` / `input-trigger-down` PASSED
- **Next:** UI-F08 指针交互实现

## 2026-10-18 — CORE-F40 Draft：Input Key Catalog

- **Scope:** 补全 InputKeys / GLFW 映射；激活合成修饰键；canonical Gamepad（拒多厂商平行状态）
- **Completed:** 登记 `CORE-F40`；[Design Draft](./Platform/Core/CORE-F40_INPUT_KEY_CATALOG_DESIGN.md)；ACTIVE_WORK 并行 input 轨；与 UI-F08 软依赖写清
- **Verification:** docs only
- **Next:** 与 UI-F08 一并 design-review → Planned

## 2026-10-18 — UI-F08 Draft：Widget 指针交互（多键 + 修饰键）

- **Scope:** minCraft ScreenUI 反馈；Button 仅为糖，机制沉到可命中 Widget
- **Completed:** 登记 `UI-F08`；[Design Draft](./Platform/UI/UI-F08_SCREENUI_WIDGET_POINTER_INTERACTION_DESIGN.md)（多键状态机、Modifiers、Widget Enter/Leave/Down/Up/Click、Button 兼容、Drag `InitiatingButton`；非目标含库存算法与 FReply 冒泡）；ACTIVE_WORK 并行 UI 轨
- **Verification:** docs only
- **Next:** 维护者审阅 / design-review → Planned → Impl Plan S01–S04

## 2026-10-18 — CORE-F38 评审修订（仍 Draft）

- **Scope:** 吸收 TaskSystem MVP 评审；只改 Design
- **Completed:** 明确 Submit/Wait/完成事件/Drain、Shutdown、异常、Worker 禁止再 Submit、功能与性能测试分离；S00–S02 完成标准写清
- **Verification:** docs only
- **Next:** 维护者审批 → Planned → 再写 Impl Plan

## 2026-10-18 — GP-F03 Done：GameplayTag ME_STRUCT + SetTagName 绑定

- **Scope:** 持久 `TagName`；比较仍走 index；反序列化经 Invisible Setter 绑定
- **Completed:** `GameplayTag` 升 `ME_STRUCT`；`SetTagName` → `BindTag`；`CORE-F39` Cancelled。Design / Registry / ACTIVE_WORK → **Done**
- **Verification:** `minEngineTests.exe test gameplay-tags` / `gameplay-events` PASSED
- **Next:** minCraft 去掉 string 旁路（游戏仓）

## 2026-10-18 — GP-F03 Design 重写：持久名 + 强制运行时 index

- **Scope:** 纠正「按 TagName 比较 / index 可选」的偏差
- **Completed:** [GP-F03](./Gameplay/GP-F03_GAMEPLAY_TAG_MESTRUCT_DESIGN.md) 重写为双层身份：`TagName` 仅存档；`==`/`Matches` 只走 Bind 后的 index；反序列化后强制 Bind
- **Verification:** docs only
- **Next:** design review

## 2026-10-18 — GP-F03：GameplayTag 升 ME_STRUCT；CORE-F39 取消

- **Scope:** minCraft Tag 不可序列化；根因是句柄只存 index，非缺 codec 机制
- **Completed:** `CORE-F39` → **Cancelled**；初版 GP-F03 Draft（后被上条重写）
- **Verification:** docs only
- **Next:** 见上条

## 2026-10-18 — CORE-F39：值类型 Codec 注册 Design Draft（已取消）

- **Scope:** （历史）曾规划可扩展 value codec；后改由 GP-F03 修正 Tag 形状
- **Completed:** 初稿后 **Cancelled**（见上条）
- **Verification:** docs only
- **Next:** 无

## 2026-10-17 — ASSET-F10 S01/S02：多态 DataAsset Load/Create

- **Scope:** `AssetMeta::RuntimeClassName` 通用字段；Save 统一写入；Load 两档；`CreateDataAsset`；按类查询
- **Completed:**
  - 可恢复资产缺字段 Warning 并重写 meta；类名不符则失败
  - DataAsset 缺字段 / 错类 / 非派生则失败；按 `MEClass` 构造
  - `GameModuleStub::SampleTableDataAsset`（`vector` of `ME_STRUCT`）roundtrip，无 handler 注册
  - `FindAssetMetasByRuntimeClass` 改扫 meta 字段
- **Verification:** `minEngine/bin/minEngineTests.exe test data-asset` PASSED
- **Next:** S03 AES/Inspector 派生（Deferred）；minCraft 去掉 Bootstrap

## 2026-10-17 — ASSET-F10：缺 RuntimeClassName 分两档

- **Scope:** 存量 `.meta` 没有类名字段时的失败策略
- **Completed:** Design 改为按「AssetType 能否唯一推出类」分档：1:1 资产 Warning 并在 Load 成功后尝试重写；DataAsset 缺字段 Error 失败（避免猜成基类再焊死）。本 Feature 之前的基类 `.medata` 走一次性迁移
- **Verification:** docs only
- **Next:** 审阅 Design → Planned → S01

## 2026-10-17 — ASSET-F10：RuntimeClassName 升为通用 AssetMeta 字段

- **Scope:** 设计修订（对照 UE `FAssetData::AssetClassPath`、Godot 资源头、Unity `.meta` 分工）
- **Completed:** [Design](./Asset/ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md) 改为：所有资产 Save 写入实际类名；DataAsset 用它 `NewObject`；其它 Loader 只做不符即失败的冗余校验；空字段仅表示旧 meta
- **Verification:** docs only
- **Next:** 维护者审阅 → Planned → S01

## 2026-10-16 — ASSET-F10 DataAsset 多态 Load Design Draft

- **Scope:** minCraft 反馈：派生 DataAsset Load 写死基类；否定游戏仓 per-type handler 注册
- **Completed:** 登记 `ASSET-F10`；[Design Draft](./Asset/ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md)（meta.`RuntimeClassName` + 共享 Loader；零派生管线注册）；F07 扩展契约勘误；ACTIVE_WORK Primary 紧急轨；Registry next ASSET → F11
- **Verification:** docs only
- **Next:** 维护者审阅 → Planned → S01（Load/Create/Save + 派生 roundtrip 单测）

## 2026-10-18 — CORE-F38：单 Task 冒烟；无依赖图；待审批

- **Scope:** 维护者确认不要依赖图验收；sleep 封装另域
- **Completed:** Verification 改为单 Task sleep+log；Non-Goals 含依赖图与引擎 Sleep；Verdict Ready with deferred items，**等审批** 再 Planned / Impl Plan
- **Verification:** docs only
- **Next:** 维护者批准 Design → Status Planned → Impl Plan S00–S02

## 2026-10-18 — CORE-F38 再审：教学冒烟 A→B

- **Scope:** 收窄后二次 review；维护者问最小 sleep+log / 依赖顺序测试
- **Completed:** （已被下一条修订）曾写入 Drain 串联 A→B
- **Verification:** docs only
- **Next:** （见上条）

## 2026-10-17 — CORE-F38 收窄：不含 Asset 异步

- **Scope:** 维护者要求第一刀更谨慎
- **Completed:** Design 只保留 TaskFlow + Drain + 合成 CPU 证明；`ASSET-F09` → **Deferred**；Registry / ACTIVE_WORK / Digest / PROJECT_CONTEXT 同步
- **Verification:** docs only
- **Next:** F38 Impl Plan（S00–S02）；F09 等 F38 站稳后再开 Design

## 2026-10-17 — CORE-F38 Design Review：OQ 按默认锁定

- **Scope:** 维护者确认 Open Questions 用默认；agent design-review
- **Completed:** Decisions 表；同步 Load 改为等字节 future 再在游戏线程装盘（避免与 Drain 死锁）；MVP 不暴露依赖图；Shutdown 必须 Drain；Verdict **Ready with deferred items**（S00–S02）
- **Verification:** docs only；对照 `AssetSerializeHelper::ReadObjectFromFile`（尚无从内存读，属 F09）
- **Next:** Implementation Plan（S00–S02）；F09 等 ASSET-F10

## 2026-10-17 — CORE-F38 Design：Drain 讲解 + DataAsset 首客户 + UE/Godot 锚点

- **Scope:** 回应「Drain 是什么 / Texture2D 是否太险 / 是否对标 UE·Godot」
- **Completed:** Design 已含 Drain 定义、UE TaskGraph / Godot WorkerThreadPool+MessageQueue 源码锚点；首资产客户锁定 DataAsset；合成作业为性能门闩；对齐 ASSET-F10；Learning path 四步
- **Verification:** docs only
- **Next:** 维护者确认 Open Questions → Planned；实现仍后置（当前 Primary 为 ASSET-F10）

## 2026-10-16 — CORE-F38 TaskSystem Design Draft

- **Scope:** 登记 `CORE-F38` + `ASSET-F09`；撰写 TaskSystem Design
- **Completed:** [CORE-F38_TASK_SYSTEM_DESIGN.md](./Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md) Status **Draft**；Registry / ACTIVE_WORK / Digest / PROJECT_CONTEXT 同步
- **Verification:** docs only；对照 `Engine::TickOneFrame`、`LoadAssetByMeta_Internal`、`Texture2DLoader::LoadFromAssetMeta`（CPU+RHI 同路径）
- **Next:** 维护者审阅 Open Questions → Planned → Impl Plan（勿开工码）

## 2026-10-16 — 记录三条后续目标；多线程先 TaskSystem

- **Scope:** ACTIVE_WORK / PROJECT_CONTEXT / BOOTSTRAP_DIGEST
- **Completed:** 三条轨落成目标（未登记 Feature）。多线程拆成 TaskSystem（先）与逻辑/渲染/RHI 线程（后）。验收要求：同一工作上 1 worker 对 N worker，不靠空场景 FPS
- **Verification:** docs only
- **Next:** 讨论 TaskSystem 设计，再决定是否开 Design

## 2026-10-16 — 引导文档对齐当前 master

- **Scope:** `BOOTSTRAP_DIGEST`、`PROJECT_CONTEXT`、`ACTIVE_WORK`、Capability Roadmap 阶段判断
- **Completed:** 快照从 9 月 Sprite Primary 改到 10 月已落地能力；记下三条未立项方向（多线程 / Atmosphere / Plugin）
- **Verification:** docs only
- **Next:** 与维护者讨论后再决定是否登记 Feature

## 2026-10-16 — RND-F24-S03 Done：VertexCustom Custom0/1

- **Scope:** 材质图消费 Custom0@5 / Custom1@6（镜像 VertexColor）
- **Completed:**
  - `EI_Custom0/1`、`UsesCustom0/1`、Translator → `v_CustomN`
  - Shell：canonical `a_CustomN` + dense varying；私有 `VertexAttributeUsage`
  - `MaterialGraphNodeDef_VertexCustom`（`CustomIndex` 0/1；非法 → Poison+诊断）
  - Editor 调色板；`material-ir` Custom0+Custom1 金样
- **Verification:** `minEngineTests.exe test material-ir`（从 `minEngine/bin`）PASSED
- **Next:** 可选 S04 工厂糖衣；或换轨

## 2026-10-16 — RND-F24-S03 Design：Vertex Custom（Review）

- **Scope:** Custom0/1 材质消费（镜像 S02 Color）
- **Completed:** [RND-F24_S03_VERTEX_CUSTOM_DESIGN.md](./Render/RND-F24_S03_VERTEX_CUSTOM_DESIGN.md) Status **Review**
- **Verification:** docs only；对照 VertexColor / shell / VertexAttr
- **Next:** 维护者审阅批准 → 实现

## 2026-10-16 — CORE-F37-S02 Done：RHI `dynamic_cast` → `static_cast`

- **Scope:** OpenGLRHI / VulkanRHI / VulkanRHIResources / RHICommandList
- **Completed:**
  - 27 处直接 `static_cast`；无包装、无 Debug `dynamic_cast`
  - Design → **Done**；RDG cast 未动；TD-033 仍 Open
- **Verification:** `minEngineTests` build OK；`test smoke` PASS；范围文件 `dynamic_cast` = 0
- **Next:** RND-F24 S03；B2/B1-b 后置

## 2026-10-16 — CORE-F37-S02 Design：RHI static downcast（Review）

- **Scope:** 簇 C1；单后端不变量 → **纯 `static_cast`**（修订后 Debug 亦无 RTTI 认亲）
- **Completed:**
  - [CORE-F37_RHI_STATIC_DOWNCAST_DESIGN.md](./Render/CORE-F37_RHI_STATIC_DOWNCAST_DESIGN.md) Status **Review**
  - 拒 MIR `As` / Debug `dynamic_cast` assert；排除 RDG；typed handle → **TD-033**
  - Survey / Registry / ACTIVE_WORK 同步
- **Verification:** docs only
- **Next:** 批准 → S02 实现

## 2026-10-16 — MAT-F07 Done：虚 Get*PinType + 删 Util 梯子

- **Scope:** CORE-F37 簇 A1 / MAT-F07 S01+S02
- **Completed:**
  - `MaterialGraphNodeDef::GetOutputPinType` / `GetInputPinType`；固定类型子类 override
  - 删除 `MaterialValueTypeUtil::GetNode*PinType`；EdGraph / Validate 直调虚接口
  - `Material.cpp` / `MaterialTestGraph` → `Cast<>`
  - Design / Registry / ACTIVE_WORK / Survey → **Done**
- **Verification:** `minEngineTests` build OK；`test material-ir` PASS（5/5）
- **Next:** CORE-F37 C1 TD；战役其余后置

## 2026-10-16 — MAT-F07 Design 修订：纯虚 PinType + 删 Util 类型 API

- **Scope:** 修订 [MAT-F07 Design](./Render/Material/MAT-F07_NODEDEF_PIN_VALUE_TYPE_DESIGN.md)（仍 **Review**）
- **Completed:**
  - 锁定改为：虚 `Get*PinType` override；删除 `MaterialValueTypeUtil::GetNode*PinType`
  - 取消 pin transient 字段与 `RefreshPinTypes`
  - Survey §7.1 / Changelog 同步
- **Verification:** docs only
- **Next:** 批准 → S01 实现

## 2026-10-16 — MAT-F07 Design：NodeDef pin value type（Review）

- **Scope:** CORE-F37 簇 A1 → 独立 Feature `MAT-F07`
- **Completed:**
  - 初稿 Design + Registry / ACTIVE_WORK / Survey 链接（后经修订，见上条）
- **Verification:** docs only
- **Next:** （见修订条）

## 2026-10-15 — CORE-F37 B1-a：Runtime MEObject → `Cast<>`

- **Scope:** 簇 B1-a（仅 Runtime；不含 A1/C1/Editor/Tests）
- **Completed:**
  - 20 文件 / 34 处：`dynamic_cast` / `dynamic_pointer_cast` → `Cast<>` + `ObjectCast.h`
  - 覆盖：ObjectManager、GameObject/Component Outer、SceneManager、Prefab*、AssetManager、Loaders、AnimationClipImporter、EnvironmentMap
- **Verification:** `minEngineTests` build OK；`test smoke` PASS；`test prefab` PASS（17/17）
- **Next:** A1（MAT-F07 Design）或 C1 TD 登记；B1-b Editor/Tests 本战役放过

## 2026-10-15 — CORE-F37-S01 Done：MIR `As<T>()`（对齐 UE）

- **Scope:** 簇 A2 实现
- **Completed:**
  - `MIRValue::As<T>()` + 自由函数 `As<T>(MIRValue*)`；`AsDimensional` 转发
  - `MIRDebug` / `MIREmitter` / `GLSLMaterialTranslatorImpl` 去掉 MIR `dynamic_cast`
  - Design Status → **Done**
- **Verification:** `minEngineTests.exe test material-ir` PASS（5/5）；MaterialIR+GLSL grep 无 `dynamic_cast`
- **Next:** B1-a 或 A1 Design（战役仍 In Progress）

## 2026-10-15 — CORE-F37-S01：MIR Kind Cast Design（对齐 UE，待审批）

- **Scope:** 簇 A2；学 UE `FValue::As<T>()`；未改代码
- **Completed:**
  - [CORE-F37_MATERIAL_IR_KIND_CAST_DESIGN.md](./Render/Material/CORE-F37_MATERIAL_IR_KIND_CAST_DESIGN.md) Status **Review**
  - Survey §7.2 指向该 Design；执行序 S01 优先
- **Verification:** docs + UE 源码锚点对照
- **Next:** （已批准并实现，见上条）

## 2026-10-15 — CORE-F37：RTTI Hygiene Survey 落盘（Review）

- **Scope:** 将 RTTI 盘点整理为可审阅文档；未改运行时代码
- **Completed:**
  - [CORE-F37_RTTI_HYGIENE_SURVEY.md](./Platform/Core/CORE-F37_RTTI_HYGIENE_SURVEY.md)：A/B/C 定义、簇 A1–C1、开放设计题
  - Registry / ACTIVE_WORK → **Review**
- **Verification:** docs only
- **Next:** 维护者填 A1/A2（及 B/C 确认）→ 再拆 Design / 机械波

## 2026-10-15 — 源码注释 ID 清扫 + RTTI 分类调研

- **Scope:** 落实 WF-F05 注释卫生；只读盘点 `dynamic_cast` / `dynamic_pointer_cast`（未改 RTTI 码）
- **Completed:**
  - 源码（含部分日志/测试字符串）去掉 Feature/Slice/Bug/ADR ID；残留扫描 **0**
  - RTTI：约 **68 文件 / 218 处**（excl. Third-Party）；分类见 CORE-F37 Survey
- **Verification:** grep ID 模式 → 0 hits；cast 计数脚本
- **Next:** Survey 审阅（已落盘）

## 2026-10-15 — WF-F05：合入 TMPL-F05（注释/分支 ID 禁令 + Quality DoD）

- **Scope:** sync `min-agent-workflows` TMPL-F05 into minEngine-adapted paths
- **Completed:**
  - hard-constraints §4 comment ID ban + §5 topic branch naming
  - `templates/DESIGN_PRINCIPLES.md`；DOC_GOVERNANCE Process/Quality DoD
  - bootstrap / profile / WORKING_WITH_AI / triggers / implementation-discipline
  - engineering-design intent playback + anti-shell；design-review shell flags
- **Verification:** docs cross-link；paths use `.agents/skills/`
- **Next:** source comment ID sweep；then RTTI classification survey

## 2026-10-15 — CORE-F36 合入 master（S00–S04 Done）

- **Scope:** `feat/core` → `master`（ff）；文档收口
- **Completed:**
  - FixedTick 主链合入；Registry / ACTIVE_WORK / Design → **Done**
  - S05 Stub **Deferred**
- **Verification:** `gameplay-fixed-tick` / `tickable-subsystem` PASS（合入前）
- **Next:** RND-F24 S03 / F33 review / ASSET-F04

## 2026-10-15 — CORE-F36 S00–S04：FixedTick 时钟 + 分层派发（feat/core）

- **Scope:** `feat/core` 实现 → 已合入 `master`
- **Completed:**
  - `LogicalTick`：`Input → Fixed → Variable(GI→WorldTickable→GO) → Physics → …`
  - `GameplayFixedTickClock` + Engine Fixed API；GI `OnFixedTick`；World/GO←Comp Fixed 汇聚
  - suite `gameplay-fixed-tick`；回归 `tickable-subsystem`
- **Verification:** `minEngineTests.exe test gameplay-fixed-tick` / `tickable-subsystem` PASS
- **Next:** （已合 master；S05 Deferred）

## 2026-10-15 — RND-F24 S01–S02：Canonical locations + VertexColor

- **Scope:** `feat/render` → 已合入 `master`
- **Completed:**
  - S01：`VertexAttr` 表；`RHIVertexElement::Location`；GL/VK 按 Location；骨骼 shell @9/10
  - S02：`MaterialGraphNodeDef_VertexColor`（RGBA/RGB）；shell `a_Color@4` + `v_Color`；`UsesVertexColor`
  - MinGW PE：去掉 `imgui_demo`；`exclude-modules-for-implib=miniaudio.c.obj`（勿 exclude glad）
- **Verification:** `vertex-attribute-semantics` PASS；`material-ir` PASS（含 VertexColor→Albedo）
- **Next:** S03 Custom0..1；可选画面梯度金样 / 工厂糖衣

## 2026-10-15 — CORE-F36 Design：变步时序纠正为 Physics 前玩法块

- **Scope:** 纠正 R13/S00（仍未开工码）
- **Completed:**
  - 锁定：`Input → Fixed → Variable(GI→WorldTickable→GO/Comp) → Physics → …`
  - S00 = 把 GI/World Tickable **前移**与 GO 汇合（非把 GO 后移）
- **Verification:** docs only
- **Next:** 文档 commit → 审批后从 S00 开工

## 2026-10-15 — CORE-F36 Design：纳入变步 Tick 相位修复

- **Scope:** Design 补充 R13/S00（仍未开工码）
- **Completed:** （已被下一修订纠正方向）初稿曾误写 GO 迁 Physics 后
- **Verification:** docs only
- **Next:** 见上条

## 2026-10-15 — CORE-F36 Design 修订（唯一时钟 + 多层派发）

- **Scope:** 按维护者意图修订 FixedTick Design（仍未开工码）
- **Completed:**
  - 引擎唯一时钟；`OnFixedTick` 由派生 GI 编排子系统序
  - WorldTickable 跟变步 `m_Instances` 序；GO←Comp 汇聚 Participants
  - （随后一修订已把变步相位从「后置」改为本 Feature 必修）
  - agent 自审：Ready with deferred items
- **Verification:** docs only
- **Next:** 维护者审批 CORE-F36（及此前 RND-F24）→ Planned

## 2026-10-15 — CORE-F36 / RND-F24：Design Draft（待审批）

- **Scope:** minCraft 反哺缺口 → 引擎级机制设计（未开工码）
- **Completed:**
  - 注册 `CORE-F36`（Gameplay FixedTick 共享时钟）· `RND-F24`（顶点语义 Color→Custom）
  - Design：[CORE-F36](./Platform/Core/CORE-F36_GAMEPLAY_FIXED_TICK_DESIGN.md) · [RND-F24](./Render/RND-F24_MESH_VERTEX_ATTRIBUTES_DESIGN.md)
  - ACTIVE_WORK / FEATURE_REGISTRY 已挂；agent 自审 Verdict：Ready with deferred items
- **Verification:** docs only
- **Next:** 维护者审批 Design → Planned → Impl Plan；实现勿与 F33 混 PR

## 2026-10-15 — BUILD-F01 S07：GameModuleEntry 强制加载

- **Scope:** 引擎收口「游戏 SHARED 进进程」；去掉各游戏手写 Marker 的推荐路径
- **Completed:**
  - `MinEngineGameModuleEntry.h` + `MINENGINE_IMPLEMENT_GAME_MODULE_ENTRY`
  - `minengine_link_game_module`：`-Wl,-uMinEngine_GameModuleEntry`（CACHE INTERNAL 符号名；避免 CMake 拆逗号 / include_guard 空串）
  - GameModuleStub + Launcher 模板 `Module.cpp` 实现 Entry
  - SampleGame / SampleEditor PE 导入表含 `libGameModuleStubd.dll` + `MinEngine_GameModuleEntry`
- **Caveat:** 若 `libMaximumEditorLibd.a` 成员残缺（Windows `ar` 长命令行），删 `.a` 后重建即可
- **Verification:** build SampleGame/SampleEditor/Editor；`objdump -p` 导入表
- **Next:** 可选 minCraft 迁 Entry；或 F33 review / ASSET-F04

## 2026-10-14 — CORE-F35：GI 中枢 + 项目可选派生类

- **Scope:** Hub vs Subsystem；`ProjectSettings.GameInstanceClass`；Standalone/PIE 按类创建
- **Completed:**
  - Init 序：Collection → OnInit；Shutdown：OnShutdown → Deinit
  - `ResolveGameInstanceClass` / `SetGameInstanceClassOverride`；Create* 共用
  - `StubGameInstance` + `TestDerivedGameInstance`；模板 mesettings 字段
  - Feature 引擎侧 **Done**（S04 minCraft Deferred）
- **Verification:** `game-instance-subsystem` / `game-module-stub` / `tickable-subsystem` / `world-subsystem`
- **Next:** 可选 minCraft 派生 GI；或 F33 review

## 2026-10-14 — BUILD-F01 S05：minCraft 迁项目根双 exe

- **Scope:** 外仓 minCraft 以 `ENGINE_ROOT` 为根；`MinCraft.exe` + `MinCraftEditor.exe`；删 Marker/DEFER
- **Completed:**
  - `MinEngineTargets`：MinGW `--no-as-needed` 保住 GAME_MODULE DLL
  - minEngine：`exclude-libs` 补 Assimp/zlib（静态 fallback 不爆 PE ordinal）
  - minCraft：项目根 CMake；`MinCraftGame`（OUTPUT `MinCraft`）/ `MinCraftEditor`；`GameMain.cpp`；去 `MinCraftModuleMarker`
  - ASSET-F08：`BlockTextureAtlas` 改 `LoadAssetByPath` + cast
- **Verification:** configure `minCraft/build-s05`；build `MinCraftGame` + `MinCraftEditor` → `bin/MinCraft.exe` / `MinCraftEditor.exe` / `libminCraft.dll`
- **Next:** 双仓 commit；主线可选 F33 review / ASSET-F04

## 2026-10-14 — CORE-F34 S06：GameModuleStub 示范子系统

- **Scope:** 游戏 SHARED 示范 GI / TickableWorld 子系统；反射发现
- **Completed:**
  - `StubGameInstanceSubsystem` / `StubTickableWorldSubsystem` + gen
  - Marker 触达；suite `game-module-stub` 扩子系统用例
  - Feature 主链 S00–S06 **Done**（S07/S08 后置）
- **Verification:** `minEngineTests test game-module-stub`（2 cases）
- **Next:** 可选 S07 LocalPlayer；或换 Primary（F33 / ASSET-F04 / BUILD S05）

## 2026-10-14 — CORE-F34 S05：Tickable 相位

- **Scope:** TickableWorld/GI 单插入点（Physics/UI 后、EOF 前）
- **Completed:**
  - `IsTickable`；`Scene::TickWorldSubsystems` / `GameInstance::TickSubsystems` / `SceneManager::TickWorldSubsystems`
  - `Engine::TickTickableSubsystems` 接入 `LogicalTick`
  - suite `tickable-subsystem`；F33 `deferred-structural-edit` 回归
- **Verification:** build minEngineTests；`tickable-subsystem` / `game-instance-subsystem` / `world-subsystem` / `deferred-structural-edit`
- **Next:** S06 Sample 示范子系统

## 2026-10-14 — CORE-F34 S04：Play Mode 每 PIE 一 GI

- **Scope:** PIE 会话宿主；Enter/Exit 顺序对齐 UE（GI → World → BeginPlay）
- **Completed:**
  - `Engine::Create/DestroyGameInstanceForPIE` + `m_PIEGameInstances`；`GetGameInstanceForPIE`
  - `FinalizeSceneStructure` 与 World Init 分离；PIE Duplicate 只做结构
  - `PlayInEditorSession` Enter/Stop 接线 GI；World Init 移到 GI.Init 之后
  - suite `game-instance-subsystem` 扩 PIE isolation + 顺序断言
- **Verification:** clean rebuild minEngine/Tests/Editor；`game-instance-subsystem` / `world-subsystem` / `begin-play`
- **Next:** S05 Tickable 相位

## 2026-10-14 — CORE-F34 S03：Scene WorldSubsystem 集合

- **Scope:** 每 Scene 一份 World 子系统；Finalize / 卸载 / Play 相位转发
- **Completed:**
  - `Scene::InitializeWorldSubsystems` / `PostInitialize` / `Deinitialize`；`GetSubsystem<T>`
  - `FinalizeLoadedScene` / `CreateNewScene` / PIE Finalize 拉起；Unload / UnregisterPIE / Shutdown 卸下
  - `DispatchBeginPlay/EndPlay` 转发 `OnWorldBeginPlay/EndPlay`；`DoesSupportWorldType`
  - suite `world-subsystem`
- **Verification:** build minEngineTests；`world-subsystem` / `begin-play` / `game-instance-subsystem`
- **Next:** S04 Play Mode 每 PIE 一 GI

## 2026-10-14 — CORE-F34 S01+S02：Engine MEObject + GameInstance / SubsystemCollection

- **Scope:** 引擎作用域子系统骨架（Engine + GameInstance；不含 World / Tick / PIE）
- **Completed:**
  - `Engine : MEObject`；`Subsystem` / `EngineSubsystem` / `GameInstanceSubsystem`（+ Tickable / World 抽象基类）
  - `SubsystemCollection`：反射发现、`ShouldCreateSubsystem`、`InitializeDependency`、环检测、逆序 Deinit
  - `GameInstance` Init/Shutdown；`Engine::CreateGameInstance` / `DestroyGameInstance` / `GetEngineSubsystem`
  - OM `Initialize` 幂等（已有实例不 wipe registry）
  - suite `game-instance-subsystem`（含 gated test fixtures）
- **Verification:** build minEngine / minEngineTests；`test game-instance-subsystem`；`test object-manager`
- **Next:** S03 Scene WorldSubsystem 集合

## 2026-10-14 — CORE-F34 S00：ObjectManager 进程单例

- **Scope:** ObjectManager 脱离 Engine 拥有，对齐进程级设施
- **Completed:**
  - `ObjectManager::Initialize/Shutdown` 静态 API + `s_Owned`；Engine 删除 `m_ObjectManager`
  - 测试改为静态 Init/Shutdown（仍可 TestAccess 注入栈实例）
- **Verification:** build minEngineTests；`object-manager` / `smoke` / `serialization-archive`
- **Next:** S01 Engine : MEObject + SubsystemCollection 骨架

## 2026-10-13 — BUILD S03/S04：GameProject 模板 + package-game

- **Scope:** 引擎侧产品构建余量（不含 minCraft 迁根）
- **Completed:**
  - `Launcher/Templates/GameProject` + 占位符替换；cwd/settings 解析 `ENGINE_ROOT`
  - `minengine_add_package_game`；Sample `package-SampleGame` → Staging
  - `EXECUTABLE_PATH` CACHE；MSVC `/utf-8`；`.gitignore` Staging/
  - ACTIVE Primary → CORE-F34；BUILD S05 Deferred
- **Verification:** `cargo test` create_*；Launcher create + MinGW configure；`package-SampleGame`
- **Next:** CORE-F34 S00 ObjectManager 进程单例

## 2026-10-13 — 移除反向 GAME_DIR；注释去 Feature ID

- **Scope:** 产品构建清理
- **Completed:** 删除 `MINENGINE_GAME_DIR` / KeepAlive；CMake/示例注释改为机制描述（不写 Feature 编号）
- **Verification:** 无外仓 EXTRA 时 Sample* + Editor + `game-module-stub` 可编
- **Next:** S03 模板；minCraft 改项目根自建 Editor/Game

## 2026-10-13 — BUILD-F01 S02：EditorLib + Sample 双 exe

- **Scope:** 产品构建 S02（设计补 Game/Shipping 后开工）
- **Completed:**
  - Design：R8 Game/Shipping/Stage 预留；OQ2 EditorLib；顺序 BUILD→F34；Ready
  - `MaximumEditorLib` STATIC + 薄 `EditorMain.cpp`；`MinEngineTargets.cmake`（add_editor / add_game）
  - 默认 `BUILD_GAME_MODULE_SAMPLE`：`SampleGame.exe` + `SampleEditor.exe` link `GameModuleStub`（无反向拉取）
  - ED-F20 / `MINENGINE_GAME_DIR` Deprecated + WARNING
- **Verification:** configure+build Sample* / EditorLib / tests；`minEngineTests test smoke` + `game-module-stub`；`SampleGame --version`
- **Next:** S03 模板 + Launcher；S04 `package-game` Stage；再 S05 minCraft 迁根

## 2026-10-13 — BUILD-F01：Game/Shipping 预留 + Ready；主线先 BUILD

- **Scope:** 完善产品构建设计；实施顺序改为 BUILD → CORE-F34
- **Completed:**
  - BUILD-F01：R8 Game exe + Development/Shipping 挂点 + `package-game` Stage；OQ2 EditorLib 钉死；Verdict Ready w/ deferred Cook
  - ED-F20 / CMake：`MINENGINE_GAME_DIR` 标 Deprecated + WARNING
  - ACTIVE：Primary 仅 BUILD-F01；F34 queued
- **Verification:** N/A（已并入 S02 条目）
- **Next:**（见上）

## 2026-10-12 — 双轨定稿：BUILD-F01 + F34（OM 单例）

- **Scope:** 构建产品化独立 Feature；F34 锁定 ObjectManager 进程单例；F34 Design Review
- **Completed:**
  - 登记 [BUILD-F01](./Platform/ProductBuild/BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)（项目根编辑器、模板、退役 ED-F20）
  - F34：OM 升至 Log/Reflection 级；S00 切片；Verdict **Ready with deferred**（BUILD 不挡 S00–S05）
  - ACTIVE 双轨 Primary
- **Verification:** N/A（文档）
- **Next:** 你审 BUILD-F01 OQ2；批准后 F34 开 S00 或先深化 BUILD 模板变量

## 2026-10-12 — CORE-F34：模块链接溯源 + Engine:MEObject

- **Scope:** 澄清 Stub/KeepAlive/ED-F20 痛点链；OQ6 锁定 Engine 继承 MEObject
- **Completed:** Design 写入「引擎↔游戏模块」专节与 Engine MEObject bootstrap 阻碍表；否决代理 Outer；弱化未经设计的 GameModuleHost 类名
- **Verification:** N/A（文档）
- **Next:** Design Review；S01 可与「模块契约」短 Design 并行；Engine:MEObject 放在含 Engine 集合的切片

## 2026-10-12 — CORE-F34 Design 展开（反射·单 Tick·退役 KeepAlive·接口面）

- **Scope:** 同 F34；回应审阅点
- **Completed:** 锁定反射 `GetDerivedClasses`；否决对外 PreSim/PostSim（对照 UE `ETickingGroup` / Godot process）；写清 KeepAlive/Stub 真相与 `GameModuleHost` 替换；展开 Collection/GI/Scene/Engine/Play/Module API
- **Verification:** N/A（文档）
- **Next:** 钉 OQ6（Engine 作 Outer）→ Design Review → S01

## 2026-10-12 — CORE-F34 Design Draft（GI + Subsystem 生命周期升格）

- **Scope:** 引擎系统 + 游戏 App 生命周期契约；对齐 UE GameInstance/SubsystemCollection；非保守 MVP
- **Completed:** 登记 `CORE-F34` L3 Draft；[Design](./Platform/Core/CORE-F34_GAME_INSTANCE_AND_SUBSYSTEM_LIFECYCLE_DESIGN.md)；ACTIVE Primary 切换；显式划掉反哺稿无 GI / 错层 EngineSubsystem / PostFrame-only Spawn
- **Verification:** N/A（文档 Draft；待 Design Review）
- **Next:** 接口展开（本轮已做）→ Review → S01

## 2026-10-12 — CORE-F33 S01+S02 实现（feat/core）

- **Scope:** Spawn 即时入世 + Tick/Dispatch 定长；Destroy 延迟；EOF swap；Engine Flush
- **Completed:**
  - `Scene::{Spawn,Destroy,FlushDeferredDestroys}`；定长 `Tick`/`Dispatch*`；遍历中 `Remove` Error
  - `SceneManager::SendAllEndOfFrameUpdates` swap 快照；`FlushDeferredStructuralEdits`
  - `Engine::LogicalTick`：EOF 后 Flush
  - Suite `deferred-structural-edit`（Find 即时 / 同帧不 Tick / Destroy Flush / EOF 下帧 / BeginPlay Destroy）
- **Verification:** `minEngineTests.exe test deferred-structural-edit` PASS；`test begin-play` PASS
- **Next:** 你个人 code review → Doc DoD / commit；S03 Prefab 可选后置

## 2026-10-12 — CORE-F33 本机 UE/Godot 源码核对

- **Scope:** 对照 `D:/Dev/GitRepo/UnrealEngine` · `D:/Dev/GitRepo/Godot` 核 Spawn/Destroy/Ready/Flush
- **Completed:** Design 写入锚点表；确认即时入世 + 延迟销毁；补 BeginPlay 中 Destroy 推迟；标明定长不 Tick 相对 UE 更保守
- **Verification:** 只读源码（`LevelActor.cpp` / `Actor.cpp` / `node.cpp` / `scene_tree.cpp`）
- **Next:** 你终审 Open Questions（dying∈GetAll？同帧不 Tick？）→ Planned / Review

## 2026-10-12 — CORE-F33 Design 中文 + 业界 BeginPlay 对照

- **Scope:** 同一 Draft；正文改中文；补 Destroy/Spawn/BeginPlay 业界时机表
- **Completed:** [CORE-F33](./Platform/Core/CORE-F33_DEFERRED_STRUCTURAL_EDIT_DESIGN.md) 默认锁定「Flush 当帧 BeginPlay、次帧 Tick」
- **Verification:** N/A（文档；待审批）
- **Next:** 你确认 Open Questions（尤其是否下一帧 BeginPlay）→ Design Review

## 2026-10-12 — CORE-F33 Design Draft（Deferred Structural Edit）

- **Scope:** minCraft 反哺：运行期 Scene 结构变更契约（Spawn/Destroy 队列 + EOF swap-drain）
- **Completed:** 登记 `CORE-F33`；Design Draft [CORE-F33](./Platform/Core/CORE-F33_DEFERRED_STRUCTURAL_EDIT_DESIGN.md)；ACTIVE_WORK 提议 Primary
- **Verification:** N/A（文档 Draft；待审批）
- **Next:** 你审批 Open Questions 默认值 → Design Review / readiness → S01 EOF harden

## 2026-10-09 — UI-F05 / UI-F06 实现 Done（feat/ui）

- **Scope:** ScreenUI Drag-Drop + Image Sprite/Raw（工作树 `minEngine-ui` / `feat/ui`）
- **Completed:**
  - **UI-F05：** `UISystem` Pending→Dragging session；`DragSourceComponent` / `DropTargetComponent`；阈值；Click 抑制；Destroy→Cancel
  - 委托落地为**无参** Multicast（上下文 `GetDragSession` / `ConsumeLastEndSuccess`）；规避 OneParam 结构体 Broadcast AV
  - **UI-F06：** `EUIImageSourceMode` Raw/Sprite；Resolve texture/UV；Widget proxy + `ScreenUIMaterialFactory` UV
  - Suites：`screen-ui-drag`、`screen-ui-image`
- **Verification:** `minEngineTests.exe test screen-ui-drag` PASS（4）；`test screen-ui-image` PASS（3）
- **Next:** 合入 master；minCraft `TD-015` 联调；可选 UI-F07 FillAmount Design

## 2026-10-09 — UI-F05 / UI-F06 Design 改中文

- **Scope:** 同一 Draft；正文改为中文（API/类型名保持英文）
- **Completed:** 重写 [UI-F05](./Platform/UI/UI-F05_SCREENUI_DRAG_DROP_DESIGN.md) · [UI-F06](./Platform/UI/UI-F06_SCREENUI_IMAGE_SPRITE_RAW_DESIGN.md)
- **Verification:** N/A（文档）
- **Next:** 审阅 Open Questions → Design Review / readiness

## 2026-10-08 — UI-F05 / UI-F06 Design Draft（feat/ui）

- **Scope:** ScreenUI 下一波设计（工作树 `minEngine-ui`，分支 `feat/ui` 已 FF master）
- **Completed:**
  - 登记 `UI-F05` Drag-Drop、`UI-F06` Image Sprite/Raw、`UI-F07` FillAmount（Planned 占位）
  - Design：[UI-F05](./Platform/UI/UI-F05_SCREENUI_DRAG_DROP_DESIGN.md) · [UI-F06](./Platform/UI/UI-F06_SCREENUI_IMAGE_SPRITE_RAW_DESIGN.md)
  - ACTIVE_WORK 并行轨指向本波；驱动 minCraft `TD-015` + 图集图标
- **Verification:** N/A（文档 Draft；待审阅）
- **Next:** 你审阅 Open Questions → Design Review / readiness → 实现 S01

## 2026-10-08 — ASSET-F08 + ASSET-F07 Done（feat/asset-load-api）

- **F08 Done:** 删除 AM `LoadAsset<T>` / `CreateAsset<T>`；调用点 → ByPath/ByTypeId；`ObjectCast.h` `Cast<T>`；minCraft 改用 Cast
- **F07 Done:** `DataAsset` + `.medata` + Loader/Bootstrap/AES/CB Create + CB Inspector 反射编辑+Save；suite `data-asset` / `asset-manager` / `smoke` 通过
- **Docs:** F07 Design 误清空后已按 Review+实现重建；Registry/ACTIVE_WORK 同步 Done
- **Branch:** `feat/asset-load-api` — 已合入 master

## 2026-10-08 — ASSET-F08 / F07 拆轨：先去 LoadAsset<T> 糖衣

- **决定:** 分两个 Feature：先 [ASSET-F08](./Asset/ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN.md) 删除 `LoadAsset<T>`（默认含 `CreateAsset<T>`），再 [ASSET-F07](./Asset/ASSET-F07_DATA_ASSET_DESIGN.md) DataAsset
- **F08:** Design + Review Planned；真源 = `ByPath`/`ByMeta`/`CreateAssetByTypeId`
- **F07:** Design 修订为依赖 F08；Runtime 合同不再推荐 typed Load 糖衣
- **Next:** 维护者审批 F08（及 Create 默认）→ `feat/asset-load-api`；F07 暂缓

## 2026-10-08 — ASSET-F07 Thin DataAsset Design + self-review

- **Design:** [ASSET-F07](./Asset/ASSET-F07_DATA_ASSET_DESIGN.md) — 薄 `DataAsset`：`.medata`、F06 Create/Load/Save、AES/CB Create、CB Inspector 反射编辑+Save；Import/Cook/Primary Out
- **Review:** [Review](./Asset/ASSET-F07_DATA_ASSET_DESIGN_REVIEW.md) — **Ready with deferred**（游戏 AES creatable 动态注册 / Undo / 外部 Import）
- **Registry:** ASSET-F07 **Planned**；next free ASSET **F08**
- **Next:** 维护者审批 Open Questions 默认倾向 → 从当前 master 开 `feat/asset-dataasset` → S01

## 2026-10-08 — BUG-EDITOR-006 AutomationHost Stop 挂死

- **根因:** 同步 named-pipe Connect/Read；CancelIoEx 无法可靠打断 → Shutdown join 堵死
- **修复:** overlapped IO + stop event；InterruptIo 后 join 再 Close；2s 超时诊断
- **回归:** suite `automation-pipe-interrupt`
- **Docs:** [BUG-EDITOR-006](./bugs/BUG-EDITOR-006.md)

## 2026-10-08 — BUG-EDITOR-005 PIE viewport 同步 Main Camera 近平面

- **根因:** SyncPlayModeCamera 只拷位姿；viewport RenderCamera 默认 near=1；CameraComponent 镜头未推内部相机
- **修复:** ApplyLensToRenderCamera + SetNear/FOV/Far；PIE sync 拷贝 FOV/near/far
- **回归:** `play-main-camera` lens push；手动贴墙待 minCraft 验
- **Docs:** [BUG-EDITOR-005](./bugs/BUG-EDITOR-005.md)

## 2026-10-02 — BUG-CORE-005 InputComponent 析构 / Scene 晚释放

- **Review:** OM 强引用拖死 GO 已过时（`m_ObjectsByGuid` = weak）；真因是 `InputComponent` 无 `HasInstance` + Editor/PIE/Unload 对 Scene 的额外 `shared_ptr`
- **Fix:** InputComponent / MarkForNeededEOF 守卫；Unload 清同实例 editor context；PIE Stop 后丢 Scene 持有；Editor Shutdown 先 `PlayInEditorSession` + PrefabStage
- **回归:** `gameplay-input-gate` 增 `RunInputComponentDtorAfterInputShutdown`
- **Docs:** [BUG-CORE-005](./bugs/BUG-CORE-005.md)

## 2026-10-02 — BUG-ASSET-002 AssetSerializeHelper 运行时类修复

- **根因:** Helper 对 `MEObject&` 调模板 FromFile/ToFile → `MEObject::StaticClass()`，Scene 字段被 skip
- **修复:** `WriteObjectToFile` / `ReadObjectFromFile` 使用 `object.GetClass()`
- **回归:** `prefab scene AssetSerializeHelper keeps game objects [smoke]`
- **影响:** Scene/Prefab/Material 等经 Helper 的 Load/Save；解除 minCraft 冷开丢 GO / Save 写空

## 2026-10-01 — ED-F24 Done + ASSET-F05 Texture2D .metex cook

- **ED-F24 S04–S06:** CB Move/Rename/RenameFolder/CreateFolder、Delete/Reimport → AES；grep 门禁（GUI 库 mutate 仅 AES 调 AM）；Design/Registry Done
- **ASSET-F05:** `.metex` 单文件 cook；`Texture2DImporter` Product；NativeCopy 拒 png/jpg/jpeg；Load/Save/Create handlers；Reimport；Atlas `TryGetCookedPixels`；Watcher 自动 Import 经 AES；AES Create allowlist + channelSemantics options
- **测试:** `Texture2DCookTest`（suite `texture2d-cook`）；smoke from `minEngine/bin`
- **Next:** 两 commit 待批；F04 验收；Inspector 设置 UI 可后置细化

## 2026-10-01 — ED-F24 S01–S03（Create allowlist + GUI Create/Import→AES）

- **S01:** `AssetEditorServices::Create` → allowlist + `CreateAssetByTypeId`；白名单含 MaterialInstance
- **S02:** Workflow TryCreate*、Sprite Create*/Atlas、AnimGraph New Graph → AES；Sprite API 注入 `AssetEditorServices&`
- **S03:** `ImportAssetDialog` / product 选择确认 → `AES::Import`（先填 products）
- **验证:** Editor Debug 构建通过
- **Next:** S04 CB Mutation 直连 → AES

## 2026-10-01 — ED-F24 / ASSET-F05 design-review 落地；开 ED-F24 S01

- **Review:** 两份 Design Mode A → Ready with deferred；中文主文；补 Import products 合同、AES 自动导入、NativeCopy 互斥、Recook=Reimport
- **Status:** ED-F24 → Planned → **In Progress（S01）**；F05 保持 Planned
- **实现顺序:** ED-F24 S01（Create allowlist + MIC）→ …；F05 S01 随后/并行

## 2026-10-01 — ED-F24 Design Draft + ASSET-F05 Planned（post-F06）

- **ED-F24:** GUI CB/Workflow Import·Create·Mutation 收敛到 `AssetEditorServices`；Create 白名单对齐（含 MIC）；可选 `Reimport` 包装；切片 S01–S06
- **ASSET-F05:** Design 重写对齐 F06 Register*；`.metex` + Texture2DImporter/Loader；NativeCopy 让出图片；Slice S01–S06；Status **Planned**
- **Next:** 用户确认默认倾向 → L2 design-review → 择一开 S01

## 2026-10-01 — ASSET-F06 S06 + Feature Done（Create→Saver）

- **S06:** `RegisterCreateHandler` / `CreateAssetByTypeId`；`CreateAsset<T>` 薄糖衣（AssetTypeRegistry → typeId）
- **CreateHandler:** Scene / Prefab / Material / MaterialInstance / Sprite / SpriteAtlas / AnimationGraph（Loader 侧）
- **契约:** 无 Save 或无 Create → Create 失败（Mesh 等无 Create）
- **规模:** AM.cpp ~1801→~1432；Create 特化 0；Bootstrap 注册 CreateHandlers
- **验证:** minEngine + minEngineTests；`test asset-manager` / `prefab` / `smoke` PASSED（含 Material Create via MaterialLoader::Create）
- **Status:** ASSET-F06 **Done**（S01–S06）
- **Next:** 用户安排；候选 F04 验收 / ASSET-F05

## 2026-09-29 — ASSET-F06 S05（Importers 迁出 + 符号分离）

- **Importers/:** NativeCopyImporter、MeshImporter、AnimationClipImporter、AssetImportHelpers
- **AM:** Import cook / ProductAccepts / ImportProduct_* 从 cpp 移除；保留 Import()/Reimport() 门面调度
- **符号:** Static/SkeletalMeshLoader::ImportFromFile → ParseMeshFile（Load 侧）；Clip Assimp 解析迁入 AnimationClipImporter；AnimationClipLoader 仅 Load/Save .meaclip
- **规模:** AM.cpp ~2402 → ~1801；AM.h ~435 → ~404
- **验证:** minEngine Debug；asset-manager / prefab / smoke
- **Next:** S06 Create → Saver

## 2026-09-29 — ASSET-F06 S02–S04（Mutation / Saver / Load Handler）

- **S02:** `AssetContentMutation` 独立（~462 行）；AM 转发 Folder/Move/Rename/Delete
- **S03:** `AssetSerializeHelper`；`RegisterSaveHandler`；去掉 `SaveAsset_Impl`；Create 首次落盘走 Saver
- **S04:** `LoadAsset<T>` / untyped Load 只走 Handler→Loader；去掉 `LoadAsset_Impl` 与 Handler→LoadAsset 回环；缓存集中 `LoadAssetByMeta_Internal`
- **规模：** AM.cpp ~2906 → ~2402；AM.h ~483 → ~435；Load/Save 特化 0
- **验证：** `minEngine` Debug；`minEngineTests.exe test asset-manager` / `prefab` / `lua-script-mvp`
- **Next:** S05 Importers 迁出 AM.cpp

## 2026-09-29 — ASSET-F06 docs commit baseline; Status Planned

- **Baseline:** AM.h ~483 / AM.cpp ~2906; Loaders 34 files ~3425 LOC; Save特化~10; Create特化~7; Load_Impl声明~19
- **Impl note:** 迁类型时检查多重 Load/Save，禁止照搬旧链路
- **Status:** Design → Planned；下一步 S01/S02 开工
- **Docs:** F06 Client Surfaces + Baseline 附录

## 2026-09-29 — ASSET-F06 补 Client Surfaces（Editor/Automation/MCP）

- **Review:** 接触面含 AssetEditorServices、Automation asset.*、MCP（经 pipe）、CB 直连 Mutation、Workflow/Sprite/AnimGraph CreateAsset<T>、TreeModel Subscribe
- **Contract:** 冻 Automation op；AM Mutation 签名保留转发；CreateAsset<T> 默认可留糖衣；Registry 事件等价
- **Docs:** F06 增节 Client Surfaces + 验收/测试/切片联动
- **Next:** 确认后 Planned / Readiness

## 2026-09-29 — ASSET-F06 Design review 修订（Create→Saver / 去 SaveKind / Mutation 详化）

- **Decision:** Create 合入 Saver；取消 AssetSaveKind（有 Save ⇒ 引擎序列化 helper）；ContentMutation 补齐请求模型与 API
- **Docs:** F06 重写至可执行细度（~412 行）
- **Next:** 用户确认 → Planned / Readiness

## 2026-09-29 — ASSET-F06 Design 重写（可执行口径）

- **Docs:** F06 全文收敛：AM 门面；Importer/Loader/Saver 可选能力矩阵；允许 Assimp/stb Load；Builtin 集中 Register；Texture 扩 Texture2D + 引擎侧文件
- **Removed:** Codec/讨论口吻/禁 Assimp-on-Load 硬教条
- **F05:** Meta/TL 注明与 F06 对齐方向
- **Next:** Open Q 收口 → Planned / Readiness；再 Impl Plan

## 2026-09-29 — ASSET-F06 Design 加细（类型关系 / 接口一致性）

- **Docs:** F06 增补 Detailed Type Design：Pipeline 描述符、mermaid 关系图、按资产族落点表、控制流、目录布局
- **Decision:** 统一 **注册习惯（U0）**，不引入强制 IAssetIO；Import 保持 Product 表；允许类型局部 class（U1）
- **Open:** Mesh cook 是否进 F06 vs 续 F01；F04 验收与开工顺序
- **Next:** 用户确认 U0 + Open Q6/Q7 → Planned / Readiness

## 2026-09-29 — ASSET-F06 AssetManager Facade Design Draft

- **Feature:** ASSET-F06 registered (**Draft**); L3 Refactor
- **Decision:** AM 瘦门面 + 模块自注册；Import≠Load≠Save≠Create≠ContentMutation；废除模板特化脊柱；Save 用 SaveKind(S-A) 而非 Codec 双名；优先拆 Loader/Assimp-on-Load 混杂
- **Order:** F06 before F05；F04 验收可并行
- **Docs:** [ASSET-F06 Design](./Asset/ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md)；Registry/ACTIVE_WORK 已挂
- **Next:** 用户确认 Open Questions → Design Review / Planned；大段编码前 L3 Readiness

## 2026-09-29 — ASSET-F05 Texture cook Design Draft

- **Feature:** ASSET-F05 registered (**Draft**)
- **Decision:** L1 — Content 身份 .metex（设置 + cooked 像素）；源 PNG 进 Sources；Load 不依赖 raw；Color/Mask 通道语义；Inspector Recook
- **Out:** 压缩矩阵、HDR、静默全盘迁移、Atlas generated 落盘
- **Next:** 用户确认 Q1/Q2/Q4 → Planned + Impl Plan；勿与 F04 验收抢主线除非指定

## 2026-09-28 — ASSET-F04 内容工作流实现收口

- **Feature:** ASSET-F04（仍 In Progress，待验收）
- **CB:** SelectionSet 多选；空文件夹 New/Rename(F2·RMB)/Delete；资产拖拽 Move；批改名 Foo/Foo_1；Delete 确认 + session/Prefab 尽力警告
- **Automation:** import 门面；folder move/rename/batch_rename；sprite / sprite_atlas ops（含 set_padding / rebuild / create_from_selection）
- **Out 保持:** 非空 MoveFolder；全项目引用图硬门禁；新增 MCP 粗 Tool
- **Verify:** `cmake --build minEngine/build --target Editor` OK

## 2026-09-28 — ASSET-F04 S01/S02/S06 竖切开工

- **Feature:** ASSET-F04 → **In Progress**
- **Done:** ContentBrowserSelection（有序 + Primary）；CB Ctrl/Shift 多选；CreateFolder 菜单+弹窗；AssetManager folder API；AssetEditorServices Import 门面 + CreateFolder/Move/Rename；Automation sset.list_import_products / import / create_folder / move / 
ename；Editor 已编过并拉起 MyMEProject
- **Pending:** S03 Move UI、S04 批改名、S05 Delete 确认+警告、sprite.* Surface
- **Verify:** cmake --build minEngine/build --target Editor OK；MaximumEditor.exe --project MyMEProject

# minEngine Progress Log (for AI)

Last updated: 2026-09-28（ASSET-F04 Design Draft）

### 2026-09-28 - ASSET-F04 Design Draft (CB Workflow + Automation)
- **定位：** 引擎 Asset Workflow 能力补全（非 minCraft 专用后门）；外部驱动见 minCraft Atlas bake gap。
- **锁定：** SelectionSet+Primary；批改名 Foo/Foo_1…；Delete=确认+尽力警告（引用图 Out）；MCP 只扩细 op。
- **Docs：** [ASSET-F04 Design](./Asset/ASSET-F04_CONTENT_WORKFLOW_DESIGN.md)；Registry/ACTIVE_WORK Primary。
- **Next：** Design Review → Implementation Plan；Draft 下不开大段代码。

### 2026-09-24 - CORE-F32 Done (Enhanced Input hold / InputTriggerDown)
- **Fix A:** finalize `prevState = LastState`; reset tracker before Evaluate (avoid mapping NoTrigger pollution).
- **Fix B:** release/idle re-enqueue instances with LastState Ongoing|Triggered (Value=0) so Canceled/Completed fire.
- **Clarify:** NoTrigger = Triggered-while-held (Jump); InputTriggerDown = Ongoing → Started/Canceled.
- **Verify:** `minEngineTests.exe test input-trigger-down` PASS (Down Started/Canceled + Jump Triggered regression).
- **Docs:** Design/Registry/ACTIVE_WORK Done.
- **Next:** commit draft; minCraft can drop KeyDown bypass for hold binds.

### 2026-09-24 - ED-F23 Done (Viewport Pointer Capture)
- Unified Navigate/PIE policy: clip + Raw intent; Captured hide; Released show.
- Esc=Stop; Left Alt=Release; click viewport=Capture; F29 gate + EditorGameplayCaptureActive.
- Viewport toolbar hint; `gameplay-input-gate` PASS; Editor build PASS.
- **Next:** commit draft.

### 2026-09-24 - ED-F23 In Progress (Viewport Pointer Capture)
- Design + Registry; implement Navigate/PIE Captured·Released policy.
- **Prior:** BUG-EDITOR-004 S03 committed (`d5ea5d5`).

### 2026-09-24 - BUG-EDITOR-004 S03 Fixed (edge look intent)
- Clip 边界：Raw Input → virtual Mouse2D；GetMousePosition 仍 physical。
- Editor build PASS；手验边界持续看向 OK。
- **Commit:** `d5ea5d5`.

### 2026-09-24 - BUG-CORE-004 / BUG-EDITOR-004 Fixed
- **S01:** EndFrameDeviceEdgeCleanup on gated Tick; gameplay-input-gate + scroll clear unit PASS.
- **S02:** PIE focused Scene viewport ClipCursor (Win32); clear on unfocus/Stop/dtor.
- **Next:** commit draft.

### 2026-09-24 - BUG-CORE-004 / BUG-EDITOR-004 Open (post-F29 input UX)
- **CORE-004:** F29 gated Tick skips MouseScroll reset -> Edit fly speed sticks at 0.5/64.
- **EDITOR-004:** PIE has no viewport pointer confine (F05-S03 + missing Clip/DISABLED policy).
- **Fix Design:** Draft S01 device cleanup / S02 PIE pointer — BUG-CORE-004_INPUT_GATE_DEVICE_CLEANUP_AND_PIE_POINTER_FIX_DESIGN.md.
- **Next:** Implement S01 first.

### 2026-09-24 - UI-F04 Done (ScreenUI Text / Font)
- **Runtime:** TextComponent + FontGlyphAtlas/Cache + TextMeshBuilder; Widget prefers Text for proxy; local-pixel VB + ScreenUIPass.
- **Editor:** deferred string Inspector commit on deactivate; ObjectPtr+Setter via Assign (fixes Content crash / Font no-refresh).
- **Verify:** minEngineTests.exe test screen-ui-text; MCP accept_ui_text.ts visual PASS.
- **Docs:** Design/Impl/Registry Done.

### 2026-09-23 - CORE-F30 Done（Play main Camera BeginPlay policy）
- **API：** m_bSetAsMainCameraOnBeginPlay；OnBeginPlay（bool 或唯一 active Camera）→ SetSelfAsMainCamera。
- **Edit/PIE：** SetSelfAsMainCamera 仅在非 PIE 时推 Edit viewport；Play 仍靠 Main + SyncPlayModeCamera。
- **测试：** minEngineTests.exe test play-main-camera PASS。
- **Next：** 提交 F30；登记 UI-F04 Text/Font。

### 2026-09-23 - CORE-F29 Done（Play-gated gameplay Input）
- **Gate：** `InputSystem::IsGameplayInputEnabled` — `HasBegunPlayWorld` 或无 Editor viewport（standalone 兼容）。
- **Tick：** 非玩法态 early-out + `ClearGameplayActionEvents`（清 Triggered 残留）。
- **测试：** `minEngineTests.exe test gameplay-input-gate` PASS。
- **Next：** 提交 F29；再 F30 Camera BeginPlay bool。

### 2026-09-23 - CORE-F31 Done（BeginPlay / EndPlay）
- **API：** `EEndPlayReason`；`Component`/`GameObject` BeginPlay·EndPlay·HasBegunPlay；`Scene::Dispatch*`；`SceneManager::HasBegunPlayWorld`。
- **路由：** BeginPlay = Comp → GO；EndPlay = GO → Comp（对齐 UE Actor）。
- **PIE：** EnterPlay 在 System OnBeginPIE 后 DispatchBeginPlay；Stop 先 DispatchEndPlay(Stopped)。
- **动态：** 模拟中 CreateGameObject / 晚 Activate → BeginPlay。
- **测试：** `minEngineTests.exe test begin-play` PASS。
- **提交：** `d36e311`。

### 2026-09-23 - CORE-F31 Draft + Review；F29/F30 收为消费面
- **根因 reframed：** 缺 GO/Component BeginPlay/EndPlay（非仅 Camera/Input 特判）。
- **UE 锚点：** `AActor::BeginPlay` 先转发 Component 再 Receive；`EndPlay` 先 Actor 再 Component。
- **登记：** `CORE-F31` 底座；F29=Input 开关；F30=`bSetAsMainCameraOnBeginPlay`。
- **Design Review：** F31 Ready with deferred。
- **Next：** 实现 F31（已完成，见上条）。

### 2026-09-23 - SPR-F07 Done（DilateBorder inset + 边渗）
- **Pack：** `PackShelf` 改为每 sprite 独占 inset 环（outer = content + 2*pad）；返回内容矩形。
- **Dilate：** `SpriteAtlasPacker::DilateBorderRing` 边/角复制；`Rebuild` DilateBorder 调用。
- **Editor：** PaddingMode Combo；Inspector 显示。
- **测试：** `sprite-atlas` 7/7；`smoke` PASS（`minEngine/bin`）。
- **提交：** 已合入 `c106924`（与 RND-F22/F23 同波）。

### 2026-09-22 - RND-F22 Done（Texture / Sampler 分离）
- **RHI：** `RHISampler`/`RHISamplerDesc` + `GetOrCreateSampler` cache；Binding.Sampler；GL `glBindSampler`；VK `VulkanRHISampler` + descriptor write。
- **Material：** TextureSample `bOverrideSampler`+Filter/Address；`CommitCompileResult`→`MaterialTextureParameter`；Proxy Resolve + pin；Scene set1 shadow/IBL samplers。
- **Texture2D：** `PreferredSamplerDesc`。
- **验证：** `minEngine`/`minEngineTests` build；`minEngineTests.exe test smoke` PASS（from `minEngine/bin`）。
- **Next：** SPR-F07 DilateBorder。

### 2026-09-22 - RND-F23 Done（Scene Mesh 默认背面剔除）
- **代码：** `GetOrCreateSceneMeshGraphicsPipelineState` — Opaque/Translucent Back cull；ScreenUI cull off。
- **验证：** `minEngine` 目标编译通过。
- **Next：** RND-F22 Texture/Sampler 分离。

### 2026-09-22 - RND-F23 / SPR-F07 design-review（回写 Design）
- **F23：** Verdict **Ready** — Scene Mesh 单点 Back cull；API 名更正；Translucent=Back 锁定。
- **F07：** Verdict **Ready with deferred** — inset 密度下降 Accepted；PadWithZero 同 inset；Dilate 在 flipped 内存空间；Editor 补 PaddingMode。
- **Next：** Planned / 按 F23 ∥ F22 → F07 开工。

### 2026-09-22 - RND-F22 design-review → Ready with deferred（已回写 Design）
- **Verdict：** Ready with deferred items（非阻塞：Preferred meta 磁盘、AddressW UI、冲突改 error）。
- **吸收：** Compile→`MaterialTextureParameter`；`bOverrideSampler` 显式 Inherit；全 Binding 填 Sampler；Mip 映射；`glBindSampler(0)`。
- **Status：** Design/Registry → **Review**。
- **Next：** 短 Implementation Plan 或直接按 S02–S06 开工（Pre-flight）；F23/F07 仍待审。

### 2026-09-22 - RND-F22 设计升格（Texture / Sampler 分离）
- **变更：** 否决「仅 CreateDesc 加 Filtering」；重写为 L3：`RHISampler` + Desc **cache 复用** + Binding(SRV,Sampler) + GL Sampler Object + Texture Preferred。
- **消费面（硬需求）：** `TextureSample` 图节点可编 Filter/Wrap（序列化 + Inspector）；否决「仅 C++ Preferred」。
- **文档：** [RND-F22_TEXTURE_SAMPLER_SEPARATION_DESIGN.md](./Render/RND-F22_TEXTURE_SAMPLER_SEPARATION_DESIGN.md)
- **Next：** Design 审批 / design-review。

### 2026-09-22 - RND-F22 / SPR-F07 / RND-F23 Draft（Filtering + Dilate + Backface Cull）
- **来源：** minCraft 反哺（纹理 Nearest + Atlas Dilate）；另发现 Scene Mesh PSO 未开背面剔除。
- **文档：** 登记三 Feature；Design Draft 待审批。未改运行时代码。
- **核对锚点：** `Texture2D`/`Texture2DLoader`；`Configure2DTextureSampling` 写死 Linear；VK `DefaultSampler` LINEAR；`SpriteAtlas` Dilate stub；`PackShelf` 仅槽间 gap；`EnginePipelineLayouts` Mesh PSO 未设 `bCullEnabled`（desc 默认 false）。
- **Next：** F22 已升格分离设计；审批后 Planned；实现序建议 F23 ∥ F22 → F07。

### 2026-09-22 - CORE-F28 Done（Noise Math Library）
- **代码：** `Runtime/Core/Math/Noise/` — 虚基类 `Noise`；`NoiseHash`；`ValueNoise` / `PerlinNoise` / `CellularNoise`；`FractalNoise`（FBM + `shared_ptr` 源）。
- **测试：** suite `noise` — `minEngineTests.exe test noise` PASS（5/5，529 asserts）。
- **文档：** Design/Impl Done；Registry Done；ACTIVE_WORK Primary 空闲。
- **Out：** Terrain/Texture/Editor/FastNoiseLite（按 Design）。
- **Next：** minCraft 可消费；Domain Warp / Ridged 另开。

### 2026-09-22 - CORE-F28 Draft（Noise Math Library 设计登记）
- **文档：** 登记 `CORE-F28`；Design Spec Draft：`Platform/Core/CORE-F28_NOISE_MATH_LIBRARY_DESIGN.md`（正文已改中文）；ACTIVE_WORK 标为 Primary 候选。
- **拍板：** 虚基类 `Noise`（对齐 Godot 分层，非 Resource）；v1 = Value/Perlin/Cellular + Fractal FBM；不嵌入 FastNoiseLite；Terrain/Texture/Editor Out。
- **验证：** 仅文档；未改运行时代码。
- **Next：** Design Review → Implementation Plan → S01。

### 2026-09-22 - SPR-F01 Done（Sprite System MVP 路线图收口）
- **文档：** F01 Meta/表 → Done；Registry / ACTIVE_WORK Primary 空闲；Trust → Tier B。
- **范围：** F02–F06 实现均已 Done；族完成，不绑定 Flipbook 等延后项。
- **Next：** 合入 `master`；下一轨另定。

### 2026-09-22 - SPR-F06 Done（Sprite Atlas 工作流 UX）
- **代码：** `SpriteAtlasEditor` + Window（成员/MaxSize/Padding/Rebuild/Generated 预览）；Document Tab；CB 空 Atlas +「Create Sprite Atlas From Selection」（Create+Add+Rebuild+级联 Save）。
- **验证：** Editor 编译通过；手验：多选 Sprite → Create Atlas → 预览 → Save。
- **Next：** 路线图收口后合入 master。

### 2026-09-22 - SPR-F05 Done（Sprite Editor MVP）
- **代码：** `SpriteEditor` SubModule + Document Tab；`SpriteEditorWindow`（ImGui 预览/拖拽选区/SourceRect 数值）；写入走 `SpriteEditorServices`；`TryOpenAsset` / CB Create Sprite。
- **验证：** Editor 编译通过；手验：CB 打开 `.mesprite` → 改 rect → Save。
- **Next：** commit 审批后 → SPR-F06。

### 2026-09-22 - SPR-F04 Done（Sprite Authoring Services）
- **代码：** `SpriteEditorServices`（Create/SetRect/Atlas/Add/Remove/Rebuild/Save）；`EditorSpriteCommands`；`AssetEditorServices` 白名单 Sprite/SpriteAtlas；`IEditorContext` 挂接。
- **验证：** `minEngineTests.exe test sprite-authoring` PASS（3/3）。
- **Deferred：** Document Tab Session / Automation `sprite.*` Surface（F05/F06 / 后续）。
- **Next：** commit 审批后 → SPR-F05。

### 2026-09-22 - SPR-F03 Done（Sprite 运行时消费）
- **代码：** `SpriteComponent::m_Sprite` + ResolveDraw*；Material UVRectU0–V1（Lerp + MakeFloat2）；退化 UV 跳过入队；`MakeFloat2` / `Vector2`。
- **验证：** `minEngineTests.exe test sprite-runtime-consume` PASS（3/3）。
- **Next：** SPR-F04 Authoring Services。

### 2026-09-22 - SPR-F02 Done（Sprite / Atlas 资产真源）
- **代码：** `Sprite` / `SpriteAtlas` / `SpriteAtlasPacker`；`.mesprite` / `.mespriteatlas`；CreateAsset/Load/Save；Rebuild 事务式；Runtime 查询 API。
- **验证：** `minEngineTests.exe test sprite-atlas` PASS（6/6）。
- **Next：** SPR-F03 运行时消费（Component + UV→GPU）。

### 2026-09-22 - SPR-F01–F06 Draft（Sprite 系统 MVP 设计族）
- **内容：** 新领域 `SPR`；登记 F01 路线图 + F02 资产真源 + F03 运行时消费 + F04 Authoring Services + F05 Sprite Editor + F06 Atlas UX；全部 **Draft** 待逐篇审阅。目录 `docs/ai/Sprite/`。ACTIVE_WORK Primary 指向本族。minCraft 为催化剂，非本期 DoD。
- **验证：** 文档交叉链接；未改运行时代码。
- **Next：** 维护者 Review（建议序 F01→F02→…）；通过后 Planned；实现序 F02→F03∥F04→F05∥F06。

### 2026-09-22 - WF-F04 Done（sync min-agent-workflows engineering-design）
- **内容：** 合入 L0–L3 / Implementation Readiness；新增 `WORKFLOW_PROFILE`（learning-mentor）、`WORKFLOW_PRESETS`、`engineering-design` skill、`implementation-discipline` rule；增强 design/impl 模板与 DOC_GOVERNANCE；接线 bootstrap / trust tiers / triggers / design-review / mentor Pre-flight。
- **验证：** 文档交叉链接与路径检查（`.agents/skills/`）；未改运行时代码。
- **Next：** Primary 仍为 Sprite/Atlas 设计酝酿；`feat/render` 对齐本 commit。

### 2026-09-22 - RND-F20 Done（Mesh Draw Sorting）
- **代码：** `MeshDrawCommandSort`；`ForwardRenderer::SortMeshDrawQueues`；`m_bSortMeshDraws`；TranslucencyPass 不再自排序；Shadow 队列不排序。
- **验证：** `minEngineTests.exe test mesh-draw-sort` PASS；Editor（MyMEProject）手验通过。
- **Next：** Primary 空闲；`RND-F21` 仍 Deferred。

### 2026-09-22 - CORE-F27 / ED-F21 Done（多模块 reflection codegen）
- **代码：** `cmake/MinEngineReflection.cmake`；引擎迁封装；`GameModuleStub` 链 `minEngine` + `StubGamePrimitiveComponent` + 自有 Generated；suite `game-module-stub`。
- **验证：** stub ON → `test game-module-stub` PASS；默认配置 → Editor + `test smoke` PASS。
- **文档：** BOOTSTRAP；Design/Impl Done；ED-F22 仍 Cancelled。
- **Next：** 提交本批；minCraft 可按 stub 模式接 codegen。

### 2026-09-22 - RND-F20 实现（In Progress）
- **代码：** `MeshDrawCommandSort`；`ForwardRenderer::SortMeshDrawQueues`（Opaque SortKey + Translucent 远→近）；`m_bSortMeshDraws`；TranslucencyPass 不再自排序。
- **Next：** `mesh-draw-sort` 单测 + Editor 手验 → Done。

### 2026-09-22 - ED-F22 Cancelled（不做 GameModuleStartup）
- **拍板：** 世界搭建归游戏仓（CreateScene / Add GO / Add Comp / 默认场景）；引擎不提供官方 C Startup 承载业务。
- **文档：** ED-F22 / ED-F20-S03 Cancelled；ACTIVE_WORK 仅留 CORE-F27 + ED-F21。
- **仍推进：** CORE-F27（多模块 codegen）、ED-F21（始终链引擎）。

### 2026-09-22 - RND-F19 Done（主视图视锥剔除）
- **代码：** `Math::Geometry::Frustum`/`Plane`；`ResolveWorldBounds`；`BuildRenderQueue` 双 Collector + 主锥剔除；`m_bFrustumCullMainView`；`ME_PROFILE_SCOPE("Render.BuildRenderQueue")`。
- **验证：** `minEngineTests.exe test frustum` PASS；Editor（MyMEProject）手验后收口。
- **Next：** `RND-F20` Mesh Draw Sorting（Design Draft）。

### 2026-09-22 - RND-F19 实现（S02–S03，待 Editor 手验）
- **代码：** `Math::Geometry::Frustum`/`Plane`；`ResolveWorldBounds`；`BuildRenderQueue` 双 Collector + 主锥剔除；`m_bFrustumCullMainView`；`ME_PROFILE_SCOPE("Render.BuildRenderQueue")`。
- **单测：** suite `frustum`（内/外/骑缝/身后；OpenGL vs Vulkan near；active-clip 对齐）。
- **Next：** Editor 手验（锥外不画；身后 caster 仍投阴影；开关 Off 回归）→ 收口 Done。

### 2026-09-22 - Reflection 多模块 codegen System Review + CORE-F27 纠偏（未写代码）
- **Review：** [REFLECTION_MULTI_MODULE_CODEGEN_SYSTEM_REVIEW.md](./Platform/Reflection/REFLECTION_MULTI_MODULE_CODEGEN_SYSTEM_REVIEW.md)。
- **根因：** header tool CLI 已参数化，但 CMake 只扫引擎单根；游戏无 `.gen` → 无 `GetClassName`；`Ready` 后禁止再 Register。
- **纠偏：** `CORE-F27` 主线 = 多模块 codegen（CMake 封装 + stub `ME_CLASS`）；挂接热修**降级**。
- **Next：** 维护者确认 Review/Design → `CORE-F27-S02`。（ED-F22 已另条 Cancelled）

### 2026-09-22 - 外仓游戏模块支撑 Design Draft（ENG-GAP-006，初稿；已被上条纠偏）
- **初稿误点：** 以 `AddComponentInstance` 热修为主线（已撤销为终态）。
- **仍有效：** `ED-F21` 登记；`ED-F22` 后已 **Cancelled**（见上条）。

### 2026-09-22 - RND-F19/F20 design-review 修订
- **结论：** Ready with revisions（已落文档）。
- **F19：** Plane 必做；Intersect 仅 valid AABB；无 Camera/关阴影行为；GetMeshBatches world Bounds；双 Collector 锁定。
- **F20：** Opaque bit 布局锁定；半透明远→近距离；Shadow 不排序；stable_sort 必用；硬前置 F19。
- **Next：** 维护者审批 → Planned。

### 2026-09-22 - RND-F19 Design 细化（Frustum 数学域 + 数据流）
- **拍板：** `Math::Geometry::Frustum` + `Intersect(AABB)`；双 Collector 分流；Batch bounds → world。
- **占位：** `RND-F21` Deferred（光视锥 / CSM caster，复用 Frustum）。
- **Next：** 维护者继续审阅 → Planned。

### 2026-09-22 - RND-F19 / RND-F20 Design Draft（未写代码）
- **登记：** `RND-F19` 主视图视锥可见性；`RND-F20` Opaque/Translucent 排序。
- **顺序：** 实现建议 F19 → F20。F19 硬契约：主锥外仍可向 Shadow 贡献 caster。
- **Next：** 维护者审阅 Design。

### 2026-09-21 - RND-F18 S01–S02 Done
- **代码：** `SyncSceneProxy`；`MeshBatch`；Opaque / Translucent / ShadowDepth `MeshPassProcessor`；入队与 `UpdatePrimitive` 不再按 mesh 类型 `dynamic_cast`。
- **验证：** `feat/render` 工作树 Editor 手验通过（静态网格 / 骨骼 / Sprite / 阴影）。`verify.ps1` 未跑。S03 stub **Deferred**。
- **Next：** 合并 `master`。

### 2026-09-21 - RND-F18 开工（feat/render 快进 master）
- **分支：** `feat/render` fast-forward 至 `master`（`b94b6e6`）。
- **契约：** `SyncSceneProxy`；`GetMeshBatches`；Pass 子类 `AddMeshBatch`（无 `MeshPassKind` 中心开关）。
- **Next：** 编译 Editor 供手验。

### 2026-09-21 - ED-F20 / TEST-F05 Done（游戏 Dev 宿主 + 外仓测试）
- **ED-F20：** `MINENGINE_GAME_DIR` + `MINENGINE_GAME_TARGET` + `COPY_RUNTIME`；`Examples/GameModuleStub`；默认关闭零回归。`MaximumEditor.exe --version` 在 stub 链接下 PASS。
- **TEST-F05：** `MINENGINE_EXTRA_TEST_*` + `RegisterExtraTestSuites`；stub `game.stub-extra`；重复 suite Id 打 Error 日志。`test game.stub-extra` PASS；默认 `verify.ps1` smoke PASS。
- **不做：** Startup 符号 / LoadLibrary / Runner API 导出（S03 Deferred）。未改 minCraft。
- **Next：** 提交本批；minCraft 侧接钩子。

### 2026-09-21 - ED-F20 / TEST-F05 Design Draft（minCraft 反哺，未写代码）
- **登记：** `ED-F20`（Editor 可选链接外部游戏 SHARED）、`TEST-F05`（外仓测试接入 harness）。
- **选型：** ED-F20 → `MINENGINE_GAME_DIR` + `MINENGINE_GAME_TARGET`（链接期，非 LoadLibrary）；TEST-F05 → 路径 A（EXTRA 源进 minEngineTests + `RegisterExtraTestSuites`）；两 Feature 的显式 Startup / Runner 导出均 **S03 Deferred**。
- **对照：** 只读 minCraft `INFRA-F01`；未改 minCraft 仓。
- **Next：** 维护者确认 Design → Planned → 先 `ED-F20-S01`。

### 2026-09-21 - 0.1.0 验收改为 minCraft；Launcher 建项补版本戳
- **决策：** 可玩竖切不在引擎仓做 FPS。独立仓 `D:\Dev\GitRepo\minCraft` 复刻一部分 Minecraft；缺口再回流引擎。`kEngineVersion` 维持 0.0.9，切片完成后再升 0.1.0。
- **代码：** `ProjectFactory` 写入 `$schemaVersion` / `$engineVersion`，与 `MyMEProject` 及 CORE-F18 一致。缺戳时引擎只告警、仍当 legacy 加载。
- **验证：** `cargo test -p minlauncher-core` 9/9 PASS。已创建 `D:\Dev\GitRepo\minCraft\minCraft\minCraft.meproject`（schema 1，引擎戳 0.0.9，默认场景 `Assets/Scenes/default.mescene`）。
- **Next：** 用 Maximum 打开该工程做一次手测。不新开引擎 Feature。版本号仍待 minCraft 切片完成后再升。

### 2026-09-19 - ED-F19 Done
- **落地：** `MaterialEditorServices` 包现有材质 Command；动画状态/过渡补 Command 后再包 `AnimationGraphEditorServices`。GUI / Debug 拓扑改走服务。Surface 增加 `material.*` / `animgraph.*`。图 JSON 只在 Surface。
- **验证：** `cmake --build build --target Editor` PASS。动画加→撤销无独立单测。
- **不做：** 新 MCP kind / 粗 Tool；`ReverseTransition`；画布拖拽 Command。
- **Next 候选：** Create Prefab；Play + Console 反馈闭环。

### 2026-09-19 - ED-F19 设计（Review，未写代码）
- **材质：** `MaterialEditorServices` 包现有加/删/连/断 Command；`get_graph` 只在 Surface。
- **动画：** 状态与过渡今天无撤销；先补 Command，服务禁止直接调 `AddStateAt`。`ReverseTransition` 不做（选择语义）。
- **协议：** 不新增 kind / 粗 Tool。`material.*` / `animgraph.*` 走已有 `editor_edit` / `editor_query`。
- **Next：** 审阅后 S01。

### 2026-09-18 - MCP-F04 Done
- **落地：** kind `control`/`asset`；`TypeQueryServices`；`AssetEditorServices`；`EditorControlServices`；Surface + MCP 粗 Tools；Scene `move_component` / `instantiate_prefab`。
- **验证：** Editor + `tsc` 编译 PASS；维护者确认可收口。
- **Next 候选：** Material/Anim 图编辑原语（打开已能做）；或 Create Prefab / Play / Console 反馈闭环。

### 2026-09-18 - MCP-F04 设计（Review，未写代码）
- **kind：** 在 query/edit/verify 上加 `control`（给人看）与 `asset`（创建/删除资产）。打开资产是 control，不是 asset。
- **类型查询：** Editor 侧 `TypeQueryServices`，进程内返回 `MEClass*` / `MEProperty*`；过滤参数化；不挂 SceneEditor，不提供「可添加组件」API。
- **Next：** 维护者审阅 Design 后按 Impl S01 开工。


### 2026-09-18 - Editor 域服务与 Client 约束
- **落点：** `ENGINE_DESIGN_PHILOSOPHY.md` §5；ED-F18 §1.4 为实例；agent rule 提醒同条。
- **三条：** 原语不镜像 Client op；只属于 Client 的解析留在 Client；不为 Client 改 Service 形态。新原语只补原语缺口（如 Gizmo `SetTransform`）。

### 2026-09-18 - ED-F18 Done
- **验收：** 维护者确认 Scene GUI/Debug 走 `SceneEditorServices` 无明显问题。
- **余量：** Create Prefab、Material/Anim、字面量→void* `SetProperty`。

### 2026-09-18 - ED-F18 S04（GUI 发 cmd → Services；B 决策落地）
- **B1：** Gizmo Transform 暂不迁——拖动预览是 Editor 控制流；落盘是一次 Transform command，不是逐帧 SetProperty。
- **B2/B3：** Rename → `SetProperty(m_Name)`；加 `MoveComponent`；Debug/Console/GUI Submit 同路径。
- **B4：** Instantiate 只迁发 cmd；文件对话框留 Submit；Create Prefab 后置（资产 Create 族）。
- **A：** Create/Delete/Add/Remove/Reparent/`CommitObjectPropertyBlobs`；`Submit*` 薄壳。
- **验证：** Editor 已编过；待 `accept:mcp`。
- **Next：** accept → Done / commit。

### 2026-09-18 - ED-F18 S01–S03（原生 SceneEditorServices）
- **思想：** 删除协议型 `SceneAutomationServices`；`SceneEditorServices` = 原语（Result+out、path+void* Get/Set、无 GetHierarchy、无 Activate）；Surface 拼 Hierarchy/Json；Prefab 门禁进写路径。
- **所有权：** `Editor::m_SceneEditorServices`；`IEditorContext::GetSceneEditorServices`；`AutomationHost(SceneEditorServices&)`。
- **Debug：** `add_go` / `delete_go` / `add_comp` / `remove_comp` / `reparent` → Services；`rename_comp` / `move_comp` / 属性 `set` 仍旧路径（后置）。
- **验证：** 本 agent 环境无可用 CMake 工具链/build 树；**待维护者** `cmake --build … --target Editor` + `accept:mcp`。
- **Next：** 编译修错 → 验收 → Done / commit。

### 2026-09-18 - MCP P0 收口（Cursor MCP + 伞 Done）
- **配置：** `.cursor/mcp.json` → `minengine` stdio Server（`${workspaceFolder}/minEngineMcpServer/dist/index.js`）。
- **验收：** `scripts/accept_mcp_path.ts` — MCP Client → tools/list → connect/spawn → query/edit/verify → `McpAcceptDemo`；Cursor MCP 状态 OK；用户目击 Hierarchy。
- **文档：** MCP-F01 伞 **Done**；ACTIVE_WORK 收口；下一主题另议（ED-F18 等）。
- **Next：** 提交本收口；勿默认开新 MCP Feature。

### 2026-09-18 - MCP-F03 Done（minEngine MCP Server）
- **代码：** `minEngineMcpServer/` — Pipe client（u32 LE + JSON）、ConnectionManager（attach/spawn + ready-file）、stdio MCP + 粗 Tools（`editor_connect` / `get_state` / `query` / `edit` / `verify`）；`domain.action` 拼接；capabilities 来自 Host。
- **验证：** `npm run build`；`smoke:ping`；`smoke_attach`（get_hierarchy）；`smoke_spawn` PASS。
- **文档：** Design/Impl/Registry/ACTIVE_WORK → Done；MCP P0 竖切齐。
- **Next：** 准备 commit；Cursor 手配或 ED-F18 / MCP-F01 收口另议。

### 2026-09-18 - ED-F17 Done（S04 Scene MVP 铺满）
- **代码：** Surface 接齐 `get_hierarchy` / `get_object` / `create_game_object` / `delete_game_object` / `add_component` / `remove_component` / `reparent`；`SceneAutomationServices` 经 session Command 栈；`add_component` 支持全名或 unambiguous 短名。
- **验证：** `WorkingDirectory=bin` + `--automation-pipe` + `--open-scene Assets/Scenes/test.mescene`；Pipe 手测 PASS（create→add StaticMeshComponent→reparent→remove→validate stub→delete）。
- **后续小改：** Surface 改为 `op → handler map`（与 CapabilityRegistry 双向覆盖检查）；Design §3.3 / Host 注释钉死 **single in-flight**（非 queue）。
- **文档：** Design/Impl/Registry/ACTIVE_WORK → **Done**；下一刀 MCP-F03。
- **Next：** 统一 commit；MCP-F03。

### 2026-09-18 - ED-F17 S03（Scene get/set_property + save）
- **代码：** `Services/Scene/` — `SceneAutomationServices` + `ScenePropertyValueCodec`（JSON↔blob；`Transform.*`→`m_Transform.*`）；Surface 接 `get_property`/`set_property`/`save`/`validate` stub；**set 写入 session Command 栈**（非 Active Tab 默认）。
- **验证：** Pipe：`get_property` Cube/`m_Name`；`set_property` RootComp `Transform.Position`→`[0,2,0]` ok（读回 Y 可能被物理/teleport 微调）。
- **Next：** S04 铺满其余 Scene MVP op → Feature 审批。

### 2026-09-18 - ED-F17 S01–S02（Host + ping/get_state）
- **代码：** `Editor/src/Automation/` — Pipe（u32 LE + JSON）、Host（Listen→ready-file→IO 线程；主线程 Tick 分派）、CapabilityRegistry、Dispatcher、Surface（`editor.ping` / `get_state` / `get_capabilities`）。
- **验证：** `WorkingDirectory=bin` 启动带 `--automation-pipe` + ready-file；NamedPipe 客户端 ping/get_state PASS（`defaultSessionId=sess-1`）。
- **Next：** ED-F17-S03 Scene `get/set_property` + `save`（值 JSON spike）。

### 2026-09-18 - CLI-F02 启动面落地（S01–S03 Done）
- **代码：** `--automation-pipe` / `--automation-ready-file` / `--open-scene` 进 `CommandLineResult`；Editor 缓存 pipe/ready getters；`--open-scene` → `OpenSceneByPath`（覆盖默认场景）。**不**写 ready-file（属 ED-F17）。
- **验证：** `cmake --build build --target Editor` PASS；`MaximumEditor.exe --help` 可见三 flag；仅 `--automation-ready-file` → exit 2。
- **文档：** Design/Impl/Registry/ACTIVE_WORK → Done（启动面）。
- **Commit：** `c82c832` feat(cli): add automation launch flags and open-scene wiring。

### 2026-09-17 - MCP-F02 契约落地（S01–S03 Done）
- **落盘：** [`schemas/automation/v1/`](../../schemas/automation/v1/) — `request`/`response` schema、README（u32 LE length-prefix）、`ERROR_CODES.md`、examples（ping / get_state / get_capabilities / set_property ok·fail / KindOpMismatch / remove_component）。
- **校验：** `python` + `jsonschema` Draft202012Validator — 11/11 examples PASS。
- **文档：** Design/Impl → **Done**；FEATURE_REGISTRY / ACTIVE_WORK 更新。
- **Next：** 等待 **MCP-F02 Feature 审批** → 再开 CLI-F02 / ED-F17 Impl（不提前写 Host/MCP 代码）。

### 2026-09-17 - docs commit + MCP-F02 Impl Draft
- **Commit：** `d0c1234` docs(mcp): add Planned designs for editor automation and MCP server（未含 meproject）。
- **Impl：** [MCP-F02_AUTOMATION_PROTOCOL_IMPLEMENTATION](./Editor/MCP/MCP-F02_AUTOMATION_PROTOCOL_IMPLEMENTATION.md) Draft — schema/examples 三切片；**待审批**后落盘，不提前写 Host/MCP 代码。

### 2026-09-17 - MCP 设计评审修补（待审批）
- **高优已落文档：** `op` 主键 + `KindOpMismatch`；ED-F17 DoD 与 ED-F18 解耦；`get_state` 最小形状；Listen→ready-file 不变量；`componentObjectId`；validate P0 stub 诚实声明；禁 TS 独有 capabilities；§4.2 控制流改正；P0-alpha；人机并发/本机信任/RPC 30s。
- **涉及：** MCP-F01/F02/F03、ED-F17、CLI-F02。

### 2026-09-17 - MCP 轨 Design Draft（未实现代码）
- **登记：** `MCP-F01`/`F02`/`F03`、`ED-F17`、`CLI-F02` → FEATURE_REGISTRY；ACTIVE_WORK Primary 切到 MCP。
- **口径：** 外部 GPT 指南仅参考；不另造平行业务命令层。

### 2026-09-19 - MAT-F06 / MAT-F01 Done：MIC 与运行时产品化主链收口
- **F06：** `.memi` + ME_CLASS + CB Create/Inspector Save；材质槽 Interface 候选；`material-ir` 往返。
- **F01：** Proxy→Interface→MI→MID→MIC 全 Done。
- **后置：** Instance Parent 链、专用 MIC 编辑器。
- **Next：** 选定下一 Primary（非默认开新 MAT）。

### 2026-09-19 - MAT-F06 S01–S04：MIC / `.memi` 落地（待手验）
- **S01：** `MaterialInstance` `ME_CLASS`；`ResolveParentMaterial`；MID `Create` 去 RTTI。
- **S02–S03：** `.memi` 序列化 Parent + 覆盖向量；Loader + Registry；`material-ir` 往返单测。
- **S04：** CB Create Material Instance；Inspector 改 scalar 并 Save；材质槽按 `MaterialInterface` 列 Material + Instance。
- **验证：** `minEngineTests.exe test material-ir`；Editor 已编过。**收口：** 维护者合并收口 → Done。

### 2026-09-18 - MAT-F05 Done；开 MAT-F06 Design
- **F05：** MID::Create；复用 Instance Proxy；多帧 Set smoke；README 用法。
- **遗留：** Create 内 `dynamic_cast` → **F06**（ME_CLASS + ResolveParentMaterial）。
- **F06：** [MIC Design](./Render/Material/MAT-F06_MATERIAL_INSTANCE_CONSTANT_DESIGN.md) Review（`.memi` / CB / 反射）。

### 2026-09-18 - MAT-F05：MaterialInstanceDynamic
- **代码：** `MID : MaterialInstance`；`Create(Interface)`（D1 落到 Base Material）；复用 Instance Proxy。
- **测试：** 多帧 Set + Parent 未污；可从 MI 创建。
- **Next：** 手验挂 Mesh → Done → F06。

### 2026-09-18 - MAT-F04 Done
- Instance + InstanceProxy；BindingSet 上收基类；`mat_apply_instance`；`material-ir` override smoke。
- Editor 手验通过。Next：MAT-F05 MID。

### 2026-09-18 - MAT-F04：MaterialInstance + Instance Proxy
- **代码：** `MaterialInstance` / `InstanceMaterialRenderProxy`；BindingSet/UBO 上收 `MaterialRenderProxy`；Parent compile generation 同步。
- **测试：** `material-ir` 含 instance override smoke（Parent 不被改；双 Instance 异值）。
- **手验：** `mat_apply_instance <GO> <ScalarParam> <Value>`。
- **Next：** 双 Mesh 画面确认 → Done → F05 MID。

### 2026-09-18 - MAT-F02/F03 Done
- **F02：** Draw 经 MaterialRenderProxy；BindingSet 在 Bind 后刷新。Editor 手验可画。
- **F03：** `MaterialInterface : Asset`，`Material` 继承之；网格槽为 Interface。反射 gen 链入后父类与槽位可解析。场景 GUID 引用未改资产。
- **Next：** MAT-F04 MaterialInstance。

### 2026-09-18 - MAT-F03：MaterialInterface 单继承
- **约束：** 反射拒绝双 super；`MaterialInterface : Asset`，`Material` 继承之。
- **代码：** 组件材质槽 `shared_ptr<MaterialInterface>`；Draw 仍只经 `GetRenderProxy()`。
- **验证：** `cmake --build build --target Editor` OK。
- **Next：** 手验关卡材质。

### 2026-09-18 - MAT-F02 实现：MaterialRenderProxy（待手验）
- **代码：** `MaterialRenderProxy` + `DefaultMaterialRenderProxy`；BindingSet/UBO 在 Proxy；MDC/SceneProxy/Draw 经 `m_MaterialProxy`；删除 `Material::BindForDraw`。
- **验证：** `cmake --build build --target minEngineTests Editor`；`minEngineTests.exe test material-ir` / `render-graph` **PASSED**（cwd=`minEngine/bin`）。
- **Next：** 维护者手验场景/材质参数 → Done → commit → F03。

### 2026-09-18 - MaterialRenderProxy 栖息地澄清 + F05 详设
- **澄清：** MaterialRenderProxy **由 Material/MI 拥有**，SceneProxy/MDC 仅裸指针引用；**不是** RenderScene 里与 Primitive SceneProxy 同类的对象（已写入 F02 §3.7、路线图类型表）。
- **F05：** [MID Design](./Render/Material/MAT-F05_MATERIAL_INSTANCE_DYNAMIC_DESIGN.md) 展开为 Review 详设（工厂、寿命、切片、与 F06 边界）；Registry → Review。
- **Next：** 审批 F02（及 D7/D8）→ S01；F05 原则可先认可，实施等 F04。

### 2026-09-18 - MAT-F02/F03/F04 按 UE 完善（待审批）
- **F02：** GPU BindingSet/UBO **迁 Proxy**（D7）；虚 `Get*ParameterValue*`（D8）；`FDefaultMaterialInstance` 对照；F04 预留契约；Compile→Invalidate。
- **F03/F04/F05：** Interface 最小集；Instance Proxy 同构 `FMaterialInstanceResource`；MID 复用存储。
- **Review：** 见会话 design-review 报告；**等维护者审批 F02（及 D7/D8）** 后 S01。
- **未改代码。**

### 2026-09-18 - MAT 路线图修订：Proxy 先行 + F02 详设
- **决策：** 否决「Interface 首刀、Proxy 殿后」；**无 Proxy 则 Interface/MI 为空中楼阁**。
- **重排：** F02=Proxy → F03=Interface → F04=MI → F05=MID → F06=MIC；删除旧错位 Design 文件。
- **F02 详设：** [MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md](./Render/Material/MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md)。
- **Next：** 完善后审批 → S01。

### 2026-09-18 - MAT 运行时产品化：F01 路线图 + F02–F05 Design（初版，顺序已修订）
- **F01：** [Roadmap](./Render/Material/MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md) Status In Progress。
- **登记：** F02–F06；旧 MI 窄 Design → Superseded 指针页。
- **注：** 初版曾以 Interface 为 F02；同日修订为 Proxy 先行（见上条）。

### 2026-09-18 - MAT-F01 Design Review：UE 式 Interface + Material + MI
- **文档：** [MAT-F01](./Render/Material/MAT-F01_MATERIAL_INSTANCE_DESIGN.md) 重写；Registry/ACTIVE_WORK → **Review**。
- **要点：** draw 认 `MaterialInterface*`；Material=图+编译；Instance=Parent+覆盖；`.memi` 二期。
- **Next：** 维护者审批（Parent 链 / 组件 shared_ptr / `.memi` Out）后 S01。

### 2026-09-18 - RND-F17 Done：ViewId+SRT 物理缓存隔离（`feat/render`）
- **代码：** `SceneViewport` 发号；`SceneDrawDesc::ViewId`；`RenderGraph` per-view `m_ViewPhysicalCaches` + `SetActiveViewKey`；`ForwardRenderer`/`ManualRenderer` 拒 `ViewId==0`；CB Scene3D 重开。
- **验证：** `cmake --build build --target minEngineTests Editor`；`minEngineTests.exe test render-graph` **7/7**（含双 ViewKey 同尺寸异纹理用例）。
- **文档：** Design/Registry/TD-027 → Done；ACTIVE_WORK 下一刀 MAT-F01 / 手验。
- **手验待确认：** Level+Material Preview；CB Material/Mesh 缩略图。
- **Next：** 手验后准备 commit；或开 MAT-F01。

### 2026-09-18 - RND-F17 Design 定稿待批（ViewId + SRT*）
- **方案：** 单 ForwardRenderer + F07 图内分配；`SetupAttachments` 物理复用键 = **ViewId ∧ SceneRenderTarget*** + dims/format；不一致 fail-closed。
- **Out：** 多长寿 Renderer；整帧外置 RT 双轨；纯 Manual 缩略图旁路。
- **Status：** Design / Registry / ACTIVE_WORK → **Review**；**等维护者审批**后 S01。
- **未改代码。**

### 2026-09-18 - RND-F06 文档收口；登记 RND-F17 / MAT-F01（`feat/render`）
- **F06：** Design / Registry / ACTIVE_WORK → **Done**（代码早已无 `class RenderPipeline`；S03 记 `RenderPipeline/` 目录名为历史路径）。
- **登记：** `RND-F17` Isolated preview / multi-view draw scheduling（吸收 TD-027 / 0.1.0 IsoRT）；`MAT-F01` MaterialInstance（空壳类 → Planned Design）。
- **Next：** 对照 UE/Godot/Unity 讨论 F17 方向后拍板 Design；再 Pre-flight。
- **验证：** 文档收口（无新代码）。

### 2026-09-17 - 工作流补全：外部源码 + 主动 commit 草稿 + design-review skill
- **内容：** mentor / git-commit-mentor：登记 `D:/Dev/GitRepo/UnrealEngine` 与 `D:/Dev/GitRepo/Godot`；参考 UE/Godot 时默认读源码；交付批次收口后**主动草拟** commit（不等用户再说准备 commit）。
- **新增：** `design-review` skill（Mode A 开工前 / Mode B 多轮修订后）；模板 `design-review.template.md`；与 `engine-system-review`（代码复盘）区分。
- **同步：** `.agents/skills` → `.cursor/skills`；`docs-workflow-triggers`、DOC_GOVERNANCE §7.4、WORKING_WITH_AI、BOOTSTRAP_DIGEST。
- **合入：** `feat/mcp` fast-forward master `9438700`；PROGRESS_LOG 与 MCP WIP 手工合并。

### 2026-09-17 - master：合入 feat/lua-script + feat/prefab
- **lua：** CORE-F19 Call-by-name；CORE-F21 Lua Dynamic subscribe（Add(self, fn)）。
- **prefab：** CORE-F23–F26 + ED-F16 + ASSET-F03 + Review 全包；含 Cube.meprefab / test.mescene / MyMEProject.meproject（ProjectRoot 改回主仓）。
- **文档冲突：** ACTIVE_WORK / FEATURE_REGISTRY / PROGRESS_LOG / ScriptBinding manifest 已手工合并。
- **验证（合入后）：** `test lua-script-mvp` PASS；`test prefab` 16/16；`test prefab-overrides` 5/5；`test delegates` 12/12；`Editor`+`minEngineTests` 已编。
- **ProjectRoot：** `MyMEProject.meproject` 指向主仓 `D:/Dev/GitRepo/minEngine/minEngine/MyMEProject`。

### 2026-09-17 - Prefab Review 全包落地（A1–A5 / B1–B5）
- **缺口：** 实例 Add/Remove 组件门禁；Save 单写盘；MutationPass（BUG-ASSET-001）；Stage Save 失败回滚；map rebuild fail-closed；ResolvePrefab `shared_ptr`。
- **精简：** `PrefabObjectLookup`；`PrefabUtilityDetail`；Utility 拆 StageWriteback / InstanceOps；mapping 方向合并；枚举 Reserved 注释。
- **验证：** `test prefab` 16/16、`test prefab-overrides` 5/5；`Editor`+`minEngineTests` 已编。
- **文档：** [PREFAB_MVP_CODE_REVIEW](./Platform/Core/PREFAB_MVP_CODE_REVIEW.md) → Done；BUG-ASSET-001 → Fixed。
- **Next：** 准备 commit / 合入评估。

### 2026-09-16 - feat/prefab 工程 Review 报告（待批）
- **文档：** [PREFAB_MVP_CODE_REVIEW.md](./Platform/Core/PREFAB_MVP_CODE_REVIEW.md) — 区分功能缺口（A）与冗余（B）；推荐包 R1；**未改代码**。
- **验证基线：** `test prefab` 16/16、`test prefab-overrides` 5/5。

### 2026-09-16 - feat/prefab MVP 验收通过（文档对齐）
- **结论：** CORE-F23–F26 / ED-F16（含 A/B）/ ASSET-F03 / BUG-CORE-002·003 手验已通过；分支 tip `15d0ab3`。
- **文档：** ACTIVE_WORK 去掉「待手验 / 准备 commit」滞后表述；下一刀改为合入评估或 BUG-ASSET-001 / Review 精简。
- **仍 Open：** BUG-ASSET-001（Create Prefab → 全盘 ScanAssets）。

### 2026-09-16 - CORE-F26 + ED-F16-B Done：删除断链 + Stage 相机/临时灯
- **F26：** `FindInstanceRefs*` / `Unpack*`；`DeleteAsset` Prefab 门禁；Editor「Unpack and Delete Prefab」；Stage 打开时拦删。
- **ED-F16-B：** Level/Stage `EditorFlyCameraPose` stash/restore；Stage `__ME_EditorTemp_DirectionalLight`；WriteStageTree / Hierarchy 排除 temp。
- **验证：** `test prefab` 16/16、`test prefab-overrides` 5/5；`Editor`+`minEngineTests` 已编。
- **验收：** 手验通过（见上条）。

### 2026-09-16 - BUG-CORE-003 Fixed：全属性 Propagate + F24 S02/S05 收口
- **Propagate / RevertInstance：** mapping 上反射叶子；跳过根 Transform / override / Instanced / Delegate / 非资产 ObjectPtr。
- **Editor：** `ApplySetObjectProperty` → `TryRecordPropertyOverride`；Level 删/重挂 → `ValidateEdit`；Stage Save 传播后 `MarkDocumentSceneDirty`。
- **验证：** `test prefab-overrides` 5/5、`test prefab` 14/14；Editor 已编。
- **仍 Out：** Added/RemovedComponent、Apply→Prefab、Instantiate 偏移、Inspector 蓝字。
- **Next：** 手验；准备 commit；CORE-F26 Review。

### 2026-09-16 - 登记 BUG-CORE-003：传播未扫反射属性
- **结论：** F24 §3.4.3 未落地；实现只拷 Name + 非根 Transform；Editor 未接 TryRecord。
- **扩包后实现：** 见上条 Fixed。

### 2026-09-16 - BUG-CORE-002 Fixed：Stage Transform / Create Scale / Save map
- **Instantiate：** `bApplyWorldTransform`（默认 false）；Hierarchy 显式 true+identity；Stage 保留模板根姿态。
- **Create：** 捕获源世界 Transform 并 bake；外部父/attach unresolved 清空后继续（不再整单失败）。
- **Save map：** `RemapObjectGuid` 仅卸本对象槽（修 Clone 根偷源 Guid）；WriteStageTree 用 Scene 刷新 map + 并行树 rebuild + 失败回滚。
- **验证：** `test prefab` 12/12、`test prefab-overrides` 3/3 PASS；`Editor`+`minEngineTests` 已编。
- **Next：** 手验 Create→Stage→Save→Propagate；准备 commit；CORE-F26 仍 Review。

### 2026-09-15 - ASSET-F03 Done：Create Asset 身份契约
- **RegisterAsset：** 可选 preferredGuid；首次 .meta 播种对象 Guid（禁另造 Guid）。
- **CreateAsset：** Scene/Material/Prefab/AnimationGraph 单对象返回 + CacheCreatedAsset（禁 Load 换身）。
- **Prefab Save：** 先写 .meta 再 Register；实例表 PrefabAssetGuid 与资产 Guid 对齐。
- **验证：** 	est asset-manager / 	est prefab / 	est prefab-overrides PASS。
- **已知余量：** Prefab default 传播仍有手验问题（另议）；ED-F16-B 相机/灯未实现。

### 2026-09-15 - CORE-F25 Done：owning-Scene RenderScene
- **API：** `Component::GetOwningScene` / `GetOwningRenderScene`（Ensure）/ `GetOwningRenderSceneIfPresent`（Remove/dtor）。
- **迁移：** Primitive / Light / SkyBox / Widget 不再调用 `SceneManager::GetRenderScene`。
- **验证：** `Editor`+`minEngineTests` 构建；`test scene-clone` / `test prefab` PASS；Components grep 清零。
- **手验待确认：** Prefab Stage 非蓝底、Level 无串；PIE 进出。
- **Next：** 手验通过后与 ED-F16 一并准备 commit。

### 2026-09-15 - CORE-F25 Design Review：owning-Scene RenderScene
- **动机：** Prefab Stage 串 Level / 蓝底；根因是组件经 `SceneManager::GetRenderScene()` 注册。
- **文档：** [CORE-F25](./Platform/Core/CORE-F25_OWNING_SCENE_RENDERSCENE_DESIGN.md) Status **Review**；Registry/ACTIVE_WORK 已挂。
- **Next：** 审批后 S01–S04；相机隔离 Out。

### 2026-09-15 - 登记 BUG-ASSET-001；补齐 Prefab TryOpenAsset
- **BUG-ASSET-001：** Create Prefab 写盘未挂 MutationPass + watcher 目录事件 threshold=1 → 全盘 `ScanAssets`；大量 `Asset registered/updated` 即全扫副作用（**Open，稍后修**）。
- **Editor：** `TryOpenAsset` 放行 Prefab（unsaved → Stage Save）；不另开 BUG-EDITOR。
- **处置：** 撤掉误拟的 Editor bug 登记。

### 2026-09-15 - ED-F16 Amendment A Done：Hierarchy Prefab 工作流
- **S07：** `Scene::Instantiate` → `PrefabUtility::Instantiate`；`test prefab` convenience 冒烟。
- **S08/S09：** ContextMenu `Create Prefab…` / `Instantiate Prefab…` / as Child；Level only（Stage/Play 隐藏）；Save/Open 对话框；Instantiate Undo=删实例根；删 GO 时 prune `PrefabInstanceRecord`。
- **S10：** Hierarchy 实例青/蓝字 + `ICON_FA_CUBES` 根图标；`HierarchyPrefabInstance` semantic color。
- **验证：** `Editor` 构建；`minEngineTests.exe test prefab` **8/8 PASS**。
- **Next：** 准备 ED-F16（含 Amendment A + Stage）commit。

### 2026-09-15 - ED-F16 Amendment A 设计 Review（待批）
- **缺口：** Hierarchy 无 Create Prefab / Instantiate Prefab；无法在编辑器内产出资产；缺实例视觉区分；希望 `Scene::Instantiate`。
- **文档：** [ED-F16](./Editor/ED-F16_PREFAB_EDITOR_DESIGN.md) §3.10 + S07–S11；Status **Review**；**未实现**。
- **Next：** 维护者审批后再编码。

### 2026-09-15 - ED-F16 Done：Prefab 文档 Mode
- **S01–S02：** DocumentType Prefab；Stage Instantiate；InspectingScene 绑定 Scene UI。
- **S03：** `WriteStageTreeToPrefab` + Save + Propagate；`test prefab` writeback。
- **S04：** PrefabEditConstraints；禁 SaveAs/第二顶层/删根/PIE；`EnterPlay` 硬拦。
- **S05：** Dirty=Stage flag；Undo=Host per-session CommandStack；Save→Propagate。
- **验证：** `Editor` 构建；`test prefab` 7/7；`test prefab-overrides` 3/3。
- **Next：** 准备 ED-F16 commit → `feat/prefab` 合入评估。

### 2026-09-15 - ED-F16 S03/S04：Save 回写 + EditConstraints
- **S03：** `PrefabUtility::WriteStageTreeToPrefab`（策略 B Guid 保持）；`PrefabStageController::SaveActive` + `PropagateDefaultsToOpenScenes`；单测 writeback。
- **S04：** `PrefabEditConstraints` — 禁第二顶层 / 删根 / 解挂成第二根 / SaveAs Scene / PIE；菜单 Save Prefab；Play 按钮禁用。
- **验证：** `test prefab` / `test prefab-overrides`；`Editor` 构建通过。
- **Next：** S05 Dirty/Undo 隔离 + DoD。

### 2026-09-15 - ED-F16 S01/S02：Prefab Document + Stage 绑定
- **实现：** `PrefabStageController` + `PrefabEditorStage`；DocumentType `Prefab`（ModuleId=Scene 复用 Hierarchy/Viewport）；`SceneEditor::Enter/ExitPrefabStage`；CB 双击 `.meprefab` → OpenOrFocus。
- **Instantiate：** `PrefabUtility::Instantiate(..., ObjectCloneContext*)` 供 Stage Save 映射（策略 B）。
- **未做：** S03 Save 回写 Guid；S04 禁第二顶层 / SaveAs Scene / PIE；S05 Undo 隔离。
- **验证：** `cmake --build … --target Editor` 通过。
- **Next：** S03 Save 回写 + Propagate。

### 2026-09-15 - CORE-F24 Done：Prefab Overrides + Propagation
- **实现：** `PrefabPropertyPath`（`bin:`+hex 载荷）；`PrefabOverrideUtility`（TryRecord / RevertProperty / RevertInstance / PropagateDefaults*）；`PrefabEditValidator`（禁删根等）；`EPrefabOverrideKind` + 完整 `PrefabPropertyOverride`。
- **验证：** `minEngineTests.exe test prefab-overrides` **3/3 PASS**；`test prefab` 回归 PASS。
- **已知缺口：** Scene override 存盘 round-trip 单测未做；Added/RemovedComponent 未做（O3）；Apply-to-Prefab Out。
- **Next：** 准备 F24 commit；再开 ED-F16。

### 2026-09-15 - CORE-F23 Done：Prefab 资产 + Instantiate
- **实现：** `Framework/Prefab/`（`Prefab`、`PrefabUtility`、引用切断）；`ObjectCloneContext`；Scene `m_PrefabInstances`；`.meprefab` 资产管线；PIE 清实例表。
- **验证：** `minEngineTests.exe test prefab` 3/3 PASS；`test scene-clone` 回归 PASS。
- **命名：** 类名 `Prefab`（非 PrefabAsset）。
- **Next：** 准备 F23 commit；再开 CORE-F24。

### 2026-09-15 - feat/prefab：登记 CORE-F23 / F24 / ED-F16 并起草 Design
- **Registry：** Next CORE→F25；ED→F17；Vision 占位中的 Prefab 改为已登记。
- **Design Draft：**
  - [CORE-F23](./Platform/Core/CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md) — 单根 GO 树资产、Create/Instantiate、引用切断、空 PrefabInstance
  - [CORE-F24](./Platform/Core/CORE-F24_PREFAB_OVERRIDES_DESIGN.md) — 受限 override、传播、规则表
  - [ED-F16](./Editor/ED-F16_PREFAB_EDITOR_DESIGN.md) — 文档 Tab + Stage Scene + 复用 SceneEditor（单 Viewport）
- **Worktree：** `D:/Dev/GitRepo/minEngine-prefab` @ `feat/prefab`（base `1a292de`）。
- **Next：** 维护者审阅 Draft（F23 §8 等）→ Planned；再实现 F23-S01。
### 2026-09-15 - CORE-F21 Done：验收收口
- **Status：** Registry / Design / ACTIVE_WORK → **Done**。
- **验证：** 既有 `lua-script-mvp` / `delegates` PASS；后续应用中再发现问题另开 BUG。
- **Next：** 准备 commit（含 F19 文档收口 + F21 实现）。

### 2026-09-15 - CORE-F21：S05 Done（显式 Add(self,fn) / 虚 AddScript / 删 Active）
- **Core：** `AddScriptErased` → 基类虚 `AddScript`，模板 override。
- **Scripting：** `AddScriptFromLua(delegate, host, fn)`；删 `ActiveScriptComponent`。
- **Lua：** `delegate:Add(self, fn)`；Unload 仍经 `LuaComponent` Track 表。
- **验证：** `test lua-script-mvp` PASS；`test delegates` PASS。
- **Next：** 验收 → Done；准备 commit。

### 2026-09-15 - CORE-F21：§9 再修订（显式 self / 无 ScriptComponentBase / 命名）
- **拍板：** `Add(self, fn)` 对象优先；不引入 ScriptComponentBase；删 Active。
- **Core 命名：** 荐 **A** 合并虚 `AddScript`（删 Erased）；B=`AddScript_Internal` 后备。
- **Next：** 确认 A vs B → S05。

### 2026-09-15 - CORE-F21：§9 重构目标形态（文档）
- **内容：** 删 `ActiveScriptComponent`；Scripting 引入 `ScriptComponentBase`；`Add` 经 `host*` Track，Core `AddScript` 不改；默认 host=调用方 env.`self`。
- **Docs：** [CORE-F21 Design §9](./Platform/Scripting/CORE-F21_LUA_DYNAMIC_MULTICAST_SUBSCRIBE_DESIGN.md)；切片 **S05** Planned。
- **Next：** 维护者审阅 §9 / O2b → 开 S05 或改默认。

### 2026-09-15 - CORE-F21：S01–S03 落地（Add(fn) codegen）
- **实现：** `LuaDynamicDelegateBindings`（Base usertype Add/Remove）；header tool `ScriptAssignable`→`OnClicked` 属性；Button `ScriptType` + `meta=(ScriptGetter=OnClicked)`；LuaComponent Unload 解绑跟踪。
- **验证：** `minEngineTests.exe test lua-script-mvp` → **PASSED**（含 Add + Unload 用例）。
- **Next：** 验收勾选 → Done；准备 commit（可与 F19 文档一并）。

### 2026-09-15 - CORE-F21：Design Draft（ScriptAssignable codegen）
- **方案：** 扩展 ScriptBinding——识别 `ME_PROPERTY(ScriptAssignable)` Dynamic 字段；共享 `DynamicMulticastDelegateBase` usertype（`Add`/`Remove`）；Scripting `sol`→`CallableScriptFunction`；Button `ScriptType` + 0 参竖切。
- **拒绝：** 新宏 `ME_DELEGATE`；域组件 `Bind(sol::function)`。
- **Docs：** [CORE-F21 Design](./Platform/Scripting/CORE-F21_LUA_DYNAMIC_MULTICAST_SUBSCRIBE_DESIGN.md)；Registry / ACTIVE_WORK → Draft。
- **Next：** 维护者审阅 §3 / §10 → Planned → S01。

### 2026-09-15 - CORE-F19 Done：Call 文档收口
- **Status：** Registry / Design → **Done**。S02+ 委托桥标 **Cancelled（迁出）** → F20（挂钩，master Done）/ F21（Lua `Add(fn)`）。
- **工程：** 本轮仅文档；Call 验收仍以既有 `minEngineTests.exe test lua-script-mvp` 为准（2026-09-14 PASS）。
- **Next：** 讨论并起草 **CORE-F21** Design。维护者要求本轮不准备 commit。


### 2026-09-15 - master：合入 feat/core + feat/editor
- **core：** CORE-F20 Dynamic Multicast；CORE-F22 Profiler Harness（`test profiler` 5/5）。
- **editor：** ED-F11 W3 大部 + ED-F12–F15（Command-first / 覆盖 / Graph·CB UX）。
- **文档冲突：** ACTIVE_WORK / FEATURE_REGISTRY / PROGRESS_LOG 已手工合并。

### 2026-09-15 - CORE-F22 Done：S05 收口
- **Status：** Registry / Design → **Done**；验收勾选完成。
- **验证：** `minEngineTests.exe test profiler` 5/5 PASS。
- **Next：** 执行收口 commit。

### 2026-09-15 - CORE-F22：`test profiler` 全绿（MinGW AV 根因）
- **根因：** GCC 对 ~32B `ProfileEvent` **按值**传参发 `vmovdqa`；Win64 栈仅 16B 对齐，从 Ender 析构调用时 SIGSEGV。
- **修复：** `PushEvent(..., const ProfileEvent&)`；宏改为 **POD + `ProfileScope_Begin/End` + 本地 Ender**；MVP **MainBuffer**（非 TLS）。
- **验证：** `minEngineTests.exe test profiler` → **5/5 PASS**（30 assertions）。
- **Next：** S05 DoD（见上条）。

### 2026-09-15 - CORE-F22：S01–S04 实现落地
- **实现：** `Runtime/Core/Profiling/`（Clock/Name/MainBuffer/Scope/Phase/Frame/Session/Analyze/Chrome Trace/Query）；Engine Startup/Runtime/Shutdown 插桩；`test profiler` suite。
- **修正：** `ForEachSpan` 头文件模板（避免 `std::function` 跨 DLL AV）。
- **验证：** 见上条（已绿）。

### 2026-09-15 - CORE-F22：Design 细化（数据结构 / API）+ 开放点拍板
- **Status：** Draft → **Planned**；§8 全部按默认。
- **增补：** §3.8 POD/Session/Span/Stats；§3.9 完整 API；§3.10 不变量；§3.11 Chrome 映射；§3.12 文件布局。
- **Docs：** [CORE-F22 Design](./Platform/Core/CORE-F22_PROFILER_HARNESS_DESIGN.md)。
- **Next：** In Progress + 实现（见上条）。

### 2026-09-15 - CORE-F22：Profiler Harness Design 草稿
- **登记：** `CORE-F22`；Next CORE → F23。
- **方案要点：** Session + Phase + Frame；Chrome Trace + 查询 API；无 Editor UI。
- **Next：** 细化数据结构（已完成见上条）。

### 2026-09-15 - CORE-F20 Done：S05 反射收口
- **实现：** `DynamicMulticastDelegateBase` + `IsDynamicMulticastDelegateField`（避免 Reflection include 环）；`CreatePropertyByType` 走 `MEDynamicMulticastDelegateProperty`；`SetValueOps` 对不可拷贝类型跳过 copy；Button `ME_PROPERTY(ScriptAssignable) m_OnClicked`。
- **验证：** `test delegates` 12/12 PASS；`test screen-ui-button` 4/4 PASS。
- **Status：** Registry / Design → **Done**。
- **Next：** 准备收口 commit；F21 Lua `Add(fn)` 或 Profiler。

### 2026-09-14 - CORE-F20：Dynamic Multicast MVP 落地（S01–S04）
- **实现：** `DynamicMulticastDelegate`（B1 包装 Native）、`CallableScriptFunction`、`AddDynamic` / `AddScript`、`ME_ADD_DYNAMIC`、DYNAMIC 宏、`MEDynamicMulticastDelegateProperty` + `ScriptAssignable`、Serializer 跳过 MulticastDelegate。
- **Button：** `OnClicked` → `DECLARE_DYNAMIC_MULTICAST_DELEGATE`。
- **验证：** `minEngineTests.exe test delegates` 11/11 PASS；`test screen-ui-button` 4/4 PASS。
- **后置：** S05 header-tool 自动生成 Dynamic 属性；绑定列表落盘；CORE-F21 Lua `Add(fn)`。
- **Next：** 准备 commit；或开 F21 Design。

### 2026-09-14 - CORE-F20 Design 修订：CallableScriptFunction · 独立 Property · 序列化策略
- **改名：** 运行时类型 `ScriptCallable` → **`CallableScriptFunction`**（与 `ME_FUNCTION(ScriptCallable)` 区分）。
- **反射：** 明确 **`MEDynamicMulticastDelegateProperty`**（对齐 UE `FMulticastDelegateProperty` 独立类型思路）。
- **序列化：** F20 **不落绑定表**；Serializer 对类别 **显式跳过**；预留仅 `AddDynamic` 可落盘的 JSON 形状（§3.4.1 / O8）。
- **Next：** 维护者确认 O8 等 → In Progress。

### 2026-09-14 - CORE-F20：Dynamic Multicast Design 草稿
- **拍板输入：** 存储 **B1**（Dynamic 包装 Native）；Lua 主订阅 **`Add(fn)`** → 另轨 **CORE-F21**；`feat/core` 只做基建。
- **Registry：** `CORE-F19`（Lua Call，lua-script）· **`CORE-F20`** Dynamic · `CORE-F21` Lua Add(fn) Planned；Next CORE → F22。
- **Docs：** [CORE-F20 Design](./Platform/Core/CORE-F20_DYNAMIC_MULTICAST_DELEGATES_DESIGN.md)；ACTIVE_WORK Primary → F20；TD-006 注 F20/F21。
- **未做：** 实现代码；等维护者审阅 §10/§11。
- **Next：** 拍板开放点 → In Progress + S01。

### 2026-09-15 - ED-F15 Done：验收收口
- Design §验收全勾；Registry / ACTIVE_WORK → **Done**。
- 手测通过（维护者）：Anim 导航、Material chrome/右键、CB 文件夹、Inspector icon。

### 2026-09-15 - ED-F15 跟进：Material 节点 UE 化 + 右键菜单
- **视觉：** Header 低明度色；标题/Pin 行 padding；Pin 空心/实心（连线状态）；连线 thickness 2.5；去掉默认节点重复副标题。
- **交互：** 画布右键 Add Node（Creatable 列表）；节点右键 Rename（`m_Title` + Undo）/ Delete（禁 Output）。
- **验证：** Editor 构建 OK；手测通过。

### 2026-09-15 - ED-F15 实现：Graph / CB / Inspector UX
- **W4：** `ResolveIconGlyph(MEClass*)` 经 `GetShortTypeName` 查表 → Inspector 与 AddComponent 一致。
- **W1：** SmGraph RMB 拖平移（阈值保住菜单）+ LMB 空处框选（单 State MVP）；Middle/Alt pan 保留。
- **W3：** CB 文件夹 FA icon + 单击选中边框；双击进入清选中。
- **W2：** Material 节点 Header 色条 + `ApplyNodeEditorTheme`（palette / 选中描边）。
- **验证：** Editor 构建 OK；手测通过。

### 2026-09-15 - 登记 ED-F15：Graph / CB / Inspector UX 抛光
- **范围：** AnimGraph 导航对齐 Material（RMB pan + LMB 框选单选 MVP）；Material 节点 UE-ish chrome；CB 文件夹 icon + 选中框；Inspector 组件 icon 与 AddComponent 对齐。
- **Scope cut：** Anim 框选不扩多选模型（已确认）。
- **Inspector icon 根因：** `ResolveIconGlyph(MEClass*)` 用全名 `minEngine::X` 查短名表 → puzzle；string 路径会剥 `::`。
- **Design：** [ED-F15_EDITOR_GRAPH_CB_UX_POLISH_DESIGN.md](./Editor/ED-F15_EDITOR_GRAPH_CB_UX_POLISH_DESIGN.md)。待审批后按 W4→W1→W3→W2 开工。

### 2026-09-15 - ED-F11 W3 跟进：Reveal / Pin 直关 / Tab 类型图标
- **Reveal：** 弃用 `explorer /select`（正斜杠/`"` 易错位）；改 `make_preferred` + `SHOpenFolderAndSelectItems`。
- **Pin 直关：** `BeginTabItem` 选中帧点 X 时 `selected==true && open==false` 曾只 `EndTabItem` 不 `RequestClose`；现始终处理 `!open`。Pin 仍只挡 Close Others/All。
- **Tab 图标：** `EditorAssetTypeIcons` 与 Content Browser 共用 FA glyph；标签改 `* Title` + 左侧 overlay 图标。
- **验证：** Editor 构建 OK；手测 Reveal / Pin 直关 / Tab 图标通过（维护者）。

### 2026-09-15 - ED-F11 W3：Pin / Close All / Reopen / 路径 / 快捷键
- **Pin/Unpin：** Tab 右键；Pinned 沉到左侧区；批量 Close 跳过 Pinned；Close All 同策略。
- **Reopen Closed：** 关闭历史（≤16）→ `OpenOrFocus`；Ctrl+Shift+T；Ctrl+Tab / Ctrl+Shift+Tab。
- **路径：** Copy Relative；Reveal in Explorer（Windows `/select`）；Copy Asset Path 改为绝对路径。
- **钩子：** `AppendTabContextMenu`；Tab 标签 `* TypeInitial: Title`。
- **余量：** Save As；Scene 多开策略；手测。
- **验证：** Editor 构建 OK。

### 2026-09-15 - ED-F14 Done：验收 + Console Domain / ObjectPtr Undo
- **验收：** 人手 Undo/Redo 冒烟通过（维护者）。
- **跟进修：** Debug 补全/`help` 按 `ActiveSessionTypeId` + 命令 `Domain` 裁剪；Material ObjectPtr 走 `OnSelectionCommitted` 后再入栈。
- **验证：** Editor 构建 OK；`test command-system` 28/28 PASS。准备整批 commit。

### 2026-09-14 - ED-F14 W4：Material / AnimGraph Debug 域动词
- **Material：** `mat_list_node_types` / `mat_list_nodes` / `mat_add_node` / `mat_remove_node` / `mat_connect` / `mat_disconnect` / `mat_set_shading` / `mat_set_blend` → 同 GUI `Submit*` / `SetShading|Blend`。
- **AnimGraph：** `anim_list_states|transitions|params`；`anim_add/remove/rename_state`；transition / any / reverse / set_default；`anim_add/remove_param` → `SubmitOwnedPropertyMutation(m_StateMachine|m_Schema)`。
- **注册：** `RegisterEditorConsoleCommands` 旁挂 `EditorMaterialDebugCommands` / `EditorAnimGraphDebugCommands`。
- **验证：** Editor 构建 OK；`test command-system` 28/28 PASS。
- **余量：** 人手 Undo/Redo 冒烟；通过后 Registry → Done + 准备 commit。未 commit。

### 2026-09-14 - ED-F14 W2/W3：Material 拓扑 + AnimGraph 入栈
- **Material：** `EditorAdd/Remove/Connect/DisconnectMaterial*Command`；位姿拖拽结束 SetProperty。
- **AnimGraph：** `AnimationGraphEditor` 实现 `EditorSetObjectPropertyTarget`；变异走 `m_StateMachine` / `m_Schema` 整块 blob（数组叶子 path 不可用）。
- **验证：** Editor 构建 OK；`test command-system` 28/28 PASS。
- **余量：** W4 Material/AnimGraph Debug；人手 Undo 冒烟。未 commit。

### 2026-09-14 - ED-F14 W2 画布：节点 DragFloat 入栈
- **路径：** `MaterialGraphNodeRegistry::DrawNode` 本地缓冲 → 激活捕获 before → 失活 `SubmitSetObjectProperty`。
- **Constant3：** 一次手势提交 R/G/B 三条（同 editId 多字段）。
- **未做：** 拓扑、位姿、Debug。
- **验证：** Editor 构建 OK；`test command-system` 回归。

### 2026-09-14 - ED-F14 W2-A：Material Inspector 字段入栈
- **共用：** `EditorSetObjectPropertyTarget` + `EditorObjectPropertyApply`；`EditorSetObjectPropertyCommand` 不再绑死 SceneEditor。
- **Material：** `Apply/SubmitSetObjectProperty`；Shading/Blend 经 Command + prune；Undo 还原 Output 连线快照。
- **Inspector 节点字段：** 激活/失活捕获 blob → Submit（`applyOnFirstExecute=false`）。
- **未做：** 画布 DragFloat、拓扑、位姿、Debug。
- **验证：** Editor 构建 OK；`test command-system` 28/28 PASS。未 commit（与 W1 同工作区）。

### 2026-09-14 - ED-F14 W1 Done：edit/verify + Scene 结构 Debug
- **edit：** `PropertyWriteMode::RespectPolicy`；`set` 仍 Force。`EditDefaultsOnly` 在 SceneInstance 下 `edit` 失败、`set` 成功。
- **verify：** 可选 `=` / `==`；Payload `ok` / expected / actual。
- **Scene Debug：** `add_go` / `delete_go` / `add_comp` / `remove_comp` / `reparent` / `rename_comp` / `move_comp` → 现有 `Submit*`。
- **验证：** `minEngineTests.exe test command-system` 28/28 PASS；Editor 构建 OK。
- **Next：** W2 Material 字段 Set/Edit + 拓扑 Command。未 commit。

### 2026-09-14 - ED-F14 W1 开工：edit/verify + Scene 结构 Debug
- **edit：** RespectPolicy（`PropertyEditPolicy` / SceneInstance）；`set` 仍 Force。
- **verify：** `==` 断言 + PayloadJson。
- **Scene Debug：** `add_go` / `delete_go` / `add_comp` / `remove_comp` / `reparent` / `rename_comp` / `move_comp` → 现有 Submit*。
- **测试：** command-system 增 edit/verify 用例。

### 2026-09-14 - ED-F14 Design 展开稿
- 代码实况矩阵：Scene 已入栈；Material/AnimGraph 直写；Debug 动词缺口。
- 锁定候选：Material 属性竖切（B 优先）或枚举字段（A）；AnimGraph Add/Remove State 或 Transition。
- `edit`/`verify` 契约；Deferred 表；待审批。
- F13 已提交：`c7b3f34`。

### 2026-09-14 - ED-F13 Done：显式 AddComponent（去 Select 前置）
- **API：** `ApplyAddComponentToGameObject(id,…)`；`ApplyRemoveComponentFromGameObject`；删 `ApplyAddComponentToSelected*`。
- **Command：** `EditorAddComponentCommand` 不再先 Select；Undo 按 owner id 移除。
- **Picker：** Inspector/Context 提交显式 id；去掉 `selectTargetBeforeAdd`。
- **验证：** Editor 构建 OK；`test command-system` 25/25 PASS。
- **保留：** 其它显式 `Apply*`；Delete 恢复后 Select 仅 UX。

### 2026-09-14 - ED-F13 / F14 Design 修订（选中态口径）
- **纠偏：** F13 不删 Editor `Apply*`、不把逻辑搬进 Command；只消灭「先 Select 再改」。
- **分层：** GUI 产生 Command → Command 调用 → Editor 实现。
- **状态：** 修订稿待维护者审批；未开码、未 commit。

### 2026-09-14 - ED-F13 / ED-F14 Design 首稿
- **F13：** [ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md](./Editor/ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md) — 扫描 Scene Command→Apply* / Select 耦合；删除列表；Wave W1–W5。
- **F14：** [ED-F14_EDITOR_COMMAND_COVERAGE_DESIGN.md](./Editor/ED-F14_EDITOR_COMMAND_COVERAGE_DESIGN.md) — 覆盖矩阵；Blocked on F13；edit/verify + Material/AnimGraph MVP。
- **Commit backlog：** `fab9a4c` 登记 ID。

### 2026-09-14 - ED-F12 committed；下一刀 ED-F13/F14 登记
- **Commit：** `aa56421` feat(editor): move debug commands into Editor and unify EditorCommand naming
- **Next：** **ED-F13** EditorCommand-first（去 Select/Apply 套壳）；**ED-F14** 各 SubEditor 命令面完备（edit/verify 等可并入）

### 2026-09-14 - ED-F12：DebugCommand/PropertyPath 迁入 Editor
- **目录：** `Editor/src/DebugCommand/`、`Editor/src/PropertyPath/`；Engine Runtime 不再含调试命令面。
- **测试：** `minEngineTests` 直接编译上述 Editor 源（Editor 仍为 exe，未拆 Lib）。
- **记账（另 Feature）：** `Editor*Command` 经 `SceneEditor::Select* + Apply*` 转发，应改为直接调用被编辑对象能力。
- **验证：** （构建后）`test command-system`。

### 2026-09-14 - ED-F12 Done：DebugCommand* / EditorCommand* 命名收敛 + W1–W2
- **命名：** `Runtime/Core/Command` → `DebugCommand/`（`DebugCommandRegistry/Executor/Context/Result…`）；`IEditorCommand` → `EditorCommand`；Scene 操作类统一 `Editor*Command`。
- **协议：** `DebugParseContext`；Console 填 Active Session；`PayloadJson`（get/find/list_go）。
- **未做（下一 Feature）：** `edit` / `verify` / `invoke`；查询侧完整 EditorCommand 对象化；UI `CommandConsole` 文件夹改名。
- **验证：** `minEngineTests.exe test command-system` 25/25 PASS；`Editor` 构建通过。

### 2026-09-14 - ED-F12 W1–W2 开工：ParseContext + PayloadJson
- **范围收窄：** 本 Feature 不新增 `edit`/`verify`/`invoke`。
- **实现：** `DebugParseContext`；Console 从 `DocumentHost` Active Session 填充；`CommandResult.PayloadJson`；`get`/`find`/`list_go` 填机读 Payload。
- **验证：** `minEngineTests.exe test command-system` — 25/25 PASS。
- **Next：** W3 `set` 写路径整理；查询 EditorCommand 对象化（W4）。

### 2026-09-14 - ED-F12：Command 目录表 + 迁移/目录布局
- **Docs：** §3.9 续用/新增（get/set/edit/verify…）与 EditorCommand 映射；§3.10 迁移阶段与 `Commands/{Global,Scene}` + `DebugCommand/` 目标树。

### 2026-09-14 - ED-F12：基线 vs 方言、双上下文、set/edit
- **补强：** EditorCommand 基线（如 get Guid+path）vs Debug Suite 方言糖；DebugParseContext ≠ 执行载荷；`set` 侵入 / `edit` 守 specifier（对齐反射与 PropertyEditPolicy 精神）。
- **Docs：** [ED-F12](./Editor/ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md) §0.4–0.8。

### 2026-09-14 - ED-F12：再校准为 EditorCommand 单核心
- **口径：** Command≡EditorCommand；DebugCommand（原 Console）/ GUI / MCP 均为构造前端；补全与 Active Session 隐式→显式属 Debug 配件；命令按域（Global / Scene / Material…）管理。
- **Docs：** [ED-F12](./Editor/ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md) §0.3–0.5、§1、§3 已改。
- **Next：** 维护者确认后拍 §9 → Planned。

### 2026-09-14 - ED-F12：口径修订为 Agent Edit Protocol
- **Docs：** [ED-F12 Design](./Editor/ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md) — 资产中心；Edit Session 上下文；Command / EditorCommand / ConsoleCommand / MCP Tool 四层；§1 对照现码与 ED-F04 偏差；Scene 降为竖切。
- **旧文件名** `ED-F12_WORLD_QUERY_*` 已替换。
- **Next：** 维护者确认 §0–§1 → 拍板 §9 → Planned。

### 2026-09-14 - ED-F12：World Query–Modify–Verify Design 初稿（已被上条口径修订取代）
- 初稿聚焦 Scene list/get/set/verify；随后抬升为资产会话协议。

### 2026-09-14 - CORE-F19：拆除 Lua↔Delegate 反模式竖切
- **删除：** `LuaDelegateBind.h`、`ButtonComponent::BindOnClicked`、`LuaDelegateProbeComponent`、`DelegateProbeDemo.lua`、PostGenerated Button/Probe usertype、`TrackLuaBinding`。
- **保留：** `LuaComponent::Call` / `TryGetFunction`（含 tick 缓存修复）。
- **方向：** Lua `Add(fn)` → **CORE-F21**（基建已在 **CORE-F20**）。
- **验证：** `minEngineTests.exe test lua-script-mvp` PASS。

### 2026-09-14 - CORE-F19：C++→Lua Call + BindLua（0/1/2）+ Button 竖切（已部分撤回）
- 曾落地 BindLua / Button Bind / Probe；**同日纠偏拆除**（见上条）。Call 路径保留。

### 2026-09-14 - CORE-F19 Planned：Lua ↔ Delegate / C++→Lua Design 草稿
- **Registry：** `CORE-F19`。
- **Docs：** [CORE-F19 Design](./Platform/Scripting/CORE-F19_LUA_DELEGATE_AND_INVOKE_DESIGN.md)。

### 2026-09-13 - ED-F11：Mode→Active Session 语义 + Shared CB 入布局
- **设计：** [ED-F11](./Editor/ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md) §3.1.1 — Shared vs Type Suite；Tab/Active Session 为前台真相。
- **实现：** Material/AnimGraph 默认布局 dock Content Browser；Window → **Focus Document**（无打开文档则提示）；CB Shared owner。
- **旁路已修：** OpenOrFocus 后同帧旧 Tab 抢焦点（仅 `IsItemActivated` 切 Session）。
- **验证：** `cmake --build … --target Editor` → `MaximumEditor.exe`。
- **Next：** 手测切 Material/AnimGraph 时 CB 在 dock 内；准备 ED-F11 commit。

### 2026-09-13 - ED-F09 Done + ED-F11 W0–W2：Console 过滤与多文档 Tab Host
- **ED-F09：** `LogSystem::ForEachRegisteredChannel`；Console Channel 多选、时间显示/窗、Collapse、Clear on Play、source tooltip；去掉 Core/App Source 二分。
- **ED-F11：** `EditorDocumentHost` + 可注册 DocumentType；OpenOrFocus（同资产聚焦）；Material/AnimGraph 多 Session 槽；Scene 单例策略；文档 Tab 条 + W1 右键；`GetCommandStack()` → Active Session 栈；切域默认不撕后台 Session。
- **验证：** `cmake --build … --target Editor` → `Maximum.exe`；`minEngineTests.exe test logging-channels` PASS。
- **未做（F11 W3+）：** Pin、Close All、Reopen Closed、撕窗/分屏。
- **勿提交：** worktree 本地 `MyMEProject.meproject` ProjectRoot、`ScriptBinding/manifest.json` 绝对路径。
- **Next：** 手测多 Tab；准备 commit；或 F11 W3 / 登记 Query 面。

### 2026-09-13 - ED-F09 / ED-F11 Design 修订（维护者反馈）
- **F09：** 完整过滤维度表 + UE/Unity 对比；纳入时间窗 / Collapse / Clear on Play；分期交付，非「仅 Channel MVP」。
- **F11：** 去掉 Host 核心 enum kind → **DocumentTypeRegistry**；同类型多 Session + 同资产 Focus；**CommandStack per Session** 硬性；澄清方案 A = 共享 Dock **壳** + **按类型面板套件**（轨道编辑器属类型套件）。
- **Next：** 维护者确认开放点 → In Progress / Impl。

### 2026-09-13 - Phase P editor：登记 ED-F10/F11 + 起草 Console / Tab Design
- **Registry：** `ED-F09` 仍 Planned（Design 充实）；新增 `ED-F10` Settings 占位、`ED-F11` Multi-document Tab Host Planned；ED next → **F12**。
- **Docs：** [ED-F09](./Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)；[ED-F11](./Editor/ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md)（方案 A 共享 Dock + Session）；ACTIVE_WORK Primary 改为先 UI 壳后 Query。
- **决策待审：** F09 O1–O3；F11 O1–O5（默认 A、Scene 单例、Material 多开、Undo P2）。
- **Next：** 维护者审阅 → In Progress；实现前可再补 `*_IMPLEMENTATION.md`。

### 2026-09-12 - WF-F03 Done：Maximum 产品显示名
- **实现：** `ProductBranding.h`；窗口标题 / GLFW 初始 / CLI `--version` / Vulkan 名 / Help→About。
- **产物：** CMake `OUTPUT_NAME Maximum` → `bin/Maximum.exe`（`--target Editor` 不变）；Launcher / debug 脚本对齐。
- **版本：** 仍消费 `GetEngineVersion()`（0.0.9）；D8 完整「0.1.0」数字待 Phase D。
- **Next：** Phase F 可 fan-out；可选 `ED-F09`；准备 commit。

### 2026-09-12 - WF-F03 Design 充实 → In Progress
- **契约：** 显示名 `Maximum`；版本仍 `GetEngineVersion()`（0.0.9）；不改仓库/exe/命名空间。
- **切片：** S00 ProductBranding + 标题/CLI/GLFW/Vulkan；S01 About 最小模态。
- **Next：** 确认 §开放点推荐默认 → 实现。

### 2026-09-12 - CORE-F18 Done：EngineVersion 0.0.9 + schema 门闸
- **实现：** `EngineVersion.h`（0.0.9）；`kDiskSchemaVersion=1`；根写 `$engineVersion`；`ValidateDiskSchema`；Serializer 统一门闸。
- **资产：** stamp MyMEProject / EngineConfig / Launcher Empty / **`.meta` 侧车**（含 EngineDefault）；`scripts/stamp_disk_meta.py` 已纳入 `.meta`（首轮漏 stamp 导致 Editor 开项目 Warn）。
- **验证：** `minEngine`/`Editor`/`minEngineTests` 构建；`test engine-version` 3/3 PASS。
- **Next：** Phase F3 `WF-F03` Maximum 品牌；准备 commit。

### 2026-09-11 - CORE-F18 Design：EngineVersion 一等公民 + 资产字符串戳
- **修订：** C++ `EngineVersion{Major,Minor,Patch}` 真源；盘面 `$engineVersion` 用 **string**；门闸仍只认 `$schemaVersion`。
- **Docs:** [CORE-F18](./Platform/Serialization/CORE-F18_SCHEMA_ENGINE_VERSION_DESIGN.md)；Registry/ACTIVE_WORK/WF-F03 交叉引用。
- **Next:** 确认 §10 余项（缺 schema 策略等）→ In Progress → S00。

### 2026-09-11 - CORE-F18 Design 详设草稿（待审阅）
- **Docs:** [CORE-F18_SCHEMA_ENGINE_VERSION_DESIGN.md](./Platform/Serialization/CORE-F18_SCHEMA_ENGINE_VERSION_DESIGN.md) 充实：`$schemaVersion` 门闸 + `$engineVersion` 戳；无迁移表。
- **Next:** 维护者总览 §10 开放点 → 拍板 → In Progress → S00。

### 2026-09-11 - CORE-F17：按模块重归属 Channel + LogPlatform
- **新增：** `LogPlatform`（NFD/FileDialog、GLFW WindowSystem）。
- **映射：** Asset Loaders→`LogAsset`；Vulkan/OpenGL→`LogRHI`；其余 Render→`LogRender`；Editor→`LogEditor`；Tests→`LogTest`；其余按推荐表。
- **脚本：** `scripts/remap_log_channels.py`
- **Next：** 构建验证后可准备 commit。

### 2026-09-11 - CORE-F17 Done：LogChannel + LogRecord 硬切
- **实现：** typed Channel（方案 A）、`ME_LOG` 唯一宏面、`LogRecord`（channel* + name）、Sink、Fatal=Flush+abort；删除旧宏/`LogConsoleEntry`/`Get*Logger`。
- **迁移：** 全仓机械替换（脚本 `scripts/migrate_log_macros.py`）；ConsoleWindow 最小读 `LogRecord`。
- **验证：** `cmake --build … -j 2` → minEngine / Editor / minEngineTests；`minEngineTests.exe test logging-channels` PASS。
- **Next：** Phase F2 `CORE-F18`；旁路 `ED-F09` Console 过滤 UX。

### 2026-09-11 - CORE-F17：Fatal=abort；Record 持 Channel*；登记 ED-F09
- **Fatal：** Flush 后 `std::abort`。
- **LogRecord：** `const LogChannelBase*` + `channelName` 备份（进程内可解析 Channel 元数据）。
- **ED-F09：** Editor Console LogRecord UI/过滤 — [草稿 Design](./Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)。
- **Next:** 确认后 CORE-F17 In Progress → S00。

### 2026-09-11 - CORE-F17 拍板写入 Design
- **显示名 A**；**先 runtime 过滤**；**旧宏硬切无兼容**。（Fatal 随后改为 abort，见上条）
- **LogField** 暂缓（占位）；**删除 LogConsoleEntry**，Console 直接存/读 `LogRecord`。
- **Next:** 全文确认 → In Progress → S00（含全仓 ME_LOG 替换）。

### 2026-09-11 - CORE-F17 Design：typed LogChannel + 数据结构/迁移详设
- **采纳：** 第一期上 UE 式 `ME_DECLARE/DEFINE_LOG_CHANNEL`（术语 LogChannel）。
- **Docs:** Design §3–§5 数据结构/接口/旧宏迁移；Impl S00 DoD 同步；开放点（显示名方案 A、Fatal、Get*Logger）。
- **Next:** 维护者审阅开放点后 → In Progress → S00。

### 2026-09-11 - CORE-F17 Design 修订：LogChannel + structured LogRecord
- **命名：** 用 **LogChannel**（不用 UE Category）。
- **模型：** LogRecord L0/L1、Severity、ILogSink；spdlog 仅 Backend。
- **Docs:** Design 重写 + [Impl](./Platform/Core/CORE-F17_LOGGING_CHANNELS_IMPLEMENTATION.md) S00–S03。
- **Next:** 维护者确认 → In Progress → S00。

### 2026-09-11 - Phase F kickoff: register F17/F18/WF-F03 + Logging Design
- **Commit prior:** `7962a5f` 0.1.0 Roadmap docs.
- **Registry:** `CORE-F17` Logging · `CORE-F18` Schema/engine version · `WF-F03` Maximum branding — **Planned**.
- **Docs:** Roadmap Status → In Progress；ACTIVE_WORK Primary = F1 Logging；[CORE-F17 Design](./Platform/Core/CORE-F17_LOGGING_CHANNELS_DESIGN.md)；F18/WF-F03 stubs.
- **Next:** 维护者确认 CORE-F17 Design → In Progress → Impl S00（Channel 注册表）。

### 2026-09-10 - 0.1.0 Roadmap 修订：地基 fan-out + Prefab A/B
- **采纳：** Phase F（Log+Schema+Brand）先合入再并行；editor 主攻世界 Query/Verify；mcp 跟 API；core Profiler。
- **Prefab：** A=Instantiate；B=子编辑器依赖隔离 RT（单 Forward+单 RDG 同尺寸串图挡点）。
- **Doc:** [ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md) §3 重写。

### 2026-09-10 - Draft: Maximum 0.1.0 Roadmap（FPS Demo + Agent-friendly）
- **Doc:** [ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md) — Waves 0–6；Prefab / Schema / Agent·MCP / Logger / Lua callbacks / Profiler / EditorSettings / Tab / Maximum 命名；Anim·UI 支线；Net·完整 AI Framework 后置。
- **ACTIVE_WORK / Capability TL;DR:** 指向该 Draft；Feature ID **待批准后登记**。
- **Next:** 维护者评审 → Status In Progress → 登记 CORE-F17… → Wave 1 Design。

### 2026-09-10 - Docs align: Registry / ACTIVE_WORK / Capability after merge wave
- **Why:** Active Registry rows still showed ANIM/UI/RND-F16 as Planned；Next free IDs and ACTIVE_WORK still pointed at pre-merge feat/* focus.
- **Done:** FEATURE_REGISTRY 补齐 CORE-F13–F16、ED-F06–F08、ASSET-F01–F02、ANIM-F01–F05、UI-F01–F03；修正 Next free；ACTIVE_WORK 改为「无锁定 Primary + 候选 A–D」；Capability Roadmap M1–M3 landed + §4/§7；PROJECT_CONTEXT / BOOTSTRAP_DIGEST；RND-F16 Design/Impl → Done。
- **Engineering note:** merge fallout build fix already on master (`54b228e`); verify earlier smoke/full PASS.
- **Next:** 维护者选定下一 Primary（见 ACTIVE_WORK）。

### 2026-09-10 - Merge: feat/ui → master
- **Code:** ScreenUI / Sprite / Hierarchy / KeepWorld / LinearColor；`ForwardRenderer`/`RenderScene` 同时保留 Skeletal + Sprite；Hierarchy 树 + ED-F05 Inspector 组件 API 并存；TestAccess 保留并迁移 UI tests。
- **Docs ID remap:** Hierarchy→**CORE-F14**；KeepWorld→**CORE-F15**；LinearColor→**CORE-F16**；Hierarchy Tree→**ED-F08**。
- **Next:** merge wave 收口；按需 build/smoke。

### 2026-09-10 - Merge: feat/animation → master
- **Code:** Animation / Import / Parameter Store / SmGraph canvas；`AssetMeta.gen` 保留 ACCESSORS + `SourcePath`；manifest 路径回主仓。
- **Docs ID remap:** anim `CORE-F08`→**CORE-F13**；anim `ED-F05`→**ED-F06**；anim `ED-F06`→**ED-F07**（master 已占用 CORE-F08–12 与 ED-F05 Inspector）。
- **Deleted:** placeholder `ANIM-F01_ANIMATION_SYSTEM_DESIGN.md`（由 Skeletal Mesh Design 取代）。
- **Next:** 合入 `feat/ui`。

### 2026-09-10 - ED-F07 Done (canvas polish + empty-clip policy)（分支称 ED-F06）
- **Done:** Entry / AnyState 画布；边与 State 右键；Validate 允许空 Clip + hold-last Pose。
- **Verify:** Editor PASS；`animation-graph` 4/4；维护者 smoke OK。

### 2026-09-10 - ED-F06/F07 + ANIM-F04 Design（分支 ED-F05/F06）
- **Registry remap on merge:** 见 FEATURE_REGISTRY §Merge-wave ID remaps。
- **Docs:** SM Canvas + Polish + BlendTree Design/Impl。

### 2026-09-10 - Merge: feat/gameplay-framework 鈫?master锛圙P-F01/F02锛?- **Code:** Tag + Scene Event bus锛沗ObjectManager`/`Scene` 鍐茬獊鎸?TEST-F04 淇濈暀 `TestAccess` + private锛沗GameplayEventTest` 鏀硅蛋 `TestAccess<ObjectManager>`銆?- **Docs:** ACTIVE_WORK / FEATURE_REGISTRY / PROGRESS_LOG 骞跺叆 GP 鏉＄洰銆?- **Next:** 缁х画鍚堝叆 `feat/animation` 鈫?`feat/ui`銆?
### 2026-09-10 - Merge: feat/editor 鈫?master锛圗D-F05 + docs锛?- **Code:** Editor ED-F05锛圫00鈥揝04锛汼05 Deferred锛夊凡涓?master 鑷姩鍚堝苟銆?- **Docs:** ACTIVE_WORK / FEATURE_REGISTRY / PROGRESS_LOG 鍚堝苟鍐茬獊宸茶В锛氫繚鐣?core锛圕ORE-F08鈥揊12 / TEST-F04锛変笌 editor锛圗D-F05锛変袱渚у巻鍙层€?- **Next:** 缁х画 merge wave銆?
### 2026-09-10 - TEST-F04 Done: TestAccess\<T\> + Scene private cleanup (`feat/core`)
- **S00鈥揝03:** Core `TestAccess` 鍓嶅悜锛涚儹鐐圭郴缁熷崟 friend锛沗Tests/Access/*` 鐗瑰寲锛涘垹 `*TestScope` friend銆?- **S04:** `SceneManager` / `Scene` 鍘绘帀 temporarily-public锛涙柊澧?`Scene::SetSceneName`锛岃縼绉?Editor/Loader/Duplicator/Tests 鍐欏叆鐐广€?- **Deferred:** `LuaScriptSystem::SetInstance` 浠?public锛堥潪 friend 鐥涚偣锛夈€?- **Verified:** `minEngine` / `minEngineTests` / `Editor` 缂栬瘧锛沗scene-clone` 路 `object-manager` 路 `physics-smoke` PASS锛圫04 鍚庯級銆?- **Next:** 鍑嗗 commit銆?
### 2026-09-10 - TEST-F04 S00鈥揝03: TestAccess\<T\> migrate hotspots (`feat/core`)
- **Runtime:** `Core/Testing/TestAccess.h`锛堜富妯℃澘鍓嶅悜锛夌粡 `Core.h` 鑷姩鍖呭惈锛沗ObjectManager` / `SceneManager` / `PhysicsSystem` / `AssetManager` / `AudioSystem` 浠?`friend Testing::TestAccess<T>`锛屽垹闄ゅ叏閮?`*TestScope` friend/鍓嶅悜銆?- **Tests:** `Tests/Access/*TestAccess.h` 鐗瑰寲锛涘悇 Suite Scope 鏀硅皟 `TestAccess<T>::SetInstance`锛汚udio 缁?`InitializeWithBackend` 鍖呰銆?- **Deferred:** S04 `SceneManager` temporarily-public 瀛楁锛沗LuaScriptSystem::SetInstance` 浠嶄负 public锛堟湭杩侊級銆?- **Verified:** `minEngine` / `minEngineTests` 缂栬瘧锛沗object-manager` 路 `physics-smoke/sync/shapes` 路 `scene-clone` 路 `command-system` 路 `audio-smoke` 路 `asset-manager` 路 `serialization-archive` 路 `delegates` PASS銆?- **Next:** 鍑嗗 commit锛汼04 鍙€夈€?
### 2026-09-10 - TEST-F04 Design: Core-hosted TestAccess fwd + migration guide (`feat/core`)
- **Decision:** `Runtime/Core/Testing/TestAccess.h`锛堜粎涓绘ā鏉垮墠鍚戯級鐢?`Core.h` include锛涚敓浜х被鍨嬩竴琛?`friend Testing::TestAccess<T>`锛涚壒鍖栧彧浣?`Tests/Access/`銆?- **Docs:** Design 搂杩佺Щ 鈥?S00鈥揝04銆佸崟绫诲瀷娓呭崟銆佸绯荤粺 Scope 绛栫暐銆佸簱瀛樺湴鍥俱€?- **Next:** 纭鍚庡紑 S00锛圕ore 澶达級鈫?S01锛圤bjectManager锛夈€?
### 2026-09-10 - CORE-F12 Done: ME_GENERATED_BODY() no-arg + marker attach (`feat/core`)
- **Macro:** `ME_GENERATED_BODY()` 鍘绘帀鏈娇鐢ㄧ殑绫诲瀷瀹炲弬锛汻untime 鏍囨敞澶村叏閲忔洿鏂般€?- **Tool:** `find_attached_class_marker_args` 鈥?marker 涓?`class`/`struct` 涔嬮棿浠呯┖鐧?娉ㄩ噴鎵嶉檮鐫€锛沗TOOL_CACHE_VERSION` 16锛堥槻缂╃煭瀹忚鍚庤鍙嶅皠閭昏繎鏃犳爣璁扮被鍨嬶級銆?- **Docs:** Design + Registry Done锛沨andbook / README 绀轰緥鍚屾銆?- **Also registered:** `TEST-F04` Planned 鈥?`Testing::TestAccess<T>` 鏀舵暃 TestScope friends銆?- **Verified:** `minEngine` / `minEngineTests`锛沗reflection-function` PASS銆?- **Next:** `TEST-F04` S01锛圤bjectManager锛夋垨鍥?Primary `ANIM-F01`銆?
### 2026-09-09 - ED-F05: CB 鍙抽敭 Rename
- **瀹炵幇锛?* `EditorActionId::Rename` 瀵?TreeAsset/TileAsset 鍙锛沗ContentBrowserModule::RequestBeginAssetRename` 鈫?Window 娑堣垂杩?`InlineRenameField`銆?- **Docs锛?* Design 搂10.5 / Impl S04 DoD 鏇存柊銆?- **Next锛?* 骞跺叆鏃㈡湁 ED-F05 commit锛坅mend锛夈€?
### 2026-09-09 - ED-F05: 楠屾敹鍙嶉 polish
- **Rename 閫冮€革細** `InlineRenameField` 鏀圭敤 `IsItemDeactivated()`锛堝師鍏?`AfterEdit` 瀵艰嚧鏈敼瀛楃偣绌虹櫧浠嶅仠鍦ㄨ緭鍏ユ锛夈€?- **Add Component锛?* Inspector 鈫?鎸夐挳寮瑰嚭 searchable 鍒楄〃锛堟粴鍔級锛涘彸閿瓙鑿滃崟 鈫?`BeginChild` 婊氬姩銆?- **Icon锛?* `ComponentTypeUiCatalog::DrawIcon` 鎸夋鏂囧瓧鍙风粯鍒讹紙绾?0.92脳锛夈€?- **Header锛?* Accent 鍗婇€忔槑搴曟潯锛涘彸閿寕鍦ㄦ暣琛?Group 涓婏紱Rename 鍔ㄤ綔鏀寔 Component銆?- **Next锛?* 鍐嶆墜娴嬩笂杩伴」 鈫?鍑嗗 commit銆?
### 2026-09-09 - ED-F05: S00鈥揝04 钀藉湴锛圫05 Deferred锛?- **S00:** `InlineRenameField` 鈥?Esc / 鐐圭┖鐧?Cancel锛汦nter Commit锛涚┖鍚?Cancel銆侶ierarchy + Inspector GO/Component + CB 澶嶇敤銆?- **S01:** `ComponentTypeUiCatalog` + `AddComponentPicker`锛汚bstract specifier锛汭nspector 鍙抽敭鏍圭骇 `Add Component 鈻禶锛堝惈鎼滅储锛夛紱Hierarchy 涓嶅姞銆?- **S02:** 缁勪欢澶?`icon + instance name + TypeDisplay`锛沗RenameComponentCommand` + `Component::Rename`銆?- **S03:** `GameObject::MoveComponent`锛圧oot 绂佺Щ涓斾笉鍙惤鍒?Root 鍓嶏級+ `MoveComponentCommand` + Inspector 鈫戔啌銆?- **S04:** CB F2 / 鏍戝彾 / 鐡︾墖鍐呰仈鏀瑰悕 鈫?`AssetManager::RenameAsset`锛堟棤 Undo锛夈€?- **Build:** `Editor` 鐩爣閫氳繃銆?- **Docs:** Design/Impl/Registry/ACTIVE_WORK 鈫?Done锛圫05 Deferred锛夈€?- **Next:** 鎵嬫祴 搂Acceptance 鈫?**鍑嗗 commit**锛堝嬁鑷姩 commit锛夈€?
### 2026-09-09 - ED-F05: 鍙抽敭 Add Component + 涓婁笅鏂囪彍鍗曡瘎浼?- **璇勪及锛?* Typed Context 妯″瀷鍋ュ叏锛涗笉蹇呴噸鍋氥€傜幇缃?Add 鍩嬪湪 Create 涓嬩笖 Hierarchy 绂?Add銆?- **鐩爣锛?* 鏍圭骇 `Add Component 鈻禶 娆＄骇绫诲瀷鑿滃崟锛沗AddComponentPicker` 涓?Inspector 鍚屾瀯鏈嶅姟锛汬ierarchy GO 鏀惧紑銆?- **Docs锛?* Design 搂10.9锛涚‘璁?Q/R/S锛汭mpl S01 DoD 鏇存柊銆?
### 2026-09-09 - ED-F05: Design 搂10 瀹炵幇璁捐琛ュ己
- **鍥炬爣锛?* 鏄庣‘鐢?Editor `ComponentTypeUiCatalog` **C++ 闈欐€佽〃** + 鍩虹被鍥為€€锛涗笉鐢?Runtime `ME_CLASS` Icon / JSON registry銆?- **鏂囨。锛?* Design 澧炶ˉ 搂10.1鈥?0.8锛圫00鈥揝04 API銆佹枃浠躲€佺姸鎬佹満锛涘緟纭 L鈥揚锛夛紱Impl 瀵归綈 Touch/DoD銆?- **Next:** 瀹￠槄 搂10 涓?L鈥揚锛涚‘璁ゅ悗寮€ S00銆?
### 2026-09-09 - ED-F05: Design Review锛堢淮鎶よ€呭弽棣堝苟鍏ワ級
- **濂戠害锛?* Esc **鎴?* 鐐圭┖鐧?= 鍙栨秷鍏抽棴杈撳叆妗嗭紱浠?Enter 鎻愪氦銆?- **缁勪欢澶达細** `icon + name(涓嶅垏璇? + TypeDisplay(鍒囪瘝銆佸幓 Component)`銆?- **Scope锛?* 鍗囧叆 **CB 鍐呰仈閲嶅懡鍚嶏紙S04锛?*锛涜鍙ｄ笘鐣屽浘鏍?鈫?**S05 Deferred**锛汬ierarchy 鐖跺瓙鎷栨嫿 Out锛堜粬鍒嗘敮宸叉湁锛夈€?- **Status锛?* Registry / Design / Impl 鈫?**Review**锛涘緟寮€骞层€?- **Next锛?* 缁存姢鑰呯‘璁ゅ紑骞?鈫?S00銆?
### 2026-09-09 - ED-F05: Inspector / Component UX Design Draft
- **Registry:** `ED-F05` 鍒濈鐧昏锛堝叾鍚庡凡鍗?Review锛岃涓婃潯锛夈€?- **Next:** 宸茬敱 Review 鏉＄洰鎵挎帴銆?
### 2026-09-05 - GP-F01/F02 Done: Tag + Scene Event bus
- **GP-F01:** `GameplayTag` / Manager / Container锛沗ME_DECLARE/DEFINE_GAMEPLAY_TAG*`锛汦ngine 鎸佹湁 Manager锛沗TAG_Channel_Default`銆?- **GP-F02:** `GameplayEventSystemComponent`锛圫cene 浣滅敤鍩燂級锛沝efault channel锛沠ilter/priority/depth锛沗Scene::GetGameplayEventSystem`锛涙棤 Payload銆?- **Code:** `Runtime/Function/GameplayFramework/{Tags,Events}/`
- **Verified:** `minEngineTests.exe test gameplay-tags` PASS锛沗test gameplay-events` PASS銆?- **Next:** 涓ゆ壒 commit锛圱ag / Event锛夊緟瀹℃壒銆?
### 2026-09-05 - CORE-F11-S06: Inspector live Assign + PostEdit semantics (`feat/core`)
- **Semantics:** 灞炴€х紪杈戣涔変笂鐨嗕负 PostEdit锛涙湁 Setter 鏃剁敱 Setter 鎵挎媴浼犳挱锛屽疄鐜板眰涓嶈皟铏氬嚱鏁般€?- **Inspector:** 鐩存帴瀛楁 Primitive/flat Color锛歚GetPropertyValue`鈫抰emp鈫抈AssignProperty`锛坄notifyPostEdit=!HasSetter`锛夈€?- **Guard:** 浠?`GetMutable(owner)==propertyPtr` 璧?Assign锛岄伩鍏嶅祵濂?struct 璇敤澶栧眰 owner銆?- **Physics:** 鍒犻櫎 `RigidBodyComponent::PostEdit`锛沗ColliderComponent::PostEdit` 浠呯暀鏃?Setter 鐨?`m_bActive`銆?- **Verified:** Editor / minEngineTests 缂栬瘧锛沗reflection-function` 路 `physics-smoke` 路 `serialization-archive` 路 `scene-clone` PASS銆?- **Next:** 鍑嗗 commit锛涘洖 Primary `ANIM-F01`銆?
### 2026-09-05 - CORE-F11 Done: property Getter/Setter native thunks (`feat/core`)
- **Macros:** `ME_REFLECTION_PROPERTY_{GETTER,SETTER}_THUNK` 鍦?`ReflectionMacros.h`锛沜odegen 鍙敞鍏ュ畯 + `ADD_FIELD_ACCESSORS`銆?- **Runtime:** `AssignProperty` / `GetPropertyValue`锛沗MEProperty` 鎸?optional Get/Set fn锛沗MEObject::PostEditChangeProperty`銆?- **Call sites:** Serializer 鏈?Setter 鍒?temp鈫扐ssign锛沺ending ObjectPtr resolve 璧?Assign锛坄m_Owner`鈫抈SetOwner`锛夛紱Editor undo 鏃?Setter 鏃?PostEdit銆?- **Migration:** 浠ｈ〃鐗╃悊瀛楁鎸?meta Setter锛涘垹闄?`ApplyPhysicsEditorSideEffects`銆?- **Fix:** AssignProperty 榛樿鍙傛暟鏀逛负閲嶈浇锛堥伩鍏?MinGW AVX `vmovdqa` 鏈榻愭爤宕╂簝锛夈€?- **Debt/Bugs:** TD-026 鈫?**Done**锛汢UG-CORE-001 鈫?**Fixed**锛堟畫浣欙細鏈寕 meta 鐨勫壇浣滅敤瀛楁闇€鎸夐渶杩佺Щ锛夈€?- **Verified:** `serialization-archive` 路 `reflection-function`锛堝惈 assign锛壜?`scene-clone` 路 `physics-smoke`銆?- **Next:** 鍑嗗 commit锛汭nfra 灞炴€у啓鍏ヨ建鍙敹锛涘洖 Primary `ANIM-F01` 鎴栦笅涓€椤广€?
### 2026-09-05 - CORE-F11 Design: property Getter/Setter native thunks (`feat/core`)
- **Decision:** header tool codegen **thin native Get/Set thunks**锛堝榻?UE UPROPERTY 璁块棶鍣紝涓嶇粡 `InvokeFunction` 鐑矾寰勶級銆?- **API:** `AssignProperty` / `GetPropertyValue`锛沗meta=(Getter/Setter)`锛涙棤 Setter 瀛楁琛屼负涓嶅彉銆?- **PostEdit:** `MEObject::PostEditChangeProperty` 浣?Editor 瀵规棤 Setter 瀛楁鐨勫厹搴曪紱鏈?Setter 榛樿涓嶅弻璋冦€?- **Serialize:** 鏈?Setter 鍒?Assign锛汼04 鍏?TD-026锛坄SetOwner`锛夈€?- **Docs:** Design + Impl S01鈥揝05锛汻egistry Planned銆?- **Next:** 纭 Design 鍚庡紑鐮?S01锛坱ool + codegen锛夈€?
### 2026-09-04 - GP-F01/F02 Design Draft锛坒eat/gameplay-framework锛?- **Registry:** 鏂板煙 `GP`锛沗GP-F01` GameplayTag銆乣GP-F02` GameplayEventSystem 鈫?**Draft**銆?- **Docs:** `docs/ai/Gameplay/` 鈥?Design + Implementation锛涜惤鐐?`Runtime/Function/GameplayFramework/`銆?- **鎵€鏈夋潈锛?* TagManager 鈫?Engine 瀛愮郴缁燂紙鏆傦級锛汦vent 鈫?Scene 浣滅敤鍩?GO Component锛汸ayload **Deferred**锛涙棤 ASC銆?- **ACTIVE_WORK:** Side track 鐧昏锛涗笉鎸?Animation Primary銆?- **Next:** 瀹?Design 鈫?Planned 鈫?寮€鐮?GP-F01-S01銆?
### 2026-09-04 - CORE-F10 Done: JSON disk compatibility (`feat/core`)
- **API:** 鎺ョ嚎 `strictTypeCheck`锛涙柊澧?`writeSchemaVersion` / `schemaVersion`锛涙緞娓?`skipUnknownField`=缂哄瓧娈点€?- **Json:** EndObject 瀵瑰浣欓潪 meta 閿?Warn锛涙牴 `$schemaVersion` 鍐?1 / 缂虹渷璇?0銆?- **Loose:** 鍙跺瓙 codec 澶辫触涓旈潪 strict 鈫?Warn+淇濈暀榛樿锛涚洏璺緞 Loader/Project/PathRegistry/AssetManager 榛樿瀹芥澗銆?- **Strict:** Binary Buffer / PIE 浠嶅己鍒?`skipUnknownField=false` + `strictTypeCheck=true`銆?- **Verified:** `serialization-archive`锛堝惈 schema/unknown 閿級路 `scene-clone` 路 `verify.ps1` smoke銆?- **Next:** 鍑嗗 commit锛汭nfra 搴忓垪鍖栬建鍙敹锛涘洖 Primary `ANIM-F01` 鎴栦笅涓€椤广€?
### 2026-09-04 - CORE-F09 Done: Binary wire v2 (`feat/core`)
- **Protocol:** Magic `MEB2` + SchemaVersion + Fingerprint锛汷bject = Tag + ClassId + FieldCount + BodyLength + `[FieldId + tagged value]*`锛涙棤 wire `EndObject`銆?- **Schema:** `TransientSchemaTable` 鍦?`FinalizeReflection` 鍚庡缓琛紙dense ClassId/FieldId锛夈€?- **API:** `BeginObject`/`BeginObjectPtr` 璧?`MEClass*`锛汢uffer 璺緞寮哄埗 `skipUnknownField=false`銆?- **PIE:** `SceneDuplicator` 鎭㈠ Binary buffer锛堝叧 TD-029锛夈€?- **Debt:** TD-028 / TD-029 鈫?**Done**銆?- **Out:** 瀛樼洏 Binary / Persistent 鍚嶅瓧閿湭瀹炵幇锛堝绾﹀凡鍐欏湪 Design锛夛紱鈫?`CORE-F10` JSON銆?- **Verified:** `serialization-archive` 路 `scene-clone`锛堝惈 physics-stack锛壜?`verify.ps1` smoke銆?- **Next:** 鍑嗗 commit锛涘彲閫夌洰瑙?PIE锛涚劧鍚?`CORE-F10` 鎴栧洖 Primary銆?
### 2026-09-04 - CORE-F08 Done: serialization cleanup (`feat/core`)
- **API:** `Serialize`/`Deserialize`/`ToFile`/`FromFile`/`*ObjectToBuffer`/`*ObjectToJson` 澧炲姞 `MEClass*` 閲嶈浇 + `StaticClass` 妯℃澘锛沗string` 钖勫吋瀹广€?- **Cleanup:** 鍒犻櫎鏈娇鐢ㄧ殑 `allowObjectPtrSerialization`銆乣m_IsHandlingPtr`锛涘幓鎺夎繃鏃?ObjectPtr TODO銆?- **P1:** `ForEachPropertyInHierarchy(MEClass*)`锛沗MEObject::StaticClass()`锛涘悎骞?property 鏌ユ壘锛汥eserialize `static_cast`銆?- **Call sites:** SceneEditor snapshot銆丼ceneDuplicator銆丩oaders銆丄ssetManager銆丳rojectManager銆丳athRegistry銆乀ests銆?- **Out:** TD-026 Deferred锛汢inary wire 鏈敼锛堚啋 CORE-F09锛夈€?- **Verified:** `serialization-archive` 路 `scene-clone` 路 `verify.ps1` smoke 路 Editor build銆?- **Next:** 鍑嗗 commit锛涚劧鍚?`CORE-F09` Wire Spec銆?
### 2026-09-03 - ED-F02 doc closeout + worktree bootstrap tooling
- **ED-F02:** Design/Impl/Registry 瀵圭収 `master` 鈥?S00鈥揝02/S04 **Done**锛汼03 SkyBox 瀹炰綋銆丼05 Abstract 鏍囨敞 **Remaining/Partial**銆?- **Philosophy / Roadmap:** 瑙佹棦鏈?`ENGINE_DESIGN_PHILOSOPHY.md` / `ENGINE_CAPABILITY_ROADMAP.md`锛堟湰鎵逛竴骞舵彁浜わ級銆?- **Tooling:** `scripts/create-worktree.ps1` + `.agents`/`.github` skills `create-worktree`銆?- **Worktrees:** `minEngine-animation` (`feat/animation`)銆乣minEngine-ui` (`feat/ui`) 鍒濆鍖栥€?- **Placeholder branches:** `feat/asset-pipeline`銆乣feat/gameplay-framework`銆乣feat/network`銆乣feat/ai`銆乣feat/core`锛堟棤 worktree锛夈€?- **Misc:** `MyMEProject.meproject` ProjectRoot 鎸囧洖涓讳粨璺緞銆?- **Next:** 纭鍚庡噯澶?commit锛涚劧鍚?`ANIM-F01` Design銆?
### 2026-09-03 - Bootstrap: design philosophy + capability roadmap
- **Philosophy:** [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) 鈥?Capabilities not Opinions; Mechanism over Policy; Minimal Core; Composable; Agent-Friendly; Prefer Simplicity.
- **Roadmap:** [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) 鈥?Primary Animation鈫?D鈫扷I; parallel Infra/Render/DX; Future Gameplay Plugins/Net.
- **Agent hooks:** `.cursor/rules/engine-design-philosophy.mdc` (always); trust-tiers / bootstrap / mentor skill / BOOTSTRAP_DIGEST / ACTIVE_WORK / PROJECT_CONTEXT updated.
- **Next:** 缁存姢鑰呯‘璁?ACTIVE_WORK 鐒︾偣锛堟帹鑽?`ANIM-F01` Design锛涘閫?ED-F02 鐭啿鍒猴級銆備笉寮€鐮佺洿鑷崇‘璁ゃ€?
### 2026-09-03 - CORE-F05 MVP Done (docs closeout)
- **Registry / Design / Impl:** Status 鈫?**Done**锛圡VP锛夛紱S00鈥揝02 琛ㄧ姸鎬佸榻愪唬鐮併€?- **Deferred:** S05 Pause/Step銆?- **Debt:** TD-028/029锛圔inary/JSON锛夛紱**TD-030** EnterPlay rollback銆?- **Next:** 缁存姢鑰呴€夊畾涓嬩竴闃舵锛堝€欓€?ED-F02锛夈€?
### 2026-09-03 - CORE-F05 S06 Done: Inspecting Context
- **Editor:** `Get/SetInspectingScene`锛汸lay鈫扨IE / Stop鈫扙ditor锛汬ierarchy/Inspector/Command 璺?Inspecting銆?- **Safety:** Play `set` 鍐?PIE锛涗笉 Dirty锛涗笉鍏?Editor Undo锛沝ock 鏍囬涓嶅彉锛堢姸鎬佽鎻愮ず锛夈€?- **Verified:** 缁存姢鑰呯洰瑙嗛€氳繃銆?
### 2026-09-03 - CORE-F05 S06: Inspecting Context (code)
- **Rename:** Observing 鈫?**Inspecting**锛堝榻?Inspector锛夈€?- **Editor:** `Get/SetInspectingScene`锛汦nterPlay鈫扨IE锛孲top鈫扙ditor锛涙竻閫変腑銆?- **SceneEditor:** `GetActiveScene`=Inspecting锛沗GetDocumentScene` 缁?Save锛汸lay 涓?Dirty銆?- **Command:** Console `ActiveScene`=Inspecting锛汸lay `set` 鐩村啓銆佹棤 Undo銆?- **UI:** Hierarchy/Inspector 鏍囬 `Inspecting: PIE`銆?- **Build:** Editor OK銆?*Next:** 鐩楠屾敹銆?
### 2026-09-03 - CORE-F05 S03 Done: hide gizmo + editor input gate during Play
- **Gizmo:** Play 鏃朵笉缁樺埗 ImGuizmo銆?- **Input:** `EditorInputHub` PIE 浠呭叏灞€鍛戒护锛圫top/F5锛夈€?- **Deferred:** `IViewContextProvider`锛?*S06 Observing Context**锛圛nspector/Console 浠嶇粦 Editor 鈥?闇€姹傞敊浣嶏紝鍙﹀垏鐗囷級銆?- **Verified:** 缁存姢鑰呯洰瑙嗛€氳繃銆?
### 2026-09-03 - BUG-EDITOR-002: ASan 鏈鐜帮紱淇濇寔 Open锛堝彲涓存椂鍏?DebugDraw锛?- **浜屽垎锛?* A锛堝叧 atlas锛変粛宕╋紱B锛堝叧 Physics debug 鍏ラ槦锛?0脳0 宕?鈫?瑙﹀彂璺緞纭銆?- **S05锛?* 鍏ラ槦绉昏嚦 `ForwardRenderer::Execute` 鍚庢爤姝ｇ‘锛?*GCC Debug 浠嶅穿**銆?- **ASan锛?* LLVM-MinGW `bin-asan` 鍙窇锛涘娆″喎鍚姩杩?atlas **鏃?AV / 鏃?ASan 鎶ュ憡**锛堢枒甯冨眬鎺╃洊锛夈€?- **Docs锛?* [BUG-EDITOR-002](./bugs/BUG-EDITOR-002.md) Status=Open锛圔locked锛夛紱璁板綍涓存椂缁曡繃 = 鍏?`EnableDebugDraw`銆?- **Workaround 鍚堝叆锛?* `SceneEditingViewportClient` 榛樿涓嶇疆 `EnableDebugDraw`锛堟敞閲婃寚 BUG-EDITOR-002锛夈€?- **Next锛?* 鏍瑰洜浠嶅緟 GDB watchpoint / 鍏跺畠宸ュ叿锛涘彲缁х画鎺ㄨ繘 CORE-F05銆?
### 2026-09-03 - CORE-F05 S03 WIP: viewport binds PIE during Play (`master`)
- **Editor:** Play 鏃?viewport 瑙傚療 PIE `RenderScene`锛涗富鐩告満璺熼殢锛涚鐢?fly/gizmo/閫夋嫨锛沗RouteViewportInput` 涓嶈矾鐢便€?- **Build:** `Editor` compile OK锛涚洰瑙?deferred銆?- **Next:** commit锛堝缓璁?S04 / S03 鍒嗕袱绗旓級锛汻untime `InputSystem` 璺敱鍚庣画銆?
### 2026-09-03 - CORE-F05 S04: Per-World Audio/Physics lifecycle (`master`)
- **Audio:** `OnBeginPIE`/`OnEndPIE` 鈥?editor scene mute銆乴istener suspend/restore锛沗ShouldAcceptAudioFromScene` 闂ㄦ帶 Play/PlayOnAwake/RegisterListener銆?- **Physics:** `OnBeginPIE`/`OnEndPIE` hooks锛沗PlayInEditorSession` 缁熶竴璋冪敤銆?- **Render debug:** `SceneDrawDesc::GameplayScene`锛沗ForwardRenderer` 鍐?`PhysicsDebugDraw`锛堜笌 scene draw 鍚岃矾寰勶級锛涚Щ闄?Editor 甯уご `ClearFrameQueues`銆?- **Tests:** `audio-smoke` 鏂板 PIE playback gating 鈥?PASS銆?- **Manual:** Editor Play 鐩 deferred銆?- **Next:** commit S04 + S03锛堝缓璁垎涓ょ瑪锛夛紱Runtime Input 璺敱鍚庣画銆?
### 2026-09-02 - Merge `feat/editor` 鈫?`master`锛圗D-F02 鍩虹 + CORE-F07 + ED-F04 Console MVP锛?- **Merged:** ED-F02 asset workflow锛汣ORE-F07 reflection display names锛汦D-F04 Debug Console S00鈥揝10a锛圧egistry 鑷?feat/editor 鐨?ED-F03 閲嶇紪鍙凤級锛汣ommand system + validation + undo/redo銆?- **Conflict notes:** ED-F03 淇濈暀 Viewport Play Toolbar锛坢aster锛夛紱Console 鍗囦负 **ED-F04**锛汿D-027 鐧昏缂╃暐鍥?Scene3D 鍊哄姟锛堝師 feat/editor TD-026锛夈€?- **Next:** CORE-F05 S04 Per-World 绯荤粺鐢熷懡鍛ㄦ湡锛涗慨澶?EnterPlay 鍙嶅簭鍒楀寲澶辫触銆?
### 2026-09-02 - BUG-EDITOR-002 Resolved: Physics DebugDraw lifecycle + viewport RT (`df1ccc0`)
- **Root cause:** `PhysicsDebugDraw::SubmitScene` enqueued before skipped `SubmitSceneDraw` 鈫?`DebugDrawService` queue never cleared 鈫?AV in `EnqueueBox` (`libminEngined.dll+0x33CD4`).
- **Fix S04:** Physics debug涓?scene draw 鍚屾潯浠讹紱`Editor::Run` 甯уご `ClearFrameQueues()`锛沗Engine::Initialize` eager-init `DebugDrawService`.
- **Fix S04b:** 鎾ら攢 S03 瀵瑰凡 publish color RT 鐨勫畧鍗紙`ForwardRenderer`/`ManualRenderer`/`EndFrame`锛夆€斺€旇瀹堝崼瀵艰嚧 viewport 姘镐箙 "Scene color texture is not ready".
- **Verified:** 鐢ㄦ埛澶氭鍐峰惎鍔ㄦ棤宕╂簝锛泇iewport RT 鎭㈠鏄剧ず銆?- **Docs:** [BUG-EDITOR-002](./bugs/BUG-EDITOR-002.md) 鈫?Resolved.

### 2026-09-02 - CORE-F05 S00鈥揝03 + ED-F03 Viewport Play Toolbar (`master`)
- **Runtime:** `SceneDuplicator` / `SceneCloneContext`锛沗SceneManager` 鍙?Scene API锛沗SceneComponent::m_AttachParent` GUID 搴忓垪鍖栵紱legacy `.mescene` `skipUnknownField`锛沗scene-clone` test銆?- **Editor:** `PlayInEditorSession`銆乣IPlayModeService`銆乣ActiveSceneScope`锛沗ToolbarModule`锛團5/Shift+F5锛夛紱`EditorChrome`锛圡ainMenu only锛夛紱`ViewportPlayToolbar`锛圱ab 涓?Toolbar 琛?+ Separator + 30px Icon锛夛紱绉婚櫎 `ToolbarWindow` / `DraggableOverlay`銆?- **Docs:** `ED-F03_EDITOR_TOOLBAR_DESIGN.md`锛沗CORE-F05` design/impl 鏇存柊锛沗docs/external/minEngine Play Mode Development Guideline.md` 绾冲叆浠撳簱銆?- **Verified:** Editor build锛沗minEngineTests.exe test scene-clone` PASS銆?- **Known / Next:** EnterPlay 鍋跺彂 `SceneDuplicator` 鍙嶅簭鍒楀寲澶辫触 鈫?S04+锛泘~鍚姩鍋跺彂宕╂簝~~ 鈫?BUG-EDITOR-002 Resolved銆?
### 2026-09-02 - ED-F04: MVP slice closure锛堥潪 Feature Done锛涘師 feat/editor ED-F03锛?- **Status:** Registry **ED-F04** In Progress锛汼00鈥揝10a **Done**锛?*S10b** `activate`/`deactivate` 鈫?**Deferred**锛汼07 ExportSchema 浠?Deferred銆?- **Commits:** `a420040`锛坲ndo/`@` paths锛夈€乣06aabce`锛坴alidation + `rename`锛夈€?- **Tests:** `command-system` 鈥?25 cases, 120 assertions PASS銆?
### 2026-09-02 - CORE-F07: reflection display names (Done on `feat/editor`)
- **Runtime:** `ReflectionDisplayNames` 鈥?strip `m_`/`x_`/`b_` prefix + camelCase word breaks; `GetPropertyDisplayName`.
- **Editor:** `PropertyEditPolicy::GetDisplayName` delegates to runtime API.
- **Tests:** `minEngineTests.exe test reflection-display-names` 鈥?2 cases, 15 assertions PASSED.

### 2026-09-02 - Backlog shift: CORE-F05 focus; PHYS-F04 / BUG-PHYS-003 closed (`master`)
- **ACTIVE_WORK:** CORE-F05 Play Mode 鎶负褰撳墠鐒︾偣锛汸HYS-F04銆丅UG-PHYS-003 鏍?Done/Fixed銆?- **Registry:** CORE-F05 In Progress锛汣ORE-F07 Done锛堝悎鍏ュ悗锛夈€?
### 2026-09-02 - BUG-PHYS-004: collider disable/remove refreshes physics body (`master`)
- **Fix:** `ColliderComponent` activation/destructor 鈫?`RefreshOwningRigidBody`; `FindColliderComponent` / rebuild / override filter `IsActive()`; Editor `m_bActive` side-effect.
- **Verified:** physics-smoke, physics-shapes; Editor test.mescene manual.

### 2026-09-02 - CORE-F06 Component Activate Done (`master`)
- **Runtime:** `m_bActive`, `SetActive`/`IsActive`, activation state machine, `SyncActivationWithActiveFlag`, system hooks (tick/physics/audio/render/Lua).
- **Editor:** Inspector Active checkbox + undo; ImGui style var fix; scene load reconcile (TD-026 workaround).
- **Assets:** `test.mescene` / `default.mescene` 鈫?`m_bActive`; SkyBox `m_Enabled` removed.
- **Docs:** Design/Impl Done; TD-026 Open; BUG-PHYS-004 registered (collider shape not unregistered on disable/remove).
- **Verified:** Editor build; physics-smoke; manual Active toggle on loaded scene.

### 2026-09-02 - CORE-F06: scene load activation reconcile + TD-026
- **Fix:** `ResolvePendingActivationsForScene` 鈫?`SyncActivationWithActiveFlag` per component锛堝弽搴忓垪鍖?`m_Owner` 鏈蛋 `SetOwner` 瀵艰嚧 `m_bActivationApplied` 鍋囬槾鎬т笌棣?Deactivate 鏃犳晥锛夈€?- **TD-026:** Open 鈥?鏈潵 pending ref resolve 瀵?`m_Owner` 璧板弽灏?Setter / `SetOwner` 鏍规不銆?
### 2026-09-01 - BUG-RENDER-014: point light radius, attenuation, shadow cutoff (pending commit)
- **Shaders:** `PointLightAttenuation` / `PointLightShadowFactor`; Phong 绉婚櫎 per-point-light ambient锛汸BR 鐐瑰厜琛板噺 + 闃村奖 mask銆?- **Scene:** `test.mescene` 琛ョ偣鍏?鑱氬厜琛板噺瀛楁銆?- **Docs:** Design `BUG-RENDER-014_POINT_LIGHT_RADIUS_ATTENUATION_DESIGN.md`; Bug record updated銆?- **Verified:** `shader-compiler`, `physics-shapes` / `physics-smoke` / `physics-sync`锛汦ditor 鐩寰呯‘璁ゃ€?
### 2026-09-01 - Merge `feat/launcher` 鈫?`master`锛圠AUN-F01锛?- **Merged:** `Launcher/` Rust workspace 鈥?CLI (`minlauncher`) + Tauri GUI (`minlauncher-app`).
- **Verified (branch):** `cargo test -p minlauncher-core` 5/5; manual `cargo tauri dev`.

### 2026-09-01 - Merge `feat/audio` 鈫?`master`锛圓UD-F01 MVP锛?- **Merged:** `IAudioBackend` + miniaudio銆乣AudioSystem`/`AudioComponent`/`AudioListenerComponent`銆乣AudioClip` asset銆?D spatial sync銆?- **SceneComponent:** world matrix + attach/detach `KeepWorldTransform`锛堜互 audio 鍒嗘敮涓哄噯锛夈€?- **Tests:** `audio-smoke` + existing `shader-compiler` suites both registered.

### 2026-09-01 - Merge `feat/debug-drawing` 鈫?`master`锛堝惈 `feat/render` 鍏ㄩ噺锛?- **Merged:** ED-F01 S01鈥揝07銆丷ND-F05/F12鈥揊14銆丷ND-F11 DebugDrawing MVP銆乂K shadow playbook 绛?27 commits銆?- **Docs:** 鍚堝苟 ACTIVE_WORK / REGISTRY / PROGRESS_LOG锛涗繚鐣欏杞?worktree 鐧昏銆?
### 2026-09-01 - AUD-F01 Audio System MVP Done (`feat/audio`)
- **Runtime:** `IAudioBackend` + `MiniaudioBackend`, `AudioClip`/`AudioVoice`/`AudioSystem`, `AudioComponent`/`AudioListenerComponent`, Bus mixer, 2D/3D spatial audio, lifecycle on scene unload.
- **Fixes (post-initial MVP):** `SceneComponent` world transform + `AttachToComponent`/`DetachFromParent`; 3D sync via `GetWorldPosition()`; disable miniaudio default listener; listener enable/sync + tick order; attenuation model wired to backend.
- **Tests:** `minEngineTests.exe test audio-smoke` PASSED.
- **Manual:** Editor ear-test on `test.mescene` 鈥?spatialized audio audible with listener on camera.
- **Deferred (AUD-F02+):** gentler default attenuation / `AttenuationModel` in Inspector, custom curves, Inspector live volume/pitch.

### 2026-08-31 - LAUN-F01-S05: Tauri 2 + React GUI (`feat/launcher`)
- Added `crates/minlauncher-app` (Tauri 2) + `ui/` (React + Vite + TS, industrial dark theme).
- Tauri commands wrap `minlauncher-core`; shared `settings.json` with CLI.
- Core: `templates` module, `EditorStatus`, `clear_all_recent`.
- **Verified:** `cargo test -p minlauncher-core` 5/5; `cargo build -p minlauncher-app`.
- **Manual:** `cargo tauri dev` 鈫?Projects / Settings / New Project.

### 2026-08-31 - LAUN-F01 CLI: Rust minlauncher S01鈥揝04 (`feat/launcher`)
- Added `Launcher/` Cargo workspace: `minlauncher-core` + `minlauncher` CLI (clap).
- Commands: `open`, `create`, `recent`, `config`; spawn `Editor.exe --project <path>`.
- Empty template under `Launcher/Templates/Empty/`; settings at `%APPDATA%/minEngine/Launcher/`.
- Design + Implementation plan finalized (Tauri 2 for S05); Registry **In Progress** 鈫?CLI slice **Done**.
- **Verified:** `cargo test -p minlauncher-core` (5/5); manual `open` MyMEProject + `create` LaunSmokeTest.

### 2026-08-31 - Multi-track backlog: LAUN / AUD / ANIM / UI / PHYS thaw (`master`)
- Registered **LAUN-F01**, **AUD-F01**, **UI-F01**, **ANIM-F01**, **PHYS-F04**; **PHYS-F03** Deferred 鈫?Planned.
- Branches from `master`: `feat/launcher`, `feat/audio`, `feat/ui-anim` (`feat/physics` 宸插瓨鍦?.
- Worktrees: `D:/Dev/GitRepo/minEngine-launcher`, `minEngine-audio`; `MyMEProject` ProjectRoot per worktree.
- **Next:** merge feature branches after render track lands.


### 2026-09-01 - RND-F11 MVP 鏀跺熬锛氳寖鍥存敹绐?+ wireframe z-fighting (`feat/debug-drawing`)
- **Scope:** MVP 瀹氫负 S01鈥揝02 only锛汼03 contact/trace銆丼04 toggle **绉诲嚭**鏈?Feature锛圖esign/Impl/Registry 宸叉洿鏂帮紝Status 鈫?Done锛夈€?- **Code:** 鍥為€€ S03 瀹為獙锛沗PhysicsDebugDraw` 浠?collider + `SubmitScene(scene, options)`锛沗DebugDraw.vert` 淇濈暀 3mm view bias銆?- **Principle:** Debug 涓哄簳灞傛湇鍔★紱涓嶇紪鎺?Physics/Editor 濡備綍璋冪敤锛汸ersistent + toggle 鈫?鍚庣画鏂?Feature銆?- **Next:** commit + merge `feat/debug-drawing`锛涙柊 Feature 璁ㄨ Persistent / toggle銆?
### 2026-09-01 - RND-F11-S02: Physics collider wireframe (`feat/debug-drawing`, commit 8c3ac07)
- **Delivered:** `PhysicsDebugDraw::SubmitScene`; `DebugDraw::Sphere` / `Capsule`; sphere/capsule wireframe tessellation; Editor submits colliders before `SubmitSceneDraw` (S01 axis smoke removed).
- **Verified:** User visual acceptance (wireframe visible); `physics-smoke` / `physics-shapes` / `render-graph` pass.
- **Known issue:** [BUG-PHYS-003](./bugs/BUG-PHYS-003.md) 鈥?intermittent crash on Add `BoxColliderComponent` (not reproduced after retry).
- **Next:** S03 contact + LineTrace visualization.

### 2026-09-01 - RND-F11-S01: DebugDraw pass + editor axis smoke (`feat/debug-drawing`)
- **Delivered:** `DebugDrawService` / `DebugDraw::` API; `DebugDrawPass` + shaders; `Scene.Debug` RDG slot; `EnableDebugDraw` flag.
- **Editor:** `SceneEditingViewportClient` submits RGB axis lines before `SubmitSceneDraw` (smoke until S02).
- **Fix:** OpenGL `RHICmdDraw` honors PSO `LineList` (was hardcoded `GL_TRIANGLES`).
- **Docs:** Full design spec + implementation plan; registry/active work updated.
- **Verified:** `cmake --build` minEngine+Editor; Editor GL+VK axis visual acceptance; `test render-graph` pass.
- **Next:** S02 `PhysicsDebugDraw` collider wireframe.

### 2026-08-31 - RND-F11 DebugDrawing branch kickoff (`feat/debug-drawing`)
- **Branch:** `feat/debug-drawing` from `feat/render` after shadow quality handoff commit.
- **Registry:** `RND-F11` 鈫?**In Progress**; placeholder [Design](./Render/RND-F11_DEBUG_DRAWING_DESIGN.md).
- **Goal:** Editor viewport debug primitives for Physics collider/contact/trace visualization.
- **Parallel:** VK shadow quality remains on `feat/render` (RenderDoc / cull-winding).
- **Next agent:** Expand Design 鈫?Implementation Plan 鈫?S01 lines+boxes MVP.

### 2026-08-31 - VK shadow self-shadow handoff (`feat/render`)
- **Symptom refined:** Dir **and** Spot show receiver self-shadow / false shadows on VK; **Point** not observed; **different objects** per light type 鈫?winding/orientation, not global bias off.
- **E3 recap:** `MAX_CASCADES=1` + force cascade 0 鈥?unchanged 鈫?CSM **index** ruled out; camera coupling persists (dir matrix still camera-frustum-derived).
- **Depth bias audit (read-only):** Two layers 鈥?(A) raster via `RHIClipSpaceCapabilities` 鈫?ShadowPass PSO (`glPolygonOffset` / VK PSO `depthBiasEnable`); (B) shader receiver bias in `MaterialSceneShadows.glslinc`. VK raster bias **should be active** for Dir/Spot; Point bypasses via `gl_FragDepth`. Raising VK slope/constant inconclusive 鈫?prioritize write-path cull/winding.
- **Docs:** `sessions/2026-08-31-vk-shadow-self-shadow-handoff.md`; playbook `VK_SHADOW_DEBUGGING.md` 搂4.5鈥?.6, 搂7; `ACTIVE_WORK.md` updated.
- **Next agent:** Debug 5/6; RenderDoc; scheme B or frontFace A/B; restore TEMP limits/shader defines before production fix.

### 2026-08-31 - VK dir self-shadow isolation: single cascade (`feat/render`)
- **Experiment:** `MAX_CASCADES=1` + `DIR_SHADOW_FORCE_CASCADE=0` + Front cull restored (Back reverted); P1 (`gl_FragDepth` omit Dir/Spot) still in tree.
- **User result:** Symptoms **unchanged** vs multi-cascade 鈥?ground self-shadow acne; cube/sphere false shadows still **camera-coupled**; PCF soft edges visible.
- **Conclusion:** **Not** multi-cascade index / cascade-boundary mixing (rules out P5 as primary). Issue is **directional-light path** specific (Spot/Point not implicated in this round).
- **Interpretation:** Single-cascade CSM still builds ortho frustum from **camera view frustum** + texel snap 鈥?camera coupling can persist without cascade *selection*. Combined with prior銆屽叧 Cast Shadow 鈫?map 娑堝け銆嶁啋 receiver still **writes** into dir shadow map on VK (cull/winding class), not read-only PCF artifact.
- **Next (analysis / no code yet):** Debug 5/6 binary; RenderDoc face/winding; scheme B (shadow viewport flip + `GetEffectiveCullMode`) or `VK_FRONT_FACE_CLOCKWISE` A/B. Restore `MAX_CASCADES=4` / `DIR_SHADOW_FORCE_CASCADE=-1` before production fix lands.
- **Doc:** `playbooks/Render/VK_SHADOW_DEBUGGING.md` 搂4.4.

### 2026-08-31 - Playbooks + VK shadow cull audit (`feat/render`)
- Added `docs/ai/playbooks/` (README + `Render/VK_SHADOW_DEBUGGING.md`) for reusable bug patterns.
- Face cull audit: ShadowPass **already** sets Front cull on VK (`RHIClipSpaceCapabilities` 鈫?`VK_CULL_MODE_FRONT_BIT`). Next round: RenderDoc PSO verify, frontFace/winding A/B, VK depth bias constant (0 vs GL 4).

### 2026-08-31 - RND-F14 Phase A: ShadowPass UBO lifetime fix (`feat/render`)
- **Root cause:** ShadowPass overwrote shared host-visible ViewProj/Params UBO at offset 0 per draw; Vulkan deferred execution 鈫?all shadow draws read last-written matrix.
- **Fix:** `ShadowUniformBuffers` 鈥?Dir/Spot fixed slots, Point ViewProj ring, Params ring; per-command `BufferOffset` in `ShadowDrawCommand`; removed single-mat4 path.
- **Diagnostic:** `ManualRenderer` (`--renderer manual`) helped isolate non-RDG root cause (RND-F13 Done).
- **Verified:** User VK full-map 鈥?Dir / Spot / Point shadows each work independently (2026-08-31).
- **Closed:** BUG-RENDER-013, BUG-RENDER-010, BUG-RENDER-011; TD-025 Done.
- **Next:** VK receiver self-shadow acne 鈥?verify ShadowPass Front face cull (user observation).

### 2026-08-31 - BUG-RENDER-013 reframed: Manual == RDG wrong shadows (`feat/render`)
- Full-map VK: ManualRenderer shows **same** wrong shadows as Forward+RDG 鈫?**RDG not primary**.
- Work shifts to ManualRenderer diagnosis: shadow PSO/attachments, VK depth array/cube create/update, set1/UBO.
- RND-F12 demoted to hygiene; fix on Manual first, then regress Forward.

### 2026-08-30 - RND-F13: dir-only isolation + full map restore (`feat/render`)
- User confirmed ManualRenderer; viewport fix (manual sky clear pass).
- Dir-only + single cascade: Manual 鈮?Forward, faint shadow 鈫?not RDG-only in isolation (BUG-013 note).
- Restored: `MAX_*_SHADOW_MAPS=2`, `MAX_CASCADES=4`, `DIR_SHADOW_FORCE_CASCADE=-1` (C++ + shader fallbacks).
- **Next:** VK `--renderer forward` vs `manual` on full map / `test` scene.

### 2026-08-30 - RND-F13-S01: ManualRenderer (`feat/render`)
- Renamed from HandPassProbe 鈫?**ManualRenderer** per maintainer.
- `ManualRenderer` subclasses `ForwardRenderer`; manual Shadow鈫払ase鈫扨resent; no RenderGraph.
- CLI: `--renderer manual` (`handpass` alias); default Forward unchanged.
- Build: minEngine + Editor OK. **Next:** S02 GL/VK parity run on `test` scene 鈫?BUG-013 note.

### 2026-08-30 - RND-F13: design draft (`feat/render`)
- Diagnostic renderer proposal: manual Shadow鈫払ase鈫扨resent, no RDG; GL/VK parity to isolate BUG-013.
- BUG-RENDER-010: user confirmed single-cascade 鈫?one shadow (multi-copy = CSM).
- Pending: design approval 鈫?Implementation Plan.

### 2026-08-30 - RND-F12-S06 + isolation experiment committed (`feat/render`)
- `3fed4ef`: set1 physical lifecycle; dir-only + single-cascade experiment toggles; set1 OOB fix when MAX maps=0.
- `ShadowResourceHandle::RdgPhysicalIndex`; `BindGraphShadowTextures` clears stale texture refs then binds physical.
- `EngineSceneBindingSets`: dirty on ptr + physical index + texture desc; invalidate when `SetupAttachments` recreates.
- User re-tested BUG-013: still open after S01鈥揝03; S06 pending VK verify.

### 2026-08-30 - RND-F12-S03: pass input barriers (`feat/render`)
- `RenderGraph::InsertPassInputBarriers` 鈥?before each pass `RunBuildRenderPass`, transition texture/depth/color-alias inputs via `RHICmdTransition` (shader-read layout on VK).
- S03 partial: `RDGTextureAccess` metadata deferred; barrier hook landed.
- Next: user VK visual verify BUG-013; then **S06** binding lifecycle.

### 2026-08-30 - RND-F12-S01/S02: read edge + per-frame Bake (`feat/render`)
- S01: `AddSceneLitShadowTextureInputs` on Base/Translucent; remove shadow `ForceIncludePass`; `pass_dependencies` + missing-writer throw; render-graph tests (6/6).
- S02: delete `BuildShadowResourceFingerprint` / pending invalidate; `Bake()` every frame in `SetupFrameRenderGraph`.
- Next: **S03** VK pass barriers (`RHICmdTransition`).

### 2026-08-30 - RND-F12: Granite RDG full semantic parity design (`feat/render`)
- North star: replicate Granite RenderGraph **semantics** (not copy code); Phase A鈥揇.
- Design 搂3 Adapter boundary; 搂4 full parity checklist; 搂6 delete non-Granite patches; Bake policy = per-frame until proven safe.
- Impl: S01鈥揝07 Phase A (BLOCK 013); S04鈥揝15 Phase B鈥揇.

### 2026-08-30 - RND-F12: Granite RDG bake semantics (`feat/render`)
- Reframe BUG-RENDER-013: not shadow-only fix 鈥?incomplete F07 bake vs Granite (`read edge`, `barrier`, `invalidate`).
- Docs: recover `RND-F07` Design UTF-8; add `RND-F12` Design + Impl; Registry / ACTIVE_WORK / BUG-013 links.
- Next: **RND-F12-S01** 鈥?Scene pass `AddTextureInput` shadow atlases; remove shadow `ForceIncludePass`.

### 2026-08-30 - BUG-RENDER-013: partial commit + RDG gap analysis (`feat/render`)
- User: pass-filter / split enqueue is patchwork; fix must be proper RDG.
- Reverted: `RenderGraph::EnqueueRenderPasses(filter)`, `ForwardRenderer` shadow鈫抯et1鈫抯cene order.
- Kept: `VulkanRHIResources` depth shadow SRV `DEPTH_STENCIL_READ_ONLY_OPTIMAL`.
- Next: Granite-aligned RDG read edges + bake invalidate for shadow atlases.

### 2026-08-30 - BUG-RENDER-013 reframe: RDG not VK binding (`feat/render`)
- User: S2鈥揝4 ineffective; pivot to Granite RDG reference; TD-025 convention largely ruled out (dir-only OK).
- Docs: BUG-013 status 鈫?Open, root cause class = RDG scheduling/lifetime; BUG-010 + TD-025 搂8 P6/P7 updated.
- Workspace: S1-only (pass filter enqueue, set1 after shadow, VK depth SRV layout) 鈥?documented as thin baseline, not root fix.
- Next: Compare minEngine `RenderGraph` bake/deps to Granite `render_graph.cpp`; shadow atlas read edges + fingerprint invalidate.

### 2026-08-30 - BUG-RENDER-013 rollback to S1-only (`feat/render`)
- User: S2鈥揝4 fixes ineffective; suspect RDG/fingerprint path; request revert to S1 baseline for fresh investigation.
- Reverted: dynamic fingerprint, RDG shadow texture inputs, transition/retain, shadow ring split, set0 offset cache, limits co-location, ShaderCompiler macro inject, pool 8192.
- Kept (S1): shadow鈫抯et1鈫抯cene enqueue order; `RenderGraph::EnqueueRenderPasses(filter)`; VK depth SRV `DEPTH_STENCIL_READ_ONLY_OPTIMAL`.
- Next: Re-diagnose from clean S1 state; consider RDG architecture review before more binding patches.

### 2026-08-30 - BUG-RENDER-013 S4 binding fixes (`feat/render`)
- User: Dir shadow intermittent with point off; point position couples to Dir when on; latched state after point off.
- S4: separate `m_ShadowPerObjectUniformBuffer`; set0 descriptor cache validates ring byte offset; force set1 rebuild after shadow passes.
- Next: User VK retest on `test` 鈥?Dir stable with point Cast Shadow on/off; move point should not move Dir shadow.

### 2026-08-30 - BUG-RENDER-013 confirmed via dir-only isolation (`feat/render`)
- Result: With `MAX_*_SHADOW_MAPS=0`, user reports **VK directional shadow visually correct**.
- Conclusion: Full-scene failure was **binding/pipeline state pollution** when point/spot shadow passes participate 鈥?not Dir light-space math as primary cause.
- Next: Implement fix slices S1鈥揝3 (set1 after shadow writes, VK depth layout, RDG read deps); restore shadow map budget=2; regress multi-light.

### 2026-08-30 - Dir-only shadow isolation + limits co-location (`feat/render`)
- Goal: BUG-RENDER-013 isolation 鈥?shut down point/spot shadow passes at engine budget; co-locate light vs shadow-map limits.
- Main changes: `EngineRenderLimits.h` owns `MAX_*_LIGHTS` / `MAX_*_SHADOW_MAPS` / sampler slots + `static_assert`; experiment `MAX_*_SHADOW_MAPS=0`; ShaderCompiler injects macros; set1 arrays sized by sampler slots.
- Next: User VK visual verify 鈥?Dir alone with point Cast Shadow on/off should no longer allocate point maps; if Dir still missing when point off, prioritize descriptor layout / set1 dirty (P0/P1).

### 2026-08-30 - BUG-RENDER-013 RDG/VK shadow binding investigation (`feat/render`)
- Goal: Explain VK Dir shadow visibility coupling to Point Cast Shadow; separate pollution vs cascade math.
- Findings (code review): VK depth descriptor layout mismatch; `BuildSceneSet1` dirty only on texture cache change; `BasePass` missing `DirShadowAtlas` RDG input; static shadow fingerprint. Documented in BUG-RENDER-013 + RND-TD025 搂8 P7 + shadow pass isolation matrix.
- Next: User experiments via `MAX_*_SHADOW_MAPS` / `MAX_CASCADES` or scene Cast Shadow toggles; then fix P0鈥揚3.

Last updated: 2026-09-10（merge wave: +ui 完成）


## Purpose

This file is an AI-oriented progress digest converted from commit messages.
It is not a full changelog. It focuses on architecture moves, rendering milestones, and known pitfalls.

## Timeline Summary

### 2026-08-28 - TD-025 RHI clip-space capabilities + VK shadow fix (`feat/render`)
- Goal: Unify clip/viewport/cull policy; fix BUG-RENDER-010 (VK shadows) and BUG-RENDER-011 (disable point/spot shadow crash).
- Main changes:
  `RHIClipSpaceCapabilities`, `RHIClipSpace`, `RHIViewportConvention`; Shadow scheme A (no flipY + Front cull).
  ShadowPass / ForwardRenderer / RenderCamera / EnvMapCapture / ShaderCompiler / Editor ImGui UV migrated.
  `EngineSceneBindingSets` clears unused spot/point shadow SRV slots; dir shadow index gated in shader.
  Docs: [RND-TD025 design](./Render/RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md), BUG-RENDER-010/011 updated.
- Validation: cmake build minEngine + Editor (pending user VK visual verify on `test` scene).

## Timeline Summary

1. Initial framework stage
- Set up basic engine framework and RenderSystem foundations.
- Built simple mesh/texture support and early input handling.

2. Scene and component architecture stage
- Introduced GameObject/Component model, Level/WorldManager, Transform.
- Added render-side abstraction using SceneProxy.
- Started AssetManager and resource-oriented structure.

3. Lighting and model import stage
- Added light components and light scene proxies.
- Extended Phong shader for directional/point/spot lights.
- Added basic model import via Assimp.

4. Rendering pipeline structuring stage
- Reorganized render files (RHI, light proxies, primitive proxies).
- Added translucency pass.
- Added present pass for offscreen-to-screen output.

5. Data upload optimization stage
- Added per-frame camera UniformBuffer.
- Added separate light UBO.
- Reduced redundant light data upload frequency.

6. Shadow and stability stage (recent)
- Added directional light shadow pass and shadow map sampling.
- Fixed repeated queue growth issue (missing clear each frame).
- Fixed Texture2D release issue causing GPU memory leak.
- Fixed light proxy write-back issue causing repeated creation/leak risk.

7. Reflection modernization stage (2026-04-10 ~ 2026-04-12)
- Introduced MEReflection in parallel with legacy reflection for migration.
- Refactored reflection/serialization boundaries and startup finalization flow.
- Added better reflection finalization error logging and repaired GameObject reflection path.

8. Serialization capability expansion stage (2026-04-12 ~ 2026-04-14)
- Implemented pointer deserialization foundation in Serializer.
- Added primitive codec registry and MEEnum codec support.
- Unblocked GameObject serialization for owned inline component arrays (shared_ptr<Component>).
- Reenabled Inspector path for GameObject and component fields.

9. Asset identity and registry kickoff stage (WIP, 2026-04-16)
- Started GUID type and generation utility for asset identity.
- Introduced AssetMeta and initial in-memory asset registry shape.
- Added AssetManager scan/register flow and Texture2DResource reflection stub.

## Current Technical Focus (Inferred)

- Complete reflection/serialization migration without breaking editor workflows.
- Finalize pointer/reference semantics for MEObject relationships.
- Build asset metadata persistence (.meta read/write) and GUID lifecycle stability.
- Keep scene serialization, inspector, and resource loading behavior aligned.

## Known Pitfalls to Recheck During Refactors

- Any per-frame vector/list/map used in pipeline build path must be cleared or reused safely.
- Any GL resource wrapper must own and release native handles in destructor.
- Scene proxies or render entries must not be recreated indefinitely without ownership policy.
- Shadow pass changes (viewport/state/targets) must be restored before later passes.
- Large flat shadow casters: do not expand CSM Z with bounding-sphere radius (see BUG-RENDER-004).
- Pointer deserialization must clearly separate ownership and reference semantics.
- Serializer signature changes must be synchronized across all callsites.
- Asset scanning must avoid duplicate registration and accidental GUID regeneration.

## Entry Template (Append for each meaningful task)

### 2026-08-30 - BUG-RENDER-010: rollback fixed ortho box; suspect GPU shader

- Reverted `kDirShadowUseFixedOrthoBox` + `kDirShadowForceCascade=0` 鈫?normal CSM path.
- Fixed ortho box: no Dir shadow on **both** GL and VK (box params invalid; experiment inconclusive for ortho depth).
- FORCE=0 prior result kept: multi mesh shadows 鈫?one; still GL鈮燰K 鈫?cascade mixing + shader read path.
- Next: GPU 鈥?`MinEngineShadowMapCoords`, Dir PCF, `sampler2DArray` layer. BUG-RENDER-013 (point shadow gate) still open.

### 2026-08-30 - BUG-RENDER-010/013: FORCE_CASCADE=0 + fixed ortho box experiment

- FORCE cascade 0: multiple Dir mesh shadows 鈫?one; GL vs VK (both FORCE=0) still mismatch 鈫?single-cascade Dir write/read still broken.
- Added `kDirShadowUseFixedOrthoBox` in `ForwardRenderer.cpp` (cascade 0 fixed light-space ortho 卤50, near/far 1/200) to isolate CSM frustum鈫扐ABB vs ortho depth.
- Filed **BUG-RENDER-013**: VK Dir shadow visibility depends on Point Cast Shadow (suspect set1 bindings); keep separate from matrix experiments.
- Keep `kDirShadowForceCascade=0` while running fixed-box visual on GL+VK.

### 2026-08-29 - BUG-RENDER-010: Dir shadow debug modes (P1)

- Added `DIR_SHADOW_DEBUG_MODE` via `TryDirShadowDebugVisual` in `MaterialSceneShadows.glslinc`; wired in Phong/PBR graph lighting.
- Toggle: `kDirShadowDebugMode` in `ShaderCompiler.cpp` (inject after `#version`). Default **1** = cascade colors.
- Modes: 1 cascade / 2 single-tap / 3 UV / 4 current Z / 5 sampled Z / 6 Z delta. See Gap Design 搂8.
- Also fixed inject skip: only skip if `#define MINENGINE_CLIP_DEPTH_ZERO_TO_ONE` already present (not bare token in `#if`).

### 2026-08-29 - BUG-RENDER-010: commit `3154700` + FlipY 鍏辫瘑淇

- **Commit:** ZO depth read (`MinEngineShadowMapCoords`) + `MinEngineShadowMapSlot` (BUG-RENDER-012); scheme A only.
- **Reverted:** scheme B shadow viewport flip, read `uv.y` flip, point `ShadowMap2D`, P4-A/P4-B experiments.
- **鍏辫瘑:** Shadow 鍐欌啋璇荤嫭绔嬮棴鐜紱Main Pass Scene flip **涓?*鍙備笌 shadow 閲囨牱銆俈K Spot ~OK.
- **Open:** Dir (CSM), Point (cube / 鍥涢噸楝煎奖). Docs: `RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md` 搂2/搂8.

### 2026-08-29 - BUG-RENDER-010: P0 read-side uv.y琛ュ伩 (B+C for manual ndc鈫抲v) 鈥?**reverted**
- VK `MinEngineShadowMapCoords`: `uv.y = 1.0 - uv.y` when ZO define set; keeps shadow viewport flip (B).
- Point cube path unchanged. Design 搂8 backlog recorded.
- Pending: VK visual Dir/Spot/multi-light.

- **Reverted:** 瑙佷笂鏉?`3154700` 鍏辫瘑锛涘嬁鍐嶆寜 Main Pass flip 鎺ㄥ璇讳晶琛ュ伩銆?
### 2026-08-29 - BUG-RENDER-012 + TD-025 Step 2B shadow viewport flip 鈥?**reverted**
- BUG-RENDER-012: `MinEngineShadowMapSlot` 鈥?gate `Params.w < 0` before shadow sample (all light types).
- TD-025 Step 2B: revert sample Y flip; VK `kVulkanShadowPass.ViewportFlipY=true` + Back cull; point faces use ShadowMap2D viewport convention.
- Pending: GL/VK visual on `test` scene.

### 2026-08-29 - BUG-RENDER-010: Step 2 shadow sample Y flip (option C)
- Goal: Close Gap 2 on read path only 鈥?VK `uv.y = 1.0 - uv.y` via `MINENGINE_SHADOW_MAP_SAMPLE_FLIP_Y`.
- Main changes: `MinEngineShadowMapCoords`, `InjectClipSpaceDefines` (ZO + flip pair).
- Shadow write viewport unchanged (scheme A).
- Pending: user GL/VK visual Dir/Spot on `test` scene.

### 2026-08-29 - BUG-RENDER-010: Step 1 ZO shadow depth read
- Goal: Close Gap 1 only 鈥?Vulkan Dir/Spot CurrentDepth uses ZO `ndc.z`, not N1 `*0.5+0.5`.
- Main changes:
  - `MinEngineShadowMapCoords` in `MaterialSceneShadows.glslinc` / `Phong.frag`.
  - `ShaderCompiler::InjectClipSpaceDefines` 鈥?ZO define only (no sample flip); restore pass-local OpenGL flat remap for set=0 ShadowPass.
- Verify: `Editor` + `minEngineTests` build OK; `test smoke` fails known MaterialIR UBO golden (`set=` vs flat) 鈥?unrelated.
- Pending: user GL/VK visual on `test` scene (Dir/Spot first).

### 2026-08-29 - BUG-RENDER-010: layered rollback baseline before convention close
- Goal: Freeze workspace as pre-fix baseline 鈥?keep TD-025 caps/matrix/viewport infra; roll back failed shader flip/`MinEngineShadowProject` inject stack; document GL鈫扸K shadow convention gaps.
- Main changes:
  - Shader sampling back to `projCoords * 0.5 + 0.5` (`MaterialSceneShadows`, `Phong.frag`); remove `InjectClipSpaceDefines` / sample flip inject from `ShaderCompiler`.
  - Retain `RHIClipSpaceCapabilities` / `RHIClipSpace` / ShadowPass convention + caps bias; dir shadow index gate.
  - Docs: reopen BUG-RENDER-010; gap design `RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md`; session handoff note.
- Verify: build not re-run in this commit; next: Step 1 ZO depth read only.
- Status: Vulkan shadows still Open / incorrect by design at this baseline.

### 2026-08-28 - BUG-RENDER-010: Vulkan directional shadow plane false self-shadow
- Goal: Fix VK Editor CSM shadow 鈥?large false shadow on 100脳100 plane (plane self-shadow via sampling); cube shadow OK at some angles.
- Root cause: ShadowPass Z remap + OpenGL `glm::ortho` light matrices vs lit-pass `*0.5+0.5` sampling mismatch; CSM frustum used OpenGL NDC corners with Vulkan `perspectiveRH_ZO` camera.
- Main changes:
  - `ForwardRenderer`: `orthoRH_ZO` / `perspectiveRH_ZO` for VK light proj; backend-aware CSM NDC near/far.
  - `ShadowPass.vert`: remove clip-Z remap (light matrices now ZO on VK).
  - `MaterialSceneShadows.glslinc`, `Phong.frag`: `MinEngineShadowProject` 鈥?ZO depth = `ndc.z`, GL = `ndc.z*0.5+0.5`.
  - Bug record: `docs/ai/bugs/BUG-RENDER-010.md`.
- Verify:
  - `cmake --build minEngine/build --target minEngine` 鈥?OK.
  - `Editor.exe --rhi vulkan` visual A/B on `test` scene 鈥?**pending user**.

### 2026-08-28 - ED-F01 S06: Vulkan Editor shadows + post flags
- Goal: Enable shadow/post pipeline on Vulkan Editor (match OpenGL draw flags); fix VK shadow path blockers.
- Main changes:
  - `SceneEditingViewportClient`: VK uses `EnableShadows | EnablePostProcess | EnableSkyBox` (no sky-only fork).
  - `VulkanRHITexture`: `Texture2DArray` create/view for `DirShadowAtlas` CSM atlas.
  - `VulkanRHI`: per-layer depth attachment views for CSM cascade slices.
  - `ShadowPass`: depth-only PSO desc; VK back-face cull; `ShadowPass.*` shaders use `set=0` bindings.
  - `ShaderCompiler`: pass-local OpenGL flat remap for ShadowPass; Vulkan `MINENGINE_CLIP_SPACE_ZO` inject.
- Verify:
  - `cmake --build minEngine/build --target Editor` 鈥?OK.
  - `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` 鈥?loads `test`, ShadowPass SPIR-V OK, no `DirShadowAtlas` / PSO bind errors in log (~12s smoke).
- Pending: user visual A/B 鈥?cube shadow on plane vs GL; then commit.

### 2026-08-26 - ED-F01 Vulkan visual bugfix: UBO ring + bake viewport + retired buffers
- Goal: Fix Cube invisible / plane Y-scale oddity / sky 卤Y split / mesh hot-swap DEVICE_LOST on Vulkan Editor.
- Done:
  - Per-Object UBO ring (aligned slots) + `RHIShaderBinding` BufferOffset/Range; scene + shadow draws bind distinct regions
  - EnvMap bake `SetViewport(..., flipY=false)` while scene path keeps Y-flip
  - `VulkanRHI` retires buffers and flushes after in-flight fences (BeginFrame / Shutdown)
  - Bug records `BUG-RENDER-005`鈥009`; design Status 鈫?In Progress
- Verify: Vulkan Editor smoke loads `test` + HDR bake, no DEVICE_LOST in stderr; **user visual A/B still required**
- Open: `BUG-RENDER-007` plane UV zoom 鈥?wait for post-UBO screenshots

### 2026-08-26 - ED-F01 S07 garbled HDR sky: float32鈫抙alf upload
- Goal: Fix psychedelic/moir茅 Vulkan sky after successful HDR bake (no DEVICE_LOST).
- Root cause: `CreateFromHdrPixels` passes float32 (stbi_loadf); OpenGL uploads with `GL_FLOAT`, but Vulkan treated `*16F` initialData as raw half bits.
- Main changes: `VulkanRHITexture` converts float32 RGB/RGBA 鈫?`R16G16B16A16_SFLOAT` on upload (2D + cube paths).
- Verify: rebuild Editor; user visual check for citrus HDR sky on `--rhi vulkan`.

### 2026-08-26 - ED-F01 S07 HDR bake DEVICE_LOST: descriptor lifetime
- Goal: Fix remaining Vulkan `DEVICE_LOST` after HDR sky bake (ImGui `VkResult=-4`).
- Root cause: EnvMapCapture loop destroyed per-face `RHIShaderBindingSet` (and reused one UBO) while the immediate CB still referenced them.
- Main changes:
  - `EnvMapCapture`: `EnvCapturePendingBindings` keeps per-face UBO + descriptor set until after `RHIEndImmediateCommands`.
  - Keep prior fixes: cube layout defer; submit before PSO destroy; depth `DEPTH_STENCIL_READ_ONLY`.
- Verify:
  - `--rhi vulkan`: HDR bake + ~10s loop + clean shutdown, no `DEVICE_LOST`.
  - `--rhi opengl`: HDR/IBL bake + clean shutdown.

### 2026-08-25 - ED-F01 S07 HDR sky bake on Vulkan (DEVICE_LOST fixed)
- Goal: Restore project HDR 鈫?cubemap bake for Vulkan sky (citrus orchard); remove temp debug scaffolding.
- Main changes:
  - `EnvironmentMap`: Vulkan no longer skips `EquirectToCubemap`; IBL irradiance/prefilter still aliased to environment cube.
  - `EnvMapCapture`: defer cube layout transition until all faces captured; submit immediate CB **before** bake PSO destroy.
  - `VulkanRHI`: cube-face `EndRenderPass` skips premature ShaderResource transition; depth 鈫?`DEPTH_STENCIL_READ_ONLY`.
  - Cleanup: removed first-frame VK debug logs; validation cube toned to mild blue.
- Verify:
  - `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` 鈥?HDR bake log, no `DEVICE_LOST`, clean shutdown.
  - `Editor.exe --rhi opengl` 鈥?full HDR/IBL bake + clean shutdown.
- Deferred: VK IBL convolution (irradiance/prefilter passes), S06 shadow/post in editor flags, `TD-025` capabilities API.
- Follow-up 2026-08-26: still saw DEVICE_LOST until descriptor/UBO lifetime fix (entry above).

### 2026-08-25 - ED-F01 S05 viewport parity + S07 SkyPass (Vulkan)
- Goal: Fix VK editor viewport (mesh/gizmo/nav); re-enable Sky with validation cube.
- Main changes:
  - `RenderCamera`: Vulkan `perspectiveRH_ZO` + pick NDC Z=0 (render/pick/gizmo unified).
  - `EditorViewportWindow`: Vulkan ImGui UV `(0,0)-(1,1)` (no double Y-flip with viewport).
  - `SceneEditingViewportClient`: `SyncSceneViewportCameraAspect`; VK flags `EnableSkyBox`.
  - `SkyBoxPass`: explicit no-cull PSO; sky UBO stage `All`; first-prepare log.
  - `EnvironmentMap`: brighter validation cube face colors for debug.
  - Docs: **TD-025** clip/handedness vs `IsVulkan()` coupling.
- Verify: `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` 鈥?plane + gizmo OK; sky TBD user.
- Deferred: HDR bake on VK (`EnvironmentMap`), IBL convolution, shadows/post (S06).

### 2026-08-17 - RND-F05-S07d visual smoke confirmed + deferred debt grouped
- Goal:
	Confirm S07d is not just alive but visibly drawing scene geometry, then record the remaining cleanup honestly before the next slice.
- Main changes:
	User visual check confirmed visible mesh output in Vulkan Editor smoke.
	Root cause for the prior blue-only frame was `SkyBoxPass` still entering the graph and clearing `SceneColor` after Opaque; smoke now gates the pass via `NeedRenderPass()`.
	Deferred follow-up grouped into two TDs: `TD-023` (scene pass ordering / clear contract) and `TD-024` (Vulkan frame sync + debug leftovers).
- Risks or caveats:
	S07d is visually accepted, but enabling Sky on the current graph still deserves a proper ordering/clear cleanup.
	Vulkan present semaphore reuse on fast shutdown is not fully clean yet; some diagnostic logs also remain intentionally temporary.
- Validation done:
	Editor `--rhi vulkan --project ...\MyMEProject.meproject` manual visual check: visible mesh output.
	Debug logs also showed BasePass draws and PresentPass blit on the same frame.
- Next step:
	S07e Shadow + scene include `set=`; pay down `TD-023` / `TD-024` when the render track has a natural cleanup window.

### 2026-08-05 - RND-F05-S07b鈥揝07d Vulkan Descriptor/PSO + Forward Unlit Base
- Goal:
	Batch S07b鈥揝07d so `--rhi vulkan` can run Forward Base (Unlit) and Present without ImGui.
- Main changes:
	Vulkan descriptor pool / set layout / pipeline layout / binding sets; lazy graphics PSO per RenderPass;
	frame recording Clear鈫扖md鈫扨resent; enable `ForwardRenderer` on Vulkan; Editor `OpenProjectForVulkanSmoke`
	loads `default` scene, forces Unlit, `SubmitSceneDraw(PresentToBackBuffer)`.
	Fixes: depth RT no default SAMPLED; `DEPTH24STENCIL8`鈫抈D32_SFLOAT_S8_UINT`; `PerFrame` visibility `All`.
- Validation done:
	Editor `--rhi vulkan --project MyMEProject`: Unlit recompile OK; 16s render loop alive;
	`VK_LAYER_KHRONOS_validation` stderr empty; `test smoke` GL+VK PASSED.
	**Human visual check still required for mesh silhouette.**
- Next step:
	S07e ShadowPass + `MaterialSceneShadows`/`lights` `set=` dialect; then prepare commit for S07a鈥揹 WIP.

### 2026-08-05 - RND-F05-S07a Vulkan Buffer/Texture2D/SRV/VertexInputLayout
- Goal:
	Fill VulkanRHI resource create/upload stubs so later S07 draws have a data plane.
- Main changes:
	`VulkanRHIAllocator` + `VulkanRHIBuffer` (host-visible map) / `VulkanRHITexture` (2D + staging) /
	`VulkanRHIShaderResourceView` / `VulkanRHIVertexInputLayout`; wire `RHICreate*`; init probes.
- Validation done:
	Editor `--rhi vulkan`: S07a buffer/texture/SRV/layout probe OK;
	`minEngineTests.exe --rhi opengl|vulkan test smoke` PASSED.
- Next step:
	S07b Descriptor / BindingSet / PipelineLayout.

### 2026-08-05 - RND-F05 S07 sub-slice table drafted (await review)
- Goal:
	Replace vague S07+ with reviewable S07a鈥揝07f before any Vulkan scene implementation.
- Main changes:
	Impl expands S07a resources 鈫?S07b descriptor 鈫?S07c PSO/Cmd 鈫?S07d Forward Base 鈫?S07e Shadow+set= 鈫?S07f Sky/IBL;
	Design 搂3.9 pending defaults (classic VkRenderPass, no ImGui-VK, enable Forward at S07d).
- Validation done:
	Docs only; no code.
- Next step:
	User review/approve 搂3.9 + Impl S07 table; then start S07a.

### 2026-08-04 - RND-F05-S06 complete (SkyBox + EnvMapCapture + Material set=/SPIR-V)
- Goal:
	Finish S06 shader dialect batches so engine fixed passes and graph materials share SPIR-V delivery.
- Main changes:
	SkyBox + EnvMapCapture (equirect/irradiance/prefilter) 鈫?`CreateShaderFromSpirvFiles` + location quals.
	MaterialCompiler emits `layout(set=kSetMaterial, binding=鈥?`; ShaderCompiler flattens set= for OpenGL.
	`Material::CommitCompileResult` 鈫?`CreateShaderFromSpirvSources`; S05 marked Done in same docs pass.
- Validation done:
	`minEngineTests.exe --rhi opengl test material-ir` PASSED;
	`test shader-compiler` PASSED; `test smoke` PASSED.
	Flatten strips `set=` inside `layout(std140, set=鈥? binding=鈥?`.
	Editor startup: material varyings `layout(location=鈥?` + remove dead `u_Material` in shadows include.
- Next step:
	S07+ 鈥?write VK scene sub-slice table before filling Vulkan resource stubs.

### 2026-08-04 - RND-F05-S05 Done + S06 SkyBox background SPIR-V
- Goal:
	Close Present-path slice; start engine-shader SPIR-V batches with SkyBox.
- Main changes:
	S05 marked Done (`PresentFrame` / no upper-layer `vulkan.h`).
	`background.vert/frag` explicit varyings/`out` locations; `SkyBoxPass` 鈫?`CreateShaderFromSpirvFiles`.
- Validation done:
	Editor build OK; `minEngineTests.exe --rhi opengl test smoke` PASSED;
	background.vert/frag SPIR-V-ready locations; SkyBoxPass loads via CreateShaderFromSpirvFiles.
- Next step:
	S06 next batch 鈥?EnvMapCapture bake shaders and/or MaterialCompiler `set=`.

### 2026-08-04 - BUG-RENDER-004 directional CSM self-shadow acne (+ BUG-RENDER-003 gate)
- Goal:
	Remove texture-following stripe banding on large ground planes under directional light; restore RND-F05 track after diagnosis.
- Main changes:
	Shadow depth PSO front-face cull + polygon offset; cascade Z expand via AABB corners (not sphere);
	receiver bias / light-dir offset; shadow map 1024; dir shadow sample gated on `Params.w` (also closes BUG-RENDER-003);
	TBN: tangent uses model matrix under non-uniform scale.
- Validation done:
	User A/B: disable dir shadow pass 鈫?stripes gone; after fix 鈫?stripes gone with CSM on (Editor OpenGL).
- Next step:
	Resume **RND-F05-S05** (Present / post-process neutrality; more SPIR-V).

### 2026-08-04 - RND-F05-S05 Present 璺緞鏀跺彛锛堣繘琛屼腑锛?- Goal:
	Align Editor/Engine frame present on neutral RHI path (no direct `SwapBuffers` in Editor OpenGL loop).
- Main changes:
	`RenderSystem::PresentFrame()` 鈫?`RHI::RHIPresent()`; `Engine::Run` and Editor (OpenGL + Vulkan) call it.
- Validation done:
	Editor OpenGL + Vulkan startup OK (user visual).
- Next step:
	Continue S05 鈥?PresentPass / post-process path parity; avoid `vulkan.h` in upper layers.

### 2026-08-04 - RND-F05 post-process OpenGL SPIR-V + S04 Vulkan triangle
- Goal:
	Extend SPIR-V hot path to FXAA/Sharpen; land Vulkan minimal graphics draw for smoke validation.
- Main changes:
	ForwardRenderer FXAA/Sharpen 鈫?`CreateShaderFromSpirvFiles`; `FXAA.frag` / `Sharpen.frag` add `layout(location=...)`.
	`VulkanRHI` render pass + pipeline; embedded triangle SPIR-V (RGB gradient).
- Validation done:
	Editor `--rhi vulkan` colored triangle PASS; OpenGL SPIR-V compile errors fixed.
- Next step:
	S05 present-path alignment.

### 2026-08-04 - RND-F05-S03 CLI `--rhi` + Vulkan clear/present vertical slice
- Goal:
	Enable runtime backend switch (`--rhi opengl|vulkan`) and land Vulkan swapchain clear/present with backend-internal sync only.
- Main changes:
	CLI adds global `--rhi` (opengl|vulkan, aliases gl|vk); TestMain synthetic argv updated so option-first test invocations keep `test` subcommand semantics.
	Introduce `RHIBackendSelection` and route Window/Render boot via selected backend.
	GLFW adds Vulkan `GLFW_NO_API` path; OpenGL path stays 4.6 + glad.
	`RHI` adds neutral `RHIPresent()`; OpenGL uses `SwapBuffers`, Vulkan owns acquire/submit/present with internal semaphore/fence.
	`VulkanRHI` S03 scope: instance/device/surface/swapchain + clear color present; resource/draw APIs intentionally stubbed for S04+.
	Editor Vulkan path runs smoke mode without ImGui/editor modules (clear/present validation only).
- Validation done:
	`minEngineTests.exe --rhi opengl test smoke` PASSED
	`minEngineTests.exe --rhi vulkan test smoke` PASSED
	`Editor.exe --rhi vulkan --project ...` startup log confirms NO_API + VulkanRHI clear/present loop.
- Next step:
	S04 鈥?Vulkan minimal graphics pipeline + SPIR-V shader path (first visible triangle/fullscreen draw).

### 2026-08-04 - RND-F05-S02 OpenGL 4.6 + Present SPIR-V hot path
- Goal:
	Consume OpenGL SPIR-V on Present; keep Material GLSL string path as migration window.
- Main changes:
	GLFW / MaterialIR / RenderGraph contexts 鈫?**4.6**.
	`RHIShaderCreateDesc` + bytecode `RHICreateShader`; OpenGL `glShaderBinary` + `glSpecializeShader`.
	`EngineShaderUtils::CreateShaderFromSpirvFiles`; PresentPass uses SPIR-V path.
	`test shader-compiler` adds GL specialize load case.
- Validation done:
	`minEngineTests.exe test shader-compiler` PASSED (2 cases); `test smoke` PASSED; `test render-graph` PASSED earlier.
- Next step:
	S03 鈥?CLI `--rhi opengl|vulkan` + VulkanRHI Clear/Present (frame sync internal).

### 2026-08-04 - RND-F05-S01 ShaderCompiler锛圥resent 鈫?VK/GL SPIR-V锛?- Goal:
	Land GLSL鈫扴PIR-V toolchain without switching GL runtime hot path yet.
- Main changes:
	`Render/ShaderCompiler/` (glslangValidator invoke + disk cache); CMake finds Vulkan SDK / glslang.
	`Present.vert/frag`: `#version 420` + explicit `location` (SPIR-V requirement); varyings `v_TexCoord`.
	Suite `test shader-compiler` (not in smoke).
- Validation done:
	`minEngineTests.exe test shader-compiler` PASSED; `test smoke` PASSED.
- Next step:
	S02 鈥?GL context 4.6 + Present loads SPIR-V via `RHICreateShader(bytecode)`.

### 2026-08-04 - RND-F05 Design Draft锛堝湴鍩鸿瘎浼?+ SPIR-V 鍙岀锛?- Goal:
	Assess render foundation for Vulkan; design SPIR-V for both GL and VK; multi-slice plan.
- Main changes:
	Expanded `RND-F05_*_DESIGN.md` + new `*_IMPLEMENTATION.md`; Registry Draft; ACTIVE_WORK pointer.
	Key finding: GL SPIR-V requires DescriptorSet=0 鈫?dual SPIR-V artifacts (VK multi-set / GL flat kGL_*).
- Validation done:
	Code survey (RHI/OpenGL/shaders/CMake); local `VULKAN_SDK` 1.4.350 + glslangValidator present.
- Next step:
	User confirms Design 搂7 鈫?Planned 鈫?S01 ShaderCompiler.

### 2026-08-04 - RND-F03 鍏宠处 + 涓荤嚎鏀逛负 F05锛坉ocs锛?- Goal:
	Close stale F03 In Progress; lock render-first roadmap (Vulkan multi-slice, physics after DebugDrawing).
- Main changes:
	F03 Design Meta/搂12 鈫?Done锛汻egistry F03 Done锛汧05 Design 鏄庣‘澶氬垁绔栧垏 + GL/VK 缁堟€侊紱ACTIVE_WORK 涓荤嚎 `feat/render`锛汸HYS-F03 绛?F11銆?- Validation done:
	Legacy grep (`WrapLegacy`/`RHIShaderLegacy`/`OpenGLRHIModern`/鈥? production src = 0.
- Next step:
	`feat/render` rebase master 鈫?F05 Pre-flight + Implementation 鍒囩墖琛ㄣ€?
### 2026-08-04 - CORE-F04 Native Multicast Delegates锛堝疄鐜?Done锛?- Goal:
	Ship Native multicast delegates; unlock PHYS-F03; close TD-006 (Native).
- Main changes:
	`Runtime/Core/Delegates/` 鈥?`DelegateHandle`, `MulticastDelegate`, macros; AddRaw / AddLambda / AddMEObject; Broadcast snapshot + stale MEObject compact.
	`Tests/Suites/DelegateTest.cpp` + suite id `delegates` (not in smoke).
	Docs: Design/Impl Done锛汿D-006 Done锛汻egistry/ACTIVE_WORK.
- Risks or caveats:
	Not thread-safe; prefer AddMEObject over AddRaw for gameplay; Dynamic/Lua still future.
	MulticastDelegate must include ObjectManager before bare GUID.h (Win32 `GetClassName` macro).
- Validation done:
	`minEngineTests.exe test delegates` 鈥?5/5 PASSED.
- Next step:
	Optional: PHYS-F03 Design on physics track; or feat/render F05.

### 2026-08-04 - CORE-F04 Native Multicast Delegates Design Draft
- Goal:
	Formalize Native-only multicast delegates (unlock PHYS-F03); split from reflection/Dynamic path.
- Main changes:
	`docs/ai/Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_{DESIGN,IMPLEMENTATION}.md`锛圫tatus Draft锛?
	Old `REFLECTION_DELEGATES_DESIGN.md` 鈫?Archived pointer; Registry / ACTIVE_WORK / TD-006 Notes / PHYS-F03 dependency updated.
- Risks or caveats:
	Superseded by implementation entry above.
- Validation done:
	Docs only.
- Next step:
	(done) implement S01鈥揝03.

### 2026-08-03 - TD-013锛歟num codec 鎸?MEEnum::GetSize 璇诲啓锛坄master`锛?- Goal:
	Stop treating every reflected enum as in-memory int64 (overran uint8 neighbors; bad scene JSON).
- Main changes:
	`ReflectionSystem::SetCodecForEnums` loads/stores via size 1/2/4/8; archive wire still WriteInt64/ReadInt64.
	`serialization-archive` smoke adds uint8 `m_ShadingModel` round-trip + neighbor `m_BlendMode` corruption check.
	Registry: `CORE-F04` Delegates + `RND-F11` DebugDrawing Planned锛汚CTIVE_WORK 椤哄簭 Vulkan鈫扗ebugDrawing.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngineTests.exe test smoke` / `physics-load` 鈫?PASSED
- Next step:
	Base `render`/`physics` onto master锛涘垎鏀敼鍚?`feat/*`锛涘紑 CORE-F04 鎴?F05 鎸?ACTIVE_WORK.

### 2026-08-03 - master 鈫?merge render锛堢幇浠?RHI/RDG/EnvMap + physics/Lua锛?- Goal:
	Integrate `render` (RND-F02鈥揊10) onto `master` (Lua/physics/Transform quat) without dropping either track.
- Main changes:
	Union AssetManager LuaScript + EnvironmentMap; TestSuiteRegistration all suites; docs ACTIVE_WORK/REGISTRY/TECH_DEBT (CORE TD-013 enum kept; render Set0 cache 鈫?TD-022).
	`test.mescene` keeps physics + SkyBox EnvironmentMap.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngineTests.exe test smoke` / `render-graph` / `lua-script-mvp` / `physics-smoke` 鈫?PASSED
	(merge follow-up: drop nonexistent `RenderGraphTest.h` include in TestSuiteRegistration)
- Next step:
	Push when ready; resume RND-F05 discussion.

### 2026-05-25 - Asset Pipeline 涓氬姟绾胯璁¤崏妗?- Goal:
	End-to-end design for AssetManager CRUD/events, cross-platform import dialog, project file watcher, Content Browser framework (asset-workflow worktree).
- Main changes:
	Added `docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md` (data flow, R0鈥揜2, E4, AssetWorkflow, Browser v0, implementation order P1鈥揚7, decision table 搂10).
	Linked from `CONTENT_BROWSER_DESIGN.md`.
- Risks or caveats:
	Awaiting user sign-off on 搂10 (NFD vs Win32, watcher polling vs native, Import rename policy, Browser list scope).
- Validation done:
	Reviewed `AssetManager` / `AssetWorkflowModule` / `ProjectManager::OpenProject` scan path against design.
- Next step:
	搂10 宸叉媿鏉匡紙NFD銆乪fsw銆丏12銆佺浉瀵?AssetPath 搂14锛夆啋 P1 + P1b 瀹炵幇銆?
### 2026-05-25 - P1 API 瀹氱锛堝緟瀹℃壒锛?- Goal:
	Freeze header-level P1/P1b interface before coding.
- Main changes:
	`docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_P1_API.md` 鈥?AssetRegistryTypes, AssetTypeRegistry, AssetManager deltas, contracts, migration table, acceptance 搂9, approval checklist 搂10.
- Risks or caveats:
	Breaking `FindAssetMetasByType` return type; Editor Material/Scene call sites updated in same PR.
- Validation done:
	Grep call sites; path rules aligned with 搂14.
- Next step:
	User approves 搂10 checklist 鈫?implement P1.

### 2026-05-25 - Asset Pipeline P1/P1b implemented
- Goal:
	AssetTypeRegistry, type buckets, registry events, ImportAsset, project-relative AssetPath, Editor EngineDefault scan removed.
- Main changes:
	New `AssetRegistryTypes.h`, `AssetTypeRegistry.{h,cpp}`; `AssetManager` extended (Subscribe, ImportAsset, FindAssetMetasByType/ByRuntimeClass, ResolveAssetAbsolutePath).
	Loaders use absolute resolve for disk IO; Editor Material/Scene call sites updated; `MyMEProject` metas 鈫?relative `AssetPath`.
- Risks or caveats:
	RegisterAsset requires `PathRegistry` project root; legacy absolute registry keys only via FindAssetMetaByPath fallback until rescan.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded.
- Next step:
	P2 DeleteAsset/MoveAsset; then P3 NFD, P5 efsw, P6 Content Browser.

### 2026-05-25 - Asset Pipeline P2 implemented
- Goal:
	Delete/Move/Rename/Unregister registry CRUD, Moved events, ClearProjectRegistry on CloseProject, headless tests.
- Main changes:
	`AssetManager` 鈥?`DeleteAsset`, `MoveAsset`, `RenameAsset`, `UnregisterAsset`, `ClearProjectRegistry`; `ProjectManager::CloseCurrentProject` clears registry; `SceneManager::IsSceneRegistered`.
	`AssetManagerTest` + `--asset-manager-test` (temp project `Assets/_P2UnitTest/`, copies EngineDefault cube; never deletes MyMEProject assets).
- Validation done:
	`Editor.exe --asset-manager-test` exit 0 (delete, move, rename, extension guard, unregister, clear, scene unregister).
- Next step:
	P3 NFD / P4 AssetWorkflow / P5 efsw.

### 2026-05-25 - Asset Pipeline P3 FileDialog (Runtime Platform)
- Goal:
	Runtime IFileDialogService + NFD; Editor consumes via Engine; asset filters from Registry.
- Main changes:
	`Runtime/Platform/FileDialog/*`, `FileDialogService` in `Engine`; `AssetTypeRegistry::BuildFileDialogFilters`;
	Editor `GetFileDialogService()`; Tools menu **File Dialog (P3)** for Open/Save/Folder smoke.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded (single-threaded link after parallel truncate).
- Next step:
	P4 `AssetWorkflowModule::ImportAssetDialog`.

### 2026-05-26 - Asset Pipeline P5 ProjectAssetWatcher (efsw)
- Goal:
	Editor-only filesystem watcher syncing external disk changes to `AssetManager` registry.
- Main changes:
	`Third-Party/efsw` @ 1.4.0; `ProjectAssetWatcher` (queue + 400 ms debounce + main-thread `Tick`);
	`AssetManager::SuppressExternalSyncScope`; Editor `OpenProject`/`CloseProject`/`Run` lifecycle;
	bulk fallback `ScanAssets`; P5 API doc status 鈫?宸插疄鐜?
- Validation done:
	`cmake --build minEngine/build --target Editor -j 1` succeeded.
- Next step:
	Manual P5 acceptance (copy/delete in Explorer, Import no storm); P5.1 `Reimported`; P6 Content Browser.

### 2026-05-26 - Asset Pipeline P6 Content Browser
- Goal:
	Content Browser module: directory tree, registered asset list, selection, Inspector bridge, Open/Delete/Import.
- Main changes:
	`ContentBrowserModule`, `AssetTreeModel`, `ContentBrowserWindow`; `FindAssetMetasUnderDirectory`;
	`AssetWorkflowModule` selection/Delete/InspectorSource; `InspectorWindow` focus patch.
- Risks or caveats:
	**Browser UI 灞曠ず闇€鍚庢湡鍐嶈璁?*锛堝綋鍓嶈８ ImGui 绾挎锛涗富棰?鍥炬爣/缂╃暐鍥惧緟 Appearance merge 鍚?P6.1锛夈€?- Validation done:
	`cmake --build` OK; user confirmed Content Browser visible and core flows work.
- Next step:
	P7 default Dock + menu integration; P6.1 Browser visual design after Appearance merge; P5.1 Reimported optional.

### 2026-05-26 - Editor / Runtime 鏂囦欢鍒嗗眰杩佺Щ
- Goal:
	Align Editor SubEditor/Inspector layout and consolidate Runtime asset loaders under `Resource/Loaders/`.
- Main changes:
	Editor: `Services/Inspector/InspectorModule`; `SubEditor/Material|Scene` + viewport clients; plan `docs/ai/Editor/EDITOR_FILE_LAYOUT_MIGRATION.md`.
	Runtime: all `*Loader` 鈫?`Runtime/Resource/Loaders/`; merge `MaterialAssetLoader` into `MaterialLoader::Load`; new `ShaderLoader`; `TextureCubeLoader` unchanged in `Render/`.
- Risks or caveats:
	`MaterialEditor::OpenSession` may still double-compile after `LoadAsset<Material>` (pre-existing).
- Validation done:
	`cmake --build minEngine/build --target minEngine Editor`; `--asset-manager-test` / `--material-ir-test` exit 0.
- Next step:
	Editor 鐩 spot-check; optional `MaterialEditor` dedupe compile.

### 2026-05-25 - Seed EngineDefault BasicShapes into MyMEProject
- Goal:
	Restore scene mesh GUID refs after stopping EngineDefault Registry scan.
- Main changes:
	Copied 7 meshes to `MyMEProject/Assets/Meshes/BasicShapes/` with relative `.meta` (GUID preserved); script `scripts/seed_basic_shapes_to_project.py`.
- Validation done:
	`cube`/`plane` GUIDs match `default.mescene` and `test.mescene` mesh references.
- Next step:
	Re-open project in Editor to `ScanAssets` register new paths (or rely on existing meta on disk).

### 2026-05-25 - Asset Pipeline 璁捐鎷嶆澘 + 鐩稿璺緞绛栫暐
- Goal:
	Freeze 搂10 decisions; resolve Engine vs Project asset roots and meta relative paths.
- Main changes:
	`ASSET_PIPELINE_DESIGN.md` 鈫?Status 宸叉媿鏉? D8=NFD, D9=efsw, D12=registered-only; new 搂14 dual-root + relative `AssetPath`.
- Risks or caveats:
	P1b migration of existing absolute metas; remove Editor scan of EngineDefault from Registry.
- Validation done:
	User confirmed defaults + path strategy discussion.
- Next step:
	P1 code slice (type bucket + events + ImportAsset); P1b relative path + stop EngineDefault Registry scan.

### 2026-03-26 - Playground player control enhancement
- Goal:
	Add vertical flight and improve camera control experience in Playground.
- Main changes:
	Added IA_UpAndDown for E/Q vertical movement.
	Added IA_Look camera rotation logic with pitch clamp and sensitivity.
	Added wheel input support path via WindowSystem/GLFWWindowSystem/InputSystem and mapped MouseScroll in Playground.
	Fixed horizontal look direction sign to match expected user control direction.
- Risks or caveats:
	Mouse2D and MouseScroll semantics are still evolving between engine-side abstraction and gameplay-side handling.
	Future refactor may separate event-like input and state-like input more clearly.
- Validation done:
	CMake build completed successfully after control/input changes.
	Manual interaction checks performed in Playground for movement and look direction.
- Next step:
	Standardize mouse delta semantics at one layer only (engine or gameplay) to avoid double-delta bugs.

### 2026-04-06 - Scene serialization MVP pipeline wiring
- Goal:
	Wire a minimal end-to-end scene save/load loop for editor usage.
- Main changes:
	Added SceneSerializer with JSON save/load for scene name and game object transform list.
	Extended Scene object creation to support explicit object IDs for deterministic reload.
	Implemented SceneManager::CreateNewScene/LoadScene and default scene path resolution.
	Wired Editor File menu actions (New/Open/Save/Exit) to SceneManager and SceneSerializer.
	Created default editor scene on startup for immediate save/load testing.
- Risks or caveats:
	Current MVP only serializes scene name, object id, and transform; component graph is not serialized yet.
	Open Scene currently uses a fixed default path and has no file dialog.
- Validation done:
	File-level diagnostics passed for all touched scene/editor source files.
- Next step:
	Add component type registry and polymorphic component serialization/deserialization.

### 2026-04-10 - MEReflection parallel migration start
- Goal:
	Introduce a safer migration path by running new reflection flow in parallel with legacy reflection.
- Main changes:
	Added MEReflection alongside legacy reflection system.
	Prepared architecture boundaries for incremental migration instead of one-shot replacement.
- Risks or caveats:
	Dual-stack reflection can drift if registration/finalization behavior diverges.
- Validation done:
	Integrated into branch history and enabled follow-up serializer/reflection work.
- Next step:
	Consolidate registration/finalization ownership and remove duplicate paths gradually.

### 2026-04-12 - Reflection finalization reliability and GameObject fix
- Goal:
	Restore reflection correctness and unblock dependent serialization/editor features.
- Main changes:
	Added reflection finalization into engine startup flow.
	Added reflection finalization error logs to surface missing registration issues.
	Fixed GameObject-related reflection path issues and generated required reflection artifacts.
- Risks or caveats:
	Generated code order and startup timing still require strict consistency.
- Validation done:
	Reflection path recovered enough to continue pointer/serializer feature work.
- Next step:
	Continue serializer refactor with stronger pointer handling and field coverage.

### 2026-04-14 - Serializer pointer path, enum codec, inspector recovery
- Goal:
	Expand serialization coverage to component graphs and reconnect editor inspection flow.
- Main changes:
	Implemented pointer deserialization baseline in Serializer.
	Added primitive codec registry with MEEnum support.
	Successfully serialized a GameObject containing owned inline shared_ptr<Component> arrays.
	Reenabled Inspector for GameObject and component fields.
- Risks or caveats:
	Non-owned shared_ptr<MEObject> reference serialization is not finished.
	Pointer ownership/memory behavior still needs hardening in edge cases.
- Validation done:
	Commit notes confirm end-to-end success for owned component array scenario.
- Next step:
	Implement reference-mode object identity path and resolve semantics for external object references.

### 2026-04-16 - Asset metadata and GUID foundation (WIP)
- Goal:
	Prepare asset identity and registry infrastructure for future asset database workflow.
- Main changes:
	Added GUID type and GenerateGUID utility.
	Introduced AssetMeta (path/type/guid) and AssetManager in-memory registry map.
	Added AssetManager::ScanAssets and RegisterAsset initial flow.
	Added Texture2DResource and generated reflection header for resource-level metadata serialization prep.
	Refactored editor default-scene population helper and aligned serializer callsites with updated parameter flow.
- Risks or caveats:
	.meta persistence is still TODO; GUID reuse/load policy is not finalized.
	Current workspace includes uncommitted WIP changes and may still need integration fixes.
- Validation done:
	Local scope and file topology are in place; full fresh build validation has not been recorded yet for this WIP batch.
- Next step:
	Implement meta read/write + GUID reuse strategy, then run full build and editor open/save smoke test.

### 2026-04-17 - ObjectPtr probe-based GUID reference path
- Goal:
	Continue serializer evolution after ObjectManager became the central owner of runtime objects.
- Main changes:
	Added GUID reference node interfaces to Archive/JsonArchive (`BeginGuidRef`/`EndGuidRef`).
	Implemented probe-based object pointer deserialization in Serializer: try inline object node first, then GUID reference node.
	Implemented direct-Outer ownership rule in object pointer serialization: direct child uses inline serialization, otherwise writes GUID reference.
	Hooked GUID reference resolve chain in deserialization: ObjectManager first, then AssetManager fallback for known asset types.
	When deserializing inline MEObject-derived pointers, assign Class/Outer and register into ObjectManager.
- Risks or caveats:
	Current object-pointer serializer path still assumes MEObject-derived pointer property types for full ownership semantics.
	Asset fallback currently relies on known class-name mapping (`StaticMesh`/`Texture2D`) and may need extension once more resource wrapper types are reflected.
- Validation done:
	File-level diagnostics passed for touched serialization files (Archive/JsonArchive/Serializer).
- Next step:
	Add a focused round-trip case for mixed inline + GUID references (including cross-owner references) to lock behavior.

### 2026-04-18 - Scene vector ownership serialization alignment
- Goal:
	Align scene save/load flow with vector-owned GameObject data and non-persistent runtime IDs.
- Main changes:
	Switched Scene tick path to iterate m_GameObjects vector (ownership source of truth).
	Added Scene::RebuildRuntimeGameObjectIndex to compact null entries, rebuild id->pointer query map, and reassign runtime GameObject IDs after load.
	Refactored SceneSerializer to serialize/deserialize Scene directly through Serializer (sceneData payload) and removed persisted per-GameObject id fields.
	Added Scene reflection fields for sceneName and m_GameObjects in generated Scene.gen.h so direct Scene serialization includes required data.
	Kept pending object reference workflow intact: clear queue before load, deserialize scene, rebuild runtime index, resolve pending refs at load-unit end.
- Risks or caveats:
	Scene file format changed to version 2 and currently follows forward-only path (no legacy id-based scene schema compatibility).
	Pylance/Problems diagnostics for SceneSerializer still showed stale legacy entries in this session despite file content update; no legacy symbols remain in source file.
- Validation done:
	File-level diagnostics passed for Scene.h, Scene.cpp, Scene.gen.h.
	Source grep confirmed legacy id-serialization symbols were removed from SceneSerializer.cpp.
- Next step:
	Run editor-level save/load smoke test with at least one cross-GameObject reference case to verify runtime id remap and pending reference resolve behavior end-to-end.

### 2026-04-18 - Editor Scene API alignment follow-up
- Goal:
	Migrate editor-side scene/gameobject queries to the new Scene storage model and encapsulated GameObject ID accessor.
- Main changes:
	Updated Editor::GetHierarchyGameObjects to read from Scene::GetGameObjects() and sort by GameObject::GetID().
	Updated Editor::GetSelectedGameObject / RenameGameObject to use vector scan by GetID instead of old map-style find on m_GameObjects.
	Updated Editor::SyncSelectionWithScene to use Scene::GetGameObjectsById() for existence check and fallback to hierarchy list front item.
	Updated HierarchyWindow and InspectorWindow to use GameObject::GetID() instead of direct m_ID field access.
- Risks or caveats:
	Current editor selection lookup by ID is linear over scene gameobject vector when shared_ptr ownership is needed.
	Pylance diagnostics may lag in this session; source-level grep confirms old m_ID and old m_GameObjects.find usage in editor code paths were removed.
- Validation done:
	File contents verified for Editor.cpp / HierarchyWindow.h / InspectorWindow.h with new Scene/GameObject access pattern.
- Next step:
	Decide whether to introduce non-owning raw pointer query APIs for hierarchy/selection to reduce shared_ptr churn in per-frame UI code.

### 2026-04-18 - Editor non-owning raw pointer query pass
- Goal:
	Adopt non-owning raw pointer query APIs for per-frame editor scene inspection paths.
- Main changes:
	Changed Editor API signatures: GetActiveScene -> Scene*, GetHierarchyGameObjects -> vector<GameObject*>, GetSelectedGameObject -> GameObject*.
	Updated Editor.cpp callsites accordingly, while preserving Scene/SceneManager ownership via shared_ptr internally.
	Updated HierarchyWindow and InspectorWindow to consume raw pointer query results.
	Selection and rename lookups now use Scene::GetGameObjectsById() map for direct ID lookup.
- Risks or caveats:
	Returned raw pointers are non-owning and only valid while scene ownership remains stable in current frame.
	Callers must not cache returned pointers across scene reload/destruction events.
- Validation done:
	File-level diagnostics passed for Editor.h, Editor.cpp, HierarchyWindow.h, InspectorWindow.h, MainMenuWindow.h.
- Next step:
	Optionally add naming convention docs (e.g., *Raw suffix) if both owning and non-owning query APIs coexist later.

### 2026-04-18 - Reflection derived-check centralization
- Goal:
	Eliminate duplicated ad-hoc inheritance checks in serialization paths by adding a reusable reflection-level API.
- Main changes:
	Added `MEClass::IsA(const MEClass*)` as the canonical class ancestry query (self match included by default).
	Added `ReflectionSystem::IsClassSameOrDerived` and `ReflectionSystem::IsClassNameSameOrDerived` utility methods.
	Refactored Serializer to remove local `IsClassSameOrDerived` helper and use `ReflectionSystem` APIs.
	Refactored JsonArchive object/object-ptr type checks to use reflection system class-name derived matching instead of direct-child-only scans.
- Risks or caveats:
	Type-name checks now require reflected type names to be registered in `ReflectionSystem`; unresolved names fail fast.
- Validation done:
	File-level diagnostics passed for MEClass.h, Reflection.h, Serializer.cpp, JsonArchive.cpp.
	Source grep confirmed serializer-local inheritance helper was removed.
- Next step:
	Re-run scene load smoke test and confirm no "neither inline object node nor GUID reference node" error on multi-level inheritance pointer fields.

### 2026-04-18 - Resource reference GUID stabilization and scene migration
- Goal:
	Fix unresolved asset references during scene deserialization by ensuring serialized resource GUIDs match asset meta GUIDs.
- Main changes:
	Updated AssetManager static-mesh loading path to normalize cache key/path consistently.
	Updated `LoadStaticMeshByMeta` and `LoadStaticMesh` to bind loaded `StaticMesh` runtime object GUID to asset meta GUID.
	Migrated `bin/Assets/Scenes/EditorDefault.scene.json` mesh GUID nodes from stale random values to current mesh meta GUIDs.
- Risks or caveats:
	If older scene files contain random runtime GUIDs that have no corresponding `.meta` entries, they still require migration.
- Validation done:
	Rebuilt CMake targets successfully (minEngine + Editor).
	Runtime log verification shows pending reference resolve pass `resolved=5, unresolved=0`.
- Next step:
	Optionally add an automatic scene GUID migration pass for legacy files with unresolved asset GUID references.

### 2026-04-18 - ProjectManager skeleton bootstrap
- Goal:
	Start project-system architecture separation by introducing a dedicated `ProjectManager` subsystem scaffold.
- Main changes:
	Added `Runtime/Function/Framework/Project/ProjectManager.h/.cpp` with empty open/close interfaces and baseline data-model structs (`ProjectDescriptor`, `ProjectContext`, `ProjectOpenResult`).
	Added project file naming constants for new convention: `.meproject` and `.measset`.
	Added startup-scene resolution interface with engine-default fallback path placeholder.
	Registered `ProjectManager` in `RuntimeGlobalContext` startup/shutdown lifecycle.
- Risks or caveats:
	Current `OpenProject` behavior is intentionally non-functional (`NotImplemented`) to keep this iteration architecture-only.
	No runtime feature wiring to editor startup path yet.
- Validation done:
	File-level diagnostics passed for new and updated framework files.
	No build/run step executed in this task (per user request).
- Next step:
	Implement minimal project discovery/load flow and wire editor startup to `ProjectManager` scene resolution chain.

### 2026-04-18 - ProjectManager descriptor split and open-flow implementation
- Goal:
	Move project metadata into a dedicated header and replace `OpenProject` placeholder logic with real descriptor loading/validation.
- Main changes:
	Split project metadata into `Runtime/Function/Framework/Project/ProjectDescriptor.h`.
	Added `ProjectDescriptorSerializer.h/.cpp` to load/save descriptor files through the generic archive layer (`JsonArchive` read/write APIs).
	Implemented `ProjectManager::OpenProject` flow: normalize root, locate descriptor, deserialize, validate required fields, fill defaults (`Assets` / `Config`), resolve startup scene with engine-default fallback and diagnostics.
	Moved `ProjectManager::Get()` implementation to cpp, decoupling header from direct `RuntimeGlobalContext` include.
- Risks or caveats:
	Descriptor format is now strict on required fields (`schemaVersion`, `projectName`, `projectId`).
	Startup-scene fallback currently records diagnostics but does not yet emit UI-level notifications.
- Validation done:
	File-level diagnostics passed for `ProjectDescriptor.h`, `ProjectDescriptorSerializer.h/.cpp`, `ProjectManager.h/.cpp`.
	No build/run step executed in this task (per user request).
- Next step:
	Wire editor startup bootstrap to call `ProjectManager::OpenProject`, then consume `ResolveEditorStartupScenePath()` as the scene-entry source.

### 2026-05-04 - Spot/Point shadow pass MVP
- Goal:
	Add shadow rendering support for spot lights and point lights without changing the base pass sampling yet.
- Main changes:
	Added spot/point shadow requests and draw command building paths.
	Implemented spot and point shadow rendering in ShadowPass, including cube face rendering for point lights.
	Added spot 2D depth and point cube depth resource allocation in ShadowResourceManager.
	Extended OpenGL cubemap creation to respect depth texture formats.
- Risks or caveats:
	Spot/point shadow resources are single-instance MVP allocations; multiple shadow-casting lights will overwrite in this stage.
	Point light shadow range uses a fixed near/far plane; sampling path must match this later.
- Validation done:
	No build/run step executed in this task.
- Next step:
	Wire base pass sampling for spot and point shadows and pass down required parameters (shadow map index, far plane).

### 2026-05-04 - BasePass shadow sampling (spot/point)
- Goal:
	Connect spot and point shadow maps into BasePass sampling with a fixed resource limit of two each.
- Main changes:
	Added spot/point shadow sampler arrays and view-projection UBO in the Phong shader.
	Bound spot/point shadow textures in BasePass and wired shadow indices through light Params.w.
	Added spot view-projection UBO updates and point shadow far-plane data in light UBO updates.
	Made point-shadow cubemap depth linear in shadow pass for correct distance comparisons.
- Risks or caveats:
	Shadow resource maps are capped at two per light type; extra shadow-casting lights skip shadows.
	Point/spot shadow near/far planes are fixed constants; keep sampling logic consistent if they change.
- Validation done:
	No build/run step executed in this task.
- Next step:
	Run an editor scene with multiple spot/point lights to verify shadow indexing and bias tuning.

### 2026-05-08 - Material IR MVP pipeline bootstrap
- Goal:
	Stand up a minimal MaterialEdGraph -> MIR -> GLSL flow with a startup test dump.
- Main changes:
	Added MIR graph/value/node data structures and literal value support for float/vector constants.
	Implemented MIRBuilder with caching, binary ops, texture parameter, and texture sampling support.
	Expanded material node defs (constant2, multiply, texture param/sample) and wired Add/Multiply builds.
	Extended GLSL compiler with texture uniform tracking and texture() sampling emission.
	Added MaterialIR test helpers and editor startup logging for IR dump and GLSL output.
	Fixed editor graph pin ownership to allow graph connections in tests.
- Risks or caveats:
	No constant folding, dead code elimination, or stage separation yet; scalar-only type promotion.
	Texture sampling uses a simple sampler2D uniform and constant UVs in the test graph.
- Validation done:
	Not run (log-based test added; no runtime shader compile executed).
- Next step:
	Add basic diagnostics surfaced in MaterialCompileResult and extend node set (lerp, params).

### 2026-05-08 - MaterialOutput Albedo wiring
- Goal:
	Force compilation through a MaterialOutput node and map Albedo to MIR/GLSL output.
- Main changes:
	Added MaterialOutput node and wired Albedo into MIRGraph outputs.
	Enforced compile entry to be MaterialOutput only and removed arbitrary node compile path.
	Updated MVP test graph to end in MaterialOutput and compile through that entry.
	GLSL output now names Albedo explicitly for the final fragment color.
- Risks or caveats:
	Only Albedo is supported; other material properties are ignored.
	Missing MaterialOutput now hard-fails compilation.
- Validation done:
	Not run (startup log is available when the editor launches).
- Next step:
	Add Emissive/Opacity outputs or wire defaults for a fuller unlit MVP.

### 2026-05-19 - Material IR P8 texture and scalar parameters
- Goal:
	Compile texture sampling and scalar uniforms through MIR to GLSL (minimal P8).
- Main changes:
	Added MIR instructions ExternalInput, TextureObject, TextureRead, UniformParameter.
	MIREmitter: ExternalInput(TexCoord0), TextureObject, TextureSample, UniformScalar; Cast float4->float3 via subscripts.
	MIRToGLSLTranslator: shader preamble (sampler2D, in vec2 v_TexCoord0, uniform float), texture() lowering.
	Graph nodes: TextureCoordinate, TextureObject, TextureSample (RGBA+RGB outputs), ScalarParameter.
	Smoke test uses texture->Albedo, uniform->Metallic, constant Emissive.
- Validation done:
	Editor.exe --material-ir-test exit 0.
- Next step:
	S2 cross-stage TexCoords (UE-style ExternalInput -> MaterialParameters.TexCoords), then runtime bridge / P9 DefaultLit.

### 2026-05-19 - Material compile S0+S1 (multi-stage contract + FragmentMaterialInputs)
- Goal:
	UE-aligned compile contract: per-stage MIR lowering, ShadingModel assembler, FragmentMaterialInputs naming.
- Main changes:
	MaterialCompileTypes.h: MaterialCompileEnvironment, MaterialCompiledShader (Stages, FullVertex/FragmentShader).
	MaterialCompiler::Compile(graph, translator, env); MaterialShaderAssembler + UnlitShadingModel.
	MIRToGLSLTranslator lowers Vertex+Fragment bodies; FragColor only in Unlit assembler.
	FragmentMaterialInputs.Albedo etc.; MP_WorldPositionOffset placeholder + MaterialShadingPropertyCount.
	SetMaterialOutput registers outputs per MaterialPropertyEvaluatesInStage (UE-style).
	Removed MaterialCompileResult; smoke tests assert stage body + full shaders.
- Validation done:
	cmake build minEngine + Editor; Editor.exe --material-ir-test exit 0.
- Next step:
	Runtime bridge (compile to Shader + viewport) or DefaultLit shading model.

### 2026-05-19 - Material compile S2 UE-style MaterialParameters.TexCoords
- Goal:
	Cross-stage UV like UE: ExternalInput lowers to MaterialParameters.TexCoords[N]; assembler fills/interpolates.
- Main changes:
	MaterialShaderParameters.h: TexCoords access + v_MaterialTexCoordN varying names.
	Unlit assembler: vertex sets MaterialParameters from a_TexCoord, fragment restores from varying.
	MIRToGLSLTranslator: texture() uses MaterialParameters.TexCoords[0] (not v_TexCoord0 in material body).
	MaterialIRTest: logs full shaders; writes Saved/Materials/GeneratedVertex.glsl and GeneratedFragment.glsl.
- Validation done:
	Editor.exe --material-ir-test exit 0.

### Material MIR 鈥?roadmap snapshot (Path A, for planning)
- **P9 (shading):** Replace unlit `FragColor = vec4(Albedo + Emissive, Opacity)` with minimal lit shading that consumes Metallic/Roughness (minimal metallic-roughness BRDF or interim Blinn-Phong); extend smoke asserts for lighting-related GLSL.
- **Runtime bridge (often before or overlapping P9):** Vertex stage `v_TexCoord0` from mesh UV; compile `MaterialEdGraph` 鈫?`Shader` at load/startup; `Material` asset holds graph + compiled shader; BasePass binds `u_TextureN` / scalar uniforms from asset; assign material on scene mesh 鈥?**first viewport-visible custom material without graph UI**.
- **Editor graph UI (parallel track):** Visual material editor, pin wiring, recompile-on-save, preview mesh 鈥?**鈥滅湡姝ｅ湪缂栬緫鍣ㄩ噷鏀瑰浘鐪嬫晥鏋溾€?* depends on this plus runtime bridge.
- **P10 (polish):** Vertex/custom interpolants beyond UV, `TrySimplifyOperator`, boolN Select, `Step_Finalize`, float2/float4 expansion, etc.

## Deferred Reminders (for future sessions)

### 2026-05-19 - Material R1: C1a vertex transform + BasePass graph branch
- Goal:
	Viewport-visible compiled graph unlit materials (R1).
- Main changes:
	Unlit assembler: PerFrameData + u_Model + ViewProj * u_Model * vec4(a_Position,1).
	Material: m_bUsesCompiledGraph, m_GraphTextureSlots, m_GraphScalarParams, BindCompiledGraph().
	BasePass: legacy Phong vs compiled graph binding split.
	Editor smoke scene: graph binding + SyncSceneToRenderPipeline.
	MaterialIRTest: vertex transform assertions.
- Validation done:
	cmake build Editor; --material-ir-test exit 0.
- Next step:
	R2 polish / R3 or visual confirm in editor viewport.

### 2026-05-19 - Shader asset API + editor material smoke scene
- Goal:
	Converge file鈫扜PU shader on Shader class; editor startup active scene with MIR smoke material quad.
- Main changes:
	Removed ShaderSourceIO; Shader.cpp owns ReadSourceFile, CompileFromFiles, CreateFromSource, TryCompileSourcesOnGpu, EngineShaderPath.
	MaterialSmokeGraph shared by MaterialIRTest and EditorDefaultScene.
	SetEditorMaterialIRSmokeActiveScene() after OpenProject; quad + camera + light.
	Fixed GameObject::AddComponent return type (return newComponentBase).
	Reverted Playground shader changes (legacy, BUILD_PLAYGROUND OFF).
- Validation done:
	cmake build Editor; --material-ir-test exit 0.

### 2026-05-19 - Material R0: RHIShader source compile + GPU test
- Goal:
	R0: RHIShader accepts GLSL source strings; file IO at AssetManager; GPU compile smoke test.
- Main changes:
	OpenGLShader(vertexSource, fragmentSource) with IsValid/GetCompileLog; CreateRHIShader returns nullptr on failure.
	ShaderSourceIO: ReadShaderSourceFile, CreateRHIShaderFromFiles, TryCompileShaderSourcesOnGpu, EngineShaderPath.
	AssetManager/PresentPass/ShadowPass/RenderPipeline/Playground use file read + source API (no paths in RHI).
	MaterialIRTest: headless GL context when needed; GPU compile/link assert on smoke shaders.
- Validation done:
	cmake build Editor; Editor.exe --material-ir-test exit 0 (includes GPU compile PASSED).
- Next step:
	R1 C1a Unlit vertex PerFrameData + BasePass graph-unlit branch.

### 2026-05-19 - Material runtime bridge checklist
- Goal:
	Document GPU/viewport integration path after S0鈥揝2 compile pipeline.
- Main changes:
	Added docs/ai/MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md (layer checklist A鈥揈, risks R1鈥揜7, phases R0鈥揜4, PR split).
- Next step:
	User confirm C1a (Unlit VS + PerFrameData/u_Model); implement R0 GPU compile test then R1 BasePass branch.

### Material compiler 鈥?shader formatting (deferred)
- **Indentation:** Generated GLSL stage body / main blocks use inconsistent leading whitespace; normalize in Assembler or MIRGLSLPrinter when convenient (not blocking compile or tests).

### Material IR 鈥?cross-stage modeling (locked for S2, UE-aligned)
- **Decision (2026-05-19):** No separate MaterialInterpolator IR. Default UV = `ExternalInput(TexCoordN)` lowered to `MaterialParameters.TexCoords[N]`; vertex pass-through + varying owned by Assembler/妯℃澘. User-modified UV later via `MP_CustomizedUVs*` (vertex MaterialProperty), like UE.
- **S2 scope:** Replace translator `v_TexCoord0` with Parameters naming; vert fills TexCoords from `a_TexCoord`.

### Material IR 鈥?texture sampling variants (post-P8)
- **When to surface:** After runtime can display at least one compiled material with `texture()` in the viewport (see P9 / runtime-bridge milestones below), or when a graph needs non-default mip/grad behavior.
- **What (incremental, UE-aligned):**
  - `TextureSampleLevel` 鈫?GLSL `textureLod` (+ mip level input on node)
  - `TextureSampleBias` 鈫?`texture(..., bias)`
  - `TextureSampleGrad` 鈫?`textureGrad` (+ ddx/ddy inputs)
  - `TextureGather`, Cube/Volume/Array object types (later)
  - Sampler state (wrap/filter) separate from texture asset
  - UE-style stage switch (hardware vs analytic gradient path) 鈥?only if needed
- **Already supported (do not re-implement):** multiple textures via `TextureSlotIndex` (`u_Texture0`, `u_Texture1`, 鈥?; single 2D path `MIRTextureRead` 鈫?`texture(sampler2D, vec2)`.
- **Why deferred (2026-05-19):** P8 goal was one end-to-end 2D sample path + uniforms; more modes add MIR/GLSL/node/test surface before Editor/GPU binding exists.
- **Suggested order:** multi-slot smoke 鈫?Level 鈫?Bias/Grad 鈫?Gather / exotic types 鈫?sampler metadata.

### Material editor 鈥?vector node pin expansion (UE-style)
- **Done (E4, 2026-05-19):** `MaterialGraphNodeDef_Constant3` 澧炲姞 Value/R/G/B 鍥涗釜 output锛沗BuildIR` 鐢?`SubscriptChannel`锛涚紪杈戝櫒澶氬紩鑴?UI 宸查殢 `m_Outputs` 鏄剧ず銆?- **Still deferred:** `MakeFloat3` 绛夊叾瀹冨悜閲忚妭鐐瑰寮曡剼锛涜 `MATERIAL_EDITOR_PLAN.md` 鍚庣画杩唬銆?
### Material asset file round-trip (Instanced graph)
- **2026-05-21:** `EditorGraph` / `EditorGraphNode` 鈫?`MEObject`; `MaterialEdGraph` / `MaterialEdGraphNode` inherit; `Material::m_Graph` 鈫?`shared_ptr` + `ME_PROPERTY(Instanced)`.
- **Test:** `RunMaterialAssetSerializationTests()` writes `%TEMP%/minengine_material_asset_roundtrip.memtl`, `ToFile` 鈫?`FromFile` 鈫?`FinalizeGraphAfterLoad`; checks inline JSON types, editor fields, Metallic link, Outer chain.
- **CLI:** `Editor.exe --material-serialize-test` (serialize only); `--material-ir-test` includes file round-trip at end.

### Material system Phase 1 design (2026-05-22)
- **Plan:** `docs/ai/MATERIAL_SYSTEM_PHASE1.md` 鈥?P1.1 Capability+Blend, P1.2 Normal, P1.3 AO, P1.4 nodes; Translucent deferred to Phase 2.
- **Next implement:** P1.1 recommended first.

### Bug: MATERIAL-001 Constant3 鈫?Normal GLSL lowering crash (2026-05-23) 鈥?**Fixed**
- **Doc:** `docs/ai/bugs/MATERIAL-001-constant3-normal-glsl-lowering.md`锛涚绾垮鏌?`docs/ai/MATERIAL_PIPELINE_REVIEW.md`
- **Fix:** live-reachability `NumUsers`锛沗Constant3`/`TextureSample` 鎸夐渶瀛?output锛汫LSL lowering diagnostic + foldable multi-use inline锛沗VerifyConstant3ToNormalBlinnPhong` in `--material-ir-test`.
- **Also:** `ME_ASSERT` 鐜拌緭鍑?message 鍒?stderr銆?
### Material system Phase 3 accepted + Phase 4 IBL design (2026-05-23)
- **P3 done:** `MaterialShadingModel::PBR`銆丟GX銆乣MaterialPBR.glslinc`銆乣PBR.*.template`銆丆omponentMask銆乀extureSample.R銆丅linnPhong Roughness锛沗VerifyPBRWorkflow`锛涚敤鎴风洰瑙?OK銆?- **Docs:** [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) checklist 鉁咃紱[MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) **IBL only**锛坰plit-sum銆佷笁璐村浘 + BRDF LUT锛夈€?- **Deferred:** Parallax銆乄PO銆佺紪杈戝櫒 Undo銆佸彲閫?`MaterialIRSmoke_PBR.memtl`銆?- **Next:** 鐢ㄦ埛鏇撮珮浼樺厛绾ч潪鏉愯川宸ヤ綔锛涘疄鐜?P4 鏃朵粠 P4.1 鐜 cubemap 鍔犺浇 + Pass 缁戝畾寮€濮嬨€?
### Material system P2.3 Normal map workflow (2026-05-23)
- **Done:** `MaterialGraphNodeDef_NormalUnpack`锛坄rgb*2-1`锛夛紱璋冭壊鏉匡紱`VerifyNormalMapWorkflow`銆?- **Editor recipe:** TexCoord 鈫?TextureSample(Normal) 鈫?NormalUnpack 鈫?MaterialOutput.Normal锛堣 PHASE2 搂5.1锛夈€?- **Next:** 鐩 + 鍙€?`MaterialIRSmoke_NormalMap.memtl` 閲戞牱渚嬨€?
### Material system Phase 4 IBL 鈥?closed (2026-05-23)
- **P4.1鈥揚4.4:** `EngineIBLEnvironment` + unit 4鈥? bind锛坄pipeline` 淇锛夛紱`MaterialIBL.glslinc` / `CalcIndirectPBR`锛涚Щ闄ゅ父鏁?ambient锛沗u_EnvIntensity`銆?- **R2:** `EnvMapCapture` HDR 鈫?mipmapped cubemap锛沗brdf_lut.png` 鎴?`BrdfLutGenerator`銆?- **Test:** `--material-ir-test`锛圥BR IBL shader銆佺幆澧?init銆乵issing-assets fallback锛夈€?- **Doc:** [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) 搂5 鐩鎸囧崡锛汭BL README銆?- **Deferred:** irradiance 鍗风Н銆佷笓鐢?prefilter pass銆乻kybox銆乀ranslucent IBL銆?
### Material system P2.2 Tangent + TBN (2026-05-23)
- **Done:** `Vertex` + `a_Tangent`锛圓ssimp `CalcTangentSpace` + fallback锛夛紱`UsesTangentFrame`锛沗MaterialTangentFrame.glslinc`锛汢linnPhong TBN 璺緞锛涢粯璁?Normal TSN `(0,0,1)`銆?- **Next:** P2.2 鐩锛汸2.3 `NormalUnpack` + 娉曠嚎璐村浘鑺傜偣銆?
### IBL load timing owned by RenderSystem (2026-05-23)
- **Change:** Removed `EngineIBLEnvironment::Initialize(rhi, "")` from `RenderPipeline::Initialize`; single load via `RenderSystem::LoadEngineRenderingAssets()` after `PathRegistry` in `Engine::Initialize`.
- **Removed:** `PathRegistry::ReloadEngineDependentSystems`; Editor duplicate config/IBL bootstrap.

### Platform M0 PathRegistry + engine startup (2026-05-23)
- **Code:** `Runtime/Core/Paths/PathRegistry` 鈥?discover `EngineConfig.meconfig` (cwd, parent walk, CLI, env); relative `EngineDefaultAssetsRoot`; `ProjectManager` sets `ProjectContent`.
- **Config:** `EngineConfig.meconfig` 鈫?`"Assets/EngineDefault"` relative to engine root (config file directory).
- **CLI:** `--engine-config=`, `--engine-root=`; env `MINENGINE_ENGINE_CONFIG`, `MINENGINE_ENGINE_ROOT`.
- **Validation:** build Editor; `Editor.exe --material-ir-test` from `minEngine/bin`.

### docs/ai reorg + Platform design drafts (2026-05-23)
- **Layout:** Material/Render docs 鈫?`docs/ai/Render/Material/`锛沗RENDER_REFACTOR`銆乣RESOURCE_PIPELINE` 鈫?`docs/ai/Render/`锛汦ditor 瑙嗗彛 鈫?`docs/ai/Editor/`锛涙柊澧?`Platform/`銆乣README.md`銆乣.cursor/rules/docs-ai-layout.mdc`銆?- **Design:** [PLATFORM_ROADMAP.md](./Platform/PLATFORM_ROADMAP.md)銆乕ENGINE_STARTUP_DESIGN.md](./Platform/Startup/ENGINE_STARTUP_DESIGN.md)銆乕MEMORY_MANAGEMENT_DESIGN.md](./Platform/MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md)锛堝惎鍔?M0 鈫?**鍘熷湴閲嶆瀯 ObjectManager** M1/M2锛夈€?- **Direction:** UE-like锛涘弽灏?Lua/Content Browser/Undo 鎺掑湪骞冲彴绾夸箣鍚庛€?
### Material system Phase 5 P5.3 Skybox (2026-05-23)
- **Done:** `SkyBoxComponent` + `SkyBoxSceneProxy` + `SkyBoxPass`锛坄background.*`锛夛紱`RenderPipeline` Clear 鈫?SkyBox 鈫?BasePass锛沗SceneDrawFlags::EnableSkyBox`锛涚紪杈戝櫒鍦烘櫙瑙嗗彛鍚敤 flag锛沗test.mescene` SkyBox GO銆?- **Fix:** `SceneEditingViewportClient` 鏈紶 `EnableSkyBox` 瀵艰嚧澶╃┖涓嶇粯鍒躲€?- **Validation:** Editor `test` 鍦烘櫙鐩 OK锛沗cmake --build Editor`锛沗--material-ir-test`锛堜笌 P5.1鈥揚5.2 鍚屾壒锛夈€?- **Docs:** [MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md) 搂6.7 鍕鹃€夛紱搂10 6a鈥?c/7 鉁呫€?
### Material system P2.1 Translucent (2026-05-23)
- **Done:** `MaterialBlendMode::Translucent`锛沗Material::IsTranslucent()`锛汣apability Opacity锛汥etails **Translucent**锛沗--material-ir-test`銆?- **Fix:** `TranslucencyPass` 涓?`BasePass` 涓€鑷寸粦瀹?`LightsData` + shadow锛圔linnPhong Translucent 涓嶅啀鍏ㄩ粦锛夈€?- **Next:** P2.1 鐩楠屾敹锛汸2.2 鍒囩嚎 + TBN銆?
### Material system Phase 1 accepted (2026-05-23)
- **Status:** P1.1鈥揚1.4 鐩楠屾敹閫氳繃锛沎MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md) checklist 宸插叏閮ㄥ嬀閫夈€?- **Automation:** `--material-ir-test`锛圲nlit/BlinnPhong銆丆onstant3鈫扤ormal銆両fThenElse銆乀exture 鍙屽睘鎬с€乨ivide poison銆乧apability struct锛夈€?- **Next:** **Phase 2.1** 鈥?`MaterialBlendMode::Translucent` + `IsTranslucent()` + `TranslucencyPass`锛堣 [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md) 搂3锛夈€?
### Material system P1.2鈥揚1.4 implemented (2026-05-22)
- **Done:** `MP_Normal` + `EI_WorldNormal` 榛樿銆乣MP_AO`銆丅linnPhong 妯℃澘鐢?`FragmentMaterialInputs.Normal/AO`锛沗IsPropertyEmittedAtCompile`锛圲nlit 涓嶇敓鎴?Hidden 灞炴€э級锛涜妭鐐?Lerp/Subtract/Divide/Min/Max 鍙垱寤?+ Lerp `Alpha` 绫诲瀷锛沗MaterialIRSmoke.memtl` MaterialOutput pin 椤哄簭杩佺Щ锛沗--material-ir-test` Unlit + BlinnPhong GPU 鍙岃矾寰勩€?
### Material system P1.1 + Phase 2 design (2026-05-22)
- **Done:** P1.1 `MaterialBlendMode` (Opaque/Masked), `MaterialCapabilityUtil`, editor Blend Mode, Masked `discard`, golden `MaterialIRSmoke.memtl` + `m_BlendMode`, on-disk / IR smoke 瀛楁鏍￠獙銆?- **Plan:** `docs/ai/MATERIAL_SYSTEM_PHASE2.md` 鈥?P2.1 Translucent+Pass, P2.2 TBN+tangent, P2.3 normal map; Masked 涓嶅啀閲嶅銆?
### Material system Phase 0 implemented (2026-05-22)
- **P0-A:** Preview camera `eye(1.15, 0.8, 1.15)` in `MaterialPreviewViewport`.
- **P0-B:** `MaterialValueType` + `MaterialValueTypeUtil`锛沗MaterialEdGraph::CanConnectPins`锛汦ditor `TryConnectPins` 鎷掔粷闈炴硶杩炵嚎锛沗ValidateMaterialAsset` 鏍￠獙锛沗--material-ir-test` 澧炲姞 pin 娴嬭瘯 + `MaterialIRTestObjectManagerScope`锛坒riend `ObjectManager`锛夈€?- **Next:** Phase 1 璇︾粏璁捐锛圢ormal銆丆apability銆佸皯閲忚妭鐐癸級锛沗MaterialIRSmoke.memtl` Editor 鐩鍥炲綊銆?
### Material system roadmap 鈥?Phase 0 spec (2026-05-22)
- **Plan:** `docs/ai/MATERIAL_SYSTEM_ROADMAP.md`

### Material Editor E0鈥揈4 plan sign-off (2026-05-22)
- **Plan:** `MATERIAL_EDITOR_PLAN.md` 楠屾敹鍕鹃€夊凡鍏ㄩ儴瀹屾垚锛圗0鈥揈4锛夛紱鐢ㄦ埛鐩 + `--material-ir-test` 纭銆?- **Deferred锛堣鍒掑锛?** 鐢诲竷鍙抽敭 Palette锛涚嫭绔?`Material*View` 鍒嗗眰锛沀ndo/Command 闃熷垪锛汣ontent Browser 鍙屽嚮鎵撳紑銆?
### Material Editor E0鈥揈2 鈥?UI mode + Preview + node graph (2026-05-19 ~ 2026-05-22)
- **EditorUIMode:** `SceneEditing` 鈫?`MaterialEditing`锛沗EditorWindowSuite`锛圫hared / Scene / Material锛夛紱鍒囨崲鏃堕噸寤?Dock銆?- **Material 濂椾欢锛?* `MaterialGraphWindow`锛堝彸锛孭icker/Compile/Save + node-editor 鐢诲竷锛夈€乣MaterialPreviewWindow`銆乣MaterialDetailsWindow`銆?- **MaterialEditor锛?* Session/鍛戒护涓灑锛堥潪 Window锛夛紱`OpenSession` / `Save` / `Compile` / `NotifyGraphChanged`锛沗InvalidateGraphCanvas()` 閫氱煡鍥剧獥鍒锋柊銆?- **E1.5锛?* `MaterialEditorPreview` 鎷ユ湁棰勮涓栫晫锛沗MaterialPreviewViewportClient` 浠?resize + Submit锛汭mGui 鍦?`*Window` 鍐咃紙鏃犵嫭绔?`*View` 灞傦級銆?- **E2锛堝凡楠屾敹锛夛細** imgui-node-editor锛沗MaterialGraphIds`锛圢ode/Pin/Link 鍒?tag锛岄伩鍏嶄笌 NodeId 鍐茬獊锛夛紱`MaterialGraphNodeRegistry`锛涜繛绾?鏂嚎 鈫?`ConnectPins` / `DisconnectInput`锛涘叏闆跺潗鏍囪嚜鍔ㄧ綉鏍煎竷灞€锛涗粎 Invalidate 鏃?`SetNodePosition`锛堝彲鎷栬妭鐐癸級锛沗BeginCreate` 澶辫触涔熼』 `EndCreate`銆?- **娓叉煋锛?* Scene 妯″紡涓昏鍙?Submit锛汳aterial 妯″紡浠?`material_editor_preview` Submit銆?- **鏈湴澧為噺锛堟湭鍏ㄩ儴鍏ュ簱鏃朵互宸ヤ綔鍖轰负鍑嗭級锛?* `Reflection::GetDerivedClasses` + `MEClass::IsA<T>()`锛堜负 NodeDef 娉ㄥ唽琛?璋冭壊鏉块摵璺級锛涙洿澶?`MaterialGraphNodeDef_*` 鍙嶅皠娉ㄥ唽锛沗Runtime/Core/Hash/Hash.h`銆?- **E3锛?026-05-22锛夛細** E3.1 Details Combo 鍔犺妭鐐癸紱E3.2 `Registry::DrawNode` 闂ㄩ潰锛汦3.3 `MaterialNodeDefPropertyDrawer` 鍙嶅皠锛汦3.4 `RemoveNode` + 缂栬緫鍣?Delete 鑺傜偣锛堢姝㈠垹 MaterialOutput锛夈€?- **E4锛?026-05-19锛夛細** `MaterialCompileDiagnosticsDrawer`锛汣ompile debounce 0.3s + `MaterialEditor::Tick`锛沗Constant3` R/G/B 澶氳緭鍑哄紩鑴氾紱閫€鍑?Material 妯″紡鏃?`RemoveViewportClient` + Preview 浠?Material 妯″紡 Submit銆?
### Material Editor plan + imgui-node-editor vendoring (2026-05-19)
- **Plan:** `docs/ai/MATERIAL_EDITOR_PLAN.md` 鈥?E0鈥揈4锛涚獥鍙ｅ竷灞€ **宸?Preview+Details / 鍙宠妭鐐瑰浘**銆?- **Third-Party:** `minEngine/Third-Party/imgui-node-editor/`锛圛mGui 1.92 `imgui_extra_math` patch锛夈€?
### Golden MaterialIRSmoke + Editor default scene
- **2026-05-21:** `MyMEProject/Assets/Materials/MaterialIRSmoke.memtl` (+ `.meta`) committed as golden IR smoke graph asset.
- **Editor:** `PopulateEditorDefaultScene` loads via `AssetManager::LoadAsset<Material>` then `ApplyMaterialIRSmokeRuntimeDefaults` (white BaseColor texture).
- **Tests:** serialize round-trip also verifies golden `.memtl` deserializes + `FinalizeGraphAfterLoad`.

### Material asset texture refs + test cleanup
- **2026-05-21:** `TextureObject.DefaultTexture` serializes as texture asset `$guid` (`BaseColorWhite.png`); removed `ApplyMaterialIRSmokeRuntimeDefaults`.
- **Deprecated:** `Simple.memtl` (legacy MaterialResource JSON); `test.mescene` / `default.mescene` reference `MaterialIRSmoke` GUID.
- **Tests:** removed `MaterialAssetSerializationTest` and `--material-serialize-test`; `MaterialIRTest` keeps MIR compile/GPU smoke only (asset tests TBD).

### 2026-05-26 - Platform ROADMAP 搂8 瀹屾垚鎯呭喌鎬昏
- Goal:
	Sync PLATFORM_ROADMAP with design docs and repo state (P0鈥揚5, P2 submodules, Material maintenance).
- Main changes:
	`PLATFORM_ROADMAP.md` 搂8 per-module done/deferred tables + summary; 搂2/搂3/搂5 updated (P0/P1 no longer 鈥滃叏鍚庣疆鈥?.
- Next step:
	E1 鈫?P7 per 搂8 鎬荤粨椤哄簭.

### 2026-05-27 - ROADMAP 鏂颁换鍔″畨鎺掞紙婊氬姩锛夎惤鐩?- Goal:
	Record today鈥檚 incremental task order aligned with ROADMAP, while leaving room for additional tasks.
- Main changes:
	`PLATFORM_ROADMAP.md` 鏂板 搂10锛圓/B/C 涓夋潯涓荤嚎锛夛紱鏂板 `docs/ai/Editor/EDITOR_TASK_ROLLOUT_2026-05-27.md`锛堟帹杩涘垏鐗?S1鈥揝6锛夈€?- Next step:
	鎸?S1 寮€濮嬶細鍙抽敭 action 缁熶竴鎶借薄 + Content Browser 鍏堟帴鍏ャ€?
### 2026-05-27 - M1 鏀跺彛锛歊eveal/Rename 鏆傜紦锛堣璁★級
- Goal:
	Align design after trial: drop non-cross-platform Reveal and modal CB Rename.
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` 搂6.1銆伮?2銆伮?3銆伮?5.4锛況ollout S1b 浠ｇ爜娓呯悊椤广€?- Next step:
	M2 Hierarchy + Inspector.

### 2026-05-27 - CB Import/Delete 鍏ㄩ噺鍒锋柊闂璁板綍
- Doc:
	`docs/ai/Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md`锛沗CONTENT_BROWSER_DESIGN.md` 閾炬帴銆?- Next:
	鍚庣画浼樺寲 AssetTreeModel 澧為噺鍒锋柊 / 閫氱煡鍚堝苟銆?
### 2026-05-27 - M1 浠ｇ爜娓呯悊锛圧eveal/Rename 绉婚櫎锛?- Main changes:
	鍒?`EditorPlatformShell`锛汣B Action 浠?Delete/Import/Refresh锛涚Щ闄?AssetWorkflow/ContentBrowser Rename 妯℃€侀摼銆?- Next step:
	M2.

### 2026-05-27 - 鍙抽敭鑿滃崟 M1锛圕ontent Browser锛?- Goal:
	Scheme A visibility + CB context menu actions.
- Main changes:
	`IsVisibleInMenu`锛沗ContentBrowserMenuContext`锛涗簲 CB Action锛汣B 鏍?Tile/绌虹櫧鍙抽敭锛沗ImportAssetDialog(rel)`锛沗EditorPlatformShell` Reveal銆?- Next step:
	M2 Hierarchy + Inspector.

### 2026-05-27 - 璁″垝瀵归綈锛圡0 Done / M1 寰呮壒锛?- Goal:
	鍕鹃€?S0锛涜璁?搂15 瀹炵幇瀵圭収锛汳1 瀹℃壒椤癸紙鑿滃崟鐏版樉 A/B锛夈€?- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` 搂13鈥撀?5锛沗EDITOR_TASK_ROLLOUT` 瀛愪换鍔¤〃锛沗PLATFORM_ROADMAP` 搂10 澶囨敞銆?- Next step:
	鐢ㄦ埛瀹℃壒 搂15.3 鈫?M1 ContentBrowser銆?
### 2026-05-28 - TEST-F02 doctest, minEngineTests, Tests/ layout
- Goal:
	Move headless suites out of Runtime; vendored doctest; primary entry minEngineTests.exe.
- Main changes:
	`minEngine/Tests/` + `Third-Party/doctest/`; `Editor.exe test` forwards to minEngineTests;
	`scripts/verify.ps1` uses minEngineTests; one `TEST_CASE` per suite with CHECK.
- Validation done:
	`minEngineTests.exe test smoke` exit 0; `verify.ps1` exit 0.
- Next step:
	Optional: split large suites into finer doctest cases; per-suite runtime reset (reflection ordering).

### 2026-05-28 - TEST-F01 unified test runner (S01鈥揝05)
- Goal:
	TestRunner + registry + TestContext; all five suites via `Editor.exe test`; verify.ps1.
- Main changes:
	`Runtime/Test/*`; `main.h` legacy pre-parse + TestRunner dispatch; removed `ShouldRun*` from `*Test.cpp`.
	`scripts/verify.ps1`; smoke order puts reflection-function first (in-process isolation).
- Validation done:
	`cmake --build minEngine/build --target Editor`; `test smoke`/`test material-ir`/legacy flags; `.\scripts\verify.ps1`.
- Next step:
	TEST-F02 鈥?doctest + `minEngine/Tests/` + `minEngineTests.exe`.

### 2026-05-28 - TEST-F03 suite slim-down complete (S00鈥揝06)
- Goal:
	Doctest cases per suite; fixture B; smoke/full tags; faster verify smoke.
- Main changes:
	All five suites split; `EngineReflectionFixture` / `EngineTestFixture`; Material IR smoke vs full;
	reflection meta/invoke smoke, types/static/ref full; active docs CLI strings updated.
- Validation done:
	`minEngineTests test smoke` exit 0.
- Next step:
	Optional: further split reflection/material cases; enable C3 FillOut when registered.

### 2026-05-28 - TEST-F03 S00鈥揝01 fixture B + object-manager split
- Goal:
	Implement fixture B smoke order; split object-manager into doctest cases.
- Main changes:
	`EngineReflectionFixture` / `EngineTestFixture`; `DoctestSuiteRunner::RunSuiteForContext`;
	smoke order in `TestSuiteRegistry`; `ObjectManagerTest` 鈫?3 `TEST_CASE`s; reflection A6/A7/B6 skip when already Ready.
- Validation done:
	`minEngineTests test object-manager`, `test reflection-function`, `test smoke` exit 0.
- Next step:
	F03-S02 serialization-archive split.

### 2026-05-28 - TEST-F03 suite slim-down plan (fixture B)
- Goal:
	Per-suite doctest cases; central reflection init; faster smoke; dependency-ordered registry.
- Main changes:
	`Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md`; Registry `TEST-F03` Planned; INFRASTRUCTURE/TEST_UNIFIED/BOOTSTRAP links.
- Validation done:
	Docs-only; implementation starts at F03-S00 (fixture + smoke order).
- Next step:
	F03-S00 validation gate, then F03-S01 object-manager split.

### 2026-05-28 - Legacy headless test flags removed (commit)
- Goal:
	Drop deprecated `--*-test` argv; align infra docs with minEngineTests primary UX.
- Main changes:
	`TestRunner`, `ApplicationCommandLine`, `main.h`, `TestMain`; TECH_DEBT TD-001鈥?03 Done.
- Validation done:
	`minEngineTests.exe test smoke` exit 0.
- Next step:
	TEST-F03 planning (see entry above).

### 2026-05-28 - TEST-F01/F02 unified test runner design
- Goal:
	Design engine TestRunner (self-built) + doctest in F02; smoke/full tables; verify.ps1 path.
- Main changes:
	`Platform/Test/TEST_UNIFIED_DESIGN.md`, `TEST_F01_IMPLEMENTATION.md`, `TEST_F02_LAYOUT_MIGRATION.md`;
	Registry `TEST-F01`/`TEST-F02`; INFRASTRUCTURE_ROADMAP M1鈥揗2 Done, M3鈥揗4 planned.
- Validation done:
	Docs-only; user approved doctest + Editor-then-minEngineTests exe strategy.
- Next step:
	TEST-F01-S01 implementation.

### 2026-05-28 - CLI-F01 S01鈥揝03 unified command-line parse + dispatch
- Goal:
	Introduce CLI11-backed `ApplicationCommandLine`; single `main.h` dispatch; migrate Material IR test to `test material-ir`.
- Main changes:
	`Third-Party/CLI11/CLI11.hpp` (v2.4.2); `Runtime/Core/CLI/*` (`CommandLineResult`, `ApplicationCommandLine`).
	`main.h` 鈥?parse first; `test material-ir` + legacy `--material-ir-test` alias (warn); other `--*-test` flags unchanged until TEST-F01.
	`PathRegistry::LoadEngineConfiguration(CommandLineResult)`; `Editor` project path from parsed result (removed argv scan loop).
- Validation done:
	`cmake --build minEngine/build --target minEngine Editor`.
	`Editor.exe --help`, `Editor.exe test --help`, `Editor.exe test material-ir` exit 0; `--material-ir-test` warns + exit 0.
- Next step:
	TEST-F01 鈥?suite registry, migrate remaining four `--*-test` flags, `test smoke`/`full`.

### 2026-05-28 - CLI-F01 unified command-line design (approved)
- Goal:
	Design CLI: subcommand = mode, -- = params; CLI11 + ApplicationCommandLine.
- Main changes:
	`Platform/CLI/CLI_UNIFIED_DESIGN.md`, `CLI_F01_IMPLEMENTATION.md`; Registry/roadmap/README links; Status Planned.
- Validation done:
	User approved convention and command tree.
- Next step:
	CLI-F01-S01 implementation (vendor CLI11 + parse core).

### 2026-05-28 - Infrastructure roadmap (CLI 路 Test 路 Verify)
- Goal:
	Near-term plan: CLI unification, test centralization, verify + DoD; defer product features.
- Main changes:
	`Platform/INFRASTRUCTURE_ROADMAP.md`; Registry `CLI-F01`/`TEST-F01`; `PLATFORM_ROADMAP` 搂12; README/BOOTSTRAP/TECH_DEBT links.
- Validation done:
	Docs-only.
- Next step:
	`CLI-F01` Design Spec + S01 when implementation starts; commit batch B.

### 2026-05-28 - Bootstrap digest and tech debt register
- Goal:
	One-page session recovery + explicit debt queue before infra roadmap.
- Main changes:
	`BOOTSTRAP_DIGEST.md`; `TECH_DEBT.md` (TD-001..012); `README` links; session-bootstrap reads digest.
- Validation done:
	Docs-only.
- Next step:
	Commit batch A; then infra roadmap (CLI + Test + verify) 鈥?not in digest/debt files.

### 2026-05-28 - WF collaboration v2 (registry, Slice DoD, handoff)
- Goal:
	Team workflow: Feature registry, engineering+doc Slice DoD, session handoff; rules/skills wired.
- Main changes:
	`FEATURE_REGISTRY.md`; `DOC_GOVERNANCE` 搂7 Slice DoD, 搂9.1 Handoff, registry rule; `docs-workflow-triggers` handoff row; mentor + git-commit-mentor DoD; `WORKING_WITH_AI` handoff prompt.
- Validation done:
	Docs-only; no build required.
- Next step:
	Use registry for next Feature (e.g. TEST-F01 CLI); optional post-commit hook later.

### 2026-05-27 - P4/P5 Core 璺嚎瀵归綈锛堝嚱鏁板弽灏?+ 濮旀墭 + Lua锛?- Goal:
	Align platform roadmap with next master work: MEFunction, delegates, Lua scripting.
- Main changes:
	`PLATFORM_ROADMAP.md` 搂11 Core 涓荤嚎锛汸4/P5 璁捐鑽夌 `Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md`銆乣Scripting/LUA_SCRIPTING_DESIGN.md`锛沗PROJECT_CONTEXT` / `README` 閾炬帴鏇存柊銆?- Next step:
	P4.1 `MEFunction` 鎻忚堪绗?+ 鎵嬪啓娉ㄥ唽涓€涓祴璇曞嚱鏁般€?
### 2026-05-27 - 鍙抽敭鑿滃崟 M0 楠ㄦ灦钀藉湴
- Goal:
	EditorContextMenuSystem + Context/Registry/Builder shells; wire IEditorContext.
- Main changes:
	`minEngine/Editor/src/ContextMenu/`锛圗ditorMenuContext銆両EditorAction銆丒ditorActionRegistry銆丒ditorMenuBuilder銆丒ditorContextMenuSystem锛夈€?	`IEditorContext::GetContextMenu()`锛沗Editor` PostInitialize/OpenProject RegisterBuiltInActions銆丆loseProject/Shutdown Shutdown銆?- Next step:
	M1 ContentBrowser 鎺ュ叆 + 棣栨壒 Action 娉ㄥ唽銆?
### 2026-05-27 - 鍙抽敭鑿滃崟璁捐 v2锛堝綊灞?+ 闂ㄩ潰锛?- Goal:
	Clarify Editor infrastructure ownership; single GetContextMenu() facade.
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` 搂1 褰掑睘涓庣敓鍛藉懆鏈燂紱`EditorContextMenuSystem` 缁勫悎 Registry/Builder锛涙槑纭笉缁ф壙 EditorServiceModule銆?- Next step:
	User review v2 鈫?M0 implementation.

### 2026-05-27 - 鍙抽敭鑿滃崟璁捐閲嶇疆锛堝彲鎵╁睍鏋舵瀯锛?- Goal:
	Replace interim enum/service draft with UE-aligned extensible context-menu system.
- Main changes:
	閲嶅啓 `EDITOR_CONTEXT_MENU_DESIGN.md`锛圗ditorMenuContext 琚嬨€両EditorAction銆丷egistry銆丮enuBuilder銆丆ommand銆丮0鈥揗5锛夈€?	`docs/external/README.md`锛況ollout 鏇存柊 S0鈥揝3銆?
### 2026-05-27 - 鍙抽敭鑿滃崟缁熶竴璁捐鑽夋锛堝凡搴熷純锛?- 杞婚噺 `EditorContextMenuService` + enum 鏂规宸茬敱涓婃潯閲嶇疆鐗堟浛浠ｃ€?
### 2026-05-26 - Content Browser P6.1-polish implemented
- Goal:
	Flat Caption breadcrumb + square icon tile grid per 搂2.6.
- Main changes:
	`ContentBrowserWindow` 鈥?`ViewMetrics`, `DrawBreadcrumbLink`, `DrawAssetTile`; Appearance Caption/Selection/Field tokens.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded.
- Next step:
	Editor 鐩 Dark/Light锛浡?.6.4 涓婚椤瑰嬀閫夈€?
### 2026-08-02 - master锛氬悎骞?luaScript + physics锛汿ransform quat 鈫?CORE-F03
- Goal:
	Integrate Lua scripting and Jolt physics on `master`; resolve Transform storage + Feature ID collision.
- Main changes:
	`luaScript` fast-forward 鈫?`master`锛涘啀 merge `physics`銆?	`Transform::Rotation` = physics `Quaternion`锛涗繚鐣?Script\*锛圥osition / MakeIdentity / SetPosition / Translate锛夈€?	Physics 鍘?`CORE-F01` Transform Feature 鏀瑰彿涓?**`CORE-F03`**锛堟枃妗ｈ矾寰勫悓姝ラ噸鍛藉悕锛夛紱Lua 淇濈暀 F01/F02銆?	Engine/CMake/friends/test suites 鍚堝苟涓や晶绯荤粺銆?- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`lua-script-mvp` + all physics suites + `smoke` 鈫?PASSED
- Next step:
	Delegate 绯荤粺璁捐锛圱D-006 / PHYS-F03锛夛紱闇€瑕佹椂鍙噯澶囨寮?commit 璇存槑銆?
### 2026-08-01 - CORE-F02 Done锛歋cript binding codegen 鏀跺彛
- Goal:
	Close Feature after S01鈥揝07 vertical delivery.
- Main changes:
	Registry / Design / ACTIVE_WORK / Platform README / PROJECT_CONTEXT 鈫?Done.
	Out of Feature: broader GO API surface, Matrix/quat ME_STRUCT, weak handles.
- Validation done:
	Prior `lua-script-mvp` PASSED; docs-only closeout amend.
- Next step:
	Optional follow-ups on `luaScript`, or switch tracks.

### 2026-08-01 - CORE-F02 S05+S07锛氭暟瀛﹀師璇墜鍐?+ 鐢熸垚瑙勫垯鍔犲帤
- Goal:
	Keep VectorN hand-bound; harden ScriptBinding codegen (bases, Pure/static, topo order).
- Main changes:
	`LuaScriptBindingPrimitives`: Vector2/Vector3/Vector4.
	header tool: dependency topo-sort; `sol::base_classes` + `sol::bases<>`; ScriptPure鈮allable.
	`LuaComponent` ScriptType + `IsScriptLoaded`; inject `self` as LuaComponent*; `Transform::MakeIdentity` ScriptPure.
- Validation done:
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` 鈫?PASSED
- Next step:
	鍑嗗 commit锛涘彲閫?Editor 鍐嶉獙 HelloTick銆?
### 2026-08-01 - CORE-F02 S06锛氬満鏅叆鍙?self / Owner / Translate
- Goal:
	Let LuaComponent scripts reach Owner and write back Root transform via generated bindings.
- Main changes:
	Inject `self` as `Component*` in Lua env; ScriptType+GetOwner on Component; ScriptType+Get/SetPosition/Translate on GameObject.
	`sol::no_constructor` for MEObject types; register GameObject before Component; `LuaScriptBindingSolTraits`.
	HelloTick scene-entry check + slow Z drift; unit test `TestSceneEntrySelfOwnerTranslate`.
	Fix: `GameObject::AddComponent_Internal` used `IsA(Component)` (all comps) instead of `SceneComponent` 鈥?broke adding LuaComponent after root.
- Validation done:
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` 鈫?PASSED
- Next step:
	Editor smoke (`SceneEntry OK` + visible drift); 鍑嗗 commit.

### 2026-08-01 - CORE-F02 S04锛欻elloTick 鐢熸垚缁戝畾鑷
- Goal:
	Exercise auto-generated Transform/Vector3 bindings from an editor sample script (compact, not a full matrix).
- Main changes:
	`HelloTick.lua` one-shot verify: new / Position R/W / SetPosition / Translate; then heartbeat log.
- Validation done:
	Editor锛欻elloTick 鎸?LuaComponent 鈫?鏃ュ織 `HelloTick: ScriptBinding OK`锛堢敤鎴风‘璁?2026-08-01锛?- Next step:
	S06 鍦烘櫙瀵硅薄鍏ュ彛璁ㄨ / 瀹炵幇銆?
### 2026-07-31 - CORE-F02 S01鈥揝03锛歋criptBinding codegen + Transform
- Goal:
	Opt-in Script* 鈫?`Generated/ScriptBinding/` sol2 usertypes; first real type `Transform`.
- Main changes:
	header tool: `--script-binding-*`, `ScriptType`/`ScriptCallable`/`ScriptRead*`; skip reflection thunks for non-MEObject.
	`LuaScriptBindingPrimitives` (`Vector3`); `RegisterGeneratedLuaBindings` after Manual.
	`Transform`: ScriptType + Position ScriptReadWrite + SetPosition/Translate ScriptCallable.
	Test: `lua-script-mvp` Transform case.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` 鈫?PASSED
- Next step:
	鍑嗗 commit锛汼04+ 鎵╃被鍨嬫寜闇€銆?
### 2026-07-31 - CORE-F01 Done锛汣ORE-F02 Script Binding Draft
- Goal:
	Close Lua runtime Feature; open binding-codegen Feature with Script* specifiers.
- Main changes:
	`LUA_SCRIPTING_DESIGN` Status Done锛涘師 S06 绉讳氦銆?	Registry: CORE-F01 Done锛汣ORE-F02 Draft 鈫?[LUA_SCRIPT_BINDING_DESIGN](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md).
	Generated path plan: `Generated/ScriptBinding/`锛堜笌 Reflection 鍒嗙锛夛紱self=C++ 鎸囬拡銆?- Validation done:
	Docs + prior editor HelloTick acceptance (F01).
- Next step:
	Review F02 Draft锛堥绫诲瀷 Transform 瀛愰泦锛夛紱纭鍚?Planned 鈫?S01 header tool.

### 2026-07-31 - CORE-F01 S05锛歀uaScript 璧勪骇绔栧垏
- Goal:
	Treat `.lua` as a first-class asset (Font-style); drive LuaComponent from asset source.
- Main changes:
	`LuaScript` Asset + `LuaScriptLoader`; register `.lua` in `AssetTypeRegistry` / `AssetManager`.
	`LuaComponent` holds `shared_ptr<LuaScript>` (hardcoded chunk removed).
	Suite covers in-memory SetSource + RegisterAsset/LoadAsset disk fixture.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` 鈫?PASSED
- Next step:
	鍑嗗 commit锛汼06 codegen 鎴?Component 鍦烘櫙搴忓垪鍖栨寜闇€鎺掓湡銆?
### 2026-07-31 - CORE-F01 MVP锛圫01鈥揝04锛夎惤鍦?- Goal:
	Feasibility vertical slice: sol2 state 鈫?probe bindings 鈫?hardcoded LuaComponent tick 鈫?destroy clears env.
- Main changes:
	CMake: Lua 5.4.5 (`minengine_lua`) + sol2 pin `d805d027`锛圙CC 15 `optional::emplace` fix锛?
	`LuaScriptSystem` / `LuaManualBindings` / `LuaBindProbe`锛汦ngine Init/Shutdown 鎸傛帴銆?	`LuaComponent` 鍐呭祵 chunk + per-instance env锛涙瀽鏋?`UnloadScript`銆?	Suite `lua-script-mvp`锛堥潪 smoke/full锛夈€?- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` 鈫?PASSED
	Follow-up: Lua/sol2 moved from FetchContent into `Third-Party/` (offline/reviewable).
- Next step:
	鍑嗗 commit锛汼05+锛堟枃浠?璧勪骇/codegen锛夊緟鐢ㄦ埛鎺掓湡銆?
### 2026-07-30 - CORE-F01 Lua scripting锛氬垎鏀?+ 鐧昏 + 璁捐鍒濈
- Goal:
	Start Lua track on a dedicated branch; register Feature; replace placeholder design with Draft Spec.
- Main changes:
	Branch `luaScript` from `master`.
	`FEATURE_REGISTRY`: `CORE-F01` In Progress 鈫?[LUA_SCRIPTING_DESIGN](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md).
	`ACTIVE_WORK`: CORE-F01 focus alongside RND-F02锛堝垎杞級.
	Design: System 鈫?鎵嬪啓 sol2 鐧藉悕鍗?鈫?LuaComponent 鈫?瀵垮懡澶辨晥 鈫?璧勪骇 鈫?header tool codegen.
- Validation done:
	Docs-only; no build.
- Next step:
	User review Design Draft锛涘彲閫夎ˉ Implementation Plan锛涚劧鍚?S01 寮曞叆 sol2 + `LuaScriptSystem`.
### 2026-06-01 - RND-F02 S5 remaining passes + native texture handle
- Goal:
  Complete migration wave 2: scene render pass via modern RHI; remove glad from RenderPasses.
- Main changes:
  `RenderPipeline` scene path `RHICmdBeginRenderPass`; Base/Translucency draw via `RHICommandList`; PostProcess/SkyBox modern PSO.
  `GetRHINativeTextureHandle` + Editor viewport/thumbnails; PSO `RHIDepthCompareFunc`/`RHICullMode`; fix cull vs depth-clip mapping.
- Risks or caveats:
  Materials still `RHIShaderLegacy` + `BindForDraw`; Point shadow still legacy FBO.
- Validation done:
  `.\scripts\verify.ps1`.
- Next step:
  S5+: material binding migration; delete Legacy `RHI` public API.

### 2026-06-01 - RND-F02 S4 OpenGL modern path + Present/Shadow migration
- Goal:
  Implement S3 contract on OpenGL; migrate Present and Directional/Spot shadow passes to `RHICommandList`.
- Main changes:
  `OpenGLRHIModern.{h,cpp}` (texture/buffer/shader/layout/SRV/binding wrappers); `OpenGLRHI` `RHICreate*`/`RHICmd*` (transient FBO, PSO fallback, draw/bind).
  `PresentPass` / `ShadowPass` use modern render pass + pipeline; `SceneRenderTarget::BuildRenderPassInfo`; `RenderPipeline` passes `RHICommandList`.
  Point shadow still Legacy `FrameBuffer` + `rhi->Clear()`. `RHIGraphicsPipelineStateRef`; raw-pointer legacy wrap overloads.
- Risks or caveats:
  Editor visual regression not automated; Point shadow hybrid until S5.
- Validation done:
  `.\scripts\verify.ps1` (build + smoke).
- Next step:
  S5: remaining passes, ImGui native handle, engine shaders via BindingSet.

### 2026-06-02 - WF-F02 handbook UX phase 2 (S04鈥揝07)
- Goal:
  Auto nav, TOC, sidebar, and git last-updated footer for public handbook.
- Main changes:
  `mkdocs-awesome-pages-plugin` + `docs/handbook/**/.pages` (no root `nav` in mkdocs.yml).
  Material: `toc.follow`, `toc_depth 2-3`, `navigation.prune/path/top/indexes`.
  `mkdocs-git-revision-date-localized-plugin`; docs workflow `fetch-depth: 0`.
  `_authoring.md` + `exclude_docs`; expanded `render/overview.md` for TOC sample.
- Validation done:
  `mkdocs build --strict`.

### 2026-06-12 - Worktree bootstrap: submodules, scene asset migration, Editor run
- Goal:
  Fix physics worktree third-party gitdir paths; migrate scene Rotation to quaternion; ensure Editor launches.
- Main changes:
  `scripts/fix-worktree-submodule-gitdirs.ps1`, `scripts/migrate_transform_rotation_to_quat.py`;
  `default.mescene` / `test.mescene` Rotation 鈫?`{W,X,Y,Z}`; `MyMEProject.meproject` ProjectRoot 鈫?physics worktree.
- Validation done:
  `git status` OK after submodule gitdir fix; Editor loads `test` scene successfully with explicit `--engine-config`.
- Next step:
  Commit CORE-F01 code + asset migration batch.

### 2026-06-12 - CORE-F01 S01+S02+S04 partial: Quaternion storage and Scene API
- Goal:
  Land Transform quaternion storage (GLM-backed), Scene/RenderCamera API, Inspector Euler widget mapping.
- Main changes:
  `Quaternion.h/.cpp`, `Transform.h`, `SceneComponent`, `GameObject`, `RenderCamera`,
  `TransformWidget`, Editor viewport camera + PreviewScene + Playground call sites;
  reflection codegen for `Quaternion`; serialization round-trip test for `Transform.Rotation`.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor`; `minEngineTests.exe test smoke` PASSED.
- Next step:
  CORE-F01-S05 call-site grep sweep; S06 verify + commit; then PHYS-F01.

### 2026-06-11 - CORE-F01 Transform quaternion design (physics branch)
- Goal:
  Register Transform storage migration as `CORE-F01` before `PHYS-F01` (Jolt); document scope, Inspector Euler widget mapping, and slice plan.
- Main changes:
  `docs/ai/Platform/Core/CORE-F01_TRANSFORM_QUATERNION_DESIGN.md`,
  `CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md`;
  `FEATURE_REGISTRY.md` (`CORE-F01` Planned, `PHYS-F01` blocked);
  `ACTIVE_WORK.md` priority order updated.
- Validation done:
  Docs only; no build.
- Next step:
  Design 搂6 decisions recorded (D4 no auto read; D5 RenderCamera in scope) 鈫?`CORE-F01-S01`+`S02` first landable PR.

### 2026-06-01 - WF-F02 handbook nav under `runtime/` tree
- Goal:
  Align handbook paths and stubs with revised `mkdocs.yml` (Runtime tab + Function/Platform/Resource children).
- Main changes:
  `docs/handbook/runtime/**` placeholders; removed flat `runtime/{function,platform,resource}/overview.md`; design/impl docs updated.
- Validation done:
  `mkdocs build --strict`.

### 2026-06-11 - PHYS-F01 Jolt bootstrap design + implementation plan
- Goal:
  Physics subsystem bootstrap scope: Jolt vendor, thin `Physics/` facade, `RigidBodyComponent` + `BoxColliderComponent`, fixed-step simulate + pose pull before `SendAllEndOfFrameUpdates` (UE-aligned tick order).
- Main changes:
  `docs/ai/Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md`, `PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md`; P1鈥揚9 defaults recorded; `FEATURE_REGISTRY` / `ACTIVE_WORK` 鈫?PHYS-F01 In Progress.
- Next step:
  PHYS-F01-S01-a (Jolt submodule + CMake).

### 2026-08-01 - PHYS-F03 deferred pending Delegates (TD-006)
- Goal:
  Avoid shipping Collider virtual contact notify as a permanent API before multicast Delegates exist.
- Main changes:
  PHYS-F03 鈫?Deferred placeholder; deleted Implementation plan draft; TD-006 notes block PHYS-F03 (severity Medium).
- Next step:
  User picks next physics or CORE Delegate work; gameplay can poll GetContactEvents() meanwhile.

### 2026-08-01 - PHYS-F02 Sphere/Capsule colliders + shape traces
- Goal:
  Add Sphere/Capsule colliders and Scene SphereTrace/CapsuleTrace on the F01 channel/filter path.
- Main changes:
  `SphereColliderComponent` / `CapsuleColliderComponent`; `RigidBodyComponent::FindColliderComponent`;
  `PhysicsWorld` polymorphic shape create + `CastShapeTrace`; `Scene::{Sphere,Capsule}Trace`;
  suite `physics-shapes`; editor side effects for `m_Radius` / `m_HalfHeight`.
- Risks or caveats:
  Capsule axis = engine Y (Jolt default); HalfHeight = cylinder half-height only.
  New reflected types need cmake reconfigure after first codegen so `.gen.cpp` enters the GLOB.
- Validation done:
  `minEngineTests.exe test physics-shapes` PASS; regression `physics-linetrace` / `physics-contact` / `physics-smoke` PASS.
- Next step:
  Prepare commit for PHYS-F02; then PHYS-F03 contact gameplay dispatch.

### 2026-08-01 - PHYS-F01-S03 Scene LineTrace
- Goal:
  Public `Scene::LineTrace` (UE UWorld-style); internal Jolt CastRay with Trace脳Object matrix filtering.
- Main changes:
  `HitResult` / `CollisionQueryParams` (no F prefix); `PhysicsWorld::LineTrace`; `Scene` forward;
  rename `PhysicsContactEvent`; `physics-linetrace` suite (hit/miss/ignore-self/trigger/visibility).
- Validation done:
  `minEngineTests.exe test physics-linetrace` + `physics-contact` + `physics-smoke` PASS.
- Next step:
  Prepare commit; PHYS-F01 bootstrap slices complete.

### 2026-08-01 - PHYS-F01-S02 collision channels + Contact events
- Goal:
  Land UE-style single `ECollisionChannel` + Ignore/Overlap/Block matrix, Trigger sensors, Contact Begin/End double-buffer.
- Main changes:
  `PhysicsTypes` (Channel/Response/ContactEvent + `CollisionChannelRegistry`); `ColliderComponent` + `BoxCollider` inherit;
  `PhysicsWorld` ObjectLayer/Sensor/`ContactListener` + `GetContactEvents`; `physics-contact` suite.
- Validation done:
  `minEngineTests.exe test physics-contact` + `physics-smoke` + `physics-sync` + `physics-load` (all PASS).
- Next step:
  Prepare commit for S02; then PHYS-F01-S03 (`LineTrace`).

### 2026-06-12 - PHYS-F01-S01-e scene鈫攑hysics sync (ETeleportType)
- Goal:
  Close S01 sync gaps before S02: `ETeleportType`, Transform dirty vs render dirty, Push/Pull, `bSimulatePhysics` gate Step + deactivate.
- Main changes:
  `PhysicsTypes.h` (`ETeleportType`); `SceneComponent` authority vs simulation writeback; `PhysicsWorld::SyncBodiesFromScene` / `SyncBodiesToScene`; `RigidBodyComponent::SetSimulatePhysics` hook; `physics-sync` test suite.
- Validation done:
  `minEngineTests.exe test physics-sync` + `physics-smoke` + `smoke`; Editor build.
- Next step:
  Commit S01-e; PHYS-F01-S02 (collision channels + Contact Begin/End).

### 2026-06-12 - PHYS-F01-S01 bootstrap complete (Jolt + physics vertical slice)
- Goal:
  Land S01-a鈥揹: Jolt submodule/CMake, PhysicsSystem/World, RigidBody+BoxCollider (P10 proxy), LogicalTick simulate, physics-smoke falling box.
- Main changes:
  `Runtime/Function/Physics/*`; Jolt submodule; Engine/SceneManager lifecycle; `RigidBodyComponent`/`BoxColliderComponent` + reflection; `PhysicsSmokeTest`; fix `GameObject::AddComponent_Internal` SceneComponent-only attach.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests`; `minEngineTests.exe test physics-smoke` + `test smoke` from `minEngine/bin`.
- Next step:
  Commit S01 batch; then PHYS-F01-S02 (collision layers + contact events).

### 2026-06-12 - PHYS-F01-S01-b PhysicsSystem and PhysicsWorld shell
- Goal:
  Engine singleton + per-Scene Jolt world with fixed-step accumulator; Scene load/unload lifecycle; PhysicsConversion axis helpers.
- Main changes:
  `Runtime/Function/Physics/*`; `Engine` Start/Shutdown; `SceneManager` create/load/unload hooks.
- Validation done:
  `cmake --build minEngine/build --target minEngine minEngineTests`; `minEngineTests.exe test smoke` from `minEngine/bin`.
- Next step:
  PHYS-F01-S01-c (RigidBodyComponent + BoxColliderComponent).

### 2026-06-12 - PHYS-F01-S01-a Jolt submodule and CMake link
- Goal:
  Vendor Jolt via git submodule; link `Jolt` static target into `minEngine` with nested `add_subdirectory(Jolt/Build)`.
- Main changes:
  `.gitmodules` + `Third-Party/Jolt`; `minEngine/CMakeLists.txt` cmake 3.20; `minEngine/minEngine/CMakeLists.txt` Jolt options and `target_link_libraries`.
- Validation done:
  `cmake --build minEngine/build --target minEngine minEngineTests`; `minEngineTests.exe test smoke`.
- Next step:
  PHYS-F01-S01-b (`PhysicsSystem` / `PhysicsWorld` shell).

### 2026-06-12 - PHYS-F01 design: RigidBodyComponent as physics proxy (P10)
- Goal:
  Align rigid body model with user intent: `RigidBodyComponent` is a `Component` agent, not `SceneComponent`; no own Transform; reads/writes GO RootComponent for physics sync.
- Main changes:
  Design 搂3.2, 搂4 P10, 搂5 options E/F; Implementation S01-c/d assembly and sync wording.
- Next step:
  User approval 鈫?commit docs; then S01-a.
### 2026-06-01 - RND-F02 planning + Modern RHI design draft (`render`)
- Goal:
  Start GPU-model Modern RHI track; separate from RenderGraph (RND-F01 deferred).
- Main changes:
  `FEATURE_REGISTRY` + `ACTIVE_WORK` render focus; `docs/ai/Render/RND-F02_MODERN_RHI_DESIGN.md` (Part A 鏁欐 + Part B 璁捐 + R0鈥揜3 slices).
  Branch `render` for subsequent implementation.
- Validation done:
  Docs only; `mkdocs build` N/A for ai/Render path.
- Next step:
  F02-R0: CommandList draw + remove `gl*` from RenderPasses.

### 2026-06-01 - WF-F02 handbook site skeleton (MkDocs + GitHub Pages CI)
- Goal:
  Public docs site aligned with `src/Runtime` top-level layers; placeholders only.
- Main changes:
  `docs/handbook/` (index, getting-started, core/function/platform/resource overview stubs).
  Root `mkdocs.yml`, `requirements-docs.txt`, `.github/workflows/docs.yml`.
  README link to https://max1122chen.github.io/minEngine/
- Validation done:
  `mkdocs build --strict` from repo root succeeded.
- Next step:
  Merge to `main`; enable Pages source GitHub Actions; fill handbook content per layer.

### 2026-05-26 - Roadmap sync (P3 Undo + E2 + P6.1)
- **Docs:** `PLATFORM_ROADMAP.md`銆乣EDITOR_PLATFORM_PLAN.md`銆乣PROJECT_CONTEXT.md` aligned with repo state.
- **Done (marked):** P6.1 CB UI; P3 Undo E1.1鈥揈1.4 + S1鈥揝2; E2.1鈥揈2.3a Inspector/Material viewport preview (`6ccd9bf`).
- **Deferred (consolidated):** E1 Inspector unification; E2.2b Texture preview; E2.3b CB thumbnails; E2.4; E1.5 Material Undo; Command E2 TryMerge; P7; P0/P1; P4/P5.

### 2026-06-05 - RND-F03 M1 tail (`render`, WIP)
- Goal:
  Complete M1 D/E/F tail after golden-scene visual sign-off (dir/point/spot + shadows OK).
- Main changes:
  SkyBoxPass + EnvMapCapture 鈫?`RHICreate*` + `SetBindingSet` + `BeginRenderPass` (no `FrameBuffer`/`WrapLegacy`).
  Material `BindForDraw` 鈫?Set2 `RHIBindingSet` (textures via `GetRHITexture()`); scalars still legacy uniform upload.
  EnvMap shaders 鈫?`#version 420` + `layout(binding=0)`.
  Deleted all `WrapLegacy*` (zero production callers).
  `TextureCubeLoader::CreateRenderTargetCube` / `WrapTextureCube`.
- Validation done:
  `cmake --build` minEngine + Editor; `.\scripts\verify.ps1` smoke + material-ir PASS.
  grep: Pass path `WrapLegacy` / `CreateUniformBuffer` / `CreateFrameBuffer` / `CreateVertexBuffer` = 0.
- Remaining (M2):
  Delete `RHI.h` Legacy API block; remove `Shader` Asset path; drop `m_RHITexture` dual-track on Texture2D/Cube; merge `OpenGLRHIModern` into `OpenGLRHI`.
- Next step:
  User final acceptance 鈫?commit; then M2 or RND-F04 prep.

### 2026-06-08 - RND-F03 M2 partial (`render`, WIP)
- Goal:
  Continue M2 after M1 user sign-off: engine passes off Shader Asset, texture single-track, merge OpenGLRHIModern, delete Legacy RHI resource API.
- Main changes:
  `EngineShaderUtils` 鈥?engine fixed shaders via `RHICreateShader` (Present/Shadow/Sky/Post/EnvMap/FXAA/Sharpen).
  `Texture2D`/`TextureCube` 鈥?single `RHITextureRef`; loaders use `RHICreateTexture2D` only.
  Deleted `OpenGLRHIModern.*`; added `OpenGLRHIResources.*`.
  `RHI.h` 鈥?removed `CreateVertexBuffer`/`CreateRHITexture*`/`CreateRHIShader` (Legacy); kept GL state toggles.
  Removed `EngineIBLEnvironment::BindForPBRDraw`; Editor thumbnail + MaterialIRTest use `RHITexture::GetNativeHandle()`.
  `TextureCubeLoader` 鈥?`CreateRenderTargetCube` / `WrapTextureCube`; dropped legacy cube factories.
- Validation done:
  grep (Render/): `OpenGLRHIModern`/`CreateRHITexture`/`GetRHITextureModern`/`Shader::CreateFromFiles` in Pass path = 0.
  Full build not re-run this session (long compile); prior blocker was EnvMapCapture missing `OpenGLShader.h` (fixed).
- Remaining (M2 tail):
  Material compile still uses `Shader` Asset + `RHIShaderLegacy`; scalar uniforms still `UploadUniformFloat`.
  Dead legacy impl files (`OpenGLBuffers`, legacy `RHITexture2D` types) still in tree.
- Next step:
  User local `cmake --build` + golden scene 鈫?commit; optional Material GPU program migration.

### 2026-06-01 - RND-F03 M1 complete + M2 sign-off (`render`)
- Goal:
  Finish F03 steps 2鈥?: Material GPU path, engine Pass UBO/BindingSet, legacy file deletion, grep gate, maintainer docs.
- Main changes:
  Material: `RHIShader` + Set2 scalar UBO (`binding=8`) + per-material PSO; removed `Shader` Asset (`Shader.*`, `ShaderLoader.*`, asset registration).
  Engine passes: Shadow/Post/Sky/EnvMap/Present 鈫?`EnginePassUniforms` UBO + `SetBindingSet`; shaders `#version 420` + fixed bindings.
  Legacy cleanup: deleted `RHIShaderLegacy`, `OpenGLBuffers.*`, `OpenGLVertexArrayObject.*`; trimmed `OpenGLHeaders.h`; `OpenGLRHIResources` upload texture ownership fix.
  Docs: `ACTIVE_WORK` / `FEATURE_REGISTRY` F03 鈫?Done; `AssetManager.h` drops stale `LoadAsset_Impl<Shader>`.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor` PASS.
  `.\scripts\verify.ps1` (smoke + material-ir) PASS.
  grep `Render/`: `WrapLegacy`, `RHIShaderLegacy`, `UploadUniform*`, `CreateVertexBuffer`, `OpenGLRHIModern` = 0.
  Golden scene visual OK (prior session user sign-off).
- Next step:
  User commit on `render`; promote F04 when ready.

### 2026-06-02 - RND-F03 M4 P0鈥揚2 pipeline refactor (`render`)
- Goal:
  Execute M4 adopted plan 搂9: detach runtime IBL/EnvMap, unify draw submission, move mesh PSO authority from Material to Pass + MeshDrawCommand; M3 backend type inline alongside.
- Main changes:
  **P0:** CMake excludes `EnvMapCapture` / `EngineIBLEnvironment` / `BrdfLutGenerator` from engine link; `RenderPipeline` drops IBL init; `SkyBoxPass` self-loads `environment_*` cubemap; `BuildSceneSet1` leaves IBL slots null; PBR template + assembler use direct light + simple ambient (`AO * 0.03`); MaterialIR IBL GPU tests skipped.
  **P1:** `RHICommandList::SubmitDraw` / `SubmitDrawMesh` 鈥?sole four-step draw path (PSO 鈫?bindings 鈫?VB/IB 鈫?Draw); all passes migrated (Shadow/Present/Post/Sky/mesh).
  **P2:** `RenderPassBase::PrepareMeshDrawCommands` builds per-draw PSO with `material shader + cmd.m_VertexInputLayout + pass fixed depth/blend`; `MeshDrawCommand::m_PipelineState`; `Material` no longer owns `m_PipelineState`.
  **M3 (partial):** Delete `OpenGLShader.*` / `OpenGLTexture.*`; logic inlined into `OpenGLRHIResources`; trim legacy `RHI.h` surface.
  **Docs:** `RND-F03-M4_PIPELINE_REFACTOR_DESIGN.md` (搂9 adopted plan); `FEATURE_REGISTRY` / `ACTIVE_WORK` / F03 搂16 pointers updated.
- Risks or caveats:
  Per-frame PSO `Create` per mesh draw (no cache yet); `Material::BindForDraw` still creates BindingSet each draw (P3); `RHICreateSRV` direct `new` unchanged (P0鈥?.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor` PASS.
  `.\scripts\verify.ps1` (smoke + material-ir) PASS.
  Editor golden scene visual OK (mesh layout / lighting / sky 鈥?user sign-off).
- Next step:
  P3 material BindingSet cache; optional PSO map per pass; P0鈥?SRV factory + remaining M3 cleanup.

### 2026-08-03 - RND-F10锛欵nvMapCapture 鍘?GL 鏃佽矾 + 閫€褰瑰叏灞€ IBL锛坄render`锛?- Goal:
	鐢ㄧ幇浠?RHI 琛ㄨ揪 mip/filter锛涘垹闄?`EngineIBLEnvironment` / `BrdfLutGenerator`锛汼06 鎸?TD銆?- Main changes:
	`RHICmdGenerateMips` + CommandList锛汷penGL cube 鎸?`NumMips` 鍒嗛厤锛汣apture 鍘绘帀 glad/`GetOpenGLTextureId`锛涘垹姝讳唬鐮佷笌 CMake exclude锛涚櫥璁?**TD-021**锛圗ditor Bake UX锛夈€?- Validation done:
	`mingw32-make minEngine/Editor/minEngineTests`锛沗.\scripts\verify.ps1` smoke PASS锛汦ditor bake 鏃ュ織浠嶉€氾紙irradiance/prefilter + baked HDR锛夈€?- Risks or caveats:
	`EnvMapCapture.cpp` 浠嶆湁鍖垮悕鍛藉悕绌洪棿闈欐€?bake helpers锛堜笌鍏?static Baker API 骞跺瓨锛涙湭鍐嶅紩鍏ュ紩鎿庡眰 GL锛夛紱BRDF 浠嶄緷璧栭」鐩?`brdf_lut.png`銆?- Next step:
	鐢ㄦ埛纭鏄惁灏?F10 鏍?Done锛涘噯澶?commit銆?
### 2026-08-03 - RND-F10 S05锛氶」鐩?HDR 鈫?澶╃┖/IBL bake锛坄render`锛?- Goal:
	鏃?face PNG 鏃朵粠椤圭洰 `m_SourceHdrPath` GPU bake 鐪熷疄澶╃┖锛堜粯娓?TD-015 涓昏矾寰勶級銆?- Main changes:
	鐜颁唬璺緞閲嶅紑 `EnvMapCapture`锛圥SO/BindingSet/`CreateShaderResourceView`锛夛紱`EnvironmentMap::TryBakeFromSourceHdr`锛沗DefaultEnvironment.meenv` 鎸囧悜 `citrus_orchard_puresky_1k.hdr`銆?- Validation done:
	`mingw32-make minEngine/Editor`锛汦ditor 鏃ュ織锛歚baked sky/IBL from project HDR` + irradiance/prefilter锛沗.\scripts\verify.ps1` smoke PASS銆?- Risks or caveats:
	浠嶅惈 `glGenerateMipmap`/glad锛圧HI 鏃?GenerateMips锛夛紱S04 鍏ㄥ眬 IBL 鍏ュ彛鏈垹锛汼06 Editor 鏄惧紡 Bake 鏈仛銆?- Next step:
	鐢ㄦ埛鐩 Viewport 澶╃┖锛涘彲閫?S04 / 鍘?glad / S06銆?
### 2026-08-03 - RND-F10 S01鈥揝03锛欵nvironmentMap 椤圭洰璧勪骇鎺ョ嚎锛坄render`锛?- Goal:
	EnvironmentMap 浠呴」鐩?Content锛汼kyBoxComponent ref 鈫?SkyPass / Set1锛汦ngineDefault 鍙綔澶嶅埗绉嶅瓙銆?- Main changes:
	`EnvironmentMap` + `.meenv` loader/registry锛汼ky proxy/pass 璺?Asset锛汼et1 IBL 浠庡満鏅?EnvironmentMap锛沗MyMEProject` 绉嶅瓙 IBL + `DefaultEnvironment.meenv`銆?- Validation done:
	`mingw32-make minEngine/Editor/minEngineTests`锛沗.\scripts\verify.ps1` smoke PASS锛沗test render-graph` PASS銆?- Risks or caveats:
	鏃?face PNG 鏃?validation cube锛汢RDF 闇€鍦?Inspector 鎸囧埌椤圭洰 `brdf_lut`锛汢ake锛圱D-015锛夋湭鍋氥€?- Next step:
	S04 娓呭叏灞€ IBL 鍏ュ彛锛汼05 鐜颁唬 Baker锛涚敤鎴风洰瑙嗭細缁?SkyBox 鎸囧畾 DefaultEnvironment銆?
### 2026-08-03 - RND-F10 Draft锛欵nvironmentMap Asset + Sky/IBL锛坄render`锛?- Goal:
	鍦烘櫙 Asset 寮曠敤椹卞姩澶╃┖涓?IBL锛汫PU Bake 鍚庣疆骞剁敤鐜颁唬 RHI 浠樻竻 TD-015銆?- Main changes:
	鐧昏 `RND-F10`锛汥esign + Impl 鑽夌锛汚CTIVE_WORK / TECH_DEBT 鎸囧悜 F10銆?- Next step:
	鐢ㄦ埛纭 Draft 鈫?Planned锛涘缓璁厛 S01鈥揝03 纾佺洏鎺ョ嚎銆?
### 2026-08-03 - RND-F09 Done锛歊HI / Binding hygiene锛坄render`锛?- Goal:
	浠樻竻 TD-013/014/016/017/018/019锛堜笉鍚?TD-015锛夈€?- Main changes:
	S01 Set0 鑴忕紦瀛橈紱S02 Material `RHITextureViewCache`锛汼03 `RHISetBackbufferClearColor`/`RHIClearBackbuffer`锛汼04 Apply 琛?blend 鍥犲瓙锛汼05 鍒犻櫎 `ShaderResource`+CB 杩囨护锛汼06 unit 鈫?`EngineShaderBindings`銆?- Validation done:
	`cmake --build` minEngine/Editor/minEngineTests锛沗.\scripts\verify.ps1` smoke PASS锛沗test render-graph` PASS銆?- Risks or caveats:
	Blend 鍥犲瓙灏氭湭 desc 椹卞姩锛汦ditor 榛勯噾鍦烘櫙寰呯敤鎴风洰瑙嗐€?- Next step:
	鐢ㄦ埛鐩鍚庡噯澶?commit锛汿D-015 EnvMap 涓撻鍙﹁銆?
### 2026-08-03 - RND-F09 Planned锛歊HI / Binding hygiene sweep锛坄render`锛?- Goal:
	鎵撳寘浠樻竻 TD-013/014/016/017/018/019锛涙槑纭帓闄?TD-015 EnvMap銆?- Main changes:
	鐧昏 `RND-F09`锛汥esign + Impl锛汿ECH_DEBT / ACTIVE_WORK 鎸囧悜 F09 鍒囩墖銆?- Next step:
	鐢ㄦ埛纭鏂规鍚庝粠 S01锛圫et0 鑴忔爣璁帮級寮€宸ャ€?
### 2026-08-03 - RND-F08 follow-up锛氶槾褰?Slot 璇箟鐦﹁韩锛坄render`锛?- Goal:
	鍒犻櫎 `ShadowResourceManager`锛涚敤鏄惧紡 `SlotIndex` 瀵归綈 Set1 / LightUBO銆?- Main changes:
	`Make*ShadowBinding` 鍐呰仈杩?`ForwardRenderer`锛汢ind/UBO 鎸?`SlotIndex`锛涘垹 Manager 婧愭枃浠躲€?	鐭璁?`RND-F08_SHADOW_SLOT_SEMANTICS.md`銆?- Validation done:
	`test render-graph` / `test smoke`锛堣鏈細璇濓級銆?- Next step:
	鐢ㄦ埛鐩闃村奖锛涘噯澶?commit銆?
### 2026-08-03 - RND-F08 follow-up锛氶槾褰?Slot 璇箟鐦﹁韩锛坄render`锛?- Goal:
	鍒犻櫎 `ShadowResourceManager`锛涚敤鏄惧紡 `SlotIndex` 瀵归綈 Set1 / LightUBO銆?- Main changes:
	`Make*ShadowBinding` 鍐呰仈杩?`ForwardRenderer`锛汢ind/UBO 鎸?`SlotIndex`锛涘垹 Manager 婧愭枃浠躲€?	鐭璁?`RND-F08_SHADOW_SLOT_SEMANTICS.md`銆?- Validation done:
	`test render-graph` / `test smoke` PASSED銆?- Next step:
	鐢ㄦ埛鐩闃村奖锛涘噯澶?commit銆?
### 2026-08-02 - RND-F08锛氶槾褰辫创鍥惧浘鎵€鏈夋潈锛坄render`锛?- Goal:
	Directional/Spot/Point depth 鐢卞浘鎷ユ湁锛涘叧鎺?TD-020锛汳anager 涓嶅啀 Create 绾圭悊銆?- Main changes:
	`SetupFrameRenderGraph` 鈫?`BindGraphShadowTextures` 鈫?`BuildSceneSet1` 鈫?`EnqueueFrameRenderGraph`銆?	`ShadowGraphPass` Absolute 澹版槑锛圖ir=`DirShadowAtlas` 2DArray锛汼pot/Point 鍏变韩 `GraphDepthResourceName`锛夈€?	`ShadowResourceHandle::IsValid` 涓?`HasBoundTexture` 鍒嗙锛汳anager 浠?unit/metadata銆?	`RenderGraph::InvalidateBake` 鍦ㄩ槾褰卞昂瀵告寚绾瑰彉鍖栨椂澶辨晥銆?- Risks or caveats:
	Manager 宸插湪 2026-08-03 follow-up 鍒犻櫎銆?- Validation done:
	`test render-graph` 4 PASSED锛沗test smoke` PASSED銆?- Next step:
	Slot 璇箟鐦﹁韩锛堝凡鍋氾級銆?
### 2026-08-02 - RND-F07锛欵ditor 瑙嗗彛鍙樉绀?ImGui Image 鍗犱綅锛坄render`锛?- Goal:
	鎺ュ洖鍚庤鍙ｆ棤姝ｅ父鍦烘櫙锛屽彧瑙?ImGui Image 鍗犱綅锛涗慨甯х汗鐞嗗鍛戒笌閲囨牱/娓呭睆閾捐矾銆?- Main changes:
	`Bake` 涓嶅啀 `assign(nullptr)` 娓呯┖鐗╃悊绾圭悊锛況ebake 鍓嶆竻 pass IO / resource usage銆?	Color RT 寮哄埗 `ShaderResource`锛沗SetupAttachments` 鎸?flags 涓嶅尮閰嶅垯閲嶅缓銆?	`SceneRenderTarget::Resize` 鍚屽昂瀵镐笉 `reset` publish銆?	`SkyBoxPass` 鎭掕窇骞?clear锛汷paque LoadStore锛汸ost 閾?`NeedRenderPass` + predecessor锛孎XAA 澶辫触鏃?Sharpen 涓嶈鐩?SceneColor銆?- Risks or caveats:
	闃村奖浠?TD-020銆?- Validation done:
	`cmake --build 鈥?--target Editor minEngineTests`锛沗test render-graph` 4 PASSED锛沗test smoke` PASSED锛涚敤鎴风洰瑙嗛粍閲戝満鏅?OK銆?- Next step:
	鍑嗗 commit锛涘彲閫変粯娓?TD-020銆?
### 2026-08-02 - RND-F07 Phase1 S01鈥揝03锛氱湡娓叉煋鍋滃伐 + 甯?RT 涓嶅垎閰嶏紙`render`锛?- Goal:
	Phase1 涓棿鎬侊細鏃犱汉鍒嗛厤 SceneColor/Depth/shadow/post锛沗ForwardRenderer` 涓嶈窇鐪熷浘銆?- Main changes:
	`ForwardRenderer::Execute` / `EnsurePostBufferTexture` 鏃╅€€锛沗SceneRenderTarget::Resize` 鍙灏哄涓嶅缓绾圭悊锛沗ShadowResourceManager::Ensure*` 鎭?false銆?	`render-graph` 绉诲嚭 smoke锛汥esign 瀹氬悕鍥捐妭鐐?`RenderPass` / 閽╁瓙 `IRenderPass`锛汼tatus In Progress銆?- Risks or caveats:
	Editor 瑙嗗彛榛戝睆锛堝凡鏈?null texture 鎻愮ず锛夛紱鏃?Manual 鍥句唬鐮佹殏鐣欏緟 S04 鏇挎崲銆?- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test smoke` 鈫?PASSED锛坄render-graph` 宸蹭笉鍦?smoke锛?- Next step:
	S04 Granite 寮忓浘鏍稿績锛埪?.6锛夈€?
### 2026-08-02 - RND-F07-S06鈥揝09锛氭帴鍥炲満鏅?Post + 鏀跺彛锛坄render`锛?- Goal:
	瀹屾垚 F07锛氫富璺緞璧?Granite 寮忓浘锛汼cene/Post 鍥炬嫢鏈夛紱鍒?Manual external銆?- Main changes:
	`RenderGraphFrameContext` / `ForceIncludePass` / `Prepare(graph)`銆?	Sky/Opaque/Translucent/Post/Present 鏂扮敓鍛藉懆鏈燂紱`ForwardRenderer::Execute` 鍏ㄨ矾寰勬仮澶嶃€?	SceneRT Publish color+depth锛汼hadow ForceInclude + Manager Ensure 鎭㈠锛?*TD-020**锛夈€?	Registry F07 Done锛涙棤 `RegisterExternal`銆?- Risks or caveats:
	闃村奖 atlas 灏氭湭杩佸叆鍥撅紱Bake 姣忓昂瀵稿彉鍖栭噸寤恒€?- Validation done:
	`test render-graph` 4 PASSED锛沗test smoke` PASSED銆?- Next step:
	鐢ㄦ埛 Editor 榛勯噾鍦烘櫙鐩锛涘彲閫変粯娓?TD-020锛涘噯澶?commit銆?
### 2026-08-02 - RND-F07-S05锛氬浘鎷ユ湁璧勬簮 GPU 绔栧垏锛坄render`锛?- Goal:
	璇佹槑 Bake/SetupAttachments 浜х敓鐨勭墿鐞嗙汗鐞嗗彲涓?GPU锛屽苟鏈夊彲瑙傚療杈撳嚭銆?- Main changes:
	`GraphClearPass`锛氬０鏄?`SceneColor` + ClearStore銆?	`ForwardRenderer::Execute` 璺戠珫鍒囧浘锛沗PublishGraphColorTexture` 鎶婂浘 `shared_ptr` 浜ょ粰 SceneRT 鏄剧ず銆?	`GetPhysicalTextureShared`锛涘崟娴?`glGetTexImage` 鏍￠獙 clear 鑹层€?- Risks or caveats:
	浠?clear锛屾棤鍦烘櫙鍑犱綍锛汼07 鍓嶈鍙ｅ浐瀹?slate-blue銆?- Validation done:
	`minEngineTests.exe test render-graph` 鈫?4 PASSED锛堝惈 clear 璇诲洖锛?	`minEngineTests.exe test smoke` 鈫?PASSED
- Next step:
	S06 Pass 鐢熷懡鍛ㄦ湡鏀剁揣锛屾垨鐩存帴杩?S07 鎺ュ洖鍦烘櫙銆?
### 2026-08-02 - RND-F07-S04锛欸ranite 寮?RenderGraph Bake 鏍稿績锛坄render`锛?- Goal:
	钀藉湴 Design 搂3.6锛氬０鏄?鈫?Bake 鈫?SetupAttachments 鈫?GetPhysicalTexture锛涙浛鎹?Manual builder銆?- Main changes:
	鏂板/閲嶅啓 `RDGTypes` / `RDGResource` / `IRenderPass` / `RenderPass` / `RenderGraph`锛圔ake 渚濊禆鍥炴函 + 鐗╃悊琛紱transient/merge Deferred锛夈€?	鍒犻櫎 `RenderPassBuilder` / `RenderGraphFrameResources` / `RDGTexture`锛涘満鏅?Pass 鏀逛负鏂?`IRenderPass` stub锛沗ForwardRenderer` 鏃ф瀯鍥捐矾寰?idle銆?	`RenderGraphTest`锛歨eadless GL + Bake/鐗╃悊绾圭悊/渚濊禆搴?缂?writer銆?- Risks or caveats:
	涓昏矾寰勪粛榛戝睆锛汭nputRelative / swapchain 闈炴嫢鏈夎鍥?/ barrier 鏈仛锛汼05 鎵嶄笂 GPU 绔栧垏銆?- Validation done:
	`cmake --build minEngine/build --target minEngine minEngineTests`
	`minEngineTests.exe test render-graph` 鈫?3 PASSED
	`minEngineTests.exe test smoke` 鈫?PASSED
- Next step:
	S05 鏈€灏?GPU 绔栧垏锛堝浘鎷ユ湁 color 鈫?clear/present锛夈€?
### 2026-08-02 - RND-F07 Draft锛欸ranite-style RDG + 甯ц祫婧愭墍鏈夋潈澶ч噸鏋勶紙`render`锛?- Goal:
	鎷嶆澘鐮村潖鎬т袱闃舵锛歅hase1 鏃犱汉鍒嗛厤甯?RT銆佺湡娓叉煋鍋滃伐锛汸hase2 鍖栫敤鏈満 Granite `render_graph` 鍐嶆帴鍥炪€傚彇浠?F01 S06+ 瀹為獙 Bake 浜у搧鏂瑰悜銆?- Main changes:
	鏂板 `RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md` / `_IMPLEMENTATION.md`锛汻egistry / ACTIVE_WORK锛汧01 Meta 鏍囨敞 Superseded by F07銆?- Risks or caveats:
	涓棿鎬侀粦灞忥紱Phase2 椤诲鐓?Granite `bake()` 闃插啀鍙戞槑銆?- Validation done:
	Phase1 code landed锛涜鍚屾棩 Phase1 鏉＄洰銆?- Next step:
	S04 Granite 寮忓浘鏍稿績锛埪?.6锛夈€?
### 2026-07-24 - RND-F01 S05 RDG implementation hygiene (`render`)
- Goal:
  鏀舵暃 Manual RDG 杩囩/绌哄３瀹炵幇锛屽悕瀹炵浉绗︼紱涓嶆墿 Bake/transient銆?- Main changes:
  鍒犻櫎 `RenderGraphExecute.cpp`銆乣RDGBuffer.h` 鍗犱綅銆乣PassParameters.h` / `RenderGraphFrameContext.h` / `RenderGraphTransition.*`锛堝苟鍏?`IRenderPass` / `RenderGraphFrameResources`锛夛紱`RenderGraphScenePass.h` 鈫?`SceneRenderPassUtils.h`銆?- Risks or caveats:
  `RDGBuffer` 寰呯湡鏈?buffer 璧勬簮鏃跺啀寮曞叆锛涚洰褰曚粛鍚?`RenderPipeline/`锛團06-S03 鍙€夛級銆?- Validation done:
  `cmake --build` minEngine + minEngineTests PASS锛沗minEngineTests.exe test render-graph` PASS銆?- Next step:
  F01-S06 Bake銆?
### 2026-07-24 - RND-F06 S01鈥揝02锛欶orwardRenderer 鏇挎崲 RenderPipeline锛坄render`锛?- Goal:
  鍒犻櫎 `RenderPipeline`锛沗SceneRenderer` 钖勫熀绫?+ `ForwardRenderer` 瀹炵幇锛沗RenderSystem` 鍙緷璧栧熀绫汇€?- Main changes:
  `SceneRenderer.h`銆乣EngineRenderLimits.h`锛沗RenderPipeline.h/.cpp` 鈫?`ForwardRenderer.h/.cpp`锛汸ass/utils/Context 鎸囬拡鏀瑰悕锛沗FrameRenderGraphContext::Renderer`锛涚洰褰曞悕鏆傜暀 `RenderPipeline/`銆?- Risks or caveats:
  鐩綍涓庣被鍚嶇煭鏈熶笉涓€鑷达紙S03锛夛紱榛勯噾鍦烘櫙鐩寰呯淮鎶よ€呯‘璁ゃ€?- Validation done:
  `cmake --build` minEngine + minEngineTests + Editor PASS銆?  `minEngineTests.exe test render-graph` + `test smoke`锛坒rom `bin/`锛塒ASS銆?- Next step:
  缁存姢鑰?Editor 榛勯噾鍦烘櫙鐩锛汧06-S03 鐩綍/娉ㄩ噴鏀跺熬锛涚劧鍚?F01 S05 RDG 鍗敓銆?
### 2026-07-24 - RND-F06 Design锛欶orwardRenderer / Graph 鑱岃矗鍒嗙锛堟枃妗ｏ級
- Goal:
  閽夋 Renderer vs RenderGraph 蹇冩櫤妯″瀷锛涚櫥璁?Feature锛涜皟鏁?F01銆屼笅涓€姝?Bake銆嶅彛寰勶紝閬垮厤鍦?`RenderPipeline` 涓婂笣瀵硅薄涓婄户缁爢鏈哄埗銆?- Main changes:
  鏂板 `docs/ai/Render/RND-F06_FORWARD_RENDERER_DESIGN.md`锛汻egistry / ACTIVE_WORK / PROJECT_CONTEXT 鏇存柊锛汧01 搂6 鍒囩墖鏀逛负 **F06 闂搁棬 鈫?S05 鍗敓 鈫?S06 Bake 鈫?S08 璋冨浘褰㈡€?*銆?- Risks or caveats:
  灏氭湭鍐?C++锛涢』鎸?ACTIVE_WORK 鎺ュ姏锛屽嬁璺宠繃 F06 鐩存帴 Bake銆?- Validation done:
  Registry 宸茬櫥璁?`RND-F06`锛汻ND next free 鈫?F07銆?- Next step:
  F06-S01 鎶藉嚭 `ForwardRenderer`锛涙垨鍏堝噯澶?commit 鏈壒鏂囨。銆?
### 2026-06-12 - RND-F01 S04 Shadow passes RenderGraph migration (`render`)
- Goal:
  Split monolithic ShadowPass into per-command graph passes; declare DirShadowAtlas scene input edge.
- Main changes:
  `ShadowGraphPass` (IRenderPass per `ShadowDrawCommand`); `ShadowPass::RenderSingleDrawCommand` + `PrepareShadowPass`.
  Dynamic shadow pass pool in `m_FrameRenderGraph` (rebuild when command count changes); execution order Shadow.* 鈫?Scene 鈫?Post.
  `kRDGDirShadowAtlas`; Base/Translucent `AddTextureInput(DirShadowAtlas)`; removed legacy `m_ShadowPass.Execute` from main path.
- Risks or caveats:
  Shadow pass names use index slots (`Shadow.N` / `ShadowDepth.N`); spot/point logical names deferred; visual shadow sign-off pending.
- Validation done:
  `cmake --build` + `minEngineTests.exe test render-graph` PASS.
- Next step:
  ~~F01-S05 Bake~~ 鈫?**宸叉敼鍙ｅ緞锛?026-07-24锛夛細鍏?RND-F06锛屽啀 F01 S05 鍗敓 鈫?S06 Bake銆?*

### 2026-06-12 - RND-F01 S03 Scene passes RenderGraph migration (`render`)
- Goal:
  Migrate Sky / Opaque / Translucent into unified frame RenderGraph; extract mesh draw helpers from RenderPassBase.
- Main changes:
  `SkyBoxPass`, `BasePass`, `TranslucencyPass` implement `IRenderPass`; `SceneMeshDrawUtils` + `FrameRenderGraphContext`.
  `m_FrameRenderGraph` merges scene + post + present; per-pass `BeginRenderPass` on SceneColor/SceneDepth (Sky/Opaque clear, Translucent load).
  Removed monolithic scene `RHICmdBeginRenderPass` block from `RenderPipeline::Execute`.
- Risks or caveats:
  Visual golden-scene sign-off pending user; Shadow still Legacy outside graph (S04).
- Validation done:
  `cmake --build` + `minEngineTests.exe test render-graph` PASS.
- Next step:
  F01-S04 Shadow pass graph migration.

### 2026-06-12 - RND-F01 S02 Post chain RenderGraph migration (`render`)
- Goal:
  Migrate FXAA 鈫?Sharpen 鈫?Present to Manual RenderGraph; remove scene-pass post loop and `m_SceneColorTexture` injection.
- Main changes:
  `PostProcessPass` / `PresentPass` implement `IRenderPass` (Setup / PreparePass / BuildRenderPass); `RenderPipeline` owns `m_PostRenderGraph`, `m_PostBufferTexture`, `ExecutePostRenderGraph` after scene `EndRenderPass`.
  `RenderGraphFrameResources::BeginFrame`; `RegisterExternalTexture` re-register; `kRDGPostBufferA`.
  Binding sets created in `PreparePass`, not `BuildRenderPass`.
- Risks or caveats:
  Visual golden-scene sign-off pending user; transient PostBufferA owned by pipeline (not graph pool).
- Validation done:
  `cmake --build` + `minEngineTests.exe test render-graph` PASS.
- Next step:
  F01-S03 Scene passes (Sky / Opaque / Translucent) graph migration.

### 2026-06-12 - RND-F01 S01 Manual RenderGraph skeleton (`render`)
- Goal:
  Deliver compile-ready RenderGraph types + two-phase executor without touching main pipeline path.
- Main changes:
  New `Render/RenderGraph/`: `RenderGraph`, `RenderPass`, `RenderPassBuilder`, `RenderGraphFrameResources`, `IRenderPass`, `RDGTexture` (string names), `AddTransition`; deleted empty `RenderPipeline/RenderGraph.h` stub.
  `ExecuteGraph`: all `PreparePass` then all `BuildRenderPass`; `render-graph` smoke tests (setup IO + execution order).
- Risks or caveats:
  No `Bake()` / no main-path wiring; internal RT creation deferred to S02.
- Validation done:
  `cmake --build` clean + `minEngineTests.exe test render-graph` PASS (2 cases, 7 asserts).
- Next step:
  F01-S02 Post/Present graph migration.

### 2026-06-12 - RND-F01 S0 Binding vocabulary unification (`render`)
- Goal:
  Align RHI binding types/APIs with Vulkan descriptor mental model before RenderGraph S01.
- Main changes:
  `RHIBinding.h` 鈫?`RHIShaderBinding.h`; `RHIShaderBindingSetLayout` / `RHIShaderBindingSetLayoutEntry` / `RHIShaderBindingSet` / `RHIShaderBinding` / `RHIShaderBindingType`; `CreateShaderBindingSetLayout` / `CreateShaderBindingSet` / `SetShaderBindingSet`; `GetShaderBindingSetLayout` / `kMaxShaderBindingSets`; `MeshDrawPacket::ShaderBindingSets`; OpenGL impl classes renamed; all Pass/Material/Engine call sites updated.
- Risks or caveats:
  `EngineSceneBindingSets` class name unchanged (engine layer); Tier-B design docs still cite old `RHIBinding*` in places.
- Validation done:
  `cmake --build minEngine/build --target minEngine` PASS.
  `minEngineTests.exe test smoke` + `material-ir` PASS (existing binary; test exe relink blocked by file lock).
- Next step:
  F01-S01 Manual RenderGraph skeleton; optional commit S0.

### 2026-06-11 - RND-F04 S04 PSO/SRV cache + RHI contract cleanup (`render`)
- Goal:
  Close F04 hot-path caching and RHI contract gaps; remove legacy draw submit API.
- Main changes:
  **PSO cache:** `EnginePipelineLayouts::GetOrCreateSceneMeshGraphicsPipelineState` keyed by layout + VIL + shader + pass kind.
  **SRV flyweight:** `RHITextureViewCache`; Scene Set1 dirty rebuild; Present/Post texture-keyed BindingSet cache; Sky SRV/set at init.
  **RHI:** `RHICmdTransition` (GL no-op); `OpenGLRHI` tracks bound descriptor sets with setIndex validation; removed `SubmitDraw*` from `RHICommandList`.
  Docs: F04 Done; TECH_DEBT TD-013鈥揟D-019.
- Risks or caveats:
  `BuildSceneSet0` still rebuilds each frame; Material SRV not flyweighted; `verify.ps1` not recorded this session.
- Validation done:
  Maintainer local cmake build PASS; golden scene visual OK.
- Next step:
  Commit S04; run `verify.ps1`; start F03-M3 tail inventory (EnvMap bypass, asset ShaderResource).

### 2026-06-11 - RND-F04 S01鈥揝03 PipelineLayout + MeshDrawPacket (`render`)
- Goal:
  Modern RHI semantic evolution: glue PSO to binding via PipelineLayout; complete draw packet; unify all Pass submit paths.
- Main changes:
  **S01:** `RHIPipelineLayout`, `RHICreatePipelineLayout`, GL `OpenGLRHIPipelineLayout`; `RHIGraphicsPSODesc::PipelineLayout`; `EnginePipelineLayouts` (shadow / scene mesh / pass-local).
  **S02:** `MeshDrawPacket`, `RHICommandList::SubmitMeshDrawPacket`; Present / Post / Sky migrated.
  **S03:** `PrepareMeshDrawPackets` + `SubmitSceneMeshDrawPackets`; Base / Translucent / Shadow on full packet (Set0/1/2); `MeshDrawCommand` pass-agnostic; removed `BindSceneDrawResources`.
  Docs: F04 design 搂12鈥撀?3 slice status; `ACTIVE_WORK` / `FEATURE_REGISTRY`.
- Risks or caveats:
  Per-draw PSO create still uncached; SRV per-frame create unchanged; `setIndex` still ignored; `SubmitDraw*` legacy API retained on CommandList.
- Validation done:
  User local cmake build PASS; golden scene visual OK.
- Next step:
  S04: PSO cache, SRV flyweight, `setIndex`, `RHICmdTransition` no-op; remove legacy SubmitDraw API.

### 2026-06-02 - RND-F03 M4 P3 material BindingSet cache (`render`)
- Goal:
  Stop per-draw `CreateBindingSet` in `Material::BindForDraw`; cache Set2 at compile / texture parameter change.
- Main changes:
  `Material::m_MaterialBindingSet` + `RebuildMaterialBindingSet` (compile + `SetTextureParameter`).
  `BindForDraw` only uploads scalar UBO; material Set2 bound via `SubmitDrawMesh` in `DrawMeshCommand`.
  `PrepareMeshDrawCommands` fills `MeshDrawCommand::m_MaterialBindingSet` from `Material::GetMaterialBindingSet`.
- Risks or caveats:
  Texture SRVs still `make_shared<OpenGLRHIShaderResourceView>` (P0鈥?; per-frame PSO create unchanged; fullscreen passes still create binding sets per frame.
- Validation done:
  User local cmake build + Editor visual OK.
- Next step:
  P0鈥?`RHICreateShaderResourceView`; optional Pass PSO cache; PROGRESS_LOG commit on `render`.

### 2026-08-17 - ED-F01 Vulkan Editor Parity registered (`feat/render`)
- Goal:
  After RND-F05 S07d, restore full Vulkan Editor (ImGui-Vulkan, embedded viewport, navigation) and continue shadow/sky/IBL in real Editor 鈥?not smoke fork.
- Main changes:
  **Registry:** `ED-F01` Planned; `RND-F05` 鈫?Done (RHI vertical slice S01鈥揝07d).
  **Docs:** [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md), [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md).
  **Handoff:** F05 S07e/f 鈫?ED-F01-S06/S07; smoke mode slated for removal at ED-F01-S03.
  **Source:** `imgui_impl_vulkan` from sibling `../imgui` clone (1.92.7 aligned).
- Risks or caveats:
  Frame sync (TD-024), swapchain/ImGui render pass alignment, dynamic RT ImGui descriptors.
- Validation done:
  Design/Impl draft only; no code yet.
- Next step:
  ED-F01-S01: copy `imgui_impl_vulkan` + CMake; then S02 ImGui empty frame.

## 2026-10-24 — BUG-EDITOR-002 根因闭合：AVX 对齐栈写（不是内存踩踏）

- **Scope:** `DebugDrawService::Enqueue*` 参数按值 → `const&`；`EditorCrashDiagnostics` 三处缺陷（真实崩溃时刻 / 运行期模块基址 / `0x0x` 双前缀）
- **Root cause:** `DebugDraw::Box` 中按值传参的栈拷贝被编译成 `vmovdqa %ymm0,-0xC0(%rbp)`；Win64 ABI 只保证 16B 栈对齐 → 半对齐帧上 #GP，Windows 报 `0xC0000005 / info0=0 / info1=0xFFFFFFFFFFFFFFFF`（哨兵值，非真实地址）。对象与 guard 自始至终完好，"内存被 0xFF 涂写"的旧结论被推翻
- **Verification:** 修前 13 次冷启动 8 崩；修后 11 次 0 崩（其中 6 次 `rbp%32==16`，旧必崩条件）。独立程序复现异常签名与编译器 codegen
- **Record:** [BUG-EDITOR-002](./bugs/BUG-EDITOR-002.md) → Fixed（本树验证，未提交）；探针开关/日志/证据程序一并留档
- **Next:** 合入前删除临时探针（`MEDBG_PROBE`=0）；可选 Jolt `PUBLIC -mavx2` → `PRIVATE` 做系统性收口

## 2026-10-24 — BUG-EDITOR-002 收口：摘掉 Jolt 泄漏的 AVX + 全量冷启动回归

- **Cleanup:** 删除 `DebugDrawProbe.h` 与 `DebugDrawService.cpp` 的探针块、`DebugDraw.cpp` 的探针钩子、`ForwardRenderer.cpp` 的三个 checkpoint；这三个源文件现在只剩修复 diff。探针日志 `minEngine/bin/dbgdraw_probe.log` 不进版本库。
- **Landing point:** 收口改动落在 **自有** `minEngine/minEngine/CMakeLists.txt`（`add_subdirectory(Jolt)` 之后擦除 `INTERFACE_COMPILE_OPTIONS` 里的 `-mavx*`/`-mf16c`/`-mfma`，回补 `INTERFACE -msse4.2`），而不是上游 submodule `Third-Party/Jolt`：该 submodule 指向上游仓库，本地改动无法提交。
- **为什么只撤 `-mavx2` 不够：** GCC 的 `-mf16c`/`-mfma` 隐含 `-mavx`。只把 `-mavx2` 改 PRIVATE 后 DLL 里 `vmovdqa %ymmN,(%rbp)` 仍是 19 处（全部 fmt 符号）；用同样旗标编译 40 行 fmt 程序正好复现这 19 处，去掉旗标则为 0。
- **踩过的坑：** 把 `-mf16c`/`-mfma`/`-mbmi`/`-mlzcnt` 一并撤掉会编译失败：`Jolt/Math/Vec4.inl:20:18: error: '__builtin_ia32_blendps' needs isa option -msse4.1`（`Mat44.inl` 的 `__builtin_ia32_insertps128` 同因）—— Jolt 头的 intrinsic 受 PUBLIC 特性宏控制，所以必须回补 SSE 基线。
- **Codegen evidence (objdump, `libminEngine.dll`):** 收口前 `vmovdqa %ymmN,(%rbp)`=19、任意 ymm 栈写=1933、ymm 总数=4450；收口后 =**0** / 175 / 794，且 175 处符号全部是 `JPH::*`（Jolt 自身仍按 AVX2 构建），first-party=0；`libMaximumEditor.dll` 完全没有 ymm。
- **Build:** `cmake --build minEngine/build --target Editor -- -j 20`，ExitCode=0，约 4 分钟；first-party 目标旗标变为 `-mbmi -mpopcnt -mlzcnt -mfpmath=sse -msse4.2`，Jolt 目标仍是 `-mavx2 …`。
- **Switch:** `SceneEditingViewportClient` 的 `EnableDebugDraw` 永久恢复，注释改为说明「两种 RHI 用同一组 draw flags」。
- **Regression（`EnableDebugDraw` 打开，90s/次）:** OpenGL 20 次 = 18 存活 + 2 次正常退出（外部关窗），补充批次 21-25 全存活 → 25 次启动 **0 次 `0xC0000005`**；Vulkan 10 次 = 7 存活 + 3 次正常退出，补充批次 11-13 全存活 → 13 次启动 **0 次 `0xC0000005`**。`minEngine/bin/ed_crash.log` 在整批期间没有新增记录。
- **作废批次:** `s2gl`（被并行会话按进程名清理时互相杀，`exit=-1` / 存活 0.3-38.6s / 当次无日志）与 `s2gl2`（旧落点二进制）不采用；压测脚本已改为只 Kill 自己 `Start-Process` 返回的进程。
- **Docs:** [BUG-EDITOR-002](./bugs/BUG-EDITOR-002.md) 增「验收数据」；`TECH_DEBT` TD-034 → Done。

## 2026-10-24 — RND-F26 S01/S02 落地（feat/render）

- API: `MeshCollectContext`（Purpose + Frustum* + `AllowsWorldBounds`）；`GetMeshBatches(collector, context)`。
- Renderer: MainView 传主锥；ShadowOnly `ViewFrustum=nullptr`。
- Engine proxies: 忽略 context（行为不变）。
- Tests: `frustum` 增 MainView 细剔 / ShadowOnly 全交；`test frustum` **8/8 PASS**（本树临时去掉 EXTRA minCraft harness 以链接；与 F26 无关）。
- Consumer: minCraft `ChunkMeshSceneProxy` 跟进签名 + section `AllowsWorldBounds`。
- Next: 准备 commit（`feat/render` docs+code）；合入前勿伤 `master` 工作树。

## 2026-10-24 — RND-F26 Design Draft + feat/render 对齐 master

- Worktree: `D:/Dev/GitRepo/minEngine-render` · `feat/render` ff-only → `master` @ `a0626b67`（此前落后 ~56，无独有提交）。
- Registered `RND-F26` Draft：[RND-F26_VIEW_AWARE_MESH_COLLECT_DESIGN.md](./Render/RND-F26_VIEW_AWARE_MESH_COLLECT_DESIGN.md)。
- Registry / ACTIVE_WORK（本树）指向 F26；`RND` next → F27；`RND-F21` 仍 Deferred。
- Validation: docs only；**待审批**后再 Implementation Plan / 开码。
- Next: 维护者批 Design（Option A + Open Questions）→ Status Planned。

## 2026-10-23 — RND-F25 Render-to-Texture / Clear API / thumbnail unification (S01-S05)

- 设计: docs/ai/Render/RND-F25_RENDER_TO_TEXTURE_DESIGN.md
- S01: `SceneDrawDesc` 加 `ClearColor`/`DepthClear`; `MakeSceneRenderPassInfo` 参数化; `BasePass`/`SkyBoxPass`/`ManualRenderer` 从 DrawDesc 透传; 删死代码 `SceneRenderTarget::BuildRenderPassInfo`.
- S02: 新增 `Runtime/Function/Render/OffscreenRenderer` (runtime, 编辑器无关; 持有 SceneViewport, RenderOnce 提交到 ForwardRenderer).
- S03: 离屏路径验证 (PresentToBackBuffer 未置位渲染到 offscreen RT).
- S04: `AssetThumbnailService` Inspector + tile 路径统一到 OffscreenRenderer, PreviewScene 两边共享; 缩略图默认透明背景.
- S05: `render-graph` 新增测试 "DrawDesc transparent clear color reaches the backbuffer" — readback 验证 alpha==0; suite 8/8 PASS.
- build: libminEngined/Editor/minEngineTests 全绿; render-graph + smoke PASS.
- 退役: BUILD-F04 过期 engine-export-count 断言 (MODULE_API explicit exports 已取代固定计数); boundary audit 18/18 PASS.
- 遗留 (非本 Feature): verify 的 consumer-coverage 报 minCraft `GameplayTag::operator==` 缺失 (GP-F03 遗留; minCraft 需重建).