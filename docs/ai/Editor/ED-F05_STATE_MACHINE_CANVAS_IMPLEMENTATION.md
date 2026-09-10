# ED-F05 — State Machine Graph Canvas — Implementation Plan

## Meta
- **ID:** `ED-F05`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-09
- **Related:** [Design Spec](./ED-F05_STATE_MACHINE_CANVAS_DESIGN.md) · [ANIM-F03](../Animation/ANIM-F03_ANIMATION_GRAPH_DESIGN.md)

## TL;DR
两层落地：先 **`UI/SmGraph`（引擎无关）**，再 **`AnimGraphSmBridge` + `AnimGraphWindow` cut-over**。

## Scope
- **In:** Layer 1 SmGraph + Layer 2 AnimGraph 接入
- **Out:** Material 迁移；Preview；Undo；AnyState 画布节点

## 1) 切片总览

| Slice | 内容 | 状态 | 验证 |
|-------|------|------|------|
| S00 | `UI/SmGraph` 骨架：`Document`/`EditEvent`/`Widget` + Canvas pan/zoom/网格 | Done | Editor Debug 编译 PASS |
| S01 | State 块绘制、选中、拖动（Document.Pos） | Done | 待手测 |
| S02 | Transition 边绘制、箭头、点选边 | Done | 待手测 |
| S03 | 边缘热区 LinkDrag；Delete；禁自环；空白 AddNode 事件 | Done | 待手测 |
| S04 | `AnimGraphSmBridge` + `AnimGraphWindow` 换绑；移除 ax 节点路径 | Done | Editor Debug PASS；待 smoke |

## 2) 切片详情

### S00–S03 — Layer 1（可合并实现）
- **Touch:** `Editor/src/UI/SmGraph/`
- **Deps:** 仅 `imgui` + `imgui_canvas.h`
- **DoD:** Design §3.2 + §5；无 Animation/Asset include

### S04 — Layer 2
- **Touch:** `AnimGraphSmBridge.*`；`AnimGraphWindow.*`
- **DoD:** Design §3.3 + §7；默认 UX 为 SmGraph

## 3) Engineering notes
- **Build:** `cmake --build build --target Editor`
- **Do not commit:** 本地 Animations / Sources / `build_*.log`
- CMake：`Editor` 已 `GLOB_RECURSE ./src/*.cpp`，新文件自动编入

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-09 | 初版切片 S00–S04 |
| 2026-09-09 | 对齐两层架构；Status → In Progress |
