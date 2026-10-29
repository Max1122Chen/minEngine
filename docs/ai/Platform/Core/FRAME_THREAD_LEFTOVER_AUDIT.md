# 帧线程遗留问题审查（多线程时代漏网之鱼）

- **Status:** Review
- **Date:** 2026-10-29
- **Scope:** 帧线程分离（Game/Render/RHI）之后的全局遗留问题；不含渲染算法与功能开发
- **基线提交:** `de7428af`（三条硬约束落地之后）
- **方法:** 四路只读审查（Core 服务 / 渲染侧 / 编辑器与 PIE / 帧线程基础设施）→ 交叉核对 → 逐条判决

## 判据

命中任意一条即为遗留问题：

1. 在渲染角色之外直接调 GPU/驱动接口；
2. 录制期做影响回放结果的立即操作；
3. 渲染侧解引用游戏对象、或读游戏线程正在写的字段；
4. 跨线程共享且无同步的可变状态；
5. 假设"只有一个线程/只有一帧在飞"的隐式契约（注释里写着 MVP / main-thread only / later 的尤其可疑）；
6. 关停/销毁顺序错误；
7. 本该有断言或计数却没写。

## 交叉印证（两路独立命中的高置信项）

| 问题 | 命中 | 严重度 |
|---|---|---|
| `SceneDrawDesc::GameplayScene`：渲染线程解引用游戏 `Scene*`（`ForwardRenderer.cpp:654-657` → `PhysicsDebugDraw::SubmitScene`） | 渲染路 + 编辑器路 | **高（违反 H1）** |
| `EnvironmentMap::EnsureGPUResources` 在录制窗口内做磁盘 I/O + GPU 烘焙 + immediate 提交 | 渲染路 + Core 路 | **高** |
| `CollectOrphanedSceneProxies` 由游戏线程直接改渲染侧容器（PIE 进出） | 编辑器路 + 前轮审计 | **高（违反 H2 单写者）** |
| `ME_ASSERT` 只有硬陷一档（角色线程上触发即 `terminate`） | 帧线程路 + 既有债务 TD-042 | 高 |
| `RenderCommandPipe::PublishGameFrame` 发布顺序反了（先 Completed 后 Current → 生产者取到已可 drain 的标签 → 命令被判陈旧丢弃） | 帧线程路（已由人工复核代码确认） | 中 |
| 代理身份指针（Light/Widget/SkyBox）由游戏线程直写 | Core 路 + 既有债务 TD-037 | 中 |

## 判决

### A. 修（正确性，按修复顺序）

| # | 问题 | 位置 | 判决理由 |
|---|---|---|---|
| 1 | 崩溃处理器在故障线程上分配内存 / 取日志锁 / 无 once 守护 | `EditorCrashDiagnostics.cpp:487-557` | 它决定后续一切问题的可诊断性；故障发生在锁持有期间会把"崩溃记录"变成"挂死" |
| 2 | 环境贴图烘焙在录制窗口内（含磁盘 I/O 与 immediate 提交） | `EnvironmentMap.cpp:168-267`、`SkyBoxPass.cpp:304` 等 | 崩溃级 + 帧尖刺 + 跨线程碰 RHI 录制状态（修复进行中） |
| 3 | `SceneDrawDesc::GameplayScene`：渲染侧持游戏对象 | `ForwardRenderer.cpp:654-657` | 直接违反 H1；拆分拓扑下与游戏线程并发 |
| 4 | `EnvMapCapture` 用栈上命令列表却不录制 | `EnvMapCapture.cpp:116/342/464/597` | debug 断言崩溃、release 静默产出空贴图（修复进行中） |
| 5 | 断言分级（`ME_ENSURE` 记录并继续 / `ME_ASSERT` 硬陷） | `Assert.h:11-22` | 所有线程契约的安全网；没有它，契约只能靠人读代码守 |
| 6 | `ScopedFrameThreadKind` 析构空实现 + 身份表非原子 | `FrameThreading.cpp:216-227`、`FrameThreading.h:77-78` | 临时绑定永不恢复 → 该线程永久通过角色断言 |
| 7 | `PublishGameFrame` 发布顺序 | `RenderCommandPipe.cpp:93-96` | 两行改动，消除一条真实的静默丢命令路径 |
| 8 | 代理身份指针只由渲染角色写（Light/Widget/SkyBox） | `LightComponent.cpp:30/167` 等 | 已有 primitive 的正确样板（`m_bPendingRemove`），机械照抄 |
| 9 | `AudioVoice` 不持 `AudioClip` 强引用（miniaudio 不拷贝 PCM） | `AudioVoice.cpp:40`、`MiniaudioBackend.cpp:137` | 音频线程读已释放内存的 UAF，无日志无断言 |
| 10 | ImGui descriptor set 在游戏线程分配/释放（Vulkan） | `EditorRHIImGuiTexture.cpp:63/108` | 违反 H3，且绕过提交批次尾部 |
| 11 | `CollectOrphanedSceneProxies` 命令化 | `PlayInEditorSession.cpp:153/249` | 同一状态两个角色在写（H2 单写者） |
| 12 | 关停期命令静默丢弃且无会计 | `RenderCommandPipe.cpp:63-71` | 唯一必然丢命令的路径正好不计数（判据 7） |
| 13 | `IsImmediate` 判定缺线程身份 | `RHICommandList.cpp:90-103` | 角色停止后任何线程都会走立即分支 → GPU 调用出现在非图形线程 |

### B. 记账（已知限制 / 需先有前提）

- Vulkan Multi 下 `RHIClearBackbuffer`（帧上下文）与 `Present` 跨角色：**先要确认 Vulkan 分离拓扑在配置里是否真的启用**，再决定修法（收敛到 RHI 角色 vs 加断言）。
- `ParallelFor` 在 RHI 角色上静默不执行、`RunAndWait(count==1)` 内联：语义文档与实现不一致，需要先定策略。
- 关停 `join()` 无界（"bounded drain"名不副实）：要么改文档，要么改实现（detach + abort）。
- `ObjectManager` / `AssetManager` 同步入口 / `SceneManager::Shutdown` 缺角色断言：先加断言与计数，让越界调用变成可见错误。
- `LogSystem` 持锁做 I/O、severity 非原子：多线程下退化串行 + 形式化竞态。
- `Profile` 的 `GetActiveSession` 裸指针、渲染角色 span 无帧号：影响"看得见多线程问题"的能力。
- `ShadowTypes` 的 ring 恰好 18/18 用满、sky/widget 的 recreate 放大：容量与吞吐问题。

### C. 不修（记录理由）

- `SceneDrawDesc` 的帧标签"落后一帧"：**实测无害**（意图链每条都被取到、无丢弃）。
- 缩略图 / 离屏渲染：跨帧延迟被显示端吸收，无 readback 调用点，今天不构成问题。
- 物理"单线程 job system"注释：理由过时（把 GL 归属与组件数据竞争捆在一起），但**当前行为正确**，改为准确注释即可。

## 修复顺序（按依赖，不按难易）

1. 崩溃处理器（决定可诊断性）
2. 环境贴图烘焙 + `EnvMapCapture`（同一子系统，修复进行中）
3. `SceneDrawDesc::GameplayScene`（违反 H1）
4. 断言分级（安全网；后续条目靠它守住）
5. `ScopedFrameThreadKind` + 身份表原子化
6. `PublishGameFrame` 顺序（两行）
7. 代理身份指针（Light/Widget/SkyBox）
8. 音频 `AudioClip` 生命周期
9. ImGui descriptor set 归位
10. `CollectOrphanedSceneProxies` 命令化
11. 关停期命令会计 + `IsImmediate` 角色维度
12. 记账项的断言与计数

## 附：本次审查做对了的地方（可作为参照）

- `MaterialRenderProxyPublisher::PublishPendingGameData`：跨角色共享表的标准模板（off-role 只报一次、锁内只做 weak 提升）。
- `SceneManager::FlushRetiredScenes`：先拒绝错误角色，再 swap-under-lock / destroy-outside-lock。
- `TaskSystem` 完成队列：worker 只 Publish，执行只在游戏角色 Drain；先 Publish 再 signal。
- `AssetManager` 异步装配：worker 只解析进自己的结构，装配与回调都在 Drain 里。
- `RHICommandList` 的录制窗口计数（判据 2 的可观测钩子）与 `RHIDeferredConsumerGate`（Pending/Unusable 分流）。

## 落实状态（2026-10-29 更新）

- **A6 / 修复顺序 5 已落地。** `ScopedFrameThreadKind` 现在是真正的 RAII：只认领无人持有的角色，作用域结束即交还，嵌套按层释放（同一角色嵌套为 no-op，由外层释放）；对别的线程持有的角色只报 `ME_ENSURE` 并拒绝，不再静默抢占。身份表从 `tid + bound` 两字段改成每个角色一个 `std::atomic<std::thread::id>`，"无人持有"由默认构造的 id 表示，绑定 release 存、查询 acquire 读、认领/交还走 acq_rel compare-exchange，因此"已绑定 + 旧 id"的撕裂态不再可能。
- **仍保留的：** `StopFrameRoles` 在 join 之后把调用线程绑成三角色。理由是关停后进程回到单线程拓扑，编辑器 teardown 的合法内联图形工作必须能过角色门禁；它与身份表无关，与本条不同类——代价（关停后调用线程的角色断言恒真）改为在停帧日志里写明。
- **本条未覆盖的：** 身份表只存 tid，分不出"持有者已退出"与"持有者活着"，所以一个角色线程若退出而不交还角色，之后的临时认领会按抢占被拒；角色门禁的越界调用仍然靠调用方自觉，没有强制手段。
