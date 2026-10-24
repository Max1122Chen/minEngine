# CORE-F45 — Frame Thread Separation（综述 / 契约）

## Meta
- **ID:** `CORE-F45`
- **Type:** `Feature`（真 refactor：帧所有权与命令边界）
- **Complexity:** `L3`
- **Status:** `In Progress`（**S00–S03 Done**；下一 S04 Multi OS）
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`
- **Doc role:** **综述** — 跨线程契约、`FrameThreadMode`、切片总序、验收。角色细节见子 Design。
- **Related:**
  - [Survey](./CORE-F45_FRAME_THREAD_SEPARATION_SURVEY.md)（Reference）
  - [Game](./CORE-F45_GAME_THREAD_DESIGN.md) · [Render](./CORE-F45_RENDER_THREAD_DESIGN.md) · [RHI](./CORE-F45_RHI_THREAD_DESIGN.md)
  - [CORE-F38](./CORE-F38_TASK_SYSTEM_DESIGN.md) · [CORE-F22 Profiler](./CORE-F22_PROFILER_HARNESS_DESIGN.md)
  - UE：`D:/Dev/GitRepo/UnrealEngine`（只读）
- **Branch / worktree:** `feat/core` @ `D:/Dev/GitRepo/minEngine-core`
- **Ready for implementation?** yes（S00–S02 已落地；S03 开工）

## TL;DR

| 角色 | 职责 | 子文 |
|------|------|------|
| Game | 主窗口 / PollEvents；仿真；按家族 Enqueue；ViewIntent | [Game](./CORE-F45_GAME_THREAD_DESIGN.md) |
| Render | 先画 World(N−1)；再 Apply(N)；Cull/Sort/Batch；录制 List | [Render](./CORE-F45_RENDER_THREAD_DESIGN.md) |
| RHI | 回放 List；亲和；Present | [RHI](./CORE-F45_RHI_THREAD_DESIGN.md) |

| Mode | OS 线程 | 行为 | 用途 |
|------|---------|------|------|
| **`Single`** | 1 | 同 API；命令入同线程队列，RenderRole Flush；List **Immediate** | CI、调试、Profile 基线 |
| **`Multi`** | 3 | 同 API；跨线程有界队列；List **Recording** | 并行、Profile 对比 |

契约两模式相同。`RHICommandList` 演进录制，不另起类型。Proxy：**按家族** Enqueue，禁中央巨石 POD。

## Intent playback

| 项 | 内容 |
|----|------|
| 问题 | 逻辑卡渲染；无法公平对比单/多线程 |
| 成功 | Single/Multi 可启动；同调用点；Profiler 可比；边界可测 |
| 非目标 | 帧中切 Mode；NamedThreads；本波重写 Material；ScreenUI 绘制全迁 |
| 约束 | Physics / ScreenUI 仿真、Drain 跟 Game |

## FrameThreadMode（权威）

```text
enum class FrameThreadMode { Single, Multi };
```

- **Single：** 不拉 Render/RHI OS 线程。顺序 `Logical → RenderRole → RHIRole`。Enqueue **入队**，RenderRole **开头 FlushApply**（与 Multi 同构，禁 GT 热写旁路）。List **Immediate**（Begin/Finish 可为 no-op）。
- **Multi：** 三 OS 线程。Enqueue / ViewIntent / Finished-List 有界队列；满则阻塞生产者。List **Recording**；RHI Replay + Present。

### 配置（锁定）

| 优先级（高→低） | 来源 | 示例 |
|-----------------|------|------|
| 1 | CLI | `--frame-threads=single\|multi` |
| 2 | Env | `MINENGINE_FRAME_THREADS=single\|multi` |
| 3 | `EngineConfig.FrameThreadMode` | 若有 |
| 4 | **默认 `Single`** | |

- 仅 **Engine 启动** 选定；**禁止帧中切换**。
- 非法值：**启动失败**（Error 日志），不静默回退。
- 启动日志：Mode + Game/Render/RHI tid（Single 时三 kind 同 tid）。
- 测试：默认 Single；Multi 可选套件（无头可 skip）。

### Profile 区段（锁定名）

- `Frame.Game.Logical` / `Enqueue` / `ViewIntent` / `WaitRenderQueue`
- `Frame.Render.View` / `CullSort` / `Record` / `Apply` / `WaitRHI`
- `Frame.RHI.Replay` / `Present`  
Single 下仍按角色包区段。对比 = 两次启动换 Mode。

## Architecture

```text
  Game (= main, PollEvents)
    EnqueueRenderCommand / SubmitViewIntent
         │  Single: 同线程队列    Multi: SPSC
         ▼
  RenderRole  画 World(N-1) → Record List → Apply(N)
         │  Finished list
         ▼
  RHIRole     Replay → Present
```

### Boundaries

| 方向 | 允许 | 禁止 |
|------|------|------|
| Game→Render | Enqueue；ViewIntent | GT 热 Sync；直调 GPU |
| Render→RHI | Finished List；创建/上传请求 | 改 GO |
| RHI→Game | 无常态改 GO | 锁 MEObject |
| Worker | 文件→Json | GO / 热 Proxy / RHI |

### CS 索引

CS1 GO→Game · CS2 Proxy 家族 Enqueue→Game+Render · CS3 Intent↔World→Game+Render · CS4 Cull→Render · CS5 List→RHI · CS6 Device→RHI · CS7 Drain→Game · CS8 窗口/Mode→本文+Game · CS9 Profile→本文

## Shared API（开工名；标识符可微拼写调整，语义勿改）

| API | 说明 |
|-----|------|
| `FrameThreadKind` / `IsIn*Thread` / `AssertIsFrameThread` | S00 |
| `GetFrameThreadMode()` | Single / Multi |
| `EnqueueRenderCommand(F&&)` | UE 语义；**lambda/小可调用体**；附带 `FrameIndex`（管道包装） |
| `RenderScene::Update/Remove{Primitive,Light,SkyBox,Widget}` | GT 入口；内部 Create+Enqueue |
| `SubmitViewIntent(ViewIntent)` | 有界；满阻塞 |
| `RHICommandList::BeginRecording` / `FinishRecording` / `Replay` | Multi 必用；Single Immediate 短路 |
| Engine：`RunRenderRole` / `RunRHIRole`（名可改） | Single 内联；Multi 踢线程 |

## 帧时间线（锁定）

```text
Game 帧 N:  Poll → Logical（Enqueue FrameIndex=N）→ SubmitViewIntent(N)
RenderRole: Take Intent(N)
            Wait publishedWorldGen ≥ N-1
            Cull/Sort + Record（画 World(N-1)）→ Dispatch List
            FlushApply(FrameIndex==N) → publishedWorldGen = N
RHIRole:    Replay（Multi / 非 Immediate）→ Present
```

- 首帧 N=0：允许仅 Clear/空画；不 Assert 失败。
- **有界深度（锁定）：** ViewIntent 队列 `kMaxPendingViewIntents = 3`；Render 命令队列 `kMaxPendingRenderCommands = 4096`（条目为可调用体；可调但须有常量）；Finished List 池 `kMaxInFlightCommandLists = 3`。

## Decisions（跨线程；子文不得冲突）

| # | 决定 |
|---|------|
| 1 | 三角色；Physics / ScreenUI 仿真跟 Game |
| 2 | Game = 主窗口 / PollEvents 线程 |
| 3 | Proxy **按家族** Enqueue；禁中央巨石 POD；保留 `RenderScene::Update*` |
| 4 | ViewIntent(N) ↔ World(N−1)（含 Single） |
| 5 | 队列满 **阻塞** 生产者 |
| 6 | 演进 `RHICommandList`；不另起平行类型 |
| 7 | Drain 留 Game |
| 8 | `FrameThreadMode::{Single,Multi}` 一等；启动选定；禁帧中切 |
| 9 | 可切片；非「仅 Multi」产品态 |
| 10 | ScreenUI 绘制全迁后置 |
| 11 | Profile：两次启动 + 对称区段名 |
| 12 | Enqueue：**两模式均入队 + RenderRole Flush**（Single 禁同步热写旁路） |
| 13 | 命令载体：**lambda / `TFunction` 类可调用体（形态 A）**；禁万能 variant 当扩展轴 |
| 14 | Mode 配置优先级 CLI>Env>Config>**默认 Single**；非法值启动失败 |
| 15 | Single List = **Immediate**；Multi = **Recording** |
| 16 | Present = **RHIRole 末尾**统一调用（不强制进 List Op） |
| 17 | Proxy 推送：**UE 模型** — Create≠登记；`MarkRenderTransformDirty`→具名小包；`MarkRenderStateDirty`→Recreate；Light 另有 Color 小包；Sky 指针 Set/Disable；**禁**全家桶 Snapshot / Sync-scratch（细则见 Game） |
| 18 | 有界常量见上节 |

## Implementation slices

| Slice | Done when |
|-------|-----------|
| **S00** | ✅ 身份 + Mode 解析/日志；Single 顺序跑通≈今日；**尚不改 Proxy 热路径** |
| **S01** | ✅ 四家族 Enqueue；Transform/Color 具名小包或 Recreate；禁全家桶 Snapshot |
| **S02** | ✅ ViewIntent 代际；N−1 |
| **S03** | ✅ List Recording + Immediate；Pass 调用约定不变 |
| S04 | Multi OS 线程；有界阻塞；Wait 区段 |
| S05 | GL/Vulkan 亲和；Present 仅 RHI |
| S06 | 清旧直调清单；文档；ImGui 小决策；可选 Multi 烟测 |

## Verification

- Single：smoke + 相关 render suite（主 CI）。
- Multi：手工/可选自动化；开发断言开。
- 模式对称：同场景可接受 1 帧延迟差；Profiler 两份区段可 diff。
- 错线程断言；队列满阻塞可测；四家族无 GT 热写。

## Failure / Shutdown

停 Game 提交 → 排空命令与 Intent → 停录制 → 排空 RHI → idle → join（Multi）→ 毁窗口/RHI。

## Design Review（Mode A 收口 · 2026-10-24）

子 agent 任务曾中断；由主会话按 design-review Mode A 自审并落锁定项。

| ID | 原问题 | 处置 |
|----|--------|------|
| D01 | Single「同步 Enqueue」vs「入队 Flush」矛盾 | **D12** 锁入队+Flush |
| D02 | FlushApply 形态 A/B 未选 | **D13** 锁 A（可调用体） |
| D03 | Mode 默认/非法值未锁 | **D14** |
| D04 | Single List Immediate vs 可选 Recording 含糊 | **D15** |
| D05 | Present 在 List 内还是 Role 末 | **D16** |
| D06 | Multi 时间线曾写成 Apply 在 Record 前 | 已改正文序 |
| D07 | Recreate 标准过虚 | Game 子文 MVP 表 |
| D08 | 队列深度无数字 | **D18** |
| D09 | 巨石 POD / 缺 Light·Sky | 已纠偏（D3） |

**Verdict：** Ready after clarifications → **澄清已写入 Decisions 12–18**。  
**Ready for implementation?** `yes`（维护者已 ACK；S00 进行中）。

### 维护者终审清单（请勾）

- [x] 接受 D1–D18（尤其 D3 家族 Enqueue、D8/D14 Mode、D12–D13 管道）
- [x] 接受 S00 先只做身份+Mode、S01 再动 Proxy
- [x] ACK 后 Status → `In Progress` / 开工

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | **S01/S02 Done：** 显式 Proxy 推送 + ViewIntent/先画后 Apply |
| 2026-10-24 | **S00 Done：** `FrameThreading` + CLI + Engine assert/scope + suite `frame-threading` |
| 2026-10-24 | Status → In Progress；S00 开工 |
| 2026-10-24 | Draft→Review；拆分；List 录制；Mode；Proxy 家族纠偏 |
| 2026-10-24 | **Mode A 收口：** 锁 D12–D18；修 Single/Multi 工序矛盾；Ready=yes 待 ACK |
