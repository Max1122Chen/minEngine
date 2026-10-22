# CORE-F44 — Enhanced Input 触发器生命周期 — 实现记录

## 结论

输入系统不再「只推进被驱动的 action」。每帧对每个 action 实例走完整六步：清帧、
汇总映射值、应用 action modifier、无条件求值触发器、由 `LastState → newState` 推导事件、通知消费者。
本帧没有输入也照样推进，所以 `Pressed` 的边沿记忆必然在释放帧刷新，第二次按下可以重新触发。

## 六步管线落点（`InputSystem.cpp`）

| 步 | 函数 | 行 |
|----|------|----|
| 1 清帧（事件清零、`Value.Reset()`、`LastState` 保留为唯一前置态） | `InputSystem::StartFrame` | 71 |
| 2 逐映射求值（未激活映射贡献零值；映射触发器无条件求值） | `InputSystem::EvaluateMappingContributions` | 212 |
| 3 汇总 action 值 + action modifier | `InputSystem::EvaluateActionInstances` | 380 |
| 4 action 级触发器无条件求值（`lastValue` 每次求值后刷新，见 `InputAction.cpp`） | 同上 | 380 |
| 5 事件推导（只由状态变化产生） | `InputSystem::GetTriggerStateChangeEvent` | 857 |
| 6 通知消费者（仅事件非 None 的实例） | `InputSystem::NotifyInputComponents` | 419 |

帧驱动入口 `InputSystem::Tick`（188）依次调用上述步骤；门控关（`IsGameplayInputEnabled() == false`）时
只是「没有映射贡献」，管线照走，因此跨门控按住的 action 会在放行第一帧正常释放，`Value` 也不会残留。

## 关键决策

- **`InputTrigger::m_LastState` 删除**（写后无人读）。唯一前置态来源是 `InputActionInstance::LastState`。
- **边沿触发器按住期间钉在 `Ongoing`**：`Pressed` 的边沿一过就返回 `None`，若照抄该值会按住即「结束」，
  所以「`None` + 值非零 + 未被 blocker 拒绝」提升为 `Ongoing`。blocker 的拒绝不被软化。
- **chord 依赖**：`InputTrigger::GetChordAction()` 暴露依赖（默认为 `nullptr`，`InputTriggerChordAction` 覆盖），
  `InputSystem::BuildActionEvaluationOrder`（262）做依赖拓扑排序，`VisitActionDependencies`（277）做环检测。
  依赖态由 `InputSystem::PushChordDependencyState`（345）在求值前推入触发器，触发器不反查输入系统。
- **成环报错**：`ReportChordCycle`（366）每帧最多报一次 `ME_LOG(LogCore, Error)`，并置
  `InputSystem::HasChordCycleError()`；闭合边被丢弃，本帧继续求值（不递归、不静默截断）。

## 与设计文档的偏差（均有理由）

1. `InputTriggerChordAction::UpdateTriggerState` 改为 header 内联。该类的 vtable 不在导出表里，
   测试直接实例化会链接失败，而导出表禁改；依赖态因此改由输入系统推入（`SetChordDependencyState`）。
2. 映射级触发器无条件求值，但结果不并入 action 状态。无触发器映射的值非零时会报电平 `Triggered`，
   把它 `max` 进 action 会让边沿动作每帧重新点火。映射贡献「值」，action 级才是唯一状态来源。
3. 映射级 chord 读到的是依赖 action 上一帧末状态：映射值必须先于 action 状态产生，天然滞后一帧。
   action 级 chord 由拓扑序保证同帧可见。
4. 真实长按后释放得到 `Canceled`（按住期间状态已被钉在 `Ongoing`），单帧 press→release 得到 `Completed`；
   这与设计的状态表（`Triggered` + 值非零 → `Ongoing`）一致。
5. 验收 4 的「`Down` 仅一次 `Triggered`」按「仅一次激活事件」落地：`Down` 产出
   `Started` / 每帧 `Ongoing` / 释放 `Canceled`，从不产出 `Triggered`（既有语义未动）。

## 反向验证（临时撤销 → 新套件转红 → 已还原）

1. 求值重新按「只有本帧被驱动才求值」门控 → `Pressed cycle 0: release should emit Completed.`
2. `StartFrame` 不再 `Value.Reset()`（值跨帧残留）→ `Pressed cycle 0: value must fall to zero on release.`
3. 求值顺序反转（无拓扑）→ `Chord: consumer must fire in the same frame its chord action activates.`

## 验证结果

- `minEngineTests test input-trigger-lifecycle`：6 例全绿（含 3 次 press/release 循环、按住单次触发、
  释放后 `GetValue()` 归零、`Down` 保持 `Ongoing`、chord 门控与同帧可见、成环报错）。
- 既有 `input-trigger-down` / `input-key-catalog` / `gameplay-input-gate` 全绿。
- `scripts/verify.ps1` exit 0；smoke 套件 11 → 12（`input-trigger-lifecycle` 同时进 `kSmokeOrder`
  与 `InSmoke=true`）；DLL 导出数 1283 不变（baseline 断言 `all 1283 baseline symbols are still exported` 通过）。
