# CORE-F36 — Gameplay FixedTick + 变步 Tick 相位对齐

## Meta
- **ID:** `CORE-F36`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `In Progress`
- **Owner:** project maintainer
- **Last updated:** `2026-10-15`（S00–S04 已落码于 `feat/core`）
- **Related:**
  - 前置：[CORE-F34](./CORE-F34_GAME_INSTANCE_AND_SUBSYSTEM_LIFECYCLE_DESIGN.md)、[CORE-F35](./CORE-F35_GAME_INSTANCE_HUB_AND_PROJECT_CLASS_DESIGN.md)（GI Hub / 派生类）
  - 正交：[CORE-F33](./CORE-F33_DEFERRED_STRUCTURAL_EDIT_DESIGN.md)（Fixed/变步内结构变更沿用 F33）
  - 触媒：minCraft `ENG-GAP-FIXED-TICK` / TD-017
  - 哲学：[ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F34`、`CORE-F35`（Done）
- **Blocks:** 外仓稳定逻辑帧可迁到引擎时钟；不阻塞 F33 / ASSET-F04 / RND-F24

## TL;DR

本 Feature 两件事：**（A）引擎唯一 Gameplay Fixed 时钟**（追赶、可调 Interval/Scale；默认关）；**（B）修正变步 Tick 时序**：把错位的 GI/World Tickable **拉回 Physics 前**，与 GO→Comp 组成同一玩法块，并与 Fixed **共用层序**（GI → WorldTickable → GO→Comp）。同帧：`Input → Fixed 0..N → Variable → Physics → …`。GI Fixed 顺序由派生 GI 编排。不内置 Minecraft 20 / 随机刻倍率。Status：In Progress（S00–S04 在 feat 分支；S05 / 维护者验收待办）。

## Reader quick start

1. Requirements（含 **R13**：Tickable 前移，非 GO 后移）。  
2. Proposed §目标 `LogicalTick`。  
3. GI vs 框架边界、汇聚优化。  
4. slices（S00 变步对齐 → Fixed 各层）。

## Problem

1. **仅有变步 Tick：** 外仓自建 accumulator 易与顺序、暂停、PIE 不一致；多系统各自累加会相对漂移。  
2. **Physics Fixed ≠ Gameplay Fixed：** Jolt 已有 `1/60`；玩法常需另一频率，不应绑死物理。  
3. **派发面过窄会逼外仓再造时钟：** 仅 WorldSubsystem Fixed 不够；GI 编排、GO/Component 局部逻辑也需要同一逻辑帧。  
4. **今日变步时序安排不当（本 Feature 必修）：** GO/Component 在 Physics **前**（合理），但 F34 把 GI/World Tickable 插在 Physics **后**，把玩法块切开。正确修法是 **把 Tickable 拉回 Physics 前与 GO 汇合**，而不是把 GO 挪到 Physics 后。

## Requirements

- **R1** **唯一时钟源：** Engine（或 Engine 拥有的 `GameplayFixedTickClock`）维护 Interval / Scale / Accum / TickIndex / MaxSteps；默认关闭。  
- **R2** 启用后每帧追赶 `FixedTick` **0..N** 次；`N ≤ MaxSteps`；超额本帧停步但 **保留 accum**。  
- **R3** 同一步内所有引擎派发层共享 **同一 `fixedDt`（= Interval）与同一 TickIndex**。  
- **R4** 运行时可改 Interval（>0 启用；≤0 关闭）与 FixedTimeScale（≥0）；契约见 §State。  
- **R5** **GI 层：** 引擎每步调用 `GameInstance` 虚钩子（如 `OnFixedTick(fixedDt)`）；**不**对 GI Subsystem 做引擎侧 ForEach。子系统调用顺序由 **游戏派生 GI** 定义（Mechanism：钩子；Policy：游戏编排）。Standalone + 每个 PIE GI 各调一次钩子。  
- **R6** **WorldTickable 层：** 每个活跃 Scene 上，对 `WantsFixedTick()` 的 `TickableWorldSubsystem` 按 **`m_Instances` 创建序**（与今日变步 `ForEachSubsystem` **相同**）调用 `FixedTick`。  
- **R7** **GO/Component 层：** `Component::FixedTick` 为逻辑点；`GameObject::FixedTick` 为引擎入口（转发至需要 Fixed 的 Comp）。**Comp 开关向上汇聚**到 GO；Scene 只派发 `WantsFixedTick` 的 GO，禁止每步全量试探。  
- **R8** 变步 `Tick` 保留；参与者可 Fixed-only / 变步-only / 两者皆要。  
- **R9** 与 Jolt Fixed **解耦**。  
- **R10** Pause：不累加、不派发 Fixed；须有可测 pause 门控。  
- **R11** 金样：Interval=0.05、scale=1、约 1s 墙钟 → 全局 TickIndex 增量 ≈ 20（允许小误差 / MaxSteps 说明）。  
- **R12** 命名不出现 `Minecraft`；不把「20」写死为唯一合法间隔。  
- **R13** **变步 Tick 相位对齐（必修）：**  
  - **保留** GO/Component 在 Physics **前** 的变步 Tick（不后移）。  
  - 将 GI Tickable + WorldTickable 从 Physics **后** **前移**，与 GO→Comp 组成同一玩法块。  
  - 变步层序与 Fixed **同构**：`GI →（每 Scene）WorldTickable（m_Instances）→ GO→Comp`，且整块在 **Physics 之前**。  
  - GI 变步：引擎 `ForEach`（与今日一致）；**不**要求变步改成派生 GI 编排（仅 Fixed 强制 Hub 编排）。可选后置：`OnTick` 与 Fixed 对称。  
  - 同帧内：**Fixed 0..N → Variable 一层 → Physics**（玩法决策/逻辑在模拟前；需本帧物理结果的逻辑后置为另相位，见 Non-Goals）。

## Constraints

- 主线程；与 `LogicalTick` 同线程。  
- 不引入 UE TickGroup 矩阵 / 每对象独立 Fixed 时钟。  
- Forward-only；**Tickable 相对 Physics 的前移**是有意行为变更，须测例与 Progress 记录。  
- WorldTickable Tick **顺序不新增 Priority API**（跟现网数组序）；若将来要稳定排序，另 Feature。

## Preferences

- 对标 Godot/Unity **追赶 + cap**；不对标 UE `TickInterval`。  
- 汇聚优化：**Fixed 必须** Comp→GO→Scene Participants；变步 GO 可先保持现网全量遍历，汇聚可选同 Feature。  
- 随机刻倍率等策略留在游戏 `FixedTick` 内。

## Assumptions

- 项目可通过 CORE-F35 使用派生 `GameInstance` 编排 GI Fixed。  
- 多 Scene（Editor + PIE）共享同一引擎时钟与 TickIndex。  
- 依赖「Physics **后**再跑 World/GI Tickable」的代码（若有）须迁移；依赖「Physics 前 GO Tick」的路径 **尽量保持有效**。

### Pause 现状（代码真源）

- `PlayState::Paused` 已存在，但 Tick 路径 **未接线**。  
- Clock / Fixed 须有可测 pause（接线 PlayState **或** Clock 专用 `SetPaused`）。  
- 变步是否随 Pause 停：本 Feature **建议**与 Fixed 同门控。

## Non-Goals

- 替代 Jolt substep。  
- 网络 rollback / 锁步全套。  
- 引擎内置 `randomTickSpeed` / 流体规则。  
- 锁死全局渲染 FPS。  
- 引擎规定 GI Subsystem 的 Fixed **调用顺序**（那是游戏 Hub 职责）。  
- 变步 GI 强制改为派生类编排（仅 Fixed 强制；变步 ForEach 可留）。  
- **PostPhysics Gameplay Tick/Fixed 相位**（需要时另 Feature；本 Feature 主相位在 Physics 前）。

## Current Architecture

```text
Engine::LogicalTick(frameDt)
  Input → SceneManager::Tick(GO/Comp)     // Physics 前
  → Audio → Physics(1/60 accum) → UI
  → TickTickableSubsystems(frameDt)       // Physics 后
       GI::TickSubsystems → ForEach m_Instances（变步）
       Scene::TickWorldSubsystems → ForEach m_Instances（变步）
  → EOF → FlushDeferredStructuralEdits
```

- World / GI Tickable：仅 `Tick` + `IsTickable`。  
- `ForEachSubsystem` = **`m_Instances` 数组序** = 创建序 ≈ `GetDerivedClasses` 遍历 `unordered_map` 的遭遇序，可被 `InitializeDependency` 插队影响（依赖常被 append 在依赖者之后）。**非**名字排序、非依赖拓扑序。  
- 无 Gameplay Fixed 时钟。

## Proposed Architecture

### 1) 目标 `LogicalTick`（锁定）

```text
Engine::LogicalTick(frameDt)
  Input

  → // ===== Gameplay 玩法块（Physics 前）=====
  → GameplayFixedPhase(frameDt)     // 0..N；时钟关闭则跳过
  → GameplayVariablePhase(frameDt)
       // Layer A：GI — ForEach TickableGameInstanceSubsystem（变步）
       // Layer B/C：每活跃 Scene
       //   WorldTickable::Tick (m_Instances)   // 从 Physics 后前移
       //   GO → Component::Tick               // 留在 Physics 前（可仍由 SceneManager 派发，或收入本 Phase）

  → Audio（Impl 不无故挪；默认可紧邻 Physics 前/后）
  → PhysicsSystem::SimulateActiveScene(frameDt)   // Jolt 1/60 accum
  → UISystem::Tick
  → EOF → FlushDeferredStructuralEdits
```

**修正方向：** 删除 Physics **后**的 `TickTickableSubsystems` 玩法派发；**不是**删除 Physics 前的 GO Tick。

### 2) 唯一时钟

```text
Engine
  └─ GameplayFixedTickClock
        IntervalSeconds, FixedTimeScale, MaxSteps,
        Accumulator, TickIndex, bPaused / pause query
```

- 配置 API 挂在 Engine（或薄 facade）；**不**每 Scene 一只 accum。  
- 关闭：`Interval <= 0` → 跳过 Fixed 相位，变步玩法块仍跑。

### 3) 一步 Fixed 内的派发顺序（锁定；与变步层序同构）

```text
while (clock 本帧还可步进):
  TickIndex++
  fixedDt = Interval

  // Layer A — 会话（游戏策略）
  for each active GameInstance:
    gameInstance->OnFixedTick(fixedDt)   // 基类空；派生类编排 GI Subsystem::FixedTick

  // Layer B/C — 引擎框架（每活跃 Scene）
  for each active Scene:
    for each TickableWorldSubsystem in m_Instances:
      if WantsFixedTick(): FixedTick(fixedDt)
    for each GameObject in FixedTickParticipants:
      FixedTick → Components with WantsFixedTick

  clock.Accum -= Interval
```

**同 Scene：B 在 C 前；A 在所有 Scene 前。** 变步 A=ForEach，Fixed A=`OnFixedTick`。

### 4) Fixed vs 变步（同帧）

| 顺序 | 内容 |
|------|------|
| 1 | Input |
| 2 | Fixed 0..N（层 A→B→C） |
| 3 | Variable 一层（层 A→B→C） |
| 4 | Physics |
| 5 | UI / EOF |

主相位不拆 PostPhysics；需要物理后逻辑 → 另 Feature。

### 5) 向上汇聚（WantsFixedTick）

```text
Component.SetFixedTickEnabled(true/false) 或 WantsFixedTick()
  → GO 维护「需要 Fixed 的 Comp 计数 / 标志」
  → GO.WantsFixedTick() == (count > 0)
  → Scene 维护 FixedTickParticipants 集合（注册/注销，非每步全扫）

TickableWorldSubsystem.WantsFixedTick()
  → Scene 派发时过滤（N 小；可选旁路列表）

TickableGameInstanceSubsystem
  → 引擎不汇聚；是否调用完全由派生 GI::OnFixedTick 决定
```

### 6) Physics Fixed 关系

```text
frameDt
  ├─ Physics accum @ 1/60
  └─ Gameplay Fixed accum @ Engine.Interval   // 独立
```

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `Engine::LogicalTick` | 按目标序：Fixed → Variable → Physics → UI/EOF | 业务规则 |
| `GameplayFixedTickClock`（Engine 拥有） | 追赶、Interval/Scale/MaxSteps/TickIndex、Pause 门控 | 业务；GI 子系统 Fixed 顺序；物理步长 |
| `GameInstance::OnFixedTick` | 提供钩子；基类不强制 ForEach | 规定游戏必须如何排子系统 |
| 游戏派生 `GameInstance` | 编排 GI Subsystem::FixedTick 顺序与集合 | 改引擎 Scene 树派发 |
| `Scene` / `SceneManager` | Physics 前派发 WorldTickable + GO→Comp（变步）；Fixed Participants | GI Fixed 顺序 |
| `GameObject` | Comp Fixed/变步入口；Fixed 汇聚标志 | 承载世界级仿真 |
| `Component` | 可选 `FixedTick` / `Tick` / 开关 | 拥有独立时钟 |
| `TickableWorldSubsystem` | `WantsFixedTick` + `FixedTick`；变步 `Tick`（相位前移） | 私有 gameplay accumulator（时钟已启用时） |
| 旧 `TickTickableSubsystems`（Physics 后） | **删除或清空玩法派发** | 继续在 Physics 后 Tick 玩法 |

## Boundaries

- **Engine 管：** 唯一时钟；Scene/GO/Component 生命周期与 Fixed 派发树；WorldTickable 数组序派发。  
- **Game 管：** GI Subsystem Fixed 点谁、何序；随机刻倍率等策略。  
- **Forbidden：** Core 绑定某一游戏常量名；Fixed 内违反 F33 的结构变更假设。

## Dependencies

- Depends on: CORE-F34 Tickable 类型；CORE-F35 派生 GI。  
- Depended on by: 外仓玩法。  
- 与 RND-F24 无依赖。

## Ownership & Lifetime

- Clock：**Engine 拥有**；进程级（随 Engine Shutdown 销毁）。  
- PIE：不另建时钟；同一步派发所有活跃 GI + Scene。  
- ExitPlay：不强制清 TickIndex（单调计数可保留）；Pause/Disable 清 accum；Scene 销毁时从 FixedTickParticipants 移除 GO。

## Data Flow

```text
frameDt → × FixedTimeScale → Accum
→ N × (TickIndex++, 分层 FixedTick(Interval))
→ 余量留 Accum（MaxSteps 截断时）
```

## Control Flow

见 §Proposed。同步、主线程。无强制事件总线。

## State & Invariants

| 状态 | 含义 |
|------|------|
| Disabled | Interval≤0；不累加、不派发 |
| Enabled Running | 非 Pause；追赶 |
| Enabled Paused | 不累加；TickIndex 冻结 |

**不变量：**

- I1：同一步内，引擎框架层（WorldTickable / GO→Comp）按锁定顺序各参与者至多一次 `FixedTick`。  
- I2：`fixedDt` 等于该步开始时的 Interval。  
- I3：TickIndex 每成功完整步 +1。  
- I4：Scene Fixed 派发的 GO 集合 ⊆ 至少有一个 Comp `WantsFixedTick` 的 GO。  
- I5：引擎 **绝不**替 GI 决定 Subsystem Fixed 顺序。

**Interval 变更：** 下一步生效；accum 不比例缩放；回调内修改 → pending，本帧 Fixed 相位结束后提交。  
**FixedTimeScale：** 立即影响后续累加。  
**MaxSteps：** 截断步进、保留 accum、Warn。

## API / Interface Semantics

### Engine / Clock

| API | 语义 |
|-----|------|
| `SetGameplayFixedTickInterval(float)` | ≤0 关；>0 开 |
| `GetGameplayFixedTickInterval()` | |
| `SetGameplayFixedTicksPerSecond(float)` | 便利；≤0 关 |
| `SetGameplayFixedTimeScale(float)` | <0 → 0；默认 1 |
| `SetGameplayFixedMaxStepsPerFrame(int)` | 默认 **8**；≥1 |
| `GetGameplayFixedTickIndex()` | `uint64` |
| `SetGameplayFixedTickPaused(bool)` 或等价 | 与 PlayState 接线策略见 Assumptions |
| `IsGameplayFixedTickEnabled()` | |

### GameInstance

| API | 语义 |
|-----|------|
| `virtual void OnFixedTick(float fixedDt)` | 默认空；派生类编排 GI Subsystem |

### TickableGameInstanceSubsystem

| API | 语义 |
|-----|------|
| `virtual void FixedTick(float fixedDt)` | 默认空；**仅**被游戏 GI 显式调用时执行 |
| （无）引擎侧 `WantsFixedTick` 汇聚 | 可选游戏自管 |

### TickableWorldSubsystem

| API | 语义 |
|-----|------|
| `virtual bool WantsFixedTick() const` | 默认 false |
| `virtual void FixedTick(float fixedDt)` | 默认空 |
| 派发序 | 同变步：`m_Instances` |

### GameObject / Component

| API | 语义 |
|-----|------|
| `Component::SetFixedTickEnabled` / `WantsFixedTick` | 影响 GO 汇聚 |
| `Component::FixedTick` | 逻辑 |
| `GameObject::FixedTick` | 引擎入口 → 转发启用 Comp |
| `GameObject::WantsFixedTick` | 由 Comp 汇聚 |

### Errors

- NaN / 非法 MaxSteps：夹紧 + Warn。  
- Thread：主线程 only。

## Failure Model

| 模式 | 行为 |
|------|------|
| 长帧 > MaxSteps | 截断；保留 accum；Warn |
| Pause | 不累加、不派发 |
| 关闭时钟 | 不派发（含 GI 钩子） |
| Comp 在 Fixed 中开关 Fixed | 汇聚更新；同一步内是否已/再调 — Impl 规定「本步已进入的 GO 列表快照」或「立即生效」；推荐 **本步参与集合在步开始时快照** |

## Alternatives & Trade-offs

### Option A — 引擎唯一时钟 + 分层派发（推荐，本修订）

- Pros：单一 TickIndex；GI 顺序归游戏；框架树归引擎；GO 汇聚可扩展。  
- Cons：实现面大于「仅 WorldSubsystem」；须处理 Participants 集合。

### Option B — 每 Scene 独立时钟（初稿）

- Why not：与「一个逻辑帧身份」及维护者意图不符；多 PIE 时钟分裂。

### Option C — 引擎 ForEach 所有 GI Subsystem Fixed

- Why not：侵占 CORE-F35 Hub 编排职责；顺序变成隐式数组序。

### Option D — 仅文档外仓 accumulator

- Why not：已否决。

## Integration Impact

- `Engine::LogicalTick`：**重排** — `Input → Fixed → Variable(GI+World+GO) → Physics → UI → EOF`；删除 Physics 后玩法 `TickTickableSubsystems`。  
- `SceneManager::Tick`：可继续派发 GO→Comp，或并入 `GameplayVariablePhase`（Impl 选一，行为序不变）。  
- `GameInstance`：`OnFixedTick`。  
- `Subsystem.h`：Fixed API + World `WantsFixedTick`。  
- Fixed Participants 汇聚。  
- 测试：相位序探针 + `gameplay-fixed-tick`；回归 tickable / begin-play / deferred-structural / physics。  
- 文档：注明「Tickable 从 Physics 后前移」；GO 物理前语义尽量保留。

## Migration / Compatibility

- **Fixed 默认关闭**时仍有变步变更：GI/World Tickable **不再**在 Physics 后运行。  
- 依赖「物理后再 Tick 世界子系统」的代码须改；GO 物理前 Tick **不作为破坏项**。  
- Forward-only。

## Verification Strategy

- **变步相位：** 探针断言  
  `Input → GI Variable → WorldTickable → GO/Comp → Physics`  
  （Fixed 开启时插在 Variable 前）；Physics **后**无玩法 Tickable/GO Tick。  
- 时钟 / MaxSteps / Pause / Interval·Scale。  
- WorldTickable：Fixed 与变步均 `m_Instances` 序。  
- GI Fixed：派生类顺序可观测。  
- GO/Comp Fixed：Participants。  
- 回归：既有 suite。

## Suggested slices

| Slice | Goal | Verify | Status |
|-------|------|--------|--------|
| **S00** | **变步相位对齐**：GI+WorldTickable **前移**到 Physics 前；与 GO→Comp 同玩法块（层序 A→B→C）；去掉 Physics 后玩法 Tickable | `tickable-subsystem` + `gameplay-fixed-tick` | **Done** |
| S01 | Engine Clock + 追赶 + Pause + Interval/Scale | suite 时钟契约 | **Done** |
| S02 | Layer A `OnFixedTick` + GI Subsystem `FixedTick` API | 派生 GI 顺序测试 | **Done** |
| S03 | Layer B WorldTickable Fixed（`m_Instances` 序） | 与变步序一致 | **Done** |
| S04 | Layer C GO←Comp Fixed 汇聚 + Participants | 不全量遍历 | **Done**（API+汇聚；suite 覆盖时钟/GI/World） |
| S05 | Stub/Sample + 文档（随机刻 + Tickable 前移说明）+ Progress | 可读 | Pending |

## Open Questions

| Q | Blocking? | Resolution |
|---|-----------|------------|
| 变步相位怎么修？ | no | **Tickable 前移**（R13）；非 GO 后移 |
| 变步 GO 是否也做 Wants 汇聚？ | no | Fixed **必须**；变步可选 |
| Pause 信号 | no | S01：PlayState 或 Clock flag；变步建议同停 |
| 多 PIE 是否共用 TickIndex？ | no | **锁定：共用唯一时钟** |
| Comp 在 Fixed 中改开关 | no | **步开始快照** Participants |
| MaxSteps 默认 8？ | no | **是** |
| Audio 相对玩法块位置？ | no | 默认紧邻 Physics；失败再调 |
| PostPhysics 玩法相位？ | no | **后置**另 Feature |

## Design Review

- **Verdict:** `Ready with deferred items`（自审 2026-10-15；时序纠正为 Physics 前玩法块）  
- **Reviewer / date:** agent self-review / 2026-10-15  
- **Summary:** `Input→Fixed→Variable→Physics`；S00 前移 Tickable；GI Fixed 归派生类。后置：PostPhysics 相位、变步 GI `OnTick`、变步 GO 汇聚。

## Implementation Readiness

- [x] Requirements understood  
- [x] Constraints identified  
- [x] Relevant existing architecture inspected  
- [x] Responsibilities and boundaries defined  
- [x] Key dependencies understood  
- [x] Ownership/lifetime resolved  
- [x] Major state/invariants resolved  
- [x] API semantics sufficiently defined  
- [x] Important failure modes considered  
- [x] Alternatives considered  
- [x] Integration impact understood  
- [x] Verification strategy exists  
- [x] Open questions deferred/accepted  

**Ready for implementation?** `yes` — 维护者已指示在 `feat/core` 实现；合入 master 前再验收。勿与 F33 混 PR。

## Acceptance Checklist

- [x] Design review 维护者确认（文档 commit + 实现指示）  
- [ ] Implementation plan traces to design（L2；本波直接按 Design slices 落地）  
- [x] Progress log when status changes  
- [x] Feature registry status synced  
- [ ] 维护者验收 feat 分支后合入 master  

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-15 | 初稿：每 Scene 时钟；仅 WorldTickable |
| 2026-10-15 | 修订：引擎唯一时钟；GI=`OnFixedTick`；World/`m_Instances`；GO→Comp 汇聚 |
| 2026-10-15 | 修订：R13/S00 变步相位（初稿误写 GO 后移） |
| 2026-10-15 | **修订：** 纠正为 **Tickable 前移**；玩法块在 Physics 前；同帧 `Input→Fixed→Variable→Physics` |
| 2026-10-15 | S00–S04 实现于 feat；suite `gameplay-fixed-tick`；MinGW PE export 去掉 imgui_demo + exclude-modules |
