# ED-F06 — Anim SM Canvas Polish (Entry / AnyState / Edge Menu) — Design Spec

## Meta
- **ID:** ED-F06
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Branch:** feat/animation
- **Related:**
  - [Implementation Plan](./ED-F06_ANIM_SM_CANVAS_POLISH_IMPLEMENTATION.md)
  - Substrate: [ED-F05](./ED-F05_STATE_MACHINE_CANVAS_DESIGN.md)（SmGraph L1 + AnimGraph bridge）
  - Consumer / truth: [ANIM-F03 Design §9](../Animation/ANIM-F03_ANIMATION_GRAPH_DESIGN.md)
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Depends on:** ED-F05 Done；ANIM-F03 Runtime AnyState 已可用
- **Implementation Plan:** [ED-F06_ANIM_SM_CANVAS_POLISH_IMPLEMENTATION.md](./ED-F06_ANIM_SM_CANVAS_POLISH_IMPLEMENTATION.md)

## TL;DR
在现有 SmGraph 上补齐 AnimGraph 画布语义：**边右键菜单**、**AnyState 画布节点/边**、**Entry 装饰节点**（写回 DefaultStateName，不改资产字段名）。顺带删除 AnimGraphIds 死代码并收口相关文档。

## Scope

### In
- SmGraph：边/节点上下文菜单扩展点（L1 可复用事件；文案可由宿主定制）
- AnimGraph bridge：边右键 → Delete / Reverse；画布绘制 **AnyState** 与 **Entry**
- Entry：只读装饰节点；出边到 State = 设置 DefaultStateName；**不**删除 DefaultStateName 字段
- AnyState：画布实体节点 + AnyStateTransitions 边；选中 → 现有 Inspector
- Chore：删除无用 AnimGraphIds.*；对齐 F03/F05/ACTIVE_WORK 表述

### Out
- 改 Runtime 求值语义（AnyState 已有；Entry 仍经 DefaultStateName Bind）
- 真·多 Entry 出边竞态 / 去掉 DefaultState 字段迁移
- BlendTree / 子状态机 / Preview / Undo（见 backlog 与 ANIM-F04）
- Material 图交互

## Reader quick start
1. 本文件 §3–§5（Entry / AnyState / 菜单契约）
2. [Impl](./ED-F06_ANIM_SM_CANVAS_POLISH_IMPLEMENTATION.md) 切片
3. 代码：UI/SmGraph + AnimGraphSmBridge + AnimGraphWindow

---

## 1) 背景与目标

### 1.1 Pain
ED-F05 MVP 后：边只能点选 + Delete 键；AnyState 数据在 Runtime/Inspector 可用但画布不可见；DefaultState 仅靠无选中 Inspector 下拉，缺少 Mecanim/UE 式 Entry 锚点。

### 1.2 目标

| # | 目标 |
|---|------|
| G1 | 右键边：Delete、Reverse（与 Inspector Reverse 同真源） |
| G2 | 画布可见 **AnyState** 节点及出边；可点选进 Inspector |
| G3 | 画布 **Entry** 装饰节点；连到 State 即设定初始态 |
| G4 | 真源仍 AnimationGraph；DefaultStateName 保留 |
| G5 | 清理 ax 时代 AnimGraphIds |

### 1.3 成功长什么样
打开 .meagraph：见 Entry → DefaultState、普通 State、AnyState；右键边可删/反转；Save 后 DefaultState / AnyState 往返正确。

---

## 2) 现状

| 项 | 现状 |
|----|------|
| SmGraph | State + Transition；空白 Add State；无边右键 |
| AnyState | AnyStateTransitions Runtime Done；画布跳过 |
| Entry | 无节点；DefaultStateName + Inspector |
| AnimGraphIds | 已删除（S00） |

---

## 3) 方案（Locked 草案）

### 3.1 分层（延续 ED-F05）

`	ext
SmGraph L1: Document 可含特殊 NodeKind（State / Entry / AnyState）
            EditEvent: ContextMenu / SetDefaultTarget / ...
AnimGraph L2: 映射 Kind ↔ 真源；Apply 写 Session
`

| 层 | 允许 | 禁止 |
|----|------|------|
| L1 SmGraph | 通用 Kind、菜单事件、绘制差异 | include AnimationGraph |
| L2 Bridge | DefaultState / AnyStateTransitions | 在 Widget 内写领域逻辑 |

### 3.2 Entry（Locked）

| 项 | 行为 |
|----|------|
| 数据 | **不**新增资产字段；Entry 是 View 节点 |
| 语义 | Entry → State 边 = DefaultStateName = that State |
| 外观 | 区别于普通 State（更小/异形/标签 Entry）；**无** Clip 副标题 |
| 交互 | 不可作为 Transition 的 To；不可自环；不可删除（或删=清空 Default 需确认——MVP：**不可删**） |
| 从 Entry 拖线 | 仅允许落到 State；成功则更新 DefaultState + Dirty |
| 已有 Default | Pull 时画 Entry→该 State 的「逻辑边」（可只读显示，或可改目标） |
| Inspector | 无选中时仍可编辑 DefaultState 下拉（与画布双向） |

### 3.3 AnyState（Locked）

| 项 | 行为 |
|----|------|
| 节点 | 画布固定/可拖 **AnyState** 节点（EditorPos：可复用约定位置或专用字段——MVP 用桥接侧缓存坐标 / 或 Constants；优先：存在则写 AnimStateMachine 可选 AnyStateEditorPos* **仅当**已有字段，否则 Bridge 会话级 Pos，Save 时可不持久——**推荐 MVP：固定画布坐标常量 + 可拖写会话，二期再持久化**） |
| 边 | AnyStateTransitions；From 语义 = AnyState |
| 创建 | 从 AnyState 环带拖到 State → AddAnyStateTransition（若 API 缺失则扩展 Editor） |
| 选中 | Selection Kind = AnyStateTransition（已有枚举） |
| 删除 | 删边 / 不可删 AnyState 节点本体（MVP） |

> **持久化：** 若不想动资产 schema，AnyState 节点位置可用固定布局；用户拖动仅会话有效。审批时可改为加 AnyStateEditorPosX/Y。

### 3.4 边 / 节点右键菜单

| 项 | 行为 |
|----|------|
| 触发边 | 右键命中 Edge |
| 边菜单 | Delete；Reverse（仅普通 Transition；AnyState / Entry 边禁用 Reverse；Entry 边不可删） |
| 触发节点 | 右键命中 **State** 节点（Entry / AnyState：无菜单或全禁用） |
| 节点菜单 | **Rename**（popup InputText → `RenameState`）；**Delete**（同 Delete 键） |
| L1 | 发 `RenameNodeRequested` / `DeleteSelectionRequested` 等；宿主写真源 |

### 3.5 清理

- 删除 AnimGraphIds.h/.cpp（确认无引用）
- 文档：ED-F05 Deferred 项迁入本 Feature；ANIM-F03 §9 指向 ED-F06

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| A. 删除 DefaultStateName，纯 Entry 边模型 | Reject（迁移成本高） |
| B. Entry 装饰 + 保留 DefaultState | **选用** |
| C. AnyState 仅列表不画布 | Reject（与目标不符） |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| Entry 边与普通 Transition 混淆 | 视觉区分；Entry 边不进 Transitions 数组 |
| AnyState 位置不持久 | MVP 固定位；文档写明；二期字段 |
| Reverse AnyState | MVP 禁用 |

---

## 6) 验收

- [x] 右键边：Delete / Reverse 写回真源且不崩（代码落地；待手测）
- [x] Entry 可见；拖到 State 更新 DefaultStateName；Save 往返（代码落地；待手测）
- [x] AnyState 节点 + 出边可见；点选 Inspector；删边 OK（代码落地；待手测）
- [x] AnimGraphIds 已删
- [x] Design/Impl/Registry/ACTIVE_WORK/Progress 对齐
- [x] 右键 State：Rename / Delete（代码落地；待手测）

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done** |
| What's done | S00–S05；Entry / AnyState / 边与节点菜单；空 Clip 策略见 ANIM-F03 |
| What's not | — |
| Next | 准备 commit；ANIM-F04 错峰 |
| Blocked by | — |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-10 | S05 State 右键 Rename/Delete；配合 F03 空 State 策略 B |
| 2026-09-10 | 补：State 右键 Rename/Delete；空 State Runtime 策略指向 F03 |
| 2026-09-10 | 实现 S00–S03：边右键、Entry、AnyState 画布；删 AnimGraphIds |
| 2026-09-10 | 初稿：边菜单 + Entry 装饰 + AnyState 画布；待审批 |
