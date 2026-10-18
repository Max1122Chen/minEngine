# CORE-F40 Input Key Catalog — Design Spec

## Meta
- **ID:** `CORE-F40`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** 2026-10-18
- **Related:**
  - [UI-F08](../UI/UI-F08_SCREENUI_WIDGET_POINTER_INTERACTION_DESIGN.md)（消费修饰键 / 多鼠标键；本 Feature 可并行或略前置）
  - Enhanced Input 既有路径（IMC / IA；本 Feature **不**改 Trigger 语义）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - GLFW：`glfw3.h` key / mouse / gamepad enums；[Input guide](https://www.glfw.org/docs/latest/input_guide.html)
- **Depends on:** 现有 `InputSystem` + `GLFWWindowSystem` 键回调
- **Blocks:** 更完整的 IMC 绑定面；UI-F08 可用合成 `Key_Shift`（非硬依赖——UI 也可 `Left||Right`）
- **Does not block:** `ASSET-F10`；`CORE-F38`

## TL;DR

`InputKeys` 表与 GLFW 映射不完整：合成修饰键（`Key_Shift` 等）已登记但从未更新；缺导航/标点/小键盘/鼠标侧键；无手柄抽象键。本 Feature 补齐 **常用目录 + 接线 + 合成修饰键语义**，手柄用 **一套 canonical 布局 + 别名**，不建 Xbox/PS/NS/Hitbox 四套平行状态。

## Pre-flight

| 项 | 结论 |
|----|------|
| **Complexity** | **L2**（目录扩展 + 映射 + 合成态；手柄轮询为第二切片） |
| **Prerequisites** | InputSystem / GLFW 回调 **sound** |
| **Tech-debt risk** | **medium** 若一次塞满厂商别名与媒体键；切分后 **low** |
| **WIP** | 并行于 ASSET-F10 / UI-F08；建议 **略先于或并行 UI-F08 S01**（合成 Shift 更干净） |
| **Philosophy** | 机制（键名 + 状态）进 Core；不规定「Jump 绑哪颗键」 |
| **Recommendation** | **Go with scope cut**：S01 键盘+鼠+合成修饰；S02 gamepad canonical；媒体键/Hitbox 设备类型 **Non-Goal** |

---

## Problem

1. **死合成键：** `Key_Shift` / `Key_Control` / `Key_Alt` 在 `InputKeys` 中登记，但 `ConvertGLFWKeyToInputKey` 只映射 Left/Right 变体；`KeyDown(Key_Shift)` 恒假。
2. **目录缺口：** 无 Insert/Delete/Home/End/Page*、标点、小键盘、`Mouse_4`/`Mouse_5`（侧键）、Super/Menu 等；未映射的 GLFW 码落到 `AnyKey`，易污染状态。
3. **无手柄键：** 无法用 IMC 稳定绑定「面键下 / 左摇杆」一类抽象输入。
4. **风险误解：** 以为要为 Xbox、PS、NS、Hitbox 各建一套键名 → 重复状态、绑定爆炸。

## Requirements

### Must（S01 — Keyboard / Mouse / Synthetic modifiers）

1. **合成修饰键可轮询：**  
   - `Key_Shift` Down ⟺ `Key_LeftShift` Down ∨ `Key_RightShift` Down（Ctrl/Alt 同理）。  
   - Pressed / Released 边沿符合聚合语义（见 State）：从「两侧皆抬」→「任一侧按下」产生一次 Press；从「仍有一侧按下」→「两侧皆抬」产生一次 Release；一侧抬起但另一侧仍按下时 **不** Release 合成键。
2. **扩展并接线常用键盘键**（至少）：  
   - 导航：Insert, Delete, Home, End, PageUp, PageDown  
   - 标点：Apostrophe, Comma, Minus, Period, Slash, Semicolon, Equal, Left/Right Bracket, Backslash, GraveAccent  
   - 锁与系统：ScrollLock, NumLock, PrintScreen, Pause, Menu  
   - Super：LeftSuper, RightSuper（Win/Cmd）  
   - 小键盘：KP_0–9, KP_Decimal, KP_Divide, KP_Multiply, KP_Subtract, KP_Add, KP_Enter, KP_Equal  
   - F13–F24 可选列入表（GLFW 有）；**不**要求笔记本媒体键。
3. **鼠标侧键：** `Mouse_4` / `Mouse_5` ↔ `GLFW_MOUSE_BUTTON_4` / `_5`（常见前进/后退侧键）。可选再加 `_6`–`_8` 若零成本。
4. **未识别键：** 不得再用「静默写成 `AnyKey` 并更新其状态」作为默认；未映射码应忽略（不写状态）或记一次 Verbose/Warning（实现选一，测例钉死）。
5. **Initialize 一致：** 新键进入 `InputKeys::Initialize` → `m_KeyStateMap` 有槽；映射表与声明同步。
6. **验证：** 合成 Shift 单测（可注入 `OnKey` 或 TestAccess）；至少一条「新键出现在 GetAllKeys + 映射可到达」的静态/表驱动检查。

### Must（S02 — Gamepad catalog + poll）

7. **一套 canonical 手柄键/轴**（Xbox 布局语义为默认名），状态由 `glfwGetGamepadState`（或等价）每帧写入 `InputSystem`，使 `KeyDown` / 轴 `RawValue` 对 IMC 可用。
8. **别名：** PS 四键名（Cross/Circle/Square/Triangle）作为 **同一底层键的别名常量**（同 `Name` 或同状态槽——实现钉死一种，见 Alternatives）；**不**维护第二套 Down 位。
9. **多手柄：** MVP 默认 **Joystick 0 / 第一个 gamepad**；`playerIndex` 后置（与 InputSystem TODO 多玩家一致）。

### Should

10. `Key_Meta` / `Key_Super` 合成 = LeftSuper ∨ RightSuper（对称于 Shift）。
11. 文档表：键名 ↔ GLFW 常量一览（Design 附录或 handbook 短页）。
12. NS 面键「显示名」仅文档说明（物理对调是主机习惯）；引擎 **不** 因 NS 改 canonical Down 语义。

### Non-Goals

- 笔记本音量/亮度等 **HID Consumer / 系统媒体键**（通常不经 GLFW key callback）。
- Hitbox 作为独立设备类型（视为键盘或手柄映射即可）。
- Xbox / PS / NS **三套互不相干的状态位**。
- 重绑 UI、图标皮肤、SDL_GameControllerDB 热更新、力反馈。
- 改 Enhanced Input Trigger/Modifier 算法。
- 触摸、笔、多指向。

## Constraints

- 键名稳定、可进 IMC；避免无说明的重命名。
- 合成键是 **InputSystem 状态机责任**，不是调用方每次手写 `||`（调用方仍可查左右）。
- 不引入第二套 Input 子系统。
- 前向开发：未映射 → ignore，优于污染 `AnyKey`。

## Preferences

- 命名对齐现有风格：`Key_*` / `Mouse_*` / `Gamepad_*`。
- Canonical 手柄名偏 Xbox 英文（`Gamepad_A`…），PS 别名并列。
- 轴：`Gamepad_LeftX/Y`, `RightX/Y`, `LeftTrigger`, `RightTrigger`（`InputAxisType` 非 Button）。

## Assumptions

- 继续以 GLFW 为唯一窗口/输入后端（近期）。
- `InputKey` 仍按 `Name` 字符串相等与哈希（改 id 体系另 Feature）。
- 游戏手柄在 Windows 上多数能被 GLFW gamepad mapping 认到。

## Current Architecture

```text
GLFW key/mouse callback
  → ConvertGLFWKeyToInputKey
  → InputSystem::OnKey / OnMouseButton
  → m_KeyStateMap[key].action / RawValue
  → Tick → IMC → IA → InputComponent
```

- `InputKeys::Initialize` 填充声明列表并建 state 槽。
- 未列出的键：**无槽**；`OnKeyOrMouseButton_Internal` 找不到则 **静默跳过**。
- 映射到 `AnyKey` 的未知码：会打到 `AnyKey` 槽（若已 Initialize）——有害。

## Proposed Architecture

### S01 — Catalog + map + synthetic modifiers

```text
InputKeys.h/.cpp     + 新静态键常量 + Initialize 登记
GLFWWindowSystem     + ConvertGLFW* 分支；未知 → 不调用 OnKey（或 no-op 哨兵）
InputSystem          + 在左右修饰键更新后 RefreshSyntheticModifier(Key_Shift, L, R)
```

`RefreshSyntheticModifier` 伪契约：

```text
bool anyDown = KeyDown(left) || KeyDown(right);
prev = synthetic.action
if (anyDown && wasUp)   → synthetic Press (then Down next frames via existing CalculateKeyAction)
if (!anyDown && wasDown) → synthetic Release
// 一侧抬起另一侧仍按下：synthetic stays Down；无额外边沿
```

实现可直接写 `InputKeyState`，或走内部 `OnKeyOrMouseButton_Internal(synthetic, …)` 复用边沿算法——选后者时要防止递归再触发合成。

### S02 — Gamepad

```text
InputSystem::Tick (device path, 即使 gameplay IMC gated 也可更新设备态 — 与 MouseScroll cleanup 同层)
  → glfwJoystickIsGamepad / glfwGetGamepadState
  → 写 Gamepad_* Button / Axis 的 KeyState
```

Canonical 键（Must 最小集）：

| InputKey | GLFW |
|----------|------|
| Gamepad_A / alias Cross | BUTTON_A |
| Gamepad_B / alias Circle | BUTTON_B |
| Gamepad_X / alias Square | BUTTON_X |
| Gamepad_Y / alias Triangle | BUTTON_Y |
| Gamepad_LeftBumper / RightBumper | … |
| Gamepad_Back / Start / Guide | … |
| Gamepad_LeftThumb / RightThumb | … |
| Gamepad_DPad_* | … |
| Gamepad_LeftX/Y, RightX/Y, LeftTrigger, RightTrigger | axes |

别名推荐：**两个 `static const InputKey` 共享同一 `Name` 字符串**（例如都叫 `Gamepad_A`），或 PS 名 `Name="Gamepad_Cross"` 但在 poll 时写入与 A **同一 map 槽**（需稳定 id——今日仅有 Name）。最简单且与今日模型一致：

> **Option Alias-SameName（推荐 MVP）：** 只公开 `Gamepad_A`；文档写「Cross = A」。PS 常量作为 `constexpr` 引用 / 同对象别名：`static const InputKey& Gamepad_Cross = Gamepad_A;`

避免两个不同 Name 却要同步两份状态。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `InputKeys` | 声明目录、Initialize 列表 | 轮询硬件 |
| `GLFWWindowSystem` | 物理键码 → InputKey | 合成键、手柄轴语义 |
| `InputSystem` | 状态槽、合成修饰、gamepad poll 写入 | 玩法绑定政策、媒体键 OS 钩子 |
| IMC/IA | 继续消费 InputKey | 不为本 Feature 改 Trigger |

## Boundaries

- **Allowed：** 扩展键表；合成态；gamepad → KeyState。
- **Forbidden：** 每厂商一套 Down 位；UI 旁路再维护修饰键表；把库存快捷键语义写进 InputKeys。

## Dependencies

- Depends on: GLFW, InputSystem.
- Depended on by: UI-F08（软）、玩法 IMC、未来重绑工具。
- UI-F08：**软依赖** S01 合成键；硬依赖仅 Left/Right 已存在。

## Ownership & Lifetime

- `m_KeyStateMap` 仍由 `InputSystem` 在 Initialize/Shutdown 生命周期内拥有。
- 合成键无独立寿命；随左右键更新。

## Data Flow

```text
HW (keyboard/mouse) → GLFW callback → map → KeyState (+ synthetic refresh)
HW (gamepad)        → Tick poll     → map Gamepad_* KeyState
Consumers           → KeyDown / IMC
```

## Control Flow

- 键盘鼠标：事件驱动（现路径）+ 合成刷新（同步，同回调栈或紧随其后）。
- 手柄：`InputSystem::Tick` 轮询（设备层；建议 **不受** `IsGameplayInputEnabled` 门控，与「设备态始终新鲜」一致；IMC→IA 仍受门控）。

## State & Invariants

- `KeyDown(Key_Shift) == KeyDown(LeftShift) || KeyDown(RightShift)`（Ctrl/Alt/可选 Super 同理）。
- 每个已 Initialize 的 InputKey 在 map 中恰有一槽。
- 别名不得导致双槽双状态（S02）。
- Illegal：未知 GLFW 码写入 `AnyKey` 作为成功路径。

## API / Interface Semantics

- 现有 `KeyPressed` / `KeyDown` / `KeyReleased` **不变**；新键即用。
- 合成键：可绑 IMC，行为如普通按钮键。
- Gamepad 轴：`RawValue` 用现有 `InputKeyState` 向量通道（与 Mouse2D 类似）；具体分量约定写进 Impl（如 X→x, Y→y）。

## Failure Model

| 模式 | 行为 |
|------|------|
| 无 gamepad | S02 键保持 Idle；不报错刷屏 |
| 失去连接 | 下一 Tick 清为 Idle / 0 |
| 未映射键盘码 | 忽略 |
| 合成刷新时缺左/右槽 | Initialize 保证有槽；缺则 assert/Error 一次 |

## Alternatives & Trade-offs

### Option A（推荐）— 扩表 + 合成刷新 + canonical gamepad + 引用别名

- Pros：与现模型一致；UI/IMC 立刻受益；无双状态。
- Cons：键头文件变长；手柄仅第一只。

### Option B — 调用方永远 `Left\|\|Right`，不修 `Key_Shift`

- Pros：零实现。
- Cons：死键继续骗人；每个 UI/游戏重写；**拒绝**。

### Option C — 每平台厂商全套独立键名

- Pros：皮肤名好看。
- Cons：状态爆炸、绑定不可移植；**拒绝**（显示层后置）。

## Integration Impact

- `InputKeys.*`、`GLFWWindowSystem::Convert*`、`InputSystem`（合成 + Tick poll）。
- 现有测例应仍绿；新增 `input-keys` 或扩展既有 input suite。
- UI-F08：采样可改为优先 `Key_Shift`，并保留左右字段。

## Migration / Compatibility

- 仅新增键与修复死合成键；不删除现有名。
- `AnyKey` 行为收紧：可能改变「乱按未映射键曾触发 AnyKey」的边缘行为——视为 bugfix，changelog 写明。

## Verification Strategy

- **S01：**  
  - 表驱动：声明的键 ⊆ Initialize；映射覆盖声明的物理键子集。  
  - 合成：模拟 Left down → `Key_Shift` Pressed；Right 再 down → Shift 仍 Down 无第二次 Pressed；Left up Right still → Shift Down；Right up → Shift Released。  
  - Mouse_4 映射存在（编译期/表）。  
- **S02：** 无手柄时 Idle；有手柄时（手验或 mock）A 键 Down。  
- **Regression：** 现有 `input-trigger-down` / gameplay-input-gate 仍绿。

## Open Questions

1. F13–F24、Mouse_6–8 是否进 S01？ — **已决：全量进 S01**（维护者 2026-10-18）。
2. Gamepad 是否在 gameplay gate 关闭时仍 poll？ — **已决：仍 poll 设备态**。
3. `Key_Super` 合成是否 Must？ — **已决：与 Shift/Ctrl/Alt 一并 Must（全量）**。

## Design Review

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** agent design-review · 2026-10-18
- **Link or summary:** 聊天报告（与 UI-F08 同轮 Mode A）。S01 可开工级清晰；S02 别名同槽与 Tick 门控已有默认；AnyKey 收紧行为需 changelog。

## Implementation Readiness

- [x] Requirements / constraints / non-goals
- [x] Current architecture inspected（InputKeys / GLFW map / OnKeyOrMouseButton_Internal）
- [x] Synthetic modifier semantics defined
- [x] Gamepad naming model chosen（canonical + ref alias）
- [x] Verification strategy
- [ ] Maintainer approval → Planned → Impl Plan

**Ready for implementation?** `yes` — 维护者批准全量实现；review 后由维护者验收。

## Acceptance Checklist

- [ ] Design review complete
- [ ] Implementation plan traces to design
- [ ] Progress / Registry synced

## Suggested slices

| Slice | Goal | Verify |
|-------|------|--------|
| S01 | 键盘+鼠侧键目录/映射；合成 Shift/Ctrl/Alt；停写 AnyKey | 合成单测 + 回归 |
| S02 | Gamepad canonical + Tick poll + PS 引用别名 | 无垫 Idle；有垫手验/mock |
| S03 | Should：Super 合成；handbook 键表 | docs |

## 附录 — S01 映射增量（相对今日）

今日已有：A–Z, 0–9, F1–F12, 方向, Space/Enter/Esc/Tab/Backspace/Caps, L/R Shift/Ctrl/Alt, Mouse L/M/R (+ MouseX/Y/2D/Scroll).

S01 新增（摘要）：导航键、标点、锁键、Menu、L/R Super、KP_*、Mouse_4/5（可选 6–8）、可选 F13–F24；并激活合成 Key_Shift/Control/Alt。

## 变更记录

| Date | Note |
|------|------|
| 2026-10-18 | Draft：目录补全 + 合成修饰 + canonical gamepad；拒多厂商平行状态 |
| 2026-10-18 | 维护者：键表全量（含 F13–24、Mouse_4–8、Super 合成）；Status → In Progress |
