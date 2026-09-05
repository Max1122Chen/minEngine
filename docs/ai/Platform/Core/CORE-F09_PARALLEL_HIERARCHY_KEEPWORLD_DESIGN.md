# CORE-F09 — GO Hierarchy via Root SceneComponent Attachment

## Meta
- **ID:** `CORE-F09`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/ui`
- **Related:**
  - [CORE-F08](./CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md)（GO `m_Parent` / Attach API 已有；本 Feature **收紧语义**）
  - [ED-F05](../../Editor/ED-F05_HIERARCHY_TREE_DESIGN.md)（Hierarchy 改父；变换正确性由本 Feature 保证）
  - [CORE-F03 Transform](./CORE-F03_TRANSFORM_QUATERNION_DESIGN.md)
  - 代码：`GameObject::AttachToParent`、`SceneComponent::AttachToComponent` / `GetWorldMatrix`
- **Depends on:** CORE-F08 + ED-F05（编辑器改父可用）
- **Filename note:** 文件名含历史 `PARALLEL`；**已拍板模型不是「两套平行父子」**，而是 **GO 父子 = Root↔Root 附着**。

## TL;DR

**GameObject 父子关系通过双方 Root `SceneComponent` 的附着实现**：子 Root Attach 到父 Root。  
变换权威在 SC 附着链；`m_Parent` / `m_Children` 是 GO 查询与序列化投影，必须与 Root↔Root 同步。  
改父瞬间用 KeepWorld / KeepRelative 改写子 Root 的 **local** `m_Transform`；之后父 Root 一动，子世界位姿由 `GetWorldMatrix()` 沿附着链自动传播。

## Scope

### In
- 契约：如何理解 GO 父子；与「非 Root 的 SC 附着」边界
- Attach/Detach 瞬间的 Transform 计算（KeepWorld / KeepRelative）
- 父/子更换 Root 时的重挂与传播；无 SC 则解除 GO 父子
- Create Empty / 无 Root 策略；单一写入口
- 修正或删除「双路径」歧义；测试覆盖改父 KeepWorld、父带动子

### Out
- Hierarchy UI（已属 ED-F05）
- 兄弟排序；Socket/骨骼挂点（非 Root 附着 API 可另开）
- `activeInHierarchy`（TD-031）
- 非均匀 Scale 下完美矩阵分解（记风险；MVP 用现有 `DecomposeMatrixToTransform`）

## Reader quick start
1. §3 如何理解父子关系（必读）
2. §4 Attach 瞬间 Transform
3. §5 之后的传播与 Root 变更数据流
4. §6 API / 不变量

---

## 1) 背景与目标

CORE-F08 / ED-F05 已能改 GO `m_Parent`，但：
- 父动物体时子不跟随（若未真正落到 SC 链，或链接与变换不同步）
- KeepWorld 观感异常（拉伸等）
- 曾出现「改 GO 父却隐式改 SC」与「两套树」认知冲突

成功标准：
- Hierarchy 改父后，子在视口中符合 KeepWorld（默认）或 KeepRelative
- 移动父 GO（改其 Root Transform）时，子 GO 世界位姿跟随
- 语义清晰：**GO 父子 ≡ 子 Root 挂在父 Root 下**；非 Root 的 SC 附着不建立 GO 父子

---

## 2) 现状（代码事实）

| 项 | 现状 |
|----|------|
| GO | `m_Parent` / `m_Children`；`AttachToParent` 会 `EnsureRootAttachedToParent` |
| SC | `m_Transform` = **相对附着父的 local**；`GetWorldMatrix()` = `parentWorld * local`（无父则 local 即 world） |
| Attach | `AttachToComponent`：KeepWorld 时 `local = Decompose(inverse(parentWorld) * childWorldBefore)` |
| Detach | KeepWorld 时把 detach 前的 world 写回 `m_Transform` |
| Create Empty | 可不带 `SceneComponent` → 无法形成有效 Root↔Root |

---

## 3) 如何理解父子关系（拍板）

### 3.1 一句话

> **两个 GameObject 成父子 ⇔ 子的 Root SceneComponent 附着在父的 Root SceneComponent 上。**

这不是「额外副作用」，而是 GO 父子的**定义**。

```text
                    Scene
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
       GO_A          GO_B          GO_C
      Root_A        Root_B        Root_C
        │              │
        │   Attach     │
        └──────────────┘     （GO_B 父 = GO_A）
              ▲
              └── 仅 Root↔Root 表达「GO 级」父子
```

### 3.2 两层概念，一条变换链

| 概念 | 作用 | 是否变换权威 |
|------|------|----------------|
| **GO 父子**（`m_Parent` / `m_Children`） | Hierarchy、级联删除、序列化、查询 | **否**（投影） |
| **SC 附着**（`m_AttachParent` / `m_AttachChildren`） | 世界矩阵链、KeepWorld/Relative | **是** |

对「GO 级父子」：两者必须一致——子 Root 的 attach parent **就是**父 GO 的 Root。

### 3.3 与「随便挂一个 SC」的边界

| 操作 | 是否建立/保持 GO 父子 |
|------|------------------------|
| `GameObject::AttachToParent` | **是** → 强制子 Root → 父 Root |
| 子身上某个 Mesh SC 挂到父身上某个非 Root SC（socket 等） | **否**；仅组件级附着，Hierarchy 不出现该边 |
| 仅改 `m_Parent` 指针、不改 Root 附着 | **禁止**（非法中间态） |
| 仅改 Root 附着、不更新 GO 链接 | **禁止**（须经统一入口同步投影） |

### 3.4 无 Root / 失去 Root

- 任一方 **没有**可用作 Root 的 `SceneComponent` → **不能维持** GO 父子 → 解除（Detach + 清 `m_Parent`）。
- 编辑器 Create Empty：若允许进入 Hierarchy 父子，**应默认创建 Root SC**（实现切片中落地）。

### 3.5 父或子更换 Root

- **父更换 Root**（显式指定或删旧 Root 后引擎选出新 Root）：所有仍以该 GO 为父的子，其 Root **KeepWorld** 重挂到**新**父 Root。
- **子更换 Root**：若仍有 GO 父，新 Root **KeepWorld** 挂回父当前 Root；不破坏 GO 父子边。
- 重挂失败或新侧无 Root → 按 §3.4 解除该 GO 边。

### 3.6 对 CORE-F08「方案 A」的关系

CORE-F08 方案 A（子 Root Attach 父 Root）**方向正确**；本 Feature：
- 把该方向升级为 **正式语义定义**
- 补全：Root 变更重绑、无 SC 拆边、单一写入口、KeepWorld/传播验收
- **废弃**「GO 树与 SC 树平行同构、默认不耦合」的草稿表述（本文件早期 PARALLEL 标题仅作文件名遗留）

---

## 4) Attach 瞬间的 Transform 计算

权威数据：每个 `SceneComponent` 上的 `m_Transform` = **相对 `m_AttachParent` 的 local**（无父时即相对世界）。

世界矩阵（已有实现语义）：

```text
World(C) =
  if C.AttachParent == null:  Local(C)
  else:                       World(C.AttachParent) * Local(C)
```

其中 `Local(C) = C.m_Transform.ToMatrix()`。

### 4.1 KeepWorldTransform（Hierarchy / 编辑器改父默认）

目标：改附着后 **世界位姿不变**（在矩阵分解精度内）。

```text
W_before = World(childRoot)           // 改链接前
Attach: childRoot.AttachParent = parentRoot
L_after  = Decompose( inverse(World(parentRoot)) * W_before )
childRoot.m_Transform = L_after
```

对应现码：`SceneComponent::AttachToComponent(..., KeepWorldTransform)`。

GO 层：

```text
GameObject::AttachToParent(parent, KeepWorld)
  → 校验环、双方 Root 存在
  → childRoot.AttachToComponent(parentRoot, KeepWorld)
  → 更新 m_Parent / m_Children 投影
```

### 4.2 KeepRelativeTransform

目标：保留子 Root **当前 local 数值**，世界位姿变为相对新父。

```text
Attach: childRoot.AttachParent = parentRoot
// 不改 m_Transform
// 新 World = World(parentRoot) * Local(childRoot)  （通常会「跳」）
```

### 4.3 Detach

- KeepWorld：记下 `W_before`，断附着，`m_Transform = Decompose(W_before)`（相对世界）。
- KeepRelative：断附着，保留原 local（相对世界解释会变）。

### 4.4 非均匀 Scale

`inverse(parentWorld) * childWorld` 再 `Decompose` 在剪切/非均匀缩放下可能有误差 → 已知风险；MVP 接受现有分解，单测用均匀 Scale 场景验收观感。

---

## 5) 改父之后：Transform 传播数据流

### 5.1 日常「父带动子」（不改链接）

**不**遍历 GO 树去写每个子的 world。只要附着链正确：

```text
用户/系统修改 parentRoot.m_Transform（或父之上的链）
        │
        ▼
下次查询任意子孙 SceneComponent::GetWorldMatrix()
        │
        ▼
递归 World(parent) * Local(self)  → 子世界矩阵自动变
        │
        ▼
渲染 Proxy / 物理等读世界矩阵或等价 API
```

因此：**父 GO「动」= 改其 Root（或 Root 之上附着）的 Transform**；子 GO 无需额外 SetPosition。

GO 层若提供 `GameObject::SetTransform`，应落到 Root SC 的 SetTransform，从而触发上述链。

### 5.2 数据流总览（改父）

```text
Editor ED-F05 / 脚本
  → SceneEditor::SubmitReparent (或直接 API)
  → GameObject::AttachToParent / DetachFromParent
        │
        ├─► SceneComponent::AttachToComponent / DetachFromParent
        │         （KeepWorld/Relative 改写 local）
        │
        └─► 同步 GO 投影 m_Parent / m_Children
                    │
                    ▼
              之后每帧/每次查询：GetWorldMatrix 传播
```

### 5.3 父更换 Root

```text
Parent.SetRoot(newRoot) 或 删除旧 Root 后选出 newRoot
        │
        ▼
for each childGO in Parent.GetChildren():
    childRoot = childGO.GetRootComponent()
    if childRoot && newRoot:
        childRoot.AttachToComponent(newRoot, KeepWorld)
    else:
        DissolveGOParentEdge(childGO)   // 无 SC → 解除
        │
        ▼
Parent 的 m_Children / 子 m_Parent 与附着结果一致
```

### 5.4 子更换 Root

```text
Child 选出 newChildRoot
        │
        ▼
if Child.GetParent() 有父且父有 Root:
    newChildRoot.AttachToComponent(parentRoot, KeepWorld)
    // m_Parent 不变
else:
    // 无父：newChildRoot 为世界根；或按策略 Detach
```

### 5.5 序列化往返

- 存盘：GO `m_Parent`（GUID）+ 各 SC `m_AttachParent`（GUID）。
- 加载：先还原对象与指针，再 **以契约校验/重建**：对每个非根 GO，确保 `childRoot.AttachParent == parentRoot`（不一致则以 GO 边为准重挂，或报错；实现切片选定一种，推荐 **以 GO 边为作者意图、强制重绑 Root**）。
- Resolve 顺序与 `FinalizeLoadedScene` 对齐，避免双重 Attach 打乱 local。

---

## 6) API 与不变量

### 6.1 对外（建议）

```text
GameObject::AttachToParent(GameObject* parent, AttachmentTransformRules rules) -> bool
GameObject::DetachFromParent(AttachmentTransformRules rules)
GameObject::GetParent() / GetChildren()     // 投影
GameObject::GetRootComponent()
```

编辑器继续走 `SubmitReparentGameObject`；`kSceneRootParentId` = Detach。

### 6.2 不变量

1. `GetParent() == P`（P ≠ null）⇒ 双方有 Root，且 `GetRoot()->GetAttachParent() == P->GetRoot()`。
2. `GetParent() == null` ⇒ Root 的 attach parent **不是**「某一 GO 的 Root 作为 GO 父子意义上的父」（允许 Root 挂在别的非 GO-父子用途的 SC 上——若 MVP 过宽，可先规定根 GO 的 Root 无 attach parent）。
3. 禁止长期违反 1 的中间态；变更经单一入口。
4. 子 SC 树（Mesh 等挂在自己 Root 下）随 Root 世界矩阵一起走，不必复制 GO 边。

### 6.3 单一写入口

- **建立/解除 GO 父子：** 只允许 `AttachToParent` / `DetachFromParent`（及加载 Resolve 的等价路径）。
- **更换 Root：** 只允许引擎 Root 晋升/指定 API，内部完成 §5.3 / §5.4。

---

## 7) 备选方案

| 选项 | 说明 | 结论 |
|------|------|------|
| A. GO 父子 = Root↔Root（本文） | 一条变换链；近 UE | **选用** |
| B. GO / SC 两套平行树 | 易不同步；父带动子要写两套 | 弃用 |
| C. 仅 SC 树，不存 GO `m_Parent` | Hierarchy/级联删要扫组件 | 不选；保留 GO 投影 |

---

## 8) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| KeepWorld + 非均匀 Scale 分解误差 | 观感拉伸 | MVP 均匀 Scale 验收；债可另开 |
| Create Empty 无 Root | 无法挂接 | Create Empty 默认加 SceneComponent |
| 加载时 GO 边与 SC 边不一致 | 错父/错 local | Resolve 强制按契约重绑并打日志 |
| Root 删除瞬间悬挂 | 崩溃/错挂 | 先晋升新 Root 再毁旧；子重挂 |

---

## 9) 切片建议

| ID | 内容 |
|----|------|
| `CORE-F09-S00` | 契约落地：不变量断言/校验；AttachToParent 单一路径文档化 |
| `CORE-F09-S01` | Create Empty 默认 Root；无 Root 则拒绝 Attach / 拆边 |
| `CORE-F09-S02` | 父/子换 Root 重挂；单测 |
| `CORE-F09-S03` | KeepWorld 改父 + 父带动子 目视/单测；加载 Resolve |

---

## 10) 验收标准

- [x] 文档与代码同意：GO 父子 ≡ 子 Root → 父 Root
- [x] KeepWorld 改父后，子世界位姿（在精度内）不变；Inspector local 可变化
- [x] KeepRelative 改父后 local 数值不变，世界位姿随新父变化（API/`AttachToComponent`；Hierarchy 默认 KeepWorld）
- [x] 移动父 Root Transform，子 GO 渲染位置跟随
- [x] 父换 Root / 子换 Root 后 GO 边保持且附着指向新 Root（或按规则拆边）
- [x] 无 SC 无法维持 GO 父子
- [x] 非 Root 的 SC 附着不产生 Hierarchy GO 边
- [x] `gameobject-hierarchy`（及新增用例）通过；Editor 目视通过

## 11) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done** |
| Blocked by | — |
| Next | —（父子双 Dynamic 刚体语义另开物理 Feature；本 Feature 不覆盖） |
| Joint | 与 CORE-F08 / ED-F05 一并收口变换/Hierarchy |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | Draft：曾写「平行层级」；由 ED-F05 验收问题驱动 |
| 2026-09-05 | 补充父带动子需求 |
| 2026-09-05 | **拍板：** GO 父子 = Root↔Root；重写理解/Attach 瞬间/传播数据流；Status→Planned |
| 2026-09-05 | 实现：Attach 强制双 Root；SetRoot 重绑；Create Empty 加 Root；KeepWorld/父带动子单测；Status→In Progress |
| 2026-09-05 | 修渲染：Proxy 用 `GetWorldTransform`；脏标记下传附着子 |
| 2026-09-05 | 物理/DebugDraw/Gizmo/拾取改 world；`NotifyLocalTransformChanged` + `SetWorldTransform*` |
| 2026-09-05 | 目视通过；Status→**Done** |