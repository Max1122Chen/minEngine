# ED-F05 — Hierarchy Tree & Reparent

## Meta
- **ID:** ED-F05
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:**
  - [CORE-F08 GameObject Hierarchy](../Platform/Core/CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md)（**硬依赖**）
  - [UI-F01](../Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md)
  - 现有：HierarchyWindow（平铺列表）
- **Depends on:** CORE-F08 MVP
- **Implementation:** （待 Planned）

## TL;DR

把 Hierarchy 从 **平铺列表** 升级为 **缩进树**：展开/折叠、拖拽改父（Undo），便于搭建 Canvas UI 树与普通场景组织。

## Scope

### In（MVP）
- 按 GetChildren 递归绘制（根 = Parent == null）
- 展开/折叠状态（编辑器会话内即可）
- 拖拽：GO → 另一 GO 下（调用 CORE-F08 Attach）；拖到空白/根区 Detach
- Undo：ReparentGameObjectCommand（或等价）接入现有 CommandStack
- 多选改父：MVP 可只支持单选拖拽

### Out
- 搜索过滤、可见性眼睛图标、预制体覆盖指示（后置）
- 跨 Scene 拖拽

## 现状

HierarchyWindow：GetHierarchyGameObjects() 顺序 or + Selectable，**无**父子缩进、**无** DnD reparent。

## 方案摘要

`	ext
CORE-F08 API
     ↓
HierarchyWindow::DrawNode(go)
  ImGui::TreeNode / Selectable
  BeginDragDropSource / Target
     ↓
SceneEditor::SubmitReparent(go, newParent)
`

保持与现有选中、Rename、右键菜单兼容。

## 验收

- [ ] 树形缩进与 CORE-F08 数据一致
- [ ] 拖拽改父有 Undo/Redo
- [ ] 无 CORE-F08 时不崩溃（Feature 依赖构建顺序 / 运行时断言）

## Status note

| 字段 | 内容 |
|------|------|
| Status | **Draft** |
| Blocked by | CORE-F08 |
| Next | CORE-F08 Planned 后扩写交互细节 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft |
