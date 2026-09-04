# CORE-F09 — Parallel GO / SceneComponent Hierarchy + KeepWorld Reparent

## Meta
- **ID:** `CORE-F09`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/ui`（建议；可随讨论调整）
- **Related:**
  - [CORE-F08](./CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md)（GO `m_Parent` / `m_Children` 已有）
  - [ED-F05](../../Editor/ED-F05_HIERARCHY_TREE_DESIGN.md)（编辑器改父已能改 GO 链接）
  - `SceneComponent::AttachToComponent` / `m_AttachParent`
- **Depends on:** CORE-F08 + ED-F05（编辑体验可用后再收紧运行时契约）

## TL;DR

**GameObject 父子**与 **SceneComponent 附着**是**平行、同构**的两套关系，不是「改 GO 父 ⇒ 自动改子 Root 挂到父 Root」。

本 Feature 要：
1. 明确两套关系的契约与 API 边界（何时联动、何时禁止隐式联动）
2. 正确实现改父时的 **KeepWorld**（子物体世界 Transform 不变，只重算相对父的 local）
3. **父带动子：** 父 GO 移动/旋转/缩放时，子 GO 按相对关系跟随（GO 树自己的世界矩阵传播；不依赖误用 SC 附着）
4. 修正/替换 CORE-F08「方案 A」里 `AttachToParent` 自动 `EnsureRootAttachedToParent` 的默认行为（或改为显式 opt-in）

## 动机（来自 ED-F05 验收）

- Hierarchy 改 GO 父后，观察到非预期的 Scale/拉伸（KeepWorld 未真正达成）。
- `AttachToParent` 会把子 GO 的 Root `SceneComponent` 挂到父 GO 的 Root —— **不是**维护者期望的默认语义。

## 核心原则（拍板方向，Design 阶段细化）

1. **平行同构：** GO 树与 SC 树各自维护 parent/children；概念模型对称（Attach/Detach、环检测、序列化 GUID）。
2. **默认不隐式耦合：** 改 `GameObject::m_Parent` **不得**默认改写任意 `SceneComponent::m_AttachParent`（反之亦然），除非调用方显式请求「同步方案」或工具命令。
3. **KeepWorld：** 改父时先读世界 Transform，改链接后写回等价 local（相对新父）；与 UE `FAttachmentTransformRules::KeepWorldTransform` 对齐学习。
4. **编辑器：** ED-F05 继续只提交 GO 级 Reparent；变换正确性由本 Feature 的运行时契约保证。

## Scope（草稿）

### In
- 契约文档 + API 拆分（GO-only reparent vs SC-only attach vs 可选「同步附着」）
- KeepWorld / KeepRelative 规则实现与测试
- 父 Transform 变化时沿 GO 树更新子世界/相对 Transform
- 序列化：`m_Parent` 与 `m_AttachParent` 独立往返
- 迁移/删除 `EnsureRootAttachedToParent` 作为 AttachToParent 默认副作用（或改为显式）

### Out / 另议
- Hierarchy 兄弟排序
- UI Canvas 专用布局（仍归 UI-F01）
- inactive 层级传播（仍 TD-031）

## 开放问题

- 默认「空 GO」是否仍自动带 Root SceneComponent？与编辑器 Create Empty 策略。
- 是否提供一条显式 Editor 命令：「将子 Root 附着到父 Root」（旧方案 A 的显式版）。
- KeepWorld 在父有非均匀 Scale 时的矩阵分解策略。

## Next

1. Pre-flight 后扩写 §数据流 / §API / §验收
2. 从 CORE-F08 Design 链到本篇；标记方案 A 默认耦合为 **superseded by CORE-F09**

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | Draft：平行层级 + KeepWorld；由 ED-F05 验收问题驱动 |
| 2026-09-05 | 补充：父带动子（GO 树传播）亦属本 Feature |
