# ED-F05 — State Machine Graph Canvas — Design Spec

## Meta
- **ID:** `ED-F05`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-09
- **Branch:** `feat/animation`（首消：ANIM-F03 AnimGraph；Canvas 基板属 Editor）
- **Related:**
  - [Implementation Plan](./ED-F05_STATE_MACHINE_CANVAS_IMPLEMENTATION.md)
  - Consumer: [ANIM-F03 Design §9](../Animation/ANIM-F03_ANIMATION_GRAPH_DESIGN.md)
  - Substrate: `Third-Party/imgui-node-editor/imgui_canvas.h`（`ImGuiEx::Canvas`）
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Depends on:** ANIM-F03 Editor MVP（`.meagraph` 真源 + Inspector/Parameters 已可用）
- **Implementation Plan:** [ED-F05_STATE_MACHINE_CANVAS_IMPLEMENTATION.md](./ED-F05_STATE_MACHINE_CANVAS_IMPLEMENTATION.md)

## TL;DR
两层交付：**(1) 引擎无关的 ImGui SM Graph 控件**（`ImGuiEx::Canvas` + 自研 State/Edge）；**(2) AnimGraph 适配层**把控件接到 `AnimationGraph` / Session / Inspector。  
**不**复用 `ax::NodeEditor` 的 Pin/Link/Node；Material 继续完整 node-editor。

## Scope

### In
- **Layer 1 — `UI/SmGraph`：** 可复用 SM 图控件（document + widget + 交互）；仅依赖 ImGui / `ImGuiEx::Canvas`
- **Layer 2 — AnimGraph bridge：** 同步 `AnimState`/`AnimTransition`/`EditorPos*`；选中 → 现有 Inspector；替换 `AnimGraphWindow` 内 ax 节点层
- MVP 交互：选中、拖节点、边缘拖出建边、点选边、删边/删节点、空白添加 State、禁止自环

### Out
- 改 Runtime `AnimationGraphInstance` 行为
- Material 图迁移到本 Canvas
- Preview 视口、Undo/Redo、多选框选精修、自动布局、边中段「规则菱形」完整 UX
- 边/节点右键上下文菜单（Delete / Reverse 等）— Deferred
- Inspector 面板 UX 精修 — Deferred（另排）
- 子状态机 / BlendTree / AnyState 实体节点精修（数据可保留；画布 MVP 不画 AnyState）
- Fork 修改 `ax::NodeEditor` 内部以「改成 SM」
- 把 Layer 1 抽成独立第三方仓库（本期仍住在 Editor 树内，但 **禁止** include 动画/资产/Session）

## Reader quick start
1. 本文件 **§3 两层架构** + **§5 交互锁定**
2. [Impl](./ED-F05_STATE_MACHINE_CANVAS_IMPLEMENTATION.md)：切片 S00–S04
3. 代码：`Editor/src/UI/SmGraph/`（Layer 1）+ `AnimGraphSmBridge` + `AnimGraphWindow`（Layer 2）

---

## 1) 背景与目标

### 1.1 Pain
ANIM-F03-S08 用 `ax::NodeEditor` 伪装 SM：强制 In/Out Pin，观感是数据流节点图，不是 Mecanim/UE 状态机。社区无开箱 SM 画布；官方（imgui#1632）亦指向自研 DrawList。

### 1.2 目标

| # | 目标 |
|---|------|
| G1 | State = **整块可点选/拖动**的节点；**无**强制双 IO Pin 语义 |
| G2 | Transition = **有向边**（箭头）；可点选进 Inspector |
| G3 | 从 State **边缘热区**拖出连线到另一 State → 创建 Transition |
| G4 | 画布 pan/zoom 复用 **`ImGuiEx::Canvas`**，与 Material 的完整 node-editor **并存** |
| G5 | Layer 1 **不耦合**引擎领域类型；真源变更只经 Layer 2 |
| G6 | 真源唯一：`AnimationGraph`（首消）；View/Document 可丢弃重建 |

### 1.3 成功长什么样
打开 `.meagraph`：左侧是「状态机图」；拖边缘建边、点边改条件、拖节点写回 `EditorPos`；右侧 Inspector/Parameters 与 S08b 一致。同一套 `SmGraph` 控件日后可接别的 SM 资产而无需改 Widget 内核。

---

## 2) 现状

| 项 | 现状 |
|----|------|
| 真源 | `AnimationGraph` + Session/Dirty/Save（ANIM-F03） |
| 画布 | `AnimGraphWindow` → `ax::NodeEditor` Pin/Link（待替换） |
| Canvas 基板 | `imgui_canvas.h`：`ImGuiEx::Canvas` **不依赖** Pin |
| Material | 继续完整 `ax::NodeEditor`；本 Feature **不碰** |

---

## 3) 方案（两层 Locked）

### 3.1 分层图

```text
┌─ Layer 2: AnimGraph consumer ──────────────────────────────┐
│  AnimGraphWindow  (toolbar / Save / asset combo)           │
│  AnimGraphSmBridge  (AnimationGraph ↔ SmGraph::Document) │
│  AnimationGraphEditor Session / Inspector selection        │
│         │ pull / push / apply EditEvent                    │
│         ▼                                                  │
│  ┌─ Layer 1: UI/SmGraph (ImGui-only) ───────────────────┐ │
│  │  SmGraph::Document   (nodes / edges / selection)     │ │
│  │  SmGraph::Widget     (Canvas + draw + input FSM)     │ │
│  │  EditEvent[]         (create/delete/move/select…)    │ │
│  │  deps: imgui + ImGuiEx::Canvas ONLY                  │ │
│  └──────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────┘
```

| 层 | 目录（拟定） | 允许依赖 | 禁止 |
|----|--------------|----------|------|
| **L1 SmGraph** | `Editor/src/UI/SmGraph/` | `imgui.h`、`imgui_canvas.h`、STL、本模块头 | `AnimationGraph*`、`Asset*`、`AnimationGraphEditor*`、`IEditorContext`、Session |
| **L2 Bridge** | `SubEditor/AnimationGraph/AnimGraphSmBridge*` | L1 + `AnimationGraph` + Editor Session API | 在 Widget 内写领域逻辑 |
| **L2 Window** | `AnimGraphWindow` | Bridge + Editor UI | 直接画 ax Pin/Link（cut-over 后删除） |

### 3.2 Layer 1 合同（可复用核心）

| 类型 | 职责 |
|------|------|
| `SmGraph::NodeId` / `EdgeId` | 不透明 `uint64_t`；由宿主分配/解释 |
| `SmGraph::Node` | `Id`、左上角 `Pos`、`Size`、`Title`、`Subtitle` |
| `SmGraph::Edge` | `Id`、`From`、`To`（有向） |
| `SmGraph::Document` | 节点/边数组 + Selection；**非**权威真源 |
| `SmGraph::EditEvent` | Widget → 宿主：选中变化、节点移动、请求建边/删选中/空白加点 |
| `SmGraph::Widget` | `ImGuiEx::Canvas` pan/zoom；绘制；§5 输入状态机；**只改 Document 的选中/坐标**；结构变更只发事件 |

**复用方式：** 任意宿主自建 `Document`、每帧 `Widget::Draw`、消费 `EditEvent` 写回自己的真源。第二个消费者出现前 **不**再抽虚接口（避免空抽象）；Document/Event 已是稳定边界。

### 3.3 Layer 2 合同（AnimGraph 接入）

| 职责 | 说明 |
|------|------|
| Pull | `AnimationGraph` → `Document`（State→Node，Transition→Edge；跳过 AnyState 边 MVP） |
| Push positions | `Node.Pos` → `EditorPosX/Y`（左上角约定） |
| Apply events | `AddTransition` / `Remove*` / `AddStateAt` / `SetSelection` / `NotifyGraphChanged` |
| ID 映射 | MVP：`NodeId = stateIndex+1`，`EdgeId = transitionIndex+1`（与现 ax ID 策略同级；结构变更后重建 Document） |

### 3.4 坐标系

| 空间 | 用途 |
|------|------|
| Screen | ImGui 屏幕像素 |
| Canvas | `EditorPosX/Y` / `Node.Pos` = 节点 **左上角** |

进入 `Canvas::Begin` 后，绘制与命中均在 Canvas 空间。

### 3.5 产品锁定（审批确认）

| 项 | 决定 |
|----|------|
| 同 From→To 多条边 | **允许**（条件可不同） |
| `EditorPos` | **左上角** |
| AnyState 画布 | MVP **不画** |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 继续加深 ax 伪装 | 零新基板 | Pin 语义天花板 | Reject |
| B. 换 imnodes / ImNodeFlow | 库更轻 | 仍是 Pin 图 | Reject |
| C. Canvas + 自研 State/Edge + 两层边界 | 贴 SM；可复用；Material 不受影响 | 需自研命中/拖线 | **选用** |
| D. L1 直接依赖 `AnimationGraph` | 实现快 | 无法复用；违背本期约束 | Reject |
| E. 过早 `ISmGraphModel` 虚接口 | 看起来「干净」 | 单消费者空抽象 | Reject（Document/Event 边界足够） |

---

## 5) 交互逻辑（Locked）

> 对标 UE AnimBP State Machine / Unity Animator：块状 State、有向边、边缘拖线。  
> MVP 以「可编辑、可学」为准；视觉可后抛光。

### 5.1 State 节点

| 项 | 行为 |
|----|------|
| 外观 | **双区**（外圈 Link / 内区拖动）；按 Title+Subtitle **自适应尺寸**；选中为蓝色描边+环带着色；拖线预览暖黄 |
| 尺寸 | 由文字 `CalcTextSize` + padding/`BodyInset` 决定；`MinNodeSize` 为下限；MVP **不**支持用户拖角缩放 |
| 位置 | Document `Pos` = **左上角**（Canvas 空间） |
| 选中 | 单击节点 → Selection = Node；高亮边框 |
| 拖动 | 左键按住节点主体拖动 → 更新 `Pos` → `EditEvent::NodeMoved` |
| 删除 | 选中节点 + Delete → `DeleteSelectionRequested`（宿主删 State + 关联边） |

### 5.2 边缘热区（创建连线起点）

| 项 | 行为 |
|----|------|
| 热区 | 节点外扩环带（`EdgeRingThickness`≈18px）+ 节点外圈内缩带（`BodyInset`≈10px）；**不是** In/Out Pin |
| 开始拖线 | 热区按下并拖出 → **LinkDrag** |
| 与拖节点冲突 | 热区优先；内缩主体才拖位置 |

```text
┌─────────────────────────┐
│ ░░░░░ edge hit ring ░░░ │  ← 拖线起点
│   ┌─────────────────┐   │
│   │  State body     │   │  ← 选中 / 拖动位置
│   │  Name / Clip    │   │
│   └─────────────────┘   │
│ ░░░░░░░░░░░░░░░░░░░░░░░ │
└─────────────────────────┘
```

### 5.3 Transition 边

| 项 | 行为 |
|----|------|
| 几何 | 从源节点边缘到目标边缘的**直线** + **箭头**朝向目标 |
| 创建 | LinkDrag 释放在另一 State → `CreateEdgeRequested`；空白取消 |
| 自环 | **禁止**（Widget 拒绝） |
| 重复边 | **允许** 同 From→To 多条 |
| 选中 | 点边（距离 ≤ ~6px）→ Selection = Edge |
| 删除 | 选中边 + Delete → `DeleteSelectionRequested` |
| Reverse | 仍在 Inspector；画布不做手势反转 |

### 5.4 画布手势

| 手势 | 行为 |
|------|------|
| 中键拖 / Alt+左键拖 | 平移 Canvas View |
| 滚轮 | 以光标为中心缩放（夹紧约 0.25–2.5） |
| 左键空白 | 清除 Selection |
| 右键空白 | 菜单：Add State（点击处 Canvas 坐标）→ `AddNodeRequested` |
| 右键边 / 节点 | MVP **无**专用菜单（点选 + Delete / Inspector）；边右键（Delete / Reverse 等）**Deferred** |
| Delete | `DeleteSelectionRequested` |
| Escape | 取消进行中的 LinkDrag |

### 5.5 输入状态机（Widget）

```text
Idle
  ├─ LMB down on State body     → DragState
  ├─ LMB down on edge ring      → LinkDrag
  ├─ LMB down on Transition     → Select Edge
  ├─ LMB down on empty          → Clear selection
  ├─ MMB / Alt+LMB drag         → Pan
  ├─ Wheel                      → Zoom
  └─ Delete                     → DeleteSelectionRequested

DragState
  └─ LMB up / Escape            → Idle (+ NodeMoved)

LinkDrag
  ├─ hover State (≠ from)       → preview highlight
  ├─ LMB up on valid State      → CreateEdgeRequested → Idle
  ├─ LMB up elsewhere / Escape  → cancel → Idle
  └─ rubber-band to cursor
```

**单帧原则：** 热区 > 边命中 > 节点体 > 空白。

### 5.6 与 Inspector / Dirty（Layer 2）

| 事件 | 结果 |
|------|------|
| 真源变更 | `NotifyGraphChanged()` → Dirty |
| Selection 变更 | `SetSelection` → Inspector |
| 打开资产 / 重绑 | 清空 Widget 手势；Rebuild Document |

### 5.7 AnyState（MVP）

- 画布 **不画** AnyState 节点与边。
- Inspector 对 AnyState 的支持可保留；二期再加画布。

---

## 6) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Canvas + 自定义 hit 抢输入 | 拖不动/拖线失灵 | §5.5 优先级；手测清单 |
| 边命中不准 | 难选边 | 距离阈 + 选中加粗 |
| L1/L2 边界渗漏 | 难复用 | Code review：SmGraph 禁动画头 |
| 索引型 ID 在删改后漂移 | 选中错位 | 结构变更后 Rebuild + Clear 手势 |
| `EditorPos` 约定不一致 | 旧图错位 | 锁定左上角；Layout-if-zero 保留 |

---

## 7) 验收标准

- [x] `UI/SmGraph` **无** Animation/Asset/Session include
- [x] `AnimGraphWindow` 使用 SmGraph，**无** `ax::NodeEditor::Begin/End` 绘制 State/边
- [x] 仍使用 `ImGuiEx::Canvas` 做 pan/zoom
- [x] 拖节点写回 `EditorPos`；Save 往返位置正确
- [x] 边缘拖线创建 Transition；点选边 → Inspector；Delete 不崩溃
- [x] 禁止自环；同向多条边允许；真源仅 `AnimationGraph`
- [x] Material 图编辑回归：仍可用完整 node-editor
- [x] Design / Impl / Registry / ACTIVE_WORK / Progress 对齐

---

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done**（MVP；边右键/Inspector UX Deferred） |
| What's done | 两层 Design；L1 `UI/SmGraph`；L2 Bridge + AnimGraphWindow cut-over；Editor Debug PASS |
| What's not | 边右键菜单；Inspector UX 精修；AnyState 画布 |
| Next | 准备 commit；后续 Deferred 另排 |
| Blocked by | — |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-09 | 初稿：ED-F05；Canvas + 自研 State/Edge；交互 §5 |
| 2026-09-09 | 维护者确认：两层（可复用 ImGui SmGraph + AnimGraph 接入）；多边允许；EditorPos=左上角；Status → In Progress |
| 2026-09-09 | UX：加宽 Link 热区（环+BodyInset）；直线边；空白菜单 Add State；边右键/Inspector UX Deferred |
| 2026-09-09 | 自适应节点尺寸；蓝/暖黄高亮；HitBodyInset；缩放对齐 ax Navigate（Scroll/Zoom+ViewRect EaseOut）；Status → Done |


## 后续（2026-09-10）
Deferred（边右键 / AnyState 画布 / Entry）已迁入 [ED-F06](./ED-F06_ANIM_SM_CANVAS_POLISH_DESIGN.md)。
