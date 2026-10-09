# UI-F05 — ScreenUI 拖拽（Drag-Drop）

## Meta
- **ID:** `UI-F05`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-09`
- **Branch:** `feat/ui`（工作树 `D:/Dev/GitRepo/minEngine-ui`）
- **Related:**
  - [UI-F02](./UI-F02_SCREENUI_HITTEST_DESIGN.md)（Hit / Pointer / Click）
  - [UI-F03](./UI-F03_SCREENUI_BUTTON_DESIGN.md)（OnClicked；与 Drag 互斥）
  - [UI-F06](./UI-F06_SCREENUI_IMAGE_SPRITE_RAW_DESIGN.md)（拖影图标 Sprite/Raw，可并行）
  - [CORE-F04](../Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 外部驱动：minCraft `TD-015` / MVP 愿景「背包拖拽 Must」
- **Depends on:** `UI-F02` Done；`UI-F03` Done
- **Blocks:** minCraft 去掉点选桥、真拖拽验收；不挡 Image UV（F06）

## TL;DR

在 UISystem 指针状态上增加薄 **Drag session**：按下 → 超过阈值进入拖拽 → 松手命中 DropTarget（或取消）。  
引擎只提供 **机制**（捕获、会话、Begin/Move/End/Drop 边沿与委托）；**不**内置 Inventory / 堆叠 / 合成语义。  
与 Button Click **互斥**：一旦进入 Drag，同一次按压 **不再** 派发 `OnClicked`。

## Problem

- ScreenUI 已有 Hover / Press / Click / Button，但 **无 Drag-Drop API**（UI-F02 Explicit Out）。
- minCraft `HudOverlay` 用「点选 + 光标栈」桥接槽位交换；愿景将拖拽标为 **Must**，点选仅工程桥（`TD-015`）。
- 若游戏自研指针状态，会与 UISystem 的 Click/Capture 双轨，容易打架。

## Requirements

- Play / PIE 且指针路由开启时：
  - 在可拖 Widget 上 **Press → 移动超过阈值** → 进入 Drag session。
  - Session 期间保持 **source capture**（松手前不因 Hover 离开而丢失 source）。
  - **Release**：对当前命中 Widget 解析 DropTarget；可接收则 `OnDrop`；否则 Cancel / End。
- C++ 可订阅：Begin / Move / End / Drop（Native Multicast；**本期无 Lua**）。
- 进入 Drag 后，同一次按压 **抑制** Button `OnClicked`。
- Query API：可查「是否正在拖 / Source Widget / 指针参考坐标」。
- 单测：阈值、Click 抑制、跨 Widget Drop、无 Target 取消、Play 闸门、拖拽中 Destroy。

## Constraints

- **机制优先于政策：** 引擎不实现槽交换、堆叠合并、合成结果拾取；这些留在游戏。
- **一个伞系统：** 状态挂在现有 `UISystem`；禁止新建 `DragSystem` / `UIEventSystem`。
- **不**克隆 UE `FReply` / 完整 Capture 栈 / Slate bubble path。
- MVP 仅 **左键**；多指 / 右键拖后置。
- Editor **非 Play** 不路由（继承 F02）。
- Payload **由游戏持有**：引擎不定义 `ItemInstance`。

## Preferences

- 组合风格对齐 Button：GO 上薄 Component + UISystem 调度。
- 拖影（ghost）**默认由游戏拥有**（minCraft 已有 `m_CursorVisual`）；引擎暴露指针参考坐标，不强制引擎画 ghost。
- 命名：`DragSourceComponent` / `DropTargetComponent`；会话结构 `ScreenUIDragSession`。

## Assumptions

- minCraft 会在 API 落地后把点选桥换成真拖拽；验收以拖拽为准。
- F06（Sprite/Raw Image）可并行；F05 单测可用纯色 Raw Image 做拖影。

## Non-Goals

- Inventory / Crafting / 炉槽语义进 Core。
- 完整 UMG DragDropOperation 资产化、装饰器层、多指针。
- Editor Hierarchy「拖拽改父」（已有 ED-F08，另一路径）。
- Focus / Tab / 键盘拖放。
- Lua 拖拽回调。
- 透明像素精确命中（仍矩形 Hit）。

## Current Architecture

| 能力 | 状态 |
|------|------|
| `UISystem` PointerState（Hover/Press/Click） | **有**；松手宽松 Capture → Click |
| `ButtonComponent::OnClicked` | **有**；`TryDispatchButtonClick` |
| 多 Canvas HitTester | **有** |
| Drag session / DropTarget | **无** |
| Image 全纹理 UV | **有**（F06 将补 Sprite） |

现状（简化）：

```text
MouseDown → Pressed = Hit.Widget
MouseUp   → Click + TryDispatchButtonClick(Pressed) → clear Pressed
```

F05 需在 Down→Up 之间插入阈值判定与 Drag 分支。

## Proposed Architecture

### 3.1 分层

```text
UISystem
  ├─ PointerState（既有）
  ├─ DragSession（本 Feature）
  └─ 调度：阈值 → Begin；Move；Release → Drop/Cancel

DragSourceComponent   // 同 GO 需 Widget；声明「可发起拖」+ 可选 UserId
DropTargetComponent   // 同 GO 需 Widget；声明「可接收」+ 可选过滤

游戏（minCraft）
  └─ 订阅委托 → 改 Inventory / 更新 ghost Image；不旁路 UISystem 指针
```

### 3.2 会话状态

```text
enum class EUIDragPhase : uint8_t { None, Pending, Dragging };

struct ScreenUIDragSession
{
  EUIDragPhase Phase = None;
  WidgetComponent* SourceWidget = nullptr;
  DragSourceComponent* Source = nullptr;
  Vector2 PressReferencePx{};      // Down 时触点（Canvas 参考空间）
  Vector2 CurrentReferencePx{};  // 当前触点
  Vector2 PointerDeltaPx{};      // Current - Press（便于阈值）
  bool bSuppressClick = false;   // 一旦 Begin → true
};
```

- **Pending：** Down 在有效 DragSource 上，尚未超阈。
- **Dragging：** 已 `OnDragBegin`；Move 派发 `OnDrag`。
- **None：** 空闲。

阈值默认建议：参考空间 **6 px**（可在 UISystem 或 Source 上配置）；超过阈值即进入 Dragging。

### 3.3 与 Click / Button 互斥

| 情况 | Click / OnClicked |
|------|-------------------|
| Up 且从未进入 Dragging | 与今日一致（宽松 Capture） |
| Up 且 `bSuppressClick` | **不** 置 `bClickThisFrame`；**不** `TryDispatchButtonClick` |
| Down 在无 DragSource 的 Button 上 | 行为不变 |

同一 GO 可同时有 Button + DragSource（例如槽位：轻点 / 拖拽）。用阈值区分。

### 3.4 组件 API（草案）

**DragSourceComponent**

- `bool bInteractable`（默认 true）
- `float DragThresholdPx`（≤0 用系统默认）
- Multicast（**无参**，同 Button `OnClicked`）：`OnDragBegin` / `OnDrag` / `OnDragEnd` / `OnDragCancel`
- 上下文：`UISystem::GetDragSession()`；`ConsumeLastEndSuccess()` 读 End 是否 Drop 成功
- `int32_t UserId`：游戏映射槽位（**不**进反射资产语义）

**DropTargetComponent**

- `bool bInteractable`
- `bool CanAcceptDrop(const ScreenUIDragSession&)` — 默认 true
- Multicast（无参）：`OnDrop` — 监听方读 `GetDragSession()`
- 可选 Should 切片：`OnDragEnter` / `OnDragExit`

引擎在 Drop 时 **不**改游戏状态，只广播。

### 3.5 Release 解析

```text
On Left Mouse Up while Dragging:
  hit = HitTestAtWindowMouse
  target = DropTarget on hit.Widget's GO (if any)
  if target && target.CanAcceptDrop(session):
      OnDrop.Broadcast(...)
      Source.OnDragEnd(success=true)
  else:
      Source.OnDragCancel / OnDragEnd(success=false)
  clear session
```

松手命中为空（拖到空白）→ Cancel。

### 3.6 Ghost / 视觉

| 方案 | 决策 |
|------|------|
| 引擎自动克隆 Image 跟手 | **Out（MVP）** — 所有权与图集引用复杂 |
| 游戏订阅 `OnDrag` / 读 `CurrentReferencePx` 更新自有 Image | **In（推荐）** |
| 引擎提供 `GetDragPointerReferencePx()` | **In** |

F06 落地后，游戏 ghost 可用 Sprite 采 atlas 图标。

### 3.7 Payload 策略

**推荐：** 引擎只带 `SourceWidget` / `DragSourceComponent*`；游戏用 Source 上的 `UserId` 或外部表找回槽位。  
**MVP 不做：** 引擎 `std::any` Payload（序列化/反射噪声大）。

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `UISystem` | 阈值、Session、与 Click 互斥、调度委托 | 物品逻辑、画 ghost |
| `DragSourceComponent` | 可拖声明、Begin/Move/End 出口 | Hit 几何 |
| `DropTargetComponent` | 可放声明、CanAccept、OnDrop | 发起拖 |
| `WidgetComponent` | 几何 / Hit | Drag 政策 |
| `ButtonComponent` | Click 政策 | Drag |
| 游戏 HudOverlay | 槽交换、ghost、合成 | 自研指针状态机 |

## Boundaries

- 允许：UISystem → 读 HitTester / InputSystem；调度 Source/Target 组件。
- 禁止：Drag 组件依赖 Inventory / Item；游戏绕过 UISystem 再写一套 Press 状态驱动同一批 Button。
- 禁止：为 Drag 新建 Engine 子系统。

## Dependencies

- Depends on：UI-F02 / UI-F03；InputSystem 鼠标边沿。
- Depended on by：minCraft GP-F02 拖拽替换；可选后续 Scroll 手势（另 Feature）。
- F06 并行：不硬依赖；联调拖影图标时 Prefer F06 Done。

## Ownership & Lifetime

- `ScreenUIDragSession` 值类型，由 `UISystem` 拥有。
- Source/Target 为组件指针；GO 销毁时若 Session 仍引用 → **立即 Cancel + clear**。
- 不跨 Scene 保持 Session；`OnEndPIE` 清空（同 PointerState）。

## Data Flow

```text
InputSystem mouse
  → UISystem::UpdatePointerFromInput
  → HitTest
  → Pending / Dragging / Click 分支
  → DragSource / DropTarget multicast
  → 游戏改数据 + 刷新 Image
```

## Control Flow

- 仅 `UISystem::Tick` / `PollPointer` 路径推进 Session（与 F02 一致）。
- 组件 **不**自己轮询鼠标抢状态。

## State & Invariants

- `Phase==Dragging` ⇒ `SourceWidget != nullptr` 且 `bSuppressClick == true`。
- `Phase==None` ⇒ 无悬挂 Source。
- 同时至多 **一个** 全局 Drag session（单指针 MVP）。
- `Interactable==false` 的 Source 不能进入 Pending。

## API / Interface Semantics

### `UISystem::GetDragSession() const`

- 只读会话快照；供游戏 Tick ghost。

### `UISystem::IsDragging() const`

- `Phase == Dragging`。

### 委托

**落地：** 无参 `MulticastDelegate<>`（与 Button 同构）。  
`MulticastDelegate<大结构体 by value>` 在 MinGW 下曾导致 Broadcast AV；上下文统一走 `GetDragSession()` / Source `UserId` / `ConsumeLastEndSuccess()`。

监听者不得跨帧保存会话内原始指针；需要时拷贝 UserId / GO id。

## Failure Model

| 失败 | 行为 |
|------|------|
| Source 在 Drag 中被 Destroy | Cancel；清 Session |
| Target `CanAcceptDrop` false | 当 Cancel（或 End success=false）；不 Crash |
| 非 Play 路由关闭 | 不进入 Pending |
| 阈值配置为 NaN / 负 | 夹到系统默认 |

## Alternatives & Trade-offs

### Option A（推荐）— Source/Target 组件 + UISystem Session

- 优点：与 Button 同构；边界清晰；游戏订阅即可。
- 缺点：槽位 GO 多挂一个组件（可接受）。

### Option B — 仅扩展 Widget 虚函数

- 优点：少类型。
- 缺点：Widget 变胖；与「Widget = 几何」分层冲突。

### Option C — 游戏完全自研拖拽

- 优点：引擎零工。
- 缺点：与 Click 双轨；愿景 / TD-015 已否决为长期方案。

## Integration Impact

- 必改 `UISystem::UpdatePointerFromInput`（阈值与 Click 抑制）。
- `ScreenUIPointerState` 可增加 `bDragging` 只读镜像（或经 GetDragSession）。
- Button 单测需加「拖过阈不触发 Click」。
- minCraft：替换 `OnSlotClicked` 桥；保留数据路径。

## Migration / Compatibility

- 无资产格式破坏。
- 现有 Button 场景行为不变（无 DragSource 时）。
- 前向：只加 API。

## Verification Strategy

- Suite（新建或并入 ui）：阈值、Click 抑制、Drop 命中、Cancel、Destroy during drag、非 Play 不启动。
- Play 手验：两 Widget Source→Target；与 Button 共存。
- minCraft 联调（验收波）：背包拖拽 Must（引擎侧 DoD 不强制等游戏合入）。

## Open Questions

1. **Enter/Exit 是否进 MVP？** — 建议 **Should 切片 S02**；S01 可只有 Drop。 — blocking? no  
2. **右键 / 中键？** — 后置。 — blocking? no  
3. **UserData 用 `int32` 还是 `void*`？** — 推荐 `int32_t UserId` + 游戏侧表。 — blocking? no  
4. **阈值空间：参考分辨率 vs 视口像素？** — 推荐 **参考空间**（与 Hit 一致）。 — blocking? no  

## Design Review

- **Verdict:** `Approved`（会话内确认后实现）
- **Reviewer / date:** maintainer / 2026-10-09
- **Link or summary:** Open Questions 取默认；Enter/Exit 后置；无参委托 + GetDragSession

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes`（已实现）

## Acceptance Checklist

- [x] Design review complete when required by complexity/profile
- [x] Implementation plan traces to design decisions (L2+)
- [x] Progress log updated when status changes
- [x] Feature registry status synced

## Slices（实现时）

| Slice | 内容 | Status |
|-------|------|--------|
| S01 | Session + 阈值 + Click 抑制 + Begin/Move/End + 单测 | **Done** |
| S02 | DropTarget + OnDrop + CanAccept + Destroy cancel | **Done**（Destroy→Cancel 已挂） |
| S03 |（可选）DragEnter/Exit；文档/示例 | Deferred |
| S04 | minCraft 联调笔记（可在游戏仓；引擎 Progress 链一笔） | 后置 |

## Wave 候选（本波可排 / 后置）

| ID | 主题 | 相对 F05 |
|----|------|----------|
| UI-F06 | Image Sprite / Raw | **并行推荐** |
| UI-F07 | Image FillAmount（炉进度等） | Should；F06 后 |
| — | Nine-slice Panel | Deferred |
| — | Focus / Tab | Deferred |
| — | ScrollRect | Deferred（可后用 Drag 手势） |
