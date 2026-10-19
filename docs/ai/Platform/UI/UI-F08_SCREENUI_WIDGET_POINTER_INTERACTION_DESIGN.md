# UI-F08 ScreenUI Widget Pointer Interaction — Design Spec

## Meta
- **ID:** `UI-F08`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Review`
- **Owner:** project maintainer
- **Last updated:** 2026-10-18
- **Related:**
  - [UI-F02](./UI-F02_SCREENUI_HITTEST_DESIGN.md)（Hit / 单键 PointerState）
  - [UI-F03](./UI-F03_SCREENUI_BUTTON_DESIGN.md)（Button = Click 糖）
  - [UI-F05](./UI-F05_SCREENUI_DRAG_DROP_DESIGN.md)（Drag session；本期扩展发起键）
  - [CORE-F04](../Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 外部驱动：minCraft `ENGINE_FEEDBACK_PROMPT_SCREENUI_BUTTON_POINTER.md`（库存槽左/右/Shift/左右拖；**机制进引擎，玩法留游戏**）
  - [CORE-F40](../Core/CORE-F40_INPUT_KEY_CATALOG_DESIGN.md)（合成 `Key_Shift` 等；**软依赖**——本 Feature 也可用 Left\|\|Right 采样）
- **Depends on:** `UI-F02` Done；`UI-F03` Done；`UI-F05` Done；`InputSystem` 已有 `Mouse_Left/Right/Middle` + L/R Shift/Ctrl/Alt
- **Blocks:** minCraft GP-F02 右键半取 / Shift Quick Move / 右键拖放 1（游戏仓）；任意 ScreenUI 槽/列表项完整指针语义
- **Does not block / parallel with:** `ASSET-F10` Primary；`CORE-F38`；`UI-F07` FillAmount；`CORE-F40`（建议并行或略先）

## TL;DR

今天 ScreenUI 指针路由只认左键，且几乎只有 `ButtonComponent::OnClicked`（无参）能接到「点了」。正确分层是：**可命中 `WidgetComponent` 拥有统一指针事件**；`Button` 只是「默认左键 → Click」的糖；`DragSource`/`DropTarget` 继续是拖拽政策组件。本 Feature 把多键、修饰键、Down/Up/Click/Enter/Leave，以及 Drag 发起键做成引擎机制；不实现库存算法。

## Pre-flight

| 项 | 结论 |
|----|------|
| **Complexity** | **L2**（扩展现有 UISystem 状态机 + Widget/Button/Drag API；不做 Slate 级 Reply/冒泡） |
| **Prerequisites** | F02/F03/F05 **sound**；`InputKeys` 已有左右中键与修饰键 |
| **Tech-debt risk** | **medium** 若继续把语义堆进 Button；本方案沉到 Widget → **low** |
| **WIP** | Primary=`ASSET-F10`；本 Feature 作 **UI 并行支线**，不抢主链 |
| **Philosophy** | Capabilities not opinions；Mechanism over Policy；Button/库存规则不进 Core |
| **Recommendation** | **Go with scope cut**：Must = 多键状态机 + 修饰键 + Widget 派发 + Button 兼容糖 + Drag 发起键；Should = DoubleClick / Wheel / Middle 入状态机；Defer = FReply 冒泡、多指、焦点导航、双拖拽会话 |

---

## Problem

1. `UISystem::UpdatePointerFromInput` 只把 `Mouse_Left` 的 Pressed/Down/Released 喂给 `UpdatePointerFrame`；右键不参与 Press/Click/Drag。
2. `ScreenUIPointerState` 无按键身份、无修饰键；`ScreenUIDragSession` 无发起键。
3. `ButtonComponent::OnClicked` 无参；游戏无法区分左/右/Shift+左。
4. **架构错位：** 只有 Button 才「吃」点击，但库存槽、列表项等并不该被迫挂 Button。对标 UE `SWidget` / Godot `Control`，指针事件属于**可命中控件基面**，Button 是政策糖。

## Requirements

### Must

1. **多键指针状态机：** 至少同时跟踪 **Left + Right** 的独立 Pressed / Released / Click 边沿（各键各自的 `Pressed` Widget）。
2. **修饰键载荷：** 每次指针边沿派发附带 `UIKeyModifiers`：**合并**（`bShift` = Left\|\|Right）与 **左右**（`bLeftShift`…）同时填充；优先 `KeyDown(Key_Shift)`（若 CORE-F40 已激活），否则显式 `Left\|\|Right`。
3. **Widget 统一出口：** 可命中 Widget 可订阅至少：
   - Pointer Enter / Leave
   - Pointer Down / Up
   - Pointer Click（同 Widget 上 Down→Up、且未被 Drag 抑制）
4. **Button 糖兼容：**
   - 保留无参 `OnClicked`：每次合格的 **Left 单击边沿**都广播（含双击的第二次；**不**看 `ClickCount`）。
   - 新增带载荷 `OnPointerClick`（键 + 修饰键 + ClickCount）；右键等只走载荷，不触发无参。
5. **Drag：** `ScreenUIDragSession` 记录 `InitiatingButton`（Left vs Right）；左拖 / 右拖可区分；与 Click 互斥规则继承 F05（进入 Dragging 后抑制**该发起键**同一次按压的 Click）。
6. **同帧两键：** 行为钉死（见 State）：允许两键各自 Click；**同一时刻仅一个 Drag session**；Drag 主键 = 先进入 Pending/Dragging 的那颗键。
7. **验证：** 自动化或可重复手验覆盖左键、Shift+左、右键、左拖 vs 右拖、旧 `OnClicked` 回归。

### Should（可同 Feature 后切片，不挡 Must 收口）

8. Middle 键进入状态机与载荷枚举。
9. DoubleClick（`ClickCount == 2`）或独立 `OnDoubleClicked` 糖。
10. Mouse Wheel 边沿派发到 Hovered Widget。
11. `Button` / `DragSource` 可选 **AcceptedButtons** mask（**建议默认：** Button 的 `OnPointerClick` = Left\|Right，无参 `OnClicked` 仍仅 Left；DragSource = Left\|Right）。

### Non-Goals

- Minecraft / 任意游戏的半栈、Quick Move、均分、槽交换算法。
- Slate `FReply` 冒泡/隧道、完整 Capture 树、多指针/触摸。
- 键盘 Focus / Tab 导航 / 手柄 UI。
- Lua 指针回调桥（仍等专用脚本桥 Feature）。
- 透明像素精确命中。
- 新建 `UIEventSystem` / 第二套指针路由旁路 `UISystem`。
- 同时两路独立 Drag session（后置；库存不需要）。

## Constraints

- **一个伞系统：** 状态与路由仍在 `UISystem`；禁止平行轮询鼠标冒充命中。
- **Hit 仍只认 `WidgetComponent`：** Image/Text/Button 不自建第二套几何命中（与 F02/F03 一致）。
- **前向兼容偏好：** 无参 `OnClicked` 不得无声消失；迁移期双出口允许，changelog 写明。
- Play / PIE + 指针路由闸门继承 F02；Editor 非 Play 不路由。
- 不把 minCraft 类型 / SlotKind 塞进引擎。

## Preferences

- 对标：**机制**对齐 Slate `FPointerEvent` + Godot `InputEventMouseButton`（键 + 修饰键在事件上）；**Button 默认左键**对齐 `SButton` / Godot `button_mask=LEFT`。
- 组合风格保持：Widget（几何+交互面）+ 可选 Button / DragSource / DropTarget 政策组件。
- 载荷用小 `struct`（可拷贝），走 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam`。
- 修饰键只读 `InputSystem`，不另造 UI 专用键盘状态。

## Assumptions

- 游戏侧愿意从「只绑 OnClicked」迁到 Widget 指针委托或 Button 带载荷委托；引擎提供迁移路径即可。
- `InputSystem` 在 UI Tick 同帧可读到正确的 Mouse/Key 边沿（与今日左键路径相同）。
- 双击时间窗可用引擎常量（如 0.3s）；精确产品调参可后置。

## Current Architecture

| 部件 | 今天 | 缺口 |
|------|------|------|
| `UISystem::UpdatePointerFromInput` | 只喂 `Mouse_Left` | 右/中键、修饰键 |
| `ScreenUIPointerState` | Hovered / Pressed / Clicked / 边沿旗标 | 按键身份、每键 Pressed、Modifiers |
| `ScreenUIDragSession` | Phase / Source / 阈值 / SuppressClick | `InitiatingButton` |
| `WidgetComponent` | 布局 + HitTestVisible + 绘制代理 | **无指针委托** |
| `ButtonComponent` | 无参 `OnClicked` + tint | 无载荷；独家「吃」Click |
| `DragSource` / `DropTarget` | F05 会话 | 无发起键；默认隐式左键 |

控制流（今日）：

```text
Input(Left only) → UpdatePointerFrame
  → Hovered = Hit.Widget
  → Pressed → BeginPendingDrag(if DragSource)
  → threshold → Dragging (suppress click)
  → Release → Click edge → TryDispatchButtonClick → OnClicked()
```

## Proposed Architecture

### 分层（钉死）

```text
UISystem                         // 路由 + 多键状态机 + Drag 单会话
  │
  ├─ WidgetComponent             // 统一交互面：Enter/Leave/Down/Up/Click（带载荷）
  ├─ ButtonComponent             // 糖：过滤键 → OnClicked / OnPointerClick
  ├─ DragSourceComponent         // 糖/政策：可发起拖；读 Session.InitiatingButton
  └─ DropTargetComponent         // 糖/政策：可接收；不变

游戏仓                           // 半栈 / Quick Move / 均分
```

**心智模型：** 凡 `HitTestVisible` 的 Widget 都能收到指针语义；没有 Button 的槽 GO 仍然完整可交互。Button 只减少样板代码（tint + 默认左键 Click）。

### 载荷类型（建议名，实现可微调）

```text
enum class EUIPointerButton : uint8_t { None, Left, Right, Middle };

struct UIKeyModifiers
{
  // Combined (= Left || Right). Default for UI gestures (Quick Move, etc.).
  bool bShift = false;
  bool bCtrl  = false;
  bool bAlt   = false;
  // Sides — same sample frame; for bindings that care which physical key.
  bool bLeftShift = false;
  bool bRightShift = false;
  bool bLeftCtrl = false;
  bool bRightCtrl = false;
  bool bLeftAlt = false;
  bool bRightAlt = false;
};
// Invariant: bShift == (bLeftShift || bRightShift); same for Ctrl/Alt.
// Sample: prefer KeyDown(Key_Shift/Control/Alt); sides from Left*/Right* (CORE-F40 Done).

struct UIPointerEvent
{
  EUIPointerButton Button = EUIPointerButton::None;
  UIKeyModifiers Modifiers{};
  Vector2 ReferencePx{};           // Canvas 参考空间；无命中坐标时可为零
  int32_t ClickCount = 1;          // Down/Up 可为 0；Click/双击用 1/2
  WidgetComponent* Widget = nullptr;
};
```

放置：`ScreenUIHitTypes.h` 旁新头（如 `ScreenUIPointerTypes.h`），避免拖垮 Hit 头文件。

### Widget 委托（机制出口）

在 `WidgetComponent` 上增加 Native Multicast（命名可微调）：

| 委托 | 何时 |
|------|------|
| `OnPointerEnter` | Hovered 从非己 → 己 |
| `OnPointerLeave` | Hovered 从己 → 非己 |
| `OnPointerDown` | 某跟踪键在该 Widget 上 Pressed 边沿 |
| `OnPointerUp` | 某跟踪键在该 Widget 的 Pressed 上 Released（宽松：Up 发给 Pressed Widget，即使 Hover 已离开——与今日 Click 捕获一致） |
| `OnPointerClick` | 该键 Released 且未 Suppress，且 Pressed Widget 仍有效 |

Enter/Leave 的 `UIPointerEvent.Button` 可为 `None`；Modifiers 仍采样。

**不**在 Widget 上强制实现 Drag 委托：继续由 `DragSource`/`DropTarget` + `UISystem` session 承担（F05）；本 Feature 只给 session 补发起键，并保证 Widget Down 在 Pending 之前已派发。

### Button 糖

```text
OnPointerClick(UIPointerEvent)   // 新；尊重 Interactable；默认 AcceptedButtons=Left|Right（或可配置）
OnClicked()                       // 保留；仅 Left && ClickCount==1 时广播
```

派发顺序（同一次 Click）：先 Widget `OnPointerClick`，再 Button 载荷委托，再无参 `OnClicked`（若条件满足）。避免游戏只绑 Widget 时丢事件；Button 存在时两边都可听。

Tint Hover/Pressed：继续跟 **左键**（或「任一 AcceptedButton Down」——实现选更简单者并写进 Impl；默认建议：**任一 AcceptedButton 处于 Pressed 且 PressedWidget==己** 时用 Pressed 色）。

### 多键状态机

```text
ScreenUIPointerState
  Hovered
  Modifiers                          // 每帧或每边沿刷新
  PerButton[Left|Right(|Middle)]:
    Pressed Widget*
    bDown
  Clicked + ClickButton + bClickThisFrame   // 本帧最后一个 Click 边沿（调试/查询）
  bButtonOnClickedThisFrame                 // 兼容：无参 OnClicked 是否广播过
```

`UpdatePointerFromInput`：对每个跟踪键读 Pressed/Down/Released，**按键顺序稳定**（建议 Left → Right → Middle）调用帧更新；共享 Hovered（命中一次）。

### Drag 与两键规则（Must 钉死）

| 规则 | 行为 |
|------|------|
| 单 session | 全局仍一个 `ScreenUIDragSession` |
| 发起 | 仅当该键 Down 命中带 `DragSource` 的 Widget，且当前 Phase==None，且该键被 Source 接受（默认 Left\|Right） |
| `InitiatingButton` | 进入 Pending 时写入；直到 Clear |
| 另一键在 Dragging 中 | 可产生自己的 Down/Up/Click；**不**开第二拖拽；**不**取消现有拖（除非另开 Cancel API） |
| Click 抑制 | 仅抑制 **InitiatingButton** 那一次按压的 Click；另一键 Click 不受影响 |
| 松开发起键 | 结束 Drag（Drop/Cancel），同 F05 |

Godot 内置拖只认左键；我们**有意更宽**（右键拖），以便库存「每格放 1」，但仍保持单 session，避免双拖复杂度。

### DoubleClick（Should）

UISystem 记：`LastClickWidget` / `LastClickButton` / `LastClickTime`。若同 Widget、同键、间隔 ≤ 阈值 → `ClickCount=2`；否则 `1`。无参 `OnClicked` **不**在双击第二次时再广播（或广播一次——**推荐：双击第二次只走载荷 ClickCount=2，无参仍可再广播一次以便旧代码**；Impl 选「无参每次单击边沿都广播」并测例钉死）。

**已决（D04）：** 无参 `OnClicked` = 每次合格的 Left 单击边沿都广播（含双击的第二次）；完整语义看 `OnPointerClick.ClickCount`。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `UISystem` | 多键采样、Hovered、每键 Pressed、Click/Drag 互斥、派发顺序、Modifiers 采样 | 库存、半栈、Quick Move |
| `WidgetComponent` | 布局/命中面；持有并 Broadcast 指针委托 | tint 政策；拖拽阈值 |
| `ButtonComponent` | Interactable、tint、把 Click 收成糖委托 | 右键玩法；Drag |
| `DragSource` / `DropTarget` | 可拖/可放声明与拖拽委托 | 发起键语义以外的玩法 |
| 游戏仓 | 用事件载荷实现槽手势 | 绕过 UISystem 全局抢鼠标 |

## Boundaries

- **Allowed：** 游戏订阅 Widget/Button/Drag 委托；查询 `GetPointerState()` / `GetDragSession()`。
- **Forbidden：** 游戏为「按钮命中」自行 `MouseButtonDown` + 矩形测试；新建第二 UI 指针系统；Core 出现 Inventory/Slot 类型。

## Dependencies

- Depends on: UI-F02/F03/F05；InputSystem；CORE-F04 delegates。
- Depended on by: minCraft 容器 UI；未来列表/工具栏。
- New dependency direction: Widget → pointer types header（轻）；Button 继续依赖 Widget。

## Ownership & Lifetime

- 指针/拖拽状态：`UISystem` 拥有；帧初清边沿旗标（沿用今日 `bClickThisFrame` 模式）。
- 委托：各 Component 拥有；GO Destroy / Deactivate 时现有路径取消 Drag（F05）；本 Feature 确保 Leave 在 Widget 失活时尽量派发一次（能测则测；做不到则记债）。
- `UIPointerEvent` 按值传递；回调内指针仅保证**同步广播期间**有效。

## Data Flow

```text
InputSystem (mouse buttons + modifier keys)
  → UISystem::UpdatePointerFromInput
  → HitTest (shared Hovered)
  → per-button edges → update PerButton state / Drag session
  → sample Modifiers into UIPointerEvent
  → Widget delegates → optional Button sugar → DragSource/DropTarget (existing)
```

## Control Flow

同步、同线程、在 UISystem Tick / 指针更新路径内：

1. 刷新 Hovered；若变化 → Leave(旧) / Enter(新)
2. 对每个跟踪键处理 Pressed → Down 委托 → 或 BeginPendingDrag
3. Down 持续 → Promote / UpdateDragging
4. Released → Up；若未 suppress → Click（Widget → Button）
5. 边沿旗标供查询；下帧或帧末清除（与现实现一致处保持）

无异步；无跨线程。

## State & Invariants

- Hovered 至多一个 Widget（同今日 Hit 规则）。
- 每个跟踪键至多一个 `Pressed` Widget。
- Drag Phase ≠ None 时 `InitiatingButton ≠ None`，且 `Source`/`SourceWidget` 非空。
- `bSuppressClick` 为真时，**仅**发起键的 Click 被抑制。
- Illegal：Phase==Dragging 且无 Source；两路 Drag 并存（本 Feature 禁止）。

## API / Interface Semantics

### `UISystem::GetPointerState()` / `GetDragSession()`

- 只读查询；Drag 增加 `EUIPointerButton InitiatingButton`。
- Modifiers 在 state 上可读（便于回调外查询）；**以事件载荷为准**（回调同步期内一致）。

### Widget 委托

- 参数：`const UIPointerEvent&`（或按值）。
- 未订阅 = no-op；不强制每个 Widget 处理。
- `HitTestVisible==false`：不得成为 Hovered/Pressed（沿 F02）。

### Button

- `OnClicked()`：无参；Left 单击糖。
- `OnPointerClick(event)`：带载荷；键过滤后广播。
- Errors：无 sibling Widget / 非 Interactable → 不广播（同今日）。

### DragSource

- 现有 Begin/Move/End/Cancel 不变；监听方读 `GetDragSession().InitiatingButton`。
- 可选后续：`AcceptedButtons`（Should）。

## Failure Model

| 模式 | 行为 |
|------|------|
| 无 InputSystem | 不更新指针（同今日） |
| Hit 为空时 Press | Pressed=null；不 BeginDrag |
| Drag 中 Source Destroy | CancelDrag（F05 已有） |
| 回调内 Destroy Hovered | 本帧后续派发跳过悬空指针；不崩溃（尽力；测例覆盖） |
| 未知键 | 枚举外忽略 |

## Alternatives & Trade-offs

### Option A（推荐）— Widget 统一委托 + Button 糖 + 单 Drag session

- **Pros：** 对齐 UE/Godot；槽无需假 Button；与现 Component 组合一致；迁移清晰。
- **Cons：** Widget 头文件/反射面变大；UISystem 状态机变宽。

### Option B — 仅扩展 Button（OnRightClicked + 查 Modifiers）

- **Pros：** 改动面小。
- **Cons：** 库存槽仍被迫挂 Button；非按钮控件无 Enter/Down；**重复确认的错误分层**。
- **Why not：** 与本 Feature 目标冲突。

### Option C — 独立 `PointerInteractableComponent`

- **Pros：** Widget 保持「纯布局」。
- **Cons：** 又一个几乎必挂组件；与「命中面=Widget」重复；Godot/UE 都把事件放在可命中基类上。
- **Why not：** 组合税高于收益。

## Integration Impact

- `UISystem.cpp` / `ScreenUIPointerState` / `ScreenUIDragSession`：核心改动。
- `WidgetComponent`：新委托 + gen。
- `ButtonComponent`：新委托；`NotifyClicked` 路径扩展。
- 现有 `test screen-ui-*` / Button / Drag 测例：回归 + 新用例。
- minCraft：可在引擎切片落地后接右键/Shift；旧左键 `OnClicked` 暂可不动。

## Migration / Compatibility

| 旧用法 | 新用法 |
|--------|--------|
| `Button.OnClicked` 无参 | **继续有效**（仅左键单击） |
| 需要右键/Shift | `Button.OnPointerClick` 或 `Widget.OnPointerClick` |
| 左键拖 | 不变；可读 `InitiatingButton==Left` |
| 右键拖 | 同 DragSource；读 `InitiatingButton==Right` |

Transitional：双委托长期可留；无参不标 Deprecated 除非下一代 UI Feature 统一清理。

## Verification Strategy

- **Core：**
  - 左 Click → Widget+Button 载荷 `Left`，mods=0；无参 OnClicked 触发。
  - Shift+左 → `bShift`；无参仍触发（糖不吃修饰键）。
  - 右 Click → 载荷 `Right`；无参 **不**触发。
  - 左拖 / 右拖 → `InitiatingButton` 不同；发起键 Click 被 suppress。
  - Enter/Leave 在 Hover 变化时触发。
  - 左键按住未抬起时右键 Click → 右键 Click 到达；不崩；不第二 Drag。
- **Regression：** 既有 Button / Drag 测例仍绿。
- **Failure：** Drag 中 Destroy Source；无 Hit 按下。
- **Suite 名建议：** `screen-ui-pointer`（或扩展现有 screen-ui / button / drag suites）。

## Open Questions

1. Button 默认 `AcceptedButtons`：仅 Left，还是 Left\|Right？ — **建议默认 Left\|Right 走 `OnPointerClick`，无参仍仅 Left**；阻塞？**no**（实现前在 Impl Plan 钉死）。
2. 双击时无参 `OnClicked` 是否第二次也广播？ — **已决：是**（不看 ClickCount）。
3. Wheel / Middle / DoubleClick 是否并入本 Feature 的 S05，还是另开？ — **Middle 进 Must 状态机；DoubleClick/Wheel 为本波 Should（可同 PR 轻量做 ClickCount）**。
4. 修饰键左右字段 — **已决：** 合并 + 左右并存；采样用 CORE-F40 合成键 + 侧键。

## Design Review

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** agent design-review · 2026-10-18；维护者批准开工 2026-10-18
- **Link or summary:** D04 / 修饰键 / AcceptedButtons 默认已决；CORE-F40 Done。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected（UISystem / Button / Drag / InputKeys）
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes`

## Acceptance Checklist

- [ ] Design review complete when required by complexity/profile
- [ ] Implementation plan traces to design decisions (L2+)
- [ ] Progress log updated when status changes
- [ ] Feature registry status synced

## Suggested slices（Impl Plan 起草时用）

| Slice | Goal | Verify |
|-------|------|--------|
| S01 | 多键状态机 + Modifiers + 类型 | 单测读 state；左/右边沿 |
| S02 | Widget Enter/Leave/Down/Up/Click 派发 | 无 Button GO 能收事件 |
| S03 | Button 载荷委托 + 无参兼容 | 旧测绿；右键不触发无参 |
| S04 | Drag `InitiatingButton` + 两键规则 | 左/右拖可分；Click suppress |
| S05 | Should：Middle / DoubleClick / Wheel / AcceptedButtons | 按需 |

## 变更记录

| Date | Note |
|------|------|
| 2026-10-18 | Draft：Widget 机制面 + Button 糖；多键/修饰键/Drag 发起键；对标 UE/Godot；minCraft 反馈驱动 |
| 2026-10-18 | `UIKeyModifiers`：合并 + 左右并存；注明勿依赖未接线的 `Key_Shift` |
| 2026-10-18 | D04 已决；Status → In Progress；依赖 CORE-F40 Done |
| 2026-10-18 | 载荷类型去 F 前缀：`UIPointerEvent` / `UIKeyModifiers`（非 UE 风格） |
