# CORE-F44 — Enhanced Input 触发器生命周期（对齐 UE 逐帧求值模型）— Design Spec

## Meta
- **ID:** `CORE-F44` · **Status:** `Draft`（待主人审批） · **Complexity:** `L2`
- **输入依据:** minCraft 反馈「InputTriggerPressed 释放后未复位」（`INPUT_PRESSED_TRIGGER_NOT_REARMED`）
- **行号基线:** 2026-10-21 工作树。

## Problem（当前 master 实证）

| # | 事实 | 位置 |
|---|------|------|
| P1 | `Pressed` 需要 `lastValue` 由激活→非激活的跳变；`lastValue` 只在 `EvaluateTriggerState` 内写 | `InputTriggers.cpp:13`、`InputAction.cpp:33` |
| P2 | 映射求值被 `action & (Press或Down)` 门控，未激活帧不求值 | `InputSystem.cpp:243` |
| P3 | 收尾循环只在 `LastState ∈ {Ongoing, Triggered}` 时复位并入列；按下后第二帧 `Pressed` 返回 `None` → `LastState=None` → **释放帧被跳过**，`lastValue` 永停在激活 | `InputSystem.cpp:268-287` |
| P4 | 同一原因使 `instance.Value` 永久残留：只挂 `Pressed` 的 action 释放后 `GetValue()` 仍非零 | 同上 |
| P5 | `trigger->m_LastState` 写入后**全仓无人读取** | `InputAction.cpp:34` |
| P6 | `ChordAction` 读另一个 action 的 `TriggerStateTracker`，而该 tracker 每帧 `Reset()`；chord action 本帧未求值时读到空状态 | `InputTriggers.cpp:31-41`、`InputSystem.cpp:308` |

**结论**：现状是「状态机只在被驱动时推进」。凡触发器推进依赖「本帧被求值」，就会在停止驱动的那一帧丢掉状态转移。

## 目标模型（按 UE Enhanced Input 原版）

UE 的规则是：**每帧对在栈中的每个 action 实例都推进一次状态机**，与本帧是否有输入无关；未激活的映射贡献零值，触发器照样被求值。

```
每帧：
  1 StartFrame           清空本帧 trigger 状态与事件（对应 UE FInputActionInstance::StartFrame）
  2 逐映射求值           value = 激活 ? 求值结果 : 零；映射级触发器无论是否激活都求值
  3 汇总 action 值        本帧无任何激活映射 → Value.Reset()（修 P4）
  4 action 级触发器求值   无条件，喂当前 Value（可能为零）→ lastValue 必然被刷新（修 P1/P3）
  5 事件推导             GetTriggerStateChangeEvent(prevState, newState)
  6 通知                 InputComponent::ProcessInputAction
```

## 契约

| # | 契约 | 理由 |
|---|------|------|
| I1 | 每个 action 实例每帧恰好推进一次 | 状态机可预测；与动作是否被本帧驱动无关 |
| I2 | 触发器求值是无条件副作用：`lastValue` 在每次求值后被刷新 | `Pressed` 重新武装的唯一前提 |
| I3 | `Value` 在本帧无激活映射时必须为**零值**，不是保持上一帧 | 修 P4；消费方 `GetValue()` 才可信 |
| I4 | 按住期间 `Pressed` 只 `Triggered` 一次；`Down` 每帧 `Ongoing`；`Released` 在释放帧恰好一次 | 既有语义不得回退 |
| I5 | 事件（Started/Triggered/Completed/Canceled）只由 `PreviousState → CurrentState` 推导 | 事件与状态不双源 |
| I6 | chord 消费者读 chord action **本帧已完成**的状态；求值顺序按依赖拓扑（chord action 先） | 修 P6；成环报错而非静默 |

## 状态机（每个 action 实例）

```
prev=Idle      value=0 → None        prev=Idle      value≠0 → Triggered（Pressed 或电平）
prev=Ongoing   value≠0 → Ongoing     prev=Ongoing   value=0 → None（Canceled）
prev=Triggered value≠0 → Ongoing     prev=Triggered value=0 → None（Completed）
```

`m_LastState` 二选一处理（D2）：删除，或改为状态机内部前置态的唯一来源。**不得保留写了没人读的字段。**

## 切片

| 片 | 内容 | 结束仍绿 | 验收 |
|----|------|----------|------|
| **S01** | 引入 Step1 `StartFrame` 与 Step4 无条件求值；未激活映射贡献零值；`Value` 归零 | 电平动作与 `Down` 语义不变 | 连续 press→release→press：`Triggered` 两次、`Completed` 两次；按住只 1 次 |
| **S02** | 映射级触发器同样无条件求值；`Pressed`/`Released` 在映射级与 action 级行为一致 | 现有映射触发器用例不变 | 映射级 `Pressed` 同样可重复触发 |
| **S03** | chord 求值顺序（依赖拓扑）与环检测（P6） | 单 action 场景不变 | chord 在本帧求得正确状态；成环报错；`m_LastState` 处理完毕 |

## Failure Model

| # | 失效 | 观测 | 处理 |
|---|------|------|------|
| F1 | 无条件求值使某些触发器每帧产生 `None` 事件 | 事件洪泛 | 事件只在状态**变化**时产生（I5） |
| F2 | 零值求值让 `InputTriggerDown` 误判 | `Down` 每帧 `Ongoing` 变 `None` | `Down` 语义是「激活即 Ongoing」，零值时 `None` 属正确 |
| F3 | chord 拓扑成环 | 死循环 | 检测并报错，不做深度截断 |
| F4 | 设备断开时 action 卡在激活 | 永久按住 | 设备重置路径必须走同一 `Value` 归零逻辑 |

## Verification（可自动断言）

1. 同一 action：press→release→press，`Triggered` 2 次、`Completed` 2 次；按住 1 次。
2. 三次 press/release 稳定。
3. 释放后 `GetValue()` 为 0（修 P4 的专用断言）。
4. `Down`：激活首帧 `Started`、按住每帧 `Ongoing`、释放 `Canceled`；**从不产出 `Triggered`**（电平语义，既有行为未改）。
5. chord：chord action 未激活时消费者不得报 `Triggered`；本帧激活时消费者同帧可见。
6. 全部进 `kSmokeOrder` + `InSmoke=true`。

## 待审批决策

| # | 决策 | 推荐 | 理由 | 推翻条件 |
|---|------|------|------|----------|
| D1 | 求值模型 | **逐帧无条件求值**（UE 原版） | 消灭「依赖被驱动」的整类缺陷 | 性能实测证明 action 数量级过大（当前几十个，不成立） |
| D2 | `m_LastState` | **删除** | 写了无人读，是半成品痕迹 | 若实现中发现它能替代实例级 `LastState` 从而少一次复制 |
| D3 | chord 顺序 | **依赖拓扑 + 环检测** | 与本帧状态一致；不引入「上一帧语义」的隐性规则 | 若 chord 需求退化为弱依赖，可改读上一帧并写入契约 |
| D4 | 映射级与 action 级触发器 | 两级都无条件求值 | 与 UE 一致，避免只修一半 | 若映射级求值成本过高 |

## 已知限制（实现期暴露，需后续处理）

映射级触发器的求值结果目前**不参与** action 状态，也不门控该映射的贡献 —— 也就是说，把触发器挂在**映射**上不会产生任何效果，只有挂在 action 上才生效。

- 之所以不并入 action 状态：无触发器的映射在值非零时报电平 `Triggered`，并入会让边沿动作每帧重新点火。
- 之所以不静默：这是一个「配了却不生效」的陷阱。**建议后续二选一**：① 映射级触发器改为门控该映射的贡献（UE 形态）；② 检测到映射挂触发器时打一次告警，提示改用 action 级。在落地前，本限制必须留在文档里。