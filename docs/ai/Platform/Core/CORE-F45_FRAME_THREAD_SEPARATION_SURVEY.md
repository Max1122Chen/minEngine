# CORE-F45 — Game / Render / RHI Thread Separation Survey

## Meta
- **ID:** `CORE-F45`
- **Type:** `Feature`（调研先行；正式 Design 另文或本文升级）
- **Complexity:** `L3`
- **Status:** `Draft`（Survey）
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`（维护者锁定三线程目标模型）
- **Related:**
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - [CORE-F38 TaskSystem](./CORE-F38_TASK_SYSTEM_DESIGN.md)（池 + Drain；**不是**帧线程模型）
  - [ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md) · Prefer Simplicity（三线程成本已记录）
  - 后置客户：ASSET-F11 JSON AsyncLoad 通用化（不挡本调研）
- **Branch / worktree:** `feat/core` @ `D:/Dev/GitRepo/minEngine-core`（主树保持稳定给 minCraft）
- **Authority:** 本文为 **Survey** — 重建现状与选项；**不**授权大范围改帧循环，直至 Design + readiness。

## TL;DR

目标能力：把今日单线程「LogicalTick → RendererTick」演进为 **Game / Render / RHI 三 OS 线程**：

- **Game：** 逻辑 + 向 Render 推送 Proxy / 场景表示更新；Physics / ScreenUI 仿真跟 Game。
- **Render：** Cull / Sort / Batch，生产 Draw / RenderCommand，写入 RenderQueue。
- **RHI：** 消费 RenderQueue，真正提交 GPU。

本阶段只做 **Survey → Design**；**禁止**未 readiness 前拆线程实现。

## Pre-flight（2026-10-24）

| 项 | 评估 |
|----|------|
| Prerequisites | F38 TaskSystem Done（worker 池 ≠ 帧线程）。帧循环已有 Logical/Renderer **函数分段**，仍同线程。RHI 抽象存在，调用面需审计。 |
| Tech-debt risk if rushed | **High** — 半拆线程 + 双路径 Tick 会污染 Core/Render |
| WIP | F33 / F37 / F10 / ASSET-F04 等 In Progress；本 Feature 升 Primary 时其它大改应排队或缩窄 |
| Philosophy | Prefer Simplicity 偏向两线程分期；维护者选择 **直接三线程目标态** — 可接受，但 Design 必须写清双队列、线程亲和、调试策略，避免半成品 |
| Recommendation | **Go** — Survey 收口（三线程已锁定）→ Design（Game\|Render\|RHI）→ readiness → 实现；实现切片仍可「先通双队列契约，再挂第三线程」但**目标契约按三线程写** |

## Problem

1. 今日 `TickOneFrame` 在同一 OS 线程上顺序执行逻辑与渲染；逻辑尖刺直接拖渲染/呈现。
2. F38/F09 的 worker 池解决的是 **后台作业**，不解决 **帧相位并行**（逻辑与渲染重叠、RHI 提交与游戏仿真解耦）。
3. 若无统一命令队列 / 线程身份契约，各系统会各自 `std::thread`，比不做更糟。

## Current architecture（code truth）

```text
Engine::TickOneFrame  (single OS thread = TaskSystem game thread)
  PollEvents
  TickLogicalFrame
    LogicalTick:
      Input → DrainCompletions → Fixed → Variable gameplay
      Audio / Physics / UI → EOF / deferred structural
  TickRendererFrame
    RendererTick → RenderSystem::Tick
```

证据锚点：`Engine.cpp` `TickOneFrame` / `LogicalTick` / `RendererTick`。

| 观察 | 含义 |
|------|------|
| Logical 与 Renderer 已是 **API 分段** | 有迁移缝，但无线程边界、无帧队列 |
| Drain 挂在 Logical（Input 后 Fixed 前） | 拆线程后 Drain **必须仍在游戏线程**；完成回调不得上 Render/RHI 线程 |
| TaskSystem「游戏线程」= Init thread | 拆线程后该定义仍指 **逻辑线程**，不是渲染线程 |
| Asset AsyncLoad 组装在游戏线程 | 与「仅游戏线程碰 MEObject」一致，拆线程后不变 |

## What this Feature is / is not

| 是 | 不是 |
|----|------|
| 定义帧线程角色与合法 API 面 | 再发明一套 TaskFlow |
| 跨线程命令/资源发布契约 | 立刻三 OS 线程满配 |
| 与现有 Logical/Renderer 分段对齐的迁移 | 复制 UE NamedThreads 全家桶 |

## Candidate thread model

### Option A — Game \| Render（曾建议分期默认）

- Render 线程兼做 RHI。更简单，但与维护者目标（Render 只产命令、RHI 只消费）不一致。

### Option B — Game \| Render \| RHI（**已锁定为目标**）

| 线程 | 负责 | 不负责 |
|------|------|--------|
| **Game** | 输入、仿真、Scene/GO 可变状态、Asset 组装、TaskSystem Drain、**Physics**、**ScreenUI 仿真/命中**、向 Render 推送 **Proxy / 视图表示** | 不直接 Sort/Batch/Submit GPU |
| **Render** | 消费 Proxy/快照；Cull / Sort / Batch；生产 DrawCommand / RenderCommand 入 **RenderQueue** | 不改 GO；不直接 `vkQueueSubmit`（目标态） |
| **RHI** | 消费 RenderQueue；资源创建/上传策略（Design 细化）；真正 GPU submit | 不跑 gameplay；不碰 MEObject 图 |

风险（接受并写入 Design）：双队列同步、GL/Vulkan 线程亲和、调试难度、Shutdown 序。实现切片可先「逻辑上三角色、物理上先两线程跑通队列」，但**契约按三线程写死**，避免永久两线程债。

### Option C — 仅把 RendererTick 丢到临时线程（反模式）

- 无所有权契约 → **拒绝**。

## 「UI 跟 Game」指什么

指今日 `LogicalTick` 里的 **`UISystem::Tick`**（ScreenUI）：布局、指针命中、Widget 状态、与 gameplay 同一仿真时钟的交互。

- **跟 Game：** 这些仿真/输入逻辑仍在 **Game 线程**（与 Physics 一样），不迁到 Render/RHI。
- **不表示：** UI 的最终三角形提交永远在 Game 上画。Widget 最终仍应变成 Render 侧可消费的绘制表示（经 Proxy 或等价路径进 RenderQueue）——细节在 Design 定。
- **Editor 壳 ImGui** 是否同规则：另开一小段 Design 决策（常仍跟「呈现/主窗口」线程，与 ScreenUI 可能不同）。

## Hard questions（Survey → Design）

| # | 问题 | 维护者锁定 / 默认 |
|---|------|-------------------|
| 1 | Scene/GO 可变状态所有者 | **仅 Game**；Render 只读 Proxy/快照 |
| 2 | 帧延迟 | **允许 ≥1 帧**（Game N → Render/RHI 画 N−1）；精确延迟数 Design 定 |
| 3 | Physics | **跟 Game** |
| 3b | ScreenUI 仿真 | **跟 Game**（见上节） |
| 3c | Audio | 暂跟 Game（可后置） |
| 4 | Editor / 单测 | Design 必须给「单线程 fallback 或假三线程」策略 |
| 5 | GL/Vulkan 上下文亲和 | Design 审计现 RHI；RHI 线程为提交亲和点 |
| 6 | TaskSystem | Drain/组装仍在 **Game**；worker 禁碰 Render/RHI 与 Object |
| 7 | Shutdown / 设备丢失 | Design 写序：停提交 → 排空队列 → 拆线程 |
| 8 | 验收 | 正确性优先；加速非 CI 硬门禁 |
## External reference plan（实现前）

对比时打开本机源码树，禁止纯记忆下具体类结论：

- **UE：** GameThread / RenderingThread / RHIThread；`ENQUEUE_RENDER_COMMAND`；一帧延迟惯例。
- **Godot 4：** 进程模型与 Render 线程（按版本核对）。

每条结论在 Design 里附 **文件路径锚点**。

## Suggested phases（非实现排期承诺）

| Phase | 产出 | 完成标准 |
|-------|------|----------|
| S0 Survey | 本文收口（三线程 + Physics/UI 锁定） | 维护者批准进 Design |
| S1 Design | 三线程契约、Proxy、RenderQueue、迁移切片 | design-review Ready |
| S2+ Impl | 按 Design 切片；旧单线程路径删除或显式债 | 真 refactor |

## Relationship to ASSET-F11 / F09

- F09/F11 继续假设 **组装在游戏（逻辑）线程**。
- 帧线程分离 **不**依赖 F11；F11 **不**挡 F45。

## Non-Goals（本 Survey / 近期 Design）

- 多玩家 net 预测线程。
- Job Graph 取代 TaskSystem。
- 为拆线程而重写 Material/全部 RHI。

## Decisions（维护者已锁）

| # | 决定 |
|---|------|
| D1 | 目标模型 = **Game \| Render \| RHI 三线程**（Render 产命令，RHI 消费） |
| D2 | 允许渲染管线落后逻辑 **≥1 帧** |
| D3 | Physics、ScreenUI 仿真 **跟 Game** |
| D4 | Game 经 **Proxy（或等价）** 推送给 Render |
| D5 | 协作在 **`feat/core` + `minEngine-core` 工作树**；主树稳定供 minCraft |
| D6 | 下一步 = 收口 Survey → **三线程 Design**；不直接改帧循环代码 |

## Design Review

- **Verdict:** `Pending`（Survey；目标模型已锁，待进 Design）
- **Ready for implementation?** `no`

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | Survey Draft：现状锚点；两线程默认；硬问题表；与 F38/F09 边界 |
| 2026-10-24 | 维护者锁定三线程 + Proxy + Physics/UI 跟 Game；澄清 ScreenUI 含义；工作树策略 |
