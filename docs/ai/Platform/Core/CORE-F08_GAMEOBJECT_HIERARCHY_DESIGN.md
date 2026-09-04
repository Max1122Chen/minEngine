# CORE-F08 — GameObject Hierarchy

## Meta
- **ID:** `CORE-F08`
- **Type:** Feature
- **Status:** Review — 待 ED-F05 联合验收
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/ui`
- **Related:**
  - [UI-F01](../UI/UI-F01_UI_SYSTEM_DESIGN.md)
  - [ED-F05](../../Editor/ED-F05_HIERARCHY_TREE_DESIGN.md)
  - [CORE-F03 Transform](./CORE-F03_TRANSFORM_QUATERNION_DESIGN.md)
  - [CORE-F06](./CORE-F06_COMPONENT_ENABLE_DESIGN.md)
  - [Impl](./CORE-F08_GAMEOBJECT_HIERARCHY_IMPLEMENTATION.md)
- **Blocks:** `UI-F01`；场景树组织
- **Depends on:** 现有 `SceneComponent` 附着链；对象指针 GUID 序列化

## TL;DR

为 `GameObject` 增加父子层级。Transform 采用 **方案 A**：子 Root `AttachToComponent(父 Root)`。  
序列化对齐 `SceneComponent::m_AttachParent`：**只存父指针**（`ME_PROPERTY` + GUID），子列表运行时重建。  
删父默认 **级联销毁** 子树。`aactiveInHierarchy` 传播 **Deferred**（TD-031）。

运行时 MVP 已落地；**Hierarchy 树形/拖拽改父属 ED-F05**，本 Feature **待 ED-F05 联合验收** 后再标 Done。

## Scope

### In（MVP）
- API：`AttachToParent` / `DetachFromParent` / `GetParent` / `GetChildren`
- 序列化：`ME_PROPERTY GameObject* m_Parent`；加载后重建 `m_Children` + 确保 Root 附着
- 环检测；KeepWorld / KeepRelative
- `Scene::RemoveGameObjectById` 级联删子树

### Out / Deferred
- ED-F05 树 UI / 拖拽（联合验收项）
- **`aactiveInHierarchy` 随父传播**（Deferred → TD-031）
- 跨 Scene 引用；删父后「提升为根」策略
- **不**使用整型 `ParentId` 维护存盘边

## Reader quick start
1. §3 拍板契约  
2. Impl 切片  
3. `GameObject` / `FinalizeLoadedScene` / `FinalizePIEScene`

---

## 1) 背景与目标

无 GO 父子则无法表达 Canvas UI 树；Hierarchy 只能平铺。

成功：Attach/Detach、存盘恢复、级联删除、现有无父场景行为不变。

---

## 2) 现状

- `Scene.m_GameObjects` 平铺；运行时 `m_ID` 仅场景内查找（**不**承担存盘父子边）
- `SceneComponent::m_AttachParent` 已是 `ME_PROPERTY`，由序列化 GUID + `RebuildSceneComponentAttachHierarchy` 重建子列表
- `FinalizeLoadedScene`：`RebuildRuntimeGameObjectIndex` + SC attach rebuild

---

## 3) 方案（已拍板）

### 3.1 模型

```text
GameObject
  m_Parent   : GameObject*           // ME_PROPERTY；存盘 GUID；null = 根
  m_Children : vector<GameObject*>   // 运行时；不序列化；由父指针重建
  m_ID       : uint64                // 运行时场景内索引；不用于父子存盘
```

所有权仍在 `Scene` 的 `shared_ptr` 容器。父子指针为非拥有观察。

### 3.2 Transform — 方案 A

- GO 对外 Transform = Root `SceneComponent`
- `AttachToParent` → `childRoot->AttachToComponent(parentRoot, rules)`
- `DetachFromParent` → `childRoot->DetachFromParent(rules)`
- 复用现有 `GetWorldMatrix()` 父链

### 3.3 删除策略

`RemoveGameObjectById`：**深度优先收集子树，全部移除**（级联销毁）。

### 3.4 序列化 / 加载

对齐 SC attach 模式：

1. 序列化 `m_Parent`（GUID / pending object ref）  
2. **不**序列化 `m_Children`（避免双边与环）  
3. 反序列化完成 GUID 解析后：
   - `RebuildRuntimeGameObjectIndex`
   - `ResolveGameObjectHierarchy`：清 `m_Children` → 按 `m_Parent` 挂回子列表 → 确保子/父 Root `AttachToComponent(KeepRelative)`  
   - `RebuildSceneComponentAttachHierarchy`（在 GO Resolve 之后，因 Root 附着可能被改写）
4. 缺字段 / `m_Parent==null` → 根（兼容旧场景）

PIE：`FinalizePIEScene` 同样调用 `ResolveGameObjectHierarchy`。

### 3.5 Deferred：Inactive 传播

父 `m_bActive=false` 时子 `aactiveInHierarchy` 语义 **本期不做**。见 TD-031。

---

## 4) 验收标准

### 运行时（本提交）
- [x] Attach/Detach + GetParent/Children
- [x] KeepWorld 与 KeepRelative
- [x] 存盘再加载树一致（GUID 父指针）
- [x] 无环；非法 Attach 失败打日志
- [x] 删父级联删子；旧平铺场景仍可用
- [x] suite `gameobject-hierarchy`

### 联合验收（ED-F05）
- [ ] Hierarchy 树形显示父子
- [ ] 拖拽改父调用 CORE-F08 API；场景 dirty / 可存盘

## 5) Status note

| 字段 | 内容 |
|------|------|
| Status | **Review — 待 ED-F05 联合验收** |
| Next | 实现 ED-F05；联合验收后 CORE-F08 → Done |
| Unblocks | UI-F01（运行时）；编辑体验等 ED-F05 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft |
| 2026-09-04 | Planned：方案 A；级联删除；Inactive Deferred |
| 2026-09-04 | **改序列化：** 弃 `m_ParentId`；`ME_PROPERTY m_Parent` + GUID |
| 2026-09-04 | **实现 + 单测**；Status=Review，待 ED-F05 联合验收 |
