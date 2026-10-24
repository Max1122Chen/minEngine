# CORE-F38_TASK_SYSTEM Implementation Plan

## Meta
- **ID:** `CORE-F38`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`（S00–S02 落地）
- **Related:** [Design Spec](./CORE-F38_TASK_SYSTEM_DESIGN.md)
- **Design readiness:** `Ready with deferred items`
- **Branch:** `feat/core`（开工前先对齐当前 `master`）

## Goal

按 Design 交付可 Init/Shutdown 的 `TaskSystem`（TaskFlow 内核）、主线程 `DrainCompletions`、以及合成作业功能/性能证明。不含 Asset 异步。

## Design Decisions Being Implemented

| Decision | Design section | Notes |
|----------|----------------|-------|
| D1 Drain 相位 / 批次换出 | Drain 挂点 · `DrainCompletions` | Input 后、Fixed 前；持锁不跑回调 |
| D2 TaskFlow vendored header-only | Constraints · Decisions #2 | 默认 `Third-Party`；失败再换 submodule |
| D3 无 Asset 异步 | Non-Goals | 零 Asset 改动 |
| D4 Wait ≠ Drain | 四阶段 · API | 测试断言 Wait 后回调未跑 |
| D5 无依赖图；Worker 禁 Submit | Boundaries | |
| D6/D7 异常与 Shutdown | Failure · Shutdown | |
| D8 功能/性能分离 | Verification | 性能可跳过 |
| D9 游戏线程 = Init thread id | Game thread identity | |
| D10 回调无结果载荷 | Submit / onComplete | 调用方缓冲区 |
| D11 `MINENGINE_API` | Integration Impact | 不扩 `.def` |

## Implementation Slices

| Slice ID | Summary | Design decision | Dependencies | Verification | Status |
|----------|---------|-----------------|--------------|--------------|--------|
| `CORE-F38-S00` | TaskFlow 接入 + `TaskSystem` Init/Shutdown + Engine 挂接骨架 | D2, D9, D11 | `feat/core` ← master | 构建链接；重复 Shutdown；Init 记 game thread | **Done** |
| `CORE-F38-S01` | Submit / Wait / DrainCompletions + 功能 suite | D1, D4–D7, D9–D10 | S00 | `minEngineTests.exe test task-system` | **Done** |
| `CORE-F38-S02` | Worker 数配置 + 1 vs N 合成性能实验 | D8 | S01 | 同 suite 可跳过段或独立入口；记录耗时 | **Done** |

## Slice → Design Traceability

### CORE-F38-S00

- **Design decision:** TaskFlow 接入；`Init`/`Shutdown` 状态机；游戏线程身份；Engine 拥有 TaskSystem。
- **Implementation:**
  - Vendor TaskFlow 至引擎既有 Third-Party 布局（优先 header-only include path）；CMake 暴露 include，MinGW + MSVC 可编过。
  - 新增 `TaskSystem`（公开头 + `MINENGINE_API`）：状态 `Stopped|Running|ShuttingDown`；`Init(workerCount)` 记录 `std::thread::id`；`Shutdown` 在尚无任务时等价「拒 Submit → 毁 executor → Stopped」（完整等任务逻辑在 S01）。
  - `Engine`：子系统初始化路径调用 `Init`（默认 worker = `max(1, hardware_concurrency-1)`）；`Shutdown` 路径调用 `TaskSystem::Shutdown`；`LogicalTick` 在 `m_InputSystem->Tick` 之后、`TickGameplayFixedPhase` 之前预留/调用 `DrainCompletions`（S00 可为空实现）。
- **Verification:** 目标配置构建成功；单测或小 harness：`Init` → 重复 `Shutdown` 无崩溃；非 Init 线程调用 `Shutdown` 被拒绝（可 S01 一并测）。

### CORE-F38-S01

- **Design decision:** 四阶段契约；完成队列；异常；Shutdown 排空。
- **Implementation:**
  - `Submit(work)` / `Submit(work, onComplete)`：仅游戏线程 + Running；返回 `TaskHandle`。
  - Worker 跑 `work`；捕获异常 → `Failed`；入队完成事件（句柄 + `bSucceeded`）；`Wait` 与工作结束同步。
  - `DrainCompletions`：swap 批次、锁外回调；同批新完成留给下次；非游戏线程拒绝。
  - `Shutdown`：ShuttingDown → 等已接受工作结束 → Drain 剩余 → 毁池 → Stopped。
  - 完成回调签名 MVP：`void(bool bSucceeded)`（或等价）；不传计算结果。
- **Verification（suite `task-system`，CI 不可跳过）:**
  - Submit → Wait：工作写入对等待线程可见；Wait 后、Drain 前回调未跑；Drain 后回调在 Init 线程执行。
  - 异步：Submit 后用条件变量等到 Drain 路径上的回调（勿固定 sleep 猜时序）。
  - 工作抛异常 → Failed + `onComplete(false)`；回调抛异常 → 同批后续仍派发。
  - Shutdown：拒新 Submit；已接受任务回调被 Drain；重复 Shutdown 无操作；ShuttingDown 回调内 Submit 得空句柄。

### CORE-F38-S02

- **Design decision:** 性能实验与功能分离；1 vs N。
- **Implementation:**
  - `Init` 可指定 worker 数；测试：`Shutdown` 后以 `worker=1` 与 `worker=N` 各跑一轮。
  - 一批彼此独立的 CPU-bound 作业（禁止 sleep、禁止共享累加器锁）；核对最终结果一致；记录 worker / 任务数 / 耗时 / 加速比到 Progress 或测试日志。
- **Verification:** 结果一致为门禁；加速比本机记录，**非**全环境硬门槛；CI 可跳过本段。

## Integration Points

- Modules：`Runtime` Core（`TaskSystem`）、`Engine`（Init / Drain / Shutdown）、`Tests`（`task-system`）、CMake Third-Party。
- Data contracts：完成队列条目 = 句柄身份 + 成功/失败；无 `MEObject`。
- Export：`MINENGINE_API` only。
- Feature flags：无。

## Risks / Mitigations

| Risk | Mitigation |
|------|------------|
| TaskFlow 在 MinGW 上编译问题 | S00 先验证；必要时钉版本或换 submodule / 精简头 |
| `feat/core` 落后 master | **先 rebase 或 merge master** 再开 S00 |
| Drain 回调过重卡帧 | MVP 不 cap；调用方约束；后置再议 |
| 与 ASSET-F10 并行分心 | F38 分支只做 TaskSystem；不碰 Asset |

## Verification per Slice

| Slice | Command / check |
|-------|-----------------|
| S00 | `cmake --build minEngine/build --target minEngine`（+ Editor 可选）；Init/Shutdown 冒烟 |
| S01 | `minEngineTests.exe test task-system`；`.\scripts\verify.ps1` |
| S02 | 同上 suite 性能段或 `test task-system-perf`（若拆分）；记录 wall-clock |

## Migration / Compatibility

- 无磁盘格式变更；无现有加载行为变更。
- 无旧 API 可删。

## Deferred Work

| Item | Why deferred | Unblock condition |
|------|--------------|-------------------|
| `ASSET-F09` 异步 DataAsset | 需独立对象/缓存契约 | F38 Done + 另开 Design |
| 任务依赖图 / TaskGroup | 维护者明确不做 | 新 Feature |
| 引擎 Sleep / mock 时钟 | 另域 | 新 Feature |
| Profiler `Task.*` 挂钩 | 非 MVP | F38 站稳后可选 |
| 运行中改 worker 数 | 非 MVP | 需求出现时 |
| 逻辑/渲染线程 | 更大 Feature | Capability 轨另开 |

## Completion Criteria

- [x] S00–S02 Done 或显式 Deferred 并记账
- [x] Design Acceptance / Verification 覆盖
- [x] Registry / ACTIVE_WORK / Progress 同步；`ASSET-F09` 仍 Deferred
- [x] Docs + engineering DoD；可提议 prepare commit

## Landing notes (2026-10-24)

- TaskFlow **v3.10.0** vendored at `minEngine/Third-Party/taskflow`（header-only；无 CUDA/SYCL）。
- `TaskSystem` under `Runtime/Core/Tasks`；`Engine` Init/Shutdown + Drain after Input.
- Suite `task-system` registered + smoke order entry.
- Verify: `minEngineTests.exe test task-system` — 8 passed（含 perf）；`test smoke` from `bin` — PASS（含 task-system 7 smoke cases）。
- Perf sample（本机）：workers=1 ~0.009s vs workers=32 ~0.001s，speedup ~6.3（非 CI 硬门闩）。

## Blocked/Deferred note

- 无。后置客户仍见 Design Deferred（`ASSET-F09` 等）。
