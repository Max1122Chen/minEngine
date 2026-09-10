# ED-F05 — Hierarchy Tree & Reparent

## Meta
- **ID:** `ED-F05`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/ui`
- **Related:**
  - [CORE-F08 GameObject Hierarchy](../Platform/Core/CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md)（**硬依赖**，已 Done）
  - [CORE-F09](../Platform/Core/CORE-F09_PARALLEL_HIERARCHY_KEEPWORLD_DESIGN.md)（变换语义）
  - [UI-F01](../Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md)
  - 代码：`HierarchyWindow`、`SceneEditor` CommandStack
- **Depends on:** CORE-F08 MVP
- **Implementation:** 待写 Impl 或直接按切片开码

## TL;DR

把 Hierarchy 从平铺列表做成 **树**；拖拽改父走 CORE-F08 API + Undo。  
**拖拽交互（重点）：** 原 TreeNode 留在树内不消失；ghost 跟光标；**会话态 Sticky TargetParent**，松手才 commit Attach/Detach。  
改父默认 **KeepWorld**；MVP **不做**兄弟排序。Detach：本期接受「拖到空白」；未来对齐 Unity「拖到与父同级条带」。

## Scope

### In（MVP）
- 树：根 = `GetParent()==nullptr`；子 = `GetChildren()` 递归
- 展开/折叠：编辑器会话内即可（不必持久化）
- 拖拽改父（单选）：见 §3 交互契约
- Undo：`ReparentGameObjectCommand`（记 old/new parent）→ 现有 `CommandStack`
- 改父规则：**KeepWorldTransform**
- 环：UI 禁止拖到自身/子孙；最终仍以 CORE-F08 为准
- MarkSceneDirty；可存盘

### Out / Deferred
- 兄弟插入顺序 / sibling index API
- 多选拖拽
- 搜索过滤、可见性眼睛、Prefab 覆盖指示
- 跨 Scene 拖拽
- **Unity 式「拖到父同级条带 → Detach」**（见 §3.4 Future）
- **KeepWorld 变换传播 / GO 父子与 SceneComp 附着解耦** → 下一 Feature（`CORE-F09`），本 Feature 不改 `AttachToParent` 内 Root 自动附着

## Reader quick start
1. §3 拖拽交互（必读）  
2. §4 数据流 / Command  
3. HierarchyWindow：树 + Sticky 会话 DnD（见 §3.2）  

---

## 1) 背景与目标

CORE-F08 已有父子 API，但 Hierarchy 仍平铺，无法组织 Canvas UI 树，也无法用拖拽改父。

成功：树与 `GetParent`/`GetChildren` 一致；拖一次改父可 Undo；拖拽过程中列表不「抽空」原节点。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| `GetHierarchyGameObjects()` | 全场景 GO 按 `m_ID` 排序平铺 |
| `HierarchyWindow` | TreeNode 树 + Sticky 会话 DnD + Rename/右键 |
| Undo | Rename / AddEmpty / Delete 已走 `CommandStack` |
| CORE-F08 | Attach/Detach/GetChildren 可用 |

---

## 3) 拖拽与编辑交互（拍板）

### 3.1 原则

1. **唯一真相** = CORE-F08 父子；Hierarchy 只画、不另建 UI 树模型。  
2. **拖拽中不移除源行**：原 TreeNode（及其子树缩进结构）仍留在列表中。  
3. **ghost 表示「正在搬」**：跟光标的是虚化/半透明的 GO 名（或简条），不是把整棵子树从树里拔走。  
4. **松手才改数据**：落点判定 → `SubmitReparent` → Command 执行 → 按新父子重画树。

> 坏体验对照：拖起时源节点立刻从树消失 → 列表跳动、难对落点、Undo 前视觉已乱。

### 3.2 拖拽生命周期（编辑器会话态，拍板）

**不把 commit 绑在 ImGui `AcceptDragDropPayload` / `IsDelivery()` 上。**  
ImGui 只辅助「开始拖」手势；**权威状态在 `HierarchyWindow` 会话字段**。

会话字段（概念）：
- `DraggedGoId` — 正在拖的 GO
- `TargetParentId` — sticky 落点：`nullopt`=无；`0`=Detach 为根；其它=成为该 GO 之子
- `TargetGrace` — 命中丢失后保留 Sticky 目标的帧数（避免松手微抖清空）

```text
Press + drag threshold on row
  → 开启会话：DraggedGoId = 该 GO；清空 Target
  → 源行仍绘制（可略淡）；ghost 跟光标

每帧 Hover（会话中）
  → 鼠标命中合法 GO 行（行矩形可略膨胀）→ 更新 TargetParentId = 该 GO，重置 Grace
  → 命中面板空白（未命中任何行）→ TargetParentId = 0（Detach），重置 Grace
  → 非法行（自身/子孙）→ 不写入 Target（保持上一合法 Sticky）
  → 未命中且仍在拖 → Grace--；Grace 耗尽才清空 Target
  → 高亮画在 Sticky Target 上（不是「本帧瞬时 Accept」）

Release（鼠标左键抬起，读会话）
  → 若 Target 有效 → SubmitReparent(Dragged, Target) → Command
  → 若 Target 空 / Esc 取消 → 无 Command
  → 结束会话；树按新 GetChildren 刷新（展开按 GO Id 尽量保留）
```

> **为何：** ImGui Delivery 要求抬起帧仍命中且与上一帧同一 AcceptId，松手微抖会导致「有过高亮却不改父」。Sticky Target + 松手 commit 与常见编辑器树一致。

### 3.3 落点判定（MVP）

| 落点 | 行为 | 备注 |
|------|------|------|
| 另一 GO 的行（整行作为「成为其子」热区） | `AttachToParent(target)` | KeepWorld；拒绝环 |
| Hierarchy 面板空白 / 明确「根」投放区 | `DetachFromParent` | **本期可接受** |
| 自身或子孙 | 拒绝 | 不发 Command |
| 面板外松手 | 取消 | 同非法 |

**热区（MVP 简化）：**
- 整行 drop → 「成为该行 GO 的子」。
- **不做**「插到两行之间改兄弟序」（无 sibling API）。
- **不做**「行上半/下半 = 前/后兄弟」，避免暗示排序能力。

### 3.4 Detach：MVP vs Future（Unity 对齐）

| 阶段 | Detach 手势 |
|------|-------------|
| **MVP（本期）** | 拖到 Hierarchy **空白处**（或根投放条）→ Detach |
| **Future** | 对齐 Unity：拖到与**当前父**同级的条带/间隙 → 提升为与父同级（实质 Detach，或未来带 sibling 位）。空白 Detach 可保留为辅助 |

Future 需要更细的 drop zone，且常伴随 sibling 顺序；**不进本 Feature MVP**。

### 3.5 Ghost 视觉（编辑体验重点）

| 元素 | 行为 |
|------|------|
| **源 TreeNode** | **保留在原位**；可选略降 Alpha 或左侧「拖动中」标记，与未拖行区分 |
| **Ghost** | 跟随鼠标；半透明文字/短条；MVP **只标被拖的那一个 GO 名**，不画整棵子树 ghost |
| **目标高亮** | 合法父行底色/描边；非法目标无高亮或拒绝光标 |
| **松手后** | ghost 消失；树按新父子重布局；选中仍指向同一 GO Id |

实现约定：
- ImGui `BeginDragDropSource`（可选）仅用于拖动手势阈值；**禁止**用 `IsDelivery()` 作为改父唯一信号。
- Ghost / 目标高亮均读会话态（`DraggedGoId` / Sticky `TargetParentId`）。
- 目标高亮用编辑器同系蓝色（`HierarchySelectionBar` 等），不用 ImGui 默认黄框。

### 3.6 与选中 / Rename / 右键 / Play

- 拖拽进行中不进入 F2 Rename。  
- 右键「Unparent」可后置；MVP 不要求。  
- Play / Inspecting：与现有 Hierarchy 编辑命令同一门闩（`IsPlaying()` 策略与 Delete/Rename 对齐）。

---

## 4) 数据流与 Command

```text
HierarchyWindow 拖拽会话
  Begin: DraggedGoId
  Hover: Sticky TargetParentId (+ Grace)
  Release: commit from session
         |
SceneEditor::SubmitReparentGameObject(ctx, goId, newParentId)  // kSceneRootParentId = Detach（勿用 0）
         |
ReparentGameObjectCommand
  Undo: restore old parent (KeepWorld)
  Redo: apply new parent
         |
CORE-F08 AttachToParent / DetachFromParent
MarkSceneDirty
```

**Command 载荷：** `goId`，`oldParentId` / `newParentId`；根/Detach 用 `SceneEditor::kSceneRootParentId`（`uint64_t` max）。**禁止**用 `0` 表示根（与 GO `m_ID` 冲突）。不存裸指针。

**展开状态：** 按 `uint64_t` GO Id 记忆；改父后尽量保留。

**反模式（已弃用）：** 仅依赖 ImGui `BeginDragDropTarget` + `IsDelivery()` 在抬起帧提交。

---

## 5) 切片建议

| ID | 内容 | 依赖 |
|----|------|------|
| `ED-F05-S00` | 树形绘制（根递归）；保留选中/Rename/右键 | CORE-F08 |
| `ED-F05-S01` | DnD + ghost + 空白 Detach + Reparent Command Undo | S00 |

推荐 S00→S01 连续交付。

---

## 6) 验收标准

- [x] 树缩进与 CORE-F08 `GetChildren` 一致  
- [x] 拖拽中源节点仍在 Hierarchy；ghost 跟随光标  
- [x] 落到合法 GO → 成为其子（KeepWorld）；Undo/Redo 正确  
- [x] 落到空白/根区 → Detach；Undo 恢复原父  
- [x] 拖到自身/子孙被拒绝  
- [x] 无 sibling 排序热区/承诺  
- [x] 与 CORE-F08 联合验收后，CORE-F08 可标 Done  

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done** |
| Blocked by | — |
| Next | — |
| Joint | 与 CORE-F08 / CORE-F09 一并收口 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft |
| 2026-09-04 | **交互拍板：** KeepWorld；无兄弟序；空白 Detach（MVP）；源节点保留 + ghost；Future Unity 同级条带 Detach；Status→Planned |
| 2026-09-05 | S00+S01 代码落地：树形 Hierarchy、ghost DnD、ReparentGameObjectCommand；Status→In Progress |
| 2026-09-05 | **DnD 契约修订：** Sticky `DraggedGoId`/`TargetParentId` + Grace；松手 commit；弃用 ImGui `IsDelivery()` 作为唯一提交信号 |
| 2026-09-05 | 根哨兵改为 `kSceneRootParentId`（修复 GO id=0）；KeepWorld/SC 平行层级 defer `CORE-F09` |
| 2026-09-05 | 联合验收通过；Status→**Done** |
