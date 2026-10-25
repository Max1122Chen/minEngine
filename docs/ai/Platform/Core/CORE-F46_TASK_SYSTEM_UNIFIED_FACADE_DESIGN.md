# CORE-F46 — TaskSystem Unified Facade（Named Frame Roles + Workers + Small Graph）

## Meta
- **ID:** `CORE-F46`
- **Type:** `Feature`（演进 / 真整合；吸收 F45 S04 spike）
- **Complexity:** `L2`
- **Status:** `Done`（S00–S03）
- **Owner:** project maintainer
- **Last updated:** `2026-10-25`（S03：backpressure/Profile 收口 + Multi smoke）
- **Related:**
  - [CORE-F38](./CORE-F38_TASK_SYSTEM_DESIGN.md) · [CORE-F45](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)
  - [ACTIVE_WORK](../../ACTIVE_WORK.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)
- **Branch / worktree:** `feat/core` @ `D:/Dev/GitRepo/minEngine-core`
- **Ready for implementation?** `yes`（自审 2026-10-24；主人 ACK 命名 + 开工）

## TL;DR

统一门面 `TaskSystem`：Worker（TaskFlow）+ Multi 常驻 Render/RHI OS 线程 + `ParallelFor`（Join）+ 小 DAG（`TaskFlowBuilder`/`SubmitFlow`）+ F38 `Submit`/可选 `onComplete`/`Drain`/`Wait`。

帧侧命名对齐 Task：**RenderFrameTask / RHIFrameTask / SubmitRenderFrameTask**（不再用 Job）。

不做 Named 泵；不做跨角色图节点。`FrameThreadScheduler` = spike，机制吸收。

## Problem / Intent

见 v2 讨论：双入口、Render 需扇出、释放 ParallelFor + 小图；常驻角色线程保留。

## Requirements

| ID | Requirement |
|----|-------------|
| R1 | 统一门面：扇出 + Multi 帧角色起停/投递 |
| R2 | Multi：Render/RHI 常驻有名 OS 线程 |
| R3 | `ParallelFor`：Game 或 Render；Join；无 completion 队列 |
| R4 | `Submit`；`onComplete` 可选；`Drain` 仅 Game；`Wait` = work/图结束 |
| R5 | 吸收 Scheduler；公共类型退出 |
| R6 | Single 不拉角色 OS 线程 |
| R7 | 小图：一次构建一次提交；节点仅 Worker CPU |
| R8 | 契约：Drain≠「系统只能给 Game」 |
| R9 | 帧队列/回调类型名用 **Task**，不用 Job |

## Non-Goals

- NamedThreads 收件箱泵；图节点 DesiredThread=Render/RHI/Game
- 跨帧活图、取消令牌、改 worker 数、完整 UE GraphEvent
- Asset 异步重设计；Cull 业务；替代 RenderCommandPipe
- Render 侧 Drain（本波）

## Architecture

```text
TaskSystem
├─ Workers (tf::Executor): ParallelFor, Submit, SubmitFlow, Wait, Drain(Game)
└─ Frame roles (Multi, private): Render/RHI OS threads + bounded frame tasks
FrameThreading: tid identity only
```

## Data structures

```cpp
struct TaskHandle { uint64_t Id = 0; bool IsValid() const { return Id != 0; } };

struct TaskRecord {
    uint64_t Id = 0;
    std::mutex Mutex;
    std::condition_variable Cv;
    bool bWorkFinished = false;
    bool bSucceeded = false;
    std::function<void(bool)> OnComplete; // optional; Game Drain only
};

struct CompletionEvent {
    uint64_t Id = 0;
    bool bSucceeded = false;
    std::function<void(bool)> OnComplete;
};

class TaskFlowBuilder {
public:
    using NodeFn = std::function<void()>;
    uint32_t AddNode(NodeFn fn);
    void Precede(uint32_t a, uint32_t b);
    void Precede(uint32_t a, std::initializer_list<uint32_t> bs);
private:
    friend class TaskSystem;
    struct Node { NodeFn Fn; std::vector<uint32_t> Successors; };
    std::vector<Node> m_Nodes;
};

struct RenderFrameTask {
    float DeltaTime = 0.0f;
    bool WaitForCompletion = false;
    uint64_t Sequence = 0;
};

struct RHIFrameTask {
    uint64_t Sequence = 0;
    bool SignalCompletion = false;
};
```

## Public API（目标）

```cpp
class TaskSystem {
public:
    using WorkFn = std::function<void()>;
    using CompletionFn = std::function<void(bool)>;
    using ParallelBodyFn = std::function<void(int32_t index)>;
    using RenderFrameTaskFn = std::function<void(float deltaTime)>;
    using RHIFrameTaskFn = std::function<void()>;

    static constexpr size_t kMaxPendingRenderFrames = 3;

    void Init(int workerCount = 0);
    void Shutdown();

    TaskHandle Submit(WorkFn work);
    TaskHandle Submit(WorkFn work, CompletionFn onComplete);
    void Wait(TaskHandle handle);
    void DrainCompletions();

    void ParallelFor(int32_t count, ParallelBodyFn body);
    void RunAndWait(WorkFn work);

    TaskHandle SubmitFlow(TaskFlowBuilder&& builder);
    TaskHandle SubmitFlow(TaskFlowBuilder&& builder, CompletionFn onComplete);

    void StartFrameRoles(RenderFrameTaskFn renderTask, RHIFrameTaskFn rhiTask);
    void StopFrameRoles();
    void SubmitRenderFrameTask(float deltaTime, bool waitForCompletion);
    void WaitForRenderQueueSpace();

    bool AreFrameRolesRunning() const;
};
```

### Semantics

| API | Caller | Blocks? | Completion |
|-----|--------|---------|------------|
| Submit / SubmitFlow | Game | no | optional onComplete → Game Drain |
| Wait | holder | until CPU/flow done | not Drain |
| DrainCompletions | Game | no | dispatches queue |
| ParallelFor / RunAndWait | Game or Render | yes (Join) | none |
| SubmitRenderFrameTask | Game | backpressure; optional wait whole frame | frame sequence |

## Sync model

1. **Drain (Game):** Submit → worker → queue → Drain → onComplete  
2. **Join (Game|Render):** ParallelFor / Wait → continue  
3. **Frame (Multi):** SubmitRenderFrameTask → Render task → optional ParallelFor → RHI task → Present  

RHI CreateBuffer = handle + ordered Replay；不是 Task onComplete。  
Render cull = ParallelFor Join；不是 onComplete 回 Render。

## Slices

| Slice | Done when | Status |
|-------|-----------|--------|
| S00 | Design Ready + Job→Task（本版） | **Done** |
| S01 | ParallelFor / RunAndWait + tests | **Done** |
| S01b | TaskFlowBuilder + SubmitFlow + diamond/cycle tests | **Done** |
| S02 | StartFrameRoles / SubmitRenderFrameTask；吸 Scheduler；Engine/Editor Multi | **Done** |
| S03 | backpressure / Profile 收口；更完整 Multi smoke | **Done** |

## Design Review（自审 · Mode A · 2026-10-24）

| Check | Result |
|-------|--------|
| Requirements / Non-Goals clear | Pass |
| Named roles vs workers | Pass（D1–D2） |
| Join vs Drain vs frame | Pass（D4/D8/D9） |
| Small graph scoped | Pass（D6；无 Named 泵） |
| Naming Task not Job | Pass（R9 / v3） |
| Shell / TBD blockers | None blocking |
| Verification | Suites + slices defined |
| Principles | YAGNI：无泵；ISP：Render 用 ParallelFor 不用 SubmitFlow（OQ6） |

**Verdict:** Ready  
**Ready for implementation?** yes  

## Decisions

| ID | Decision |
|----|----------|
| D1–D5 | 门面；常驻角色；ParallelFor Join；Drain Game；吸 Scheduler |
| D6 | 小 DAG；无 Named 泵；无跨角色节点 |
| D7 | F45 S04 按本切片 |
| D8 | RHI Create ≠ Task 回调 |
| D9 | Cull 回流 = Join |
| D10 | 帧侧类型/API 用 **Task** 命名（RenderFrameTask、SubmitRenderFrameTask、…） |

## Open Questions（非阻塞）

OQ1 平铺 API · OQ2 ParallelFor 记错继续 · OQ3 小 count 阈值实现定 · OQ4 无 Render Drain · OQ5 S01 可先合 · OQ6 SubmitFlow 仅 Game · OQ7 不暴露 tf::Taskflow

## Changelog

| Date | Note |
|------|------|
| 2026-10-24 | v1 门面 + ParallelFor |
| 2026-10-24 | v2 小图 + 代码示意 |
| 2026-10-24 | v3 Job→Task；自审 Ready；UTF-8 重写；开工 |
| 2026-10-24 | S02：帧角色进 TaskSystem；删公共 Scheduler；Engine/Editor Multi；OpenGL combine；suite 17 PASS |
| 2026-10-25 | S03：统一 WaitForRenderQueueSpace；Frame.* Profile；async / backpressure Multi smoke；Status → Done |
