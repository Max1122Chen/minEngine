# CORE-F38 TaskSystem Design Spec

## Meta
- **ID:** `CORE-F38`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `Draft`
- **Owner:** project maintainer
- **Last updated:** `2026-10-17`
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md) · 后置客户：`ASSET-F09`（异步资产，**另 Feature / 另 Design**）· 后置：逻辑/渲染线程分离（未编号）

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
- API（MVP）：`Submit`（可带完成回调）、`Wait`（调试/测试）、`DrainCompletions`。**不**暴露任务依赖图。
- Worker **不得**调用：RHI、`NewObject` / ObjectManager、AssetManager 缓存、Scene。MVP 证明作业也**不得**碰这些。
- 完成回调 **只**在游戏线程、经 `DrainCompletions` 派发。
- Drain 挂点（已锁定）：`LogicalTick` 里 **Input 之后、Fixed 之前**；每帧不 cap。
- 证明客户：**合成 CPU-bound 作业**（例如固定次数的数值循环 / 哈希），无文件、无 Asset。
- 性能门闩：同一批合成作业，`worker=1` vs `worker=N`，记录 wall-clock（门槛写在 Impl Plan）。
- 无窗口测试：`Submit → Drain → 断言回调线程是游戏线程`。

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

## Assumptions

- TaskFlow 可在 MinGW + MSVC 下链接（S00 验证）。
- 游戏线程 = 跑 `Engine::TickOneFrame` 的线程；本 Feature 不改帧拓扑。

## Non-Goals（本 Feature 明确不做）

- 任何 Asset 异步加载 API（DataAsset / Texture2D / Mesh…）。
- 拆 Loader 为「worker 读字节 + 游戏线程 NewObject」。
- 同步 Load 与异步 in-flight 合并 / 死锁契约（属 `ASSET-F09`）。
- 内存反序列化 helper、Asset 缓存线程策略。
- Texture / Material / Mesh 的 GPU 上传异步。
- 逻辑线程与渲染线程分离。
- 任务依赖图、取消令牌全家桶、运行中改 worker 数。
- 复制 UE NamedThreads / Godot 完整 MessageQueue。

## Mental model

| 角色 | 对应 | 本 Feature 允许 |
|------|------|-----------------|
| 主厨 | 游戏线程 | Submit、Drain、改引擎对象（本波证明码尽量少改对象） |
| 帮厨 | TaskFlow worker | 只跑合成 CPU 函数，写局部结果 |
| 传菜口 | 完成队列 + Drain | 把「算完了」带回游戏线程 |

**Drain** = 抽干完成队列，在游戏线程派发回调。不是关停线程池。

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

`LogicalTick`：**Input 之后、Fixed 之前**。

实现：先换出队列再派发，避免回调里 `Submit` 死锁同一把锁。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `TaskSystem` | 池生命周期、提交、完成队列、Drain | Asset、RHI、Scene、游戏玩法 |
| TaskFlow | 调度执行 | 引擎对象模型 |
| 测试 / harness | 合成作业与性能对比 | 真实内容管线 |

## Boundaries

- **Allowed on worker:** 纯函数、局部堆、无引擎单例突变。
- **Forbidden on worker:** RHI、`NewObject`、AssetManager、Scene、Lua、ImGui。
- **Dependency:** Runtime Core → TaskFlow。TaskSystem **不**依赖 Asset / Render。

## Ownership & Lifetime

- `Engine` 拥有 `TaskSystem`。
- Shutdown（游戏线程）：停 Submit → 等 worker 工作结束 → Drain 剩余完成项 → 销毁 executor。不静默丢弃已完成项。
- Completion 载荷（合成作业）：简单值或 `shared_ptr` 到纯数据；不移交 `MEObject`。

## Data / Control Flow

1. 测试或引擎：`Submit` N 个 CPU 作业。
2. Worker 跑完 → push Completion。
3. 每帧 Drain → 回调在游戏线程；测试断言 `thread_id`。
4. 性能：同一批作业，Init `worker=1` 与 `worker=N` 各跑一轮，比 wall-clock。

## State & Invariants

- `Stopped | Running | ShuttingDown`。
- 完成回调只在游戏线程。
- Worker 不持有或不发布 `MEObject` / Asset。

## API / Interface Semantics（草案）

### `Submit(work)` / `Submit(work, onComplete)`

- `work`：可在任意 worker 跑；必须遵守 Forbidden 列表。
- `onComplete`：仅 Drain 时、游戏线程调用。

### `Wait(handle)`（测试 / 调试）

- 等待 **worker 侧工作**结束。不代替 Drain；若需要回调副作用，仍须 Drain。
- 禁止：游戏线程 Wait「只有 Drain 才会置位」的事件且不再 Drain（死锁）。本 Feature 的合成测试应：Submit →（可选 Wait 工作）→ Drain。

### `DrainCompletions()`

- 仅游戏线程；每帧至少一次。

### Worker 数

- Init 参数；`1` 为对比基线；默认 `max(1, hardware_concurrency - 1)`。

## Failure Model

| Failure | Behavior |
|---------|----------|
| Submit 于 Shutdown | 拒绝 |
| Worker 异常 | 捕获 → error Completion（若 API 支持）或测试失败；不 `terminate` |
| Drain 回调抛错 | 记日志；尽量继续后续项 |

## Alternatives & Trade-offs

### Option A（推荐）— 本波只 TaskSystem + 合成证明

- Pros：范围小；先建立线程契约；避开 NewObject / Asset / RHI。
- Cons：暂时看不到「加载变快」的产品体感——用 wall-clock 门闩弥补。

### Option B — 本波顺手做 DataAsset 异步

- Why not：维护者要求更谨慎；对象与缓存契约未单独设计。后置为 `ASSET-F09`。

### Option C — 自研线程池

- Why not：与「TaskFlow 作内核」冲突。

## Integration Impact

- CMake / ThirdParty：TaskFlow。
- `Engine`：Init / Shutdown / Drain 一处调用。
- 新测试 suite（如 `task-system`）。
- **零** Asset 管线改动。

## Migration / Compatibility

- 无行为变更给现有加载路径。
- 无磁盘格式变更。

## Verification Strategy

- 功能：Submit → Drain；回调线程 == 游戏线程。
- 性能（必须）：合成 CPU 批次；1 vs N wall-clock；门槛在 Impl Plan（如本机 ≥1.5×，CI 可调或标 flaky-skip）。
- 回归：`verify.ps1` / `test smoke`。

## Deferred clients（非本 Feature）

以下仅记账，**不**写入本 Feature 验收：

| 后续 | 内容 | 备注 |
|------|------|------|
| `ASSET-F09` | 异步 DataAsset（读字节在 worker；NewObject 在游戏线程） | 需独立 Design；依赖 F10 + F38 |
| 未编号 | Texture / Mesh 异步 | 另拆 GPU；等渲染线程设计 |
| 未编号 | 逻辑/渲染线程 | 另 Feature |

`ASSET-F09` 里曾讨论过的「同步 Load 等字节 future、勿干等 Drain」等契约，**等开 F09 Design 时再锁定**，不绑在 F38 上。

## Decisions（2026-10-17）

| # | 决定 |
|---|------|
| 1 | Drain：**Input 后、Fixed 前**；不 cap |
| 2 | TaskFlow 接入：S00 选 submodule 或 ThirdParty |
| 3 | **本 Feature 不含 Asset 异步**；`ASSET-F09` 后置 |
| 4 | 证明与性能门闩 = **合成 CPU 作业** |
| 5 | MVP 不暴露依赖图 |
| 6 | Shutdown 必须 Drain 剩余完成项 |
| 7 | Worker 数 Init 时固定 |

## Design Review

- **Verdict:** `Ready with deferred items`（范围收窄后；Asset 异步整段 Deferred）
- **Reviewer / date:** 维护者收窄范围 + agent，2026-10-17
- **Summary:** S00–S02 足够闭环。原 F09 相关死锁/装盘契约移出本 Design。

## Implementation Readiness

- [x] Requirements / Constraints / Non-Goals（含「无 Asset 异步」）
- [x] Boundaries / Drain / Shutdown
- [x] Verification（合成 1 vs N）
- [x] Open 项已收进 Decisions 或明确 S00 再定

**Ready for implementation?** `yes` after Implementation Plan（S00–S02 only）。

## Acceptance Checklist

- [ ] Design review complete when required
- [ ] Implementation plan for S00–S02
- [ ] Progress / Registry 同步
- [ ] `ASSET-F09` 标为后置，不挡 F38 Done

## Suggested slices

| Slice | Goal |
|-------|------|
| S00 | Vendor TaskFlow + 空 `TaskSystem` Init/Shutdown + 链接 |
| S01 | Submit + Drain + `test task-system`（线程断言） |
| S02 | Init worker 数 + 合成作业 1 vs N wall-clock 门闩 |
| S03 | （可选）Profiler 作用域；文档收口 |

**无**「S0x 异步 DataAsset」切片。

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
