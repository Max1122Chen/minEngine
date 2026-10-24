# CORE-F38 TaskSystem Design Spec

## Meta
- **ID:** `CORE-F38`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`（S00–S02 落地；suite `task-system`）
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md) · [Impl Plan](./CORE-F38_TASK_SYSTEM_IMPLEMENTATION.md) · 后置客户：`ASSET-F09`（异步资产，**另 Feature / 另 Design**）· 后置：逻辑/渲染线程分离（未编号）

## TL;DR

本 Feature **只**做一件事：把 **TaskFlow** 接入引擎，用薄包装提供 `Submit` / `Wait` / 每帧 **`DrainCompletions`**，并用**合成 CPU 作业**证明「多线程能动、完成能回到游戏线程、1 worker 对 N 有 wall-clock 差异」。

**不做**任何真实 Asset 的异步加载（含 DataAsset / Texture2D）。对象身份、`NewObject`、Asset 缓存、RHI、同步/异步同 path 等难题留给后续 Feature，等 TaskSystem 站稳再单独设计。

## Problem

1. 引擎没有统一的后台作业入口；以后各系统若各自起线程会失控。
2. 维护者需要先**学会并验证**「池 + 回流主线程」这一小步，再碰资产异步里的所有权问题。
3. 需要可复现的性能对比（1 vs N wall-clock），不靠空场景 FPS。

## Requirements

- Engine 拥有可 Init / Shutdown 的 `TaskSystem`。
- 调度内核 = **TaskFlow**；引擎不自研 work-stealing。
- API（MVP）：`Init` / `Shutdown`、`Submit`（可带完成回调）、`Wait`、`DrainCompletions`。**不**暴露任务依赖图、TaskGroup、Future/Promise、无锁队列。
- 阶段必须可区分：接受提交 → Worker 跑工作函数 → 完成事件入队 → 游戏线程 Drain 派发回调。`Wait` 只覆盖到工作函数结束；`Drain` **不是**「等所有 Worker 干完」。
- Worker **不得**调用：RHI、`NewObject` / ObjectManager、AssetManager、Scene、Lua、ImGui，也**不得**再 `Submit` / `Shutdown` / `DrainCompletions`。
- 完成回调 **只**在游戏线程、经 `DrainCompletions` 派发。
- Drain 挂点（已锁定）：`LogicalTick` 里 **Input 之后、Fixed 之前**。MVP 不设每帧派发数量或时间上限。
- 功能证明：单任务提交、结果可见、回调线程、异常与 Shutdown。教学可用 `sleep`，**不得**用 sleep 当并行加速证据。
- 性能实验（与功能测试分开）：同一批可独立计算的 CPU 工作，`worker=1` vs `N`，核对结果一致并记录耗时。固定倍率不是全环境的功能门禁；CI 可跳过性能、不可跳过正确性。

## Constraints

- 不引入逻辑 / 渲染 / RHI 线程。
- **本 Feature 不改 AssetManager / Loader。**
- TaskFlow 接入形态（submodule vs ThirdParty）在 S00 决定。
- Worker 数在 Init 时固定；MVP 不要求运行中改池大小。
- 源码注释不含 Feature/Slice ID。

## Preferences

- API 小而硬；完成队列优先于「在 worker 上直接调游戏回调」。
- 可选挂钩 Profiler（`CORE-F22`）：`Task.Submit` / `Task.Worker` / `Task.Drain`。
- 游戏 / minCraft 日后可 Submit 自己的纯 CPU 作业，但本 Feature **不**交付游戏侧用例。
- 引擎对外是否提供 `Sleep` / 可注入时钟：**不在本 Feature**；测试与示例里直接用标准库 sleep 即可。

## Assumptions

- TaskFlow 可在 MinGW + MSVC 下构建（S00 验证；优先 header-only 接入）。
- 本 Feature 不改帧拓扑（仍单游戏线程跑 `TickOneFrame`）。

## Game thread identity（已锁定）

**游戏线程** = `TaskSystem::Init` 被调用时所在的线程（记录 `std::thread::id`）。

| 规则 | 行为 |
|------|------|
| `Init` | 记录调用线程为唯一游戏线程；仅允许在 `Stopped` 时成功 |
| `Submit` / `DrainCompletions` / `Shutdown` | 非该 `thread::id` → 拒绝并记日志（Submit 返回空句柄；Drain 不派发；Shutdown 无操作） |
| Engine 挂接 | `Engine` 在主子系统初始化路径上于**游戏线程**调用 `TaskSystem::Init`；`LogicalTick` 挂点与 `Shutdown` 同属该线程 |
| 无窗口 / 单测 | 测试进程在调用 `Init` 的同一线程上做 `Submit` / `Wait` / `Drain`；不必启动完整 `Engine::Run` 循环，但必须先 `Init` |
| 不提供 | 运行中改绑游戏线程；多游戏线程 |

「跑过 `TickOneFrame` 的线程」与上述定义在正常 Editor/Game 路径上一致；**契约以 Init 记录为准**，避免测试里事后用「谁调了 Drain」回推定义。

## Non-Goals（本 Feature 明确不做）

- 任何 Asset 异步加载 API（DataAsset / Texture2D / Mesh…）。
- 拆 Loader 为「worker 读字节 + 游戏线程 NewObject」。
- 同步 Load 与异步 in-flight 合并 / 死锁契约（属 `ASSET-F09`）。
- 内存反序列化 helper、Asset 缓存线程策略。
- Texture / Material / Mesh 的 GPU 上传异步。
- 逻辑线程与渲染线程分离。
- 任务依赖图、TaskGroup、父子任务、「B 等 A」公共 API。
- 通用 Future/Promise、无锁完成队列。
- 引擎级 `Sleep` / PlatformTime / 可 mock 时钟（另 Feat；教学冒烟可用 `std::this_thread::sleep_for`）。
- 取消令牌、运行中改 worker 数、Profiler 挂钩（后置）。
- 复制 UE NamedThreads / Godot 完整 MessageQueue。

## Mental model

| 角色 | 对应 | 本 Feature 允许 |
|------|------|-----------------|
| 主厨 | 游戏线程 | Submit、Drain、改引擎对象（本波证明码尽量少改对象） |
| 帮厨 | TaskFlow worker | 只跑合成 CPU 函数，写局部结果 |
| 传菜口 | 完成队列 + Drain | 把「算完了」带回游戏线程 |

**Drain** = 取走**已经入队**的完成事件并在游戏线程派发回调。它不等待仍在跑的 Worker，也不等于 `Shutdown`。

## External references（学习，勿照搬）

- UE：`TaskGraphInterfaces.h` — `ENamedThreads::GameThread`、`ProcessThreadUntilIdle`：后台算完 ≠ 可改游戏对象。
- Godot：`WorkerThreadPool` + `MessageQueue::flush` / `call_deferred`：池与回主线程是两套机制。

本 Feature 只实现：池（TaskFlow）+ 极简完成队列（Drain）。

## Current Architecture

```text
Engine::TickOneFrame
  → PollEvents
  → LogicalTick (Input → Fixed → Variable → …)
  → RendererTick
```

无 TaskSystem；无 TaskFlow。资产仍全部同步加载（本 Feature 不碰）。

## Proposed Architecture

```text
Game thread                    Workers (TaskFlow)
  Submit(cpuJob, onComplete) ──►  run cpuJob
  …                                push Completion
  DrainCompletions() ◄──────────── queue
       onComplete()  (game thread only)
```

### Components

| Name | Role |
|------|------|
| `TaskSystem` | Init/Shutdown；Submit；Wait；DrainCompletions |
| TaskFlow executor | 算法内核；Init 时固定 worker 数 |
| Completion queue | mutex 保护；仅游戏线程 Drain |
| 合成 harness / test | 1 vs N wall-clock + 线程 id 断言 |

### Drain 挂点（已锁定）

`LogicalTick`：**Input 之后、Fixed 之前**。行为细节见 `DrainCompletions`。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `TaskSystem` | 池生命周期、提交、完成队列、Drain | Asset、RHI、Scene、游戏玩法 |
| TaskFlow | 调度执行 | 引擎对象模型 |
| 测试 / harness | 合成作业与性能对比 | 真实内容管线 |

## Boundaries

- **Allowed on worker:** 纯函数、局部数据、诊断用 `ME_LOG`（LogSystem 有 mutex）。正式完成通知仍走 Drain。
- **Forbidden on worker:** RHI、`NewObject` / ObjectManager、AssetManager、Scene、Lua、ImGui；以及 `Submit`、`Wait`、`DrainCompletions`、`Shutdown`、`Init`。
- **Dependency:** Runtime Core → TaskFlow。TaskSystem **不**依赖 Asset / Render。

## Ownership & Lifetime

- `Engine` 拥有 `TaskSystem`。本 Feature **不**管理 Asset / `MEObject` 生命周期。
- Worker 优先只写局部数据，经完成事件把结果交给游戏线程。
- `shared_ptr` 只表示共享所有权，**不**使对象内部并发读写自动安全。工作函数对调用方内存的写入，在 `Wait` 返回后对等待线程可见（`Wait` 与工作函数结束同步）；在 Drain 派发回调时对游戏线程可见（入队与出队同步）。
- 完成载荷是纯数据（值、错误标记）。不移交 `MEObject`。

## Data / Control Flow

四个阶段，不可混为一谈：

1. **接受：** `Submit` 成功则返回有效 `TaskHandle`，任务进入 `Pending`。
2. **执行：** Worker 跑工作函数（成功返回或被捕获的异常）。
3. **发布：** 完成事件入队（成功或失败）。此时 `Wait` 可以返回；**完成回调尚未执行**。
4. **派发：** 游戏线程 `DrainCompletions` 取出该事件并调用 `onComplete`。

`DrainCompletions` 只处理调用开始时已经在队列里的事件。它不阻塞等待尚未发布的任务。

## State & Invariants

状态只有三个：`Stopped | Running | ShuttingDown`。不另设 Draining。

| 调用 | Stopped | Running | ShuttingDown |
|------|---------|---------|----------------|
| `Init` | → Running | 拒绝（记日志，保持 Running） | 拒绝 |
| `Submit` | 拒绝，空句柄 | 接受 | 拒绝，空句柄（含 Drain 回调里的 Submit） |
| `Wait` | 立即返回（空句柄或 Shutdown 后已失效） | 等到该任务工作结束（成功或失败） | 可等已接受且尚未结束的任务；不接受新任务 |
| `DrainCompletions` | 无操作 | 派发当前批次 | 派发剩余完成事件（Shutdown 路径） |
| `Shutdown` | 无操作 | → ShuttingDown，走关闭序列，结束于 Stopped | 无操作（重复调用） |

- 完成回调只在游戏线程。
- Worker 不发布 `MEObject` / Asset。
- 每个已接受任务在成功或失败路径上都会结束等待并入队一次完成事件，避免 `Wait` / `Shutdown` 永久阻塞。

## API / Interface Semantics

### 阶段与句柄

`Submit` 返回 `TaskHandle`。

| 句柄状态 | 含义 |
|----------|------|
| 空 / Invalid | 提交被拒绝，或 Shutdown 已结束 |
| `Pending` | 已接受，工作函数尚未结束 |
| `Succeeded` | 工作函数正常返回；完成事件已发布（回调可能尚未 Drain） |
| `Failed` | 工作函数抛异常并已被捕获；完成事件已发布 |

句柄在 `Shutdown` 返回之后失效。MVP 不提供通用 Future/Promise。不保证多个任务完成回调的先后顺序；调用方不得依赖 FIFO。

### `Submit(work)` / `Submit(work, onComplete)`

- 仅 **游戏线程**（见上文身份定义）、且状态为 `Running` 时接受。否则返回空句柄，不排队、不调用 `onComplete`。
- `work` 只在 Worker 上跑，遵守 Boundaries。
- **Worker 内再次 `Submit`：禁止。** 不存在隐式父子任务。
- **`onComplete`（MVP）：** 只在 Drain 时于游戏线程调用；签名最小为成功/失败（`bool bSucceeded`，可选错误码）。**失败也调用回调**。无单独「失败回调」API。
- **结果载荷（MVP 锁定）：** 回调**不**携带作业计算结果。调用方用自有缓冲区 / 原子 / 局部对象；`work` 写入后，在 `Wait` 返回（等待线程）或 Drain 派发时（游戏线程，经完成队列同步）可见。完成队列条目只带句柄身份 + 成功/失败标记。
- 游戏线程在 `Running` 时，允许在 `onComplete` 里 `Submit` **另一个独立任务**（不是依赖图）。`ShuttingDown` 期间一律拒绝。

### `Wait(handle)`

- 只等到该句柄的 **工作函数结束**（`Succeeded` 或 `Failed`）。
- **不**表示 `onComplete` 已执行。要观察回调，必须再 `DrainCompletions`。
- 空句柄：立即返回，不阻塞。
- 返回后，该工作函数里的写入对等待线程可见。
- 已结束句柄上再次 `Wait`：立即返回（幂等），不重新跑工作函数。
- 调用方不得在 Worker 上 `Wait`。游戏帧路径不应为了普通后台任务阻塞在 `Wait` 上；测试可以。

### `DrainCompletions()`

- **仅游戏线程。** 其他线程调用：记错误并不派发。
- 调用开始时把完成队列 **交换到局部批次**，然后释放队列锁。**持锁期间不执行用户回调。**
- 本次只派发该批次。回调里新产生的完成事件留给**下一次** Drain，不在同一次调用里递归抽干。
- MVP **不**设条数或时间上限。大量或很慢的回调会拉长本帧主线程时间，调用方自行约束。
- 每帧在挂点调用一次。这不是「等待所有 Worker」。

### `Shutdown()`

- **仅游戏线程。** Worker 调用 `Shutdown`：禁止（记错误并返回）。
- `Running` → `ShuttingDown`：拒绝新 `Submit` → 等待**已接受**任务的工作函数结束（含失败路径）→ `DrainCompletions` 派发剩余完成回调 → 销毁 executor → `Stopped`。
- 不静默丢弃已接受任务的完成回调。
- `ShuttingDown` 的 Drain 回调里 `Submit` 被拒绝。
- 回调异常：记下后继续本批次其余项，然后仍销毁 executor。
- `Stopped` 或重复 `Shutdown`：无操作。

### Worker 数

- `Init` 时固定。`1` 为性能对比基线；默认 `max(1, hardware_concurrency - 1)`。

## Failure Model

| Failure | Behavior |
|---------|----------|
| `Submit` 在非 Running | 空句柄；无回调 |
| 工作函数抛异常 | 捕获；句柄 `Failed`；仍发布完成事件；`Wait` 返回；Drain 时 `onComplete(false)`。Worker 不退出，等待不悬挂 |
| `onComplete` 抛异常 | 记日志；继续本批次其余回调；不中断 Shutdown |
| 非游戏线程 `Drain` / Worker `Shutdown` | 拒绝并记日志 |

## Alternatives & Trade-offs

### Option A（推荐）— 本波只 TaskSystem + 合成证明

- Pros：范围小；先建立线程契约；避开 NewObject / Asset / RHI。
- Cons：暂时看不到「加载变快」的产品体感——用 wall-clock 门闩弥补。

### Option B — 本波顺手做 DataAsset 异步

- Why not：维护者要求更谨慎；对象与缓存契约未单独设计。后置为 `ASSET-F09`。

### Option C — 自研线程池

- Why not：与「TaskFlow 作内核」冲突。

## Integration Impact

- CMake / Third-Party：TaskFlow（接入形态见 Impl Plan；默认 vendored header-only，不强制 git submodule）。
- `Engine`：游戏线程上 `Init` / `Shutdown`；`LogicalTick` 在 Input 之后、Fixed 之前调用一次 `DrainCompletions`。
- 公共类型（`TaskSystem`、`TaskHandle` 等）标注 **`MINENGINE_API`**；**不**向 `minEngine_exports.def` 增加新行（BUILD-F03 explicit 路径）。
- 新测试 suite：`task-system`（正确性，进 smoke/CI）；性能实验可单独入口或同 suite 内可跳过开关。
- **零** Asset 管线改动。

## Migration / Compatibility

- 无行为变更给现有加载路径。
- 无磁盘格式变更。

## Verification Strategy

功能测试与性能实验分开。正确性测试不依赖固定 sleep 猜完成时刻。`std::this_thread::sleep_for` 只作可选教学冒烟，**不是**加速证据。引擎 Sleep 封装仍是另 Feat。正常游戏帧不为普通后台任务阻塞；测试可以 `Wait`。

### 功能（必须，CI 不可跳过）

- 单任务提交并执行；工作函数写入在 `Wait` 返回后对等待线程可见。
- **确定性回流：** `Submit → Wait(handle) → DrainCompletions`。断言：`Wait` 返回时回调尚未作为「已 Drain」的前提（可在 Wait 之后、Drain 之前确认回调未跑），Drain 之后回调已在游戏线程执行。
- **异步：** `Submit` 不要求调用方同步跑完工作函数。用条件变量或等价同步等到回调发生，不用固定时长 sleep 猜测。
- 工作函数抛异常 → 句柄 `Failed`，`Wait` 返回，Drain 时 `onComplete(false)`，进程不崩。
- 回调抛异常 → 同批次后续完成事件仍派发。
- Shutdown：拒绝新提交；已接受任务的回调被 Drain；重复 Shutdown 无操作；关闭中的回调 `Submit` 得到空句柄。
- 不测依赖图、不测第二个任务的先后顺序。

### 性能（实验，可单独跑；CI 可跳过）

- 同一批 **CPU-bound**、彼此独立的计算（不要用 sleep，不要所有任务抢同一把锁或同一个累加器）。
- `worker=1` 与 `worker=N` 计算量相同，最终结果一致。
- 记录：Worker 数、任务数、负载说明、总耗时、加速比。
- 本机期望加速比可写在实验记录里，**不是**全环境功能验收的硬门槛。

### 回归

- `verify.ps1` / `test smoke` 仍通过。功能 suite（如 `task-system`）挂在正确性上，不挂在性能倍率上。

## Deferred clients（非本 Feature）

以下仅记账，**不**写入本 Feature 验收：

| 后续 | 内容 | 备注 |
|------|------|------|
| `ASSET-F09` | 异步 DataAsset（读字节在 worker；NewObject 在游戏线程） | 需独立 Design；依赖 F10 + F38 |
| 未编号 | Texture / Mesh 异步 | 另拆 GPU；等渲染线程设计 |
| 未编号 | 逻辑/渲染线程 | 另 Feature |

`ASSET-F09` 里曾讨论过的「同步 Load 等字节 future、勿干等 Drain」等契约，**等开 F09 Design 时再锁定**，不绑在 F38 上。

## Decisions

| # | 决定 |
|---|------|
| 1 | Drain：Input 后、Fixed 前；换出批次再回调；不 cap；不保证 FIFO |
| 2 | TaskFlow 默认 vendored header-only（`Third-Party`）；S00 若链接失败可改 submodule，不改 API 契约 |
| 3 | 不含 Asset 异步 |
| 4 | `Wait` ≠ 回调已执行；`Drain` ≠ 等所有 Worker |
| 5 | 无依赖图 / TaskGroup / Future；Worker **禁止**再 `Submit` |
| 6 | 失败：工作异常 → `Failed` + `onComplete(false)`；回调异常隔离 |
| 7 | Shutdown 只在游戏线程；关闭中拒绝 Submit；必须 Drain 已接受任务 |
| 8 | 功能测试与性能实验分离；sleep 不是加速证据；固定倍率不是 CI 功能门禁 |
| 9 | 游戏线程 = `Init` 时记录的 `thread::id`；非该线程拒绝 Submit/Drain/Shutdown |
| 10 | MVP 回调不携带计算结果；结果靠调用方缓冲区 + Wait/Drain 同步 |
| 11 | 公共 API 用 `MINENGINE_API`；不扩 `.def` 白名单 |

## Design Review

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** 2026-10-24 开工前评审落地 + 维护者批准
- **Summary:** 吸收 D03–D05（游戏线程身份、回调无结果载荷、导出）。Deferred：Asset 异步、依赖图、引擎 Sleep、Profiler、运行中改池。Status → **`Planned`**；切片见 [Impl Plan](./CORE-F38_TASK_SYSTEM_IMPLEMENTATION.md)。

## Open items

无架构级待决。TaskFlow 目录/版本钉在 Impl Plan S00；失败时允许换接入形态，不改本文件契约。

## Implementation Readiness

- [x] Requirements / Constraints / Non-Goals（含「无 Asset 异步」）
- [x] Boundaries / Drain / Shutdown / 游戏线程身份 / 回调载荷
- [x] Verification（合成 1 vs N）
- [x] Open 项已收进 Decisions
- [x] Implementation Plan S00–S02

**Ready for implementation?** `yes`（S00–S02 only；先对齐 `feat/core` ← master）。

## Acceptance Checklist

- [x] Design review complete when required
- [x] Implementation plan for S00–S02
- [x] Progress / Registry 同步
- [x] `ASSET-F09` 标为后置，不挡 F38 Done

## Suggested slices

| Slice | Done when |
|-------|-----------|
| S00 | TaskFlow 接入；目标编译器可构建并链接；`Init` / `Shutdown` 可调用且重复 Shutdown 安全 |
| S01 | `Submit` / `Wait` / `DrainCompletions` 符合上文契约：游戏线程回调、`Wait` 后回调尚未依赖 Drain、工作异常与回调异常、队列锁外执行回调、Shutdown 拒绝新任务并派发剩余完成事件 |
| S02 | Init 时配置 Worker 数；CPU-bound 合成任务在 1 与 N 下结果一致，并记录耗时与加速比（可与功能 suite 分开跑） |

Profiler、Sleep 封装、Asset 异步、任务依赖图不在这些切片里。无「异步 DataAsset」切片。

## Learning path

1. F38：池 + Drain + 合成证明。  
2. 另开 Design：第一个真实客户（很可能仍是 DataAsset 字节读取，而非 Texture）。  
3. 更后：碰 RHI 的异步；再后：逻辑/渲染线程。

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-16 | Draft：初含 Texture / 后改 DataAsset 为证明客户 |
| 2026-10-17 | 评审锁定 Drain 相位；同步 Load/Drain 死锁说明（当时绑 F09） |
| 2026-10-17 | **收窄：** F38 仅 TaskFlow + 合成证明；真实 Asset 异步整段移出，归 `ASSET-F09` 后置 |
| 2026-10-18 | 再审：确认 Ready；曾写 A→B Drain 串联 |
| 2026-10-18 | 维护者：不要依赖图；单 Task 可验证即可；测试可用 std::sleep，引擎 Sleep 封装另 Feat。待审批 |
| 2026-10-18 | 评审修订：四阶段与句柄；Drain 批次与无 FIFO；Shutdown 拒绝再提交；Worker 禁止 Submit；异常必达 `Failed`；功能/性能验收分离 |
| 2026-10-24 | 开工前评审落地：游戏线程=`Init` thread id；回调无结果载荷；`MINENGINE_API`；Decision 9–11；Status **`Planned`**；维护者批准开工 |
| 2026-10-24 | S00–S02 实现落地；Status **`Done`**；验收见 Progress / Impl Landing notes |
