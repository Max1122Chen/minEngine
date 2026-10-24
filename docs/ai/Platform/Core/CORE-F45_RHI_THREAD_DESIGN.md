# CORE-F45 — RHI Thread 重构 Design

## Meta
- **ID:** `CORE-F45`（子 Design · RHI）
- **Parent:** [综述](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)
- **Status:** `In Progress`（S03 Immediate/Recording 落地中）
- **Last updated:** `2026-10-24`
- **Slices:** S03 List 录制化 / S04 回放端 / S05 亲和；录制时机见 [Render](./CORE-F45_RENDER_THREAD_DESIGN.md)
- **UE：** `FRHICommandList` Recording + `DispatchToRHIThread`；Bypass / Immediate
- **澄清：** 不废弃 `RHICommandList`；不把 GL 后端 `gl*` 再包一层游戏 API

## 角色

**RHI：** GPU 设备与上下文亲和；**回放**已 `FinishRecording` 的 List；创建/上传；Present。不碰 GO / MEObject。

**与 Mode：**

| Mode | 行为 |
|------|------|
| **Single** | `RHIRole` 同 tid；List **Immediate**（调用即 `RHICmd*`）或 Record 后同 tid 立刻 Replay；Present 在本角色 |
| **Multi** | 独立 RHI tid；只消费 Finished List；持 GL context / Vulkan submit |

Pass **永不** `if (Multi)` 换 API——只拿 `RHICommandList&`。

## 今日 → 目标

| 今日 | 目标 |
|------|------|
| List 每个方法立即 `m_RHI->RHICmd*` | Immediate **或** 写入命令流；`FinishRecording` + `Replay` |
| Present / Clear 常在 RenderSystem.Tick | 归 RHIRole |
| GL context 随窗口初始化线程 | Multi：迁到 RHI tid（S05） |

## 职责表

| 做 | 不做 |
|----|------|
| Replay List；Present | Cull / 改 Proxy 策略 |
| `RHICreate*` 热路径（或执行创建请求） | `PollEvents` |
| 维护 List 池归还 | 读 GO |

## `RHICommandList` 状态机（S03）

```text
Idle
  → BeginRecording() → Recording
       调用 Draw*/Set*/BeginPass… → append 到内部流
  → FinishRecording() → Finished
       （不可再录）
  → Replay() on RHI role → 按序 RHICmd* → Idle（reset）

Immediate 短路（Single 默认推荐）:
  任意 Cmd 方法 → 直接 RHICmd*；Begin/Finish 可为 no-op
```

### 对外 API（保持现有方法名）

已有：`BeginRenderPass`、`SetGraphicsPipelineState`、`DrawIndexed`、`SubmitMeshDrawPacket`、`Create*`…  
新增（名可微调）：

| 方法 | 含义 |
|------|------|
| `BeginRecording()` | 进入 Recording；Immediate 模式 no-op |
| `FinishRecording()` | 封口；Immediate no-op |
| `Replay()` | 仅 RHI 角色；Immediate 下不应需要 |
| `IsImmediate()` / 跟随 `GetFrameThreadMode()` | 供断言 |

### MVP 命令操作码（录制流）

优先覆盖 Forward 主路径（与现 Pass 调用同构）：

| Op | 来源方法 |
|----|----------|
| BeginRenderPass / EndRenderPass | 同名 |
| SetGraphicsPipelineState | 同名 |
| SetShaderBindingSet | 同名 |
| Transition | 同名 |
| SetViewport | 同名 |
| SetVertexBuffer / SetIndexBuffer | 同名 |
| DrawIndexed / Draw | 同名 |
| GenerateMips | 同名 |
| SubmitMeshDrawPacket | 可拆成上述序列或单复合 Op |
| Present | **不**作为 List 必选 Op；由 **RHIRole 末尾** `RHIPresent`（Decision 16） |
| Create* 请求 | Multi Recording：入队，Replay 时执行；Single Immediate：可同步 Create |

载荷：存 **指针 + POD**（PSO、Buffer、Texture 句柄）；禁存 GO*。资源须保证 Replay 前未 Destroy（retire/fence 与 Render 约定）。

### Immediate vs Recording 选择

| 条件 | 模式 |
|------|------|
| `FrameThreadMode::Single` | **默认 Immediate**（行为≈今日，利 CI/Profile 基线） |
| `FrameThreadMode::Multi` | **强制 Recording** + RHI Replay |
| 测试强制 | 允许 Single 下开 Recording+同 tid Replay，验封口逻辑（可选套件） |

## RHIRole 工序

```text
Assert RHI
Profile Frame.RHI.Replay
  while (list = dequeue finished):
      list.Replay()
      return list to pool
Profile Frame.RHI.Present
  Present / swap  （若本帧有颜色目标需要呈现）
```

Single：RenderRole 结束若已 Immediate 执行完 draw，RHIRole 可能仅 Present；若用 Recording 短路则此处 Replay。

## 与 Render 的交接

```text
Render: BeginRecording → Passes → FinishRecording → enqueue(list)
RHI:    dequeue → Replay → pool.free(list)
```

- 有界池：`kMaxInFlightCommandLists = 3`（综述 D18）。
- 满：阻塞 Render（`Frame.Render.WaitRHI`）；间接背压 Game。
- 禁止 Render 在 Finish 后继续写同一 list。

## 创建 / 上传策略

| 阶段 | 策略 |
|------|------|
| S00–S02 Single | 允许现有同步 `Create*` 同 tid |
| S03+ Multi | 热路径 Create 经 List 请求或仅 RHI 线程调；Impl 列出迁移点 |
| 资产加载 | Game/Worker 出 CPU 数据；上传 Enqueue 到 RHI |

## 上下文亲和（S05）

### OpenGL

1. 窗口仍由 Game 创建；`PollEvents` 在 Game。
2. Multi 启动后：`glfwMakeContextCurrent(window)` **只在 RHI 线程**；Game/Render **不得** `gl*`。
3. Single：context 可留在唯一 tid（今日行为）。
4. 切换 Mode 需重启进程（综述锁定）。

### Vulkan

- `BeginFrameRecording` / submit / Present：仅 RHI 角色。
- 与现 `VulkanRHI` 帧录制对齐：Replay 驱动或 Role 包裹现有 Begin/End。

## Profiler 区段（RHI）

| 名 | 包围 |
|----|------|
| `Frame.RHI.Replay` | 回放 Finished lists |
| `Frame.RHI.Present` | Present / swap |
| `Frame.RHI.Create` | 可选：同步创建热路径 |

与 Game/Render 区段名一起构成 Single vs Multi 可比对 trace。

## 代码落点

| 区域 | 变化 |
|------|------|
| `RHICommandList.h/.cpp` | 状态机、命令流、Immediate 短路、池友好 reset |
| `OpenGLRHI` / `VulkanRHI` | 仍实现 `RHICmd*`；不改 Pass |
| `GLFWWindowSystem` | context 当前线程策略；Multi 移交 |
| `RenderSystem` / Engine | RHIRole；派发/回收 List |

## 验收

- [x] Single Immediate：Pass 零改调用约定，smoke 绿
- [ ] Multi Recording：Finish 前无 Present；Replay 仅 RHI 身份
- [ ] 错线程 Present / Replay 断言
- [ ] S05：Multi 下 Game tid 无 GL draw
- [ ] Profiler：`Frame.RHI.*` 两 Mode 可见
- [x] 无第二套 Pass 命令 API

## 失败

- Replay 时资源已 Destroy：视为设计违规（retire 失败）→ 断言。
- 设备丢失：后置；先停派发。
- Shutdown：排空 Finished 队列 → idle → join → 再毁 device/window。

## 兄弟边界

- 何时 Begin/Finish → [Render](./CORE-F45_RENDER_THREAD_DESIGN.md)
- 窗口 / 仿真 → [Game](./CORE-F45_GAME_THREAD_DESIGN.md)
- Mode / 总切片 → [综述](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | 拆出；List 录制化澄清 |
| 2026-10-24 | 加厚：状态机、MVP Op、Immediate/Recording、亲和、池 |
| 2026-10-24 | **S03：** Immediate/Recording 状态机 + suite；Forward/Manual Begin/Finish/Replay |
| 2026-10-24 | Mode A：锁 Present@RHIRole、池深度=3、D15 Immediate/Recording；待 ACK |
