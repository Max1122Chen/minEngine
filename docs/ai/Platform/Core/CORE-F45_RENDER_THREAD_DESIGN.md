# CORE-F45 — Render Thread 重构 Design

## Meta
- **ID:** `CORE-F45`（子 Design · Render）
- **Parent:** [综述](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)（Mode / Decisions 权威）
- **Status:** `Review`（待维护者 ACK；随综述 Ready）
- **Last updated:** `2026-10-24`
- **Slices:** S01 apply / S02 世代 / S03 录制端 / S04 线程；配合 [RHI](./CORE-F45_RHI_THREAD_DESIGN.md) · [Game Proxy 推送](./CORE-F45_GAME_THREAD_DESIGN.md)
- **UE：** Enqueue 消费侧；`AddPrimitiveCommand` / `UpdateTransformCommand` / `FAddLightCommand` 等**按家族** RT apply

## 角色

**Render：** 消费 Game 经 `EnqueueRenderCommand` 投递的 **按家族** 命令；维护四类 Proxy 热状态与列表；Cull/Sort/Batch；录制 `RHICommandList`。不改 GO，不 Present。

**与 Mode：** Single = 同 tid `RenderRole`；Multi = 独立 tid。API 同构。

## 今日 → 目标

| 今日 | 目标 |
|------|------|
| `RenderSystem::Tick` 清屏 + `Execute` pending | `RenderRole`：画 World(N−1) → Apply(N) → Record |
| GT 在 `UpdatePrimitive/Light/Sky/Widget` 内写热字段 | 仅本角色 `Apply*_RT` |
| 文档曾偏 Primitive | **Primitive + Light + SkyBox + Widget** 均等 |
| 统一巨石 payload | **否决**；Flush 执行的是各家族小命令/lambda |

## 职责表

| 做 | 不做 |
|----|------|
| FlushApply Enqueue 队列 | 写 GO / Component |
| 维护 Proxy 热数据与列表 | `PollEvents` |
| Cull / Sort / Batch / MeshDrawPacket | Present（目标态） |
| Intent(N) × World ≤ N−1 | 假设 Intent 对应当帧逻辑世界 |
| Begin/FinishRecording（或 Immediate） | 长期持 GL context（S05 后） |

## RenderRole 工序（权威顺序）

与综述一致：**先画 N−1，再 Apply N**。

```text
Assert Render
Profile Frame.Render.View
  Take ViewIntent(N)（可多视图）
  Wait publishedWorldGen ≥ N-1

Profile Frame.Render.CullSort
  Cull/Sort/Batch 当前热世界（= World(N-1)）

Profile Frame.Render.Record
  BeginRecording? → Passes → FinishRecording → Dispatch List

Profile Frame.Render.Apply
  FlushApply(FrameIndex == N)   // 四家族命令
  publishedWorldGen = N

Profile Frame.Render.WaitRHI   // Multi 背压
```

## 世代模型（S02 口径）

### 定义

| 符号 | 含义 |
|------|------|
| `logicFrame` | Game 当前帧号（Intent / 命令上的 FrameIndex） |
| `World(G)` | 已 **FlushApply 完** 所有 `FrameIndex ≤ G` 的命令后的渲染世界 |
| 契约 | 画 `ViewIntent(N)` 时只用 `World(N-1)`（N=0 见首帧） |

### 推荐实现

- 单份热存储（四家族列表）+ `uint64 publishedWorldGen`。
- 工序固定：Intent(N) → 确认 `publishedWorldGen ≥ N-1` → Cull/Record → FlushApply(N) → `publishedWorldGen = N`。
- Game：逻辑帧 N 的 Enqueue 带 `FrameIndex=N`；Intent(N) 在 Logical 末提交 → 自然画出 N−1。

### 首帧

- N=0：无 World(−1)。允许：只 Clear/Present，或空场景 Record；不 Assert 失败。
- N=1：首次有 World(0)。

### Single vs Multi

- 世代规则 **相同**。
- Multi：Game 可超前；Render 按 Intent 序消费；队列满阻塞在 Game（或 Render WaitRHI）。

## FlushApply（S01）— 按家族，非统一 Op switch

队列里是 **`EnqueueRenderCommand` 投递的可调用体**（lambda 或小类型），不是中央 `enum Op + 万能 POD`。

### 命令载体（Decision 13 · 已锁 A）

每条命令 = **可调用体**（`TFunction<void()>` / 等价），由管道附带 `FrameIndex`。Game 捕获家族小参数，RT 调用 `Apply*_RT`。

**不采用** 中央万能 `variant` 当扩展轴（新家族会逼改公共类型）。若调试需要，允许旁路「带 tag 的包装」但不作为产品扩展模型。

### 四家族 Apply 职责（与今日 `RenderScene.cpp` 对齐）

| 家族 | Game 入口（保留） | RT apply（新/内化） | 热写内容来源 |
|------|-------------------|---------------------|--------------|
| Primitive | `UpdatePrimitive` / `RemovePrimitive` | Add：挂 `m_Primitive*`；UpdateFast：transform/castShadow/enabled…；Remove：摘列表+retire | 今日 `SyncSceneProxy` 字段迁到 RT |
| Light | `UpdateLight` / `RemoveLight` | Add：按 LightType 挂 Dir/Point/Spot 列表；UpdateFast：position/color/intensity/锥角/衰减/enabled…；Remove | 今日 `UpdateLight` else 分支赋值迁到 RT |
| SkyBox | `UpdateSkyBox` / `RemoveSkyBox` | Add/替换 `m_SkyBoxProxy`；UpdateFast：朝向/贴图句柄等；Remove | 今日 `UpdateSkyBox` 路径 |
| Widget | `UpdateWidget` / `RemoveWidget` | Add：挂 `m_WidgetSceneProxies`；UpdateFast：布局/材质等；Remove | 今日 `UpdateWidget` 路径 |

- Proxy **对象**由 Game `CreateSceneProxy`；RT **登记所有权**进 `m_*Owners`（与今日 emplace 一致），不在 RT 再 Create（除非未来 Recreate 约定改到 RT）。
- GPU 资源：`CreateRenderThreadResources` 类工作经 List/RHI（S01 Single 可暂同步创建）。
- Recreate：Game 先 Enqueue Remove，再 Create+Enqueue Add（见 Game 子文）。

### 与今日路径映射（Primitive 示例；Light/Sky/Widget 同构）

| 今日 | 新 |
|------|-----|
| null → Create + push | GT Create + Enqueue Add_RT |
| `SyncSceneProxy` / Light else 赋值 | Enqueue UpdateFast_RT（家族小快照） |
| `Remove*` 擦向量 | Enqueue Remove_RT + retire |

## Cull / Sort / Batch（CS4）

- 仅读 **本角色** 热 Proxy / 列表；禁回指 Component。
- 输出进现有 `MeshDrawPacket` / Pass 路径；**调用约定仍 `RHICommandList&`**。

## 录制端（S03）

| Mode | List 行为 |
|------|-----------|
| Single | Immediate forwarding **或** Record+同 tid Replay（综述允许）；Pass 代码不写 Mode 分支 |
| Multi | BeginRecording → Passes → FinishRecording → 入 RHI 队列 |

详见 [RHI](./CORE-F45_RHI_THREAD_DESIGN.md) 命令集与池。

Present：**不**在 Record 末尾由 Render 调；交 RHIRole（Single 顺序紧接）。

## 释放 / Retire

```text
Remove apply
  → proxy 不参与后续 Cull
  → retireQueue.push({proxy, retireAfterWorldGen or fence})
  → 当 RHI 完成对应帧（或 Single：延迟 ≥1 RenderRole）后 delete
```

- Game 禁止 delete。
- S01 MVP：`retireAfterFrame = publishedWorldGen + 1`（Single/Multi  alike）。
- S04+：与 RHI fence 对齐更严。

## OS 线程（S04 Multi）

- 循环：Wait Intent 或「有工作」→ 按上节工序 → 循环。
- 有界 Intent / Finished-List；阻塞点打 `Frame.Render.WaitRHI`。
- Shutdown：处理完或丢弃策略与综述一致（先停 Game 提交）。

## Profiler 区段（Render）

| 名 | 包围 |
|----|------|
| `Frame.Render.Apply` | FlushApply |
| `Frame.Render.View` | 取 Intent / 解析世界世代 |
| `Frame.Render.CullSort` | 可见性与排序 |
| `Frame.Render.Record` | List 录制 / Immediate Passes |
| `Frame.Render.WaitRHI` | 等 RHI 队列空间 |

## 代码落点

| 区域 | 变化 |
|------|------|
| `RenderScene` | 四家族 `Apply*_RT`；retire；GT Update* 仅入队 |
| `RenderSystem` | RenderRole 工序 |
| `ForwardRenderer` / Passes | 仍 `RHICommandList&` + intent |
| 命令管道消费端 | 执行家族命令，无巨石 POD |

## 验收

- [ ] Primitive **与** Light/Sky/Widget：无 GT 热写
- [ ] 无中央万能 payload 类型
- [ ] Intent(N) ↔ World(N−1) 可测
- [ ] Single / Multi 同 API；Profiler 对称
- [ ] Record 后无 Render Present；Remove 无 UAF

## 边界情况

- 多 Intent 同 N：同一 World(N−1)，顺序 Record 多份 List 或一份多分发（Impl 选；推荐按 Viewport 顺序多 List 或多段）。
- 无 Intent：Skip Record；仍可 FlushApply(N) 以推进世界。
- 命令乱序：按 FrameIndex 排序再 apply；同帧内保序（SPSC 自然序）。

## 兄弟边界

- 谁 Enqueue / Intent → [Game](./CORE-F45_GAME_THREAD_DESIGN.md)
- Replay / 亲和 / Present → [RHI](./CORE-F45_RHI_THREAD_DESIGN.md)
- Mode → [综述](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | 拆出；加厚世代/工序 |
| 2026-10-24 | **对齐 Game 纠偏：** 四家族 Apply*_RT；撤回巨石 payload |
| 2026-10-24 | Mode A：锁命令载体 A；工序与综述一致；待 ACK |
