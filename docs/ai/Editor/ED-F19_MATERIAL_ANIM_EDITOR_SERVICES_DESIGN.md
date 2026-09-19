# ED-F19 — Material / Anim EditorServices — Design Spec

## Meta
- **ID:** `ED-F19`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-19
- **Branch:** `feat/mcp`
- **Depends on:** `ED-F18` Done（Scene 原语模型）；`MCP-F04` Done（kind / 粗 Tools / 打开资产）；`MAT-F01`–`F06` Done（材质运行时，含 `.memi`）
- **Related:**
  - [Implementation](./ED-F19_MATERIAL_ANIM_EDITOR_SERVICES_IMPLEMENTATION.md)
  - [ED-F18](./ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md)
  - [MCP-F04](./MCP/MCP-F04_AUTOMATION_DOMAIN_EXPANSION_DESIGN.md)
  - [ENGINE_DESIGN_PHILOSOPHY.md](../ENGINE_DESIGN_PHILOSOPHY.md) §5 Editor 域服务与 Client
- **Blocks:** 无

## TL;DR

Agent 已能创建并打开 Material / AnimationGraph，但不能改图。本 Feature 加两个 Editor 原语服务，Automation 用已有 `editor_edit` / `editor_query` 调用，**不新增 kind、不新增粗 Tool**。材质图包现有 Command。动画图的状态/过渡今天是直接改数据，先补 Command，服务只入栈，不调用无撤销的 `AddStateAt`。

## Scope
- **In:**
  - `MaterialEditorServices`：加/删节点、连/断引脚、属性 blob、保存、编译
  - `AnimationGraphEditorServices`：状态与过渡的撤销原语、属性 blob、保存
  - Surface：`material.*` / `animgraph.*`；图的 JSON 只在 Surface 拼
  - GUI `Submit*` 改为薄壳（材质现有路径；动画在 Command 落地后）
- **Out:**
  - 新 MCP Tool 或新 kind
  - `list_material_node_types` / `list_states` 之类的 Client 语义 API（节点类用已有 `TypeQueryServices`；图清单由 Surface 走指针自己拼）
  - Blend Tree、嵌套状态机（`ANIM-F04` / `F05`）
  - 专用 MIC 编辑器、Instance Parent 链
  - 画布拖拽的逐帧提交（与 Gizmo 相同：松手一次 before/after，本波若做也是专用 Command，不走会抓错 before 的 `SetProperty`）
  - Create Prefab、Play、Console

## Reader quick start
1. §3.1 两个服务、一种调用
2. §3.2 材质（已有 Command）
3. §3.3 动画（先补 Command）
4. §3.4 Surface op
5. Implementation 切片

---

## Pre-flight

| 项 | 结论 |
|----|------|
| 性质 | 编辑器原语 + Surface 适配。材质是薄封装；动画含真实补命令 |
| Prerequisites | 文档 TypeId：`Material`、`AnimationGraph`。打开/聚焦已有 `asset.open` |
| 现状缺口 | 材质：`EditorAdd/Remove/Connect/Disconnect*Command` 已在。动画：`AddStateAt` 等直接改图，只有属性走 `EditorSetObjectPropertyCommand` |
| Go | 服务入**指定 session** 的 Command 栈。禁止为「Agent 要一棵图 JSON」在服务里做 `GetGraph` |

---

## 1) 背景与目标

`asset.create` / `asset.open` 之后，人能看到材质或动画编辑器，Agent 仍不能加节点、连线、加状态。成功时：同一套原语给 GUI 和 Automation；撤销在该文档的栈上；打开仍是 `control`，改图是 `edit`。

## 2) 现状

| 域 | 写路径 | 撤销 |
|----|--------|------|
| Material 图 | `SubmitAddNode` / `Remove` / `ConnectPins` / `DisconnectInput` / `SubmitSetObjectProperty` | 已是 Command |
| Material 编译 / 保存 | `CompileActiveMaterial` / `SaveActiveMaterial` | 编译不进撤销 |
| Anim 图 | `AddStateAt`、`RemoveStateByName`、`RenameState`、`AddTransition`、`AddAnyStateTransition`、`RemoveTransition*`、`SetDefaultStateName` | **无** Command |
| Anim 属性 | `SubmitSetObjectProperty` | 有 |
| Automation | 无 `material.*` / `animgraph.*` | — |

`SceneEditorServices::SaveDocument` 拒绝非 Scene session。材质/动画保存必须在各自服务里。

## 3) 方案

### 3.1 边界

```text
GUI / Debug / Surface
  → MaterialEditorServices | AnimationGraphEditorServices
  → 该 EditorDocumentSession 的 Command 栈
  → 现有 Apply*（执行体留在 SubEditor）
```

- 服务返回 `Result` + out（新建节点 Guid、状态名）。不返回拼好的图 JSON。
- 图遍历、节点类过滤、显示名留在 Client。节点类目录用已有 `TypeQueryServices`。
- 不在每次 Edit 后 `activate_session`。要给人看，Client 再发 `control`。
- Editor 持有两份服务，经 `IEditorContext` 取出。不挂进 `SceneEditorServices`。

### 3.2 MaterialEditorServices

会话 TypeId 必须是 `Material`。入栈用 session 栈，不用「当前 Active」的隐式栈。

| 原语 | 行为 |
|------|------|
| `GetMaterial(session)` | out `Material*`。失败即停 |
| `AddNode` | 类名 + 编辑器坐标。out 新节点 Guid。Command = 现有 `EditorAddMaterialNodeCommand` |
| `RemoveNode` | 节点 Guid |
| `ConnectPins` | from/to Guid + 引脚下标 |
| `DisconnectInput` | to Guid + input 下标 |
| `CommitPropertyBlobs` | 与 Scene 相同：已算好的 before/after。不在服务里解析字面量 |
| `SaveDocument` | 调现有保存。无新撤销单元 |
| `Compile` | 调现有编译。`undo: false` |

`AddNode` 不解释「加一个纹理采样」。类名由 Client 给出。

### 3.3 AnimationGraphEditorServices

会话 TypeId 必须是 `AnimationGraph`。

**先补 Command，再暴露服务。** 执行体可以仍是今天的 `AddStateAt` / `RemoveStateByName` 等，但服务和 GUI 都只 `Execute` Command。禁止服务直接调用这些无撤销函数。

| 原语 | Command 需记住的足以撤销的数据 |
|------|--------------------------------|
| `AddState` | 坐标；out 状态名。Undo = 按名删除 |
| `RemoveState` | 状态名 + 删除前快照（含挂在上面的过渡，实施时以能还原为准） |
| `RenameState` | old/new 名 |
| `AddTransition` / `AddAnyStateTransition` | 端点名。out 下标或稳定 id（今天是下标；服务 out 下标即可，不发明新 id） |
| `RemoveTransition` / `RemoveAnyStateTransition` | 下标 + 还原所需端点 |
| `SetDefaultState` | old/new 默认名 |
| `CommitPropertyBlobs` | 同材质 |
| `SaveDocument` | 现有保存 |

`ReverseTransition` 本波不做：它依赖「当前选中边」，是 GUI 选择语义，不是原语。Client 用删+加表达。

位置拖拽：与 Gizmo 相同，本波不迁。若以后要，是 `SetStatePosition(before, after)` 专用 Command，不是松手时的 `SetProperty`。

### 3.4 Surface（无新 kind）

`editor_edit` / `editor_query` 已能转发任意 `domain.action`。只在 Host `CapabilityRegistry` 登记。

| op | kind | session | undo |
|----|------|---------|------|
| `material.get_graph` | query | Material | — |
| `material.add_node` | edit | Material | 是 |
| `material.remove_node` | edit | | 是 |
| `material.connect_pins` | edit | | 是 |
| `material.disconnect_input` | edit | | 是 |
| `material.set_property` | edit | | 是 |
| `material.save` | edit | | 否 |
| `material.compile` | edit | | 否 |
| `animgraph.get_graph` | query | AnimationGraph | — |
| `animgraph.add_state` | edit | | 是 |
| `animgraph.remove_state` | edit | | 是 |
| `animgraph.rename_state` | edit | | 是 |
| `animgraph.add_transition` | edit | | 是 |
| `animgraph.add_any_state_transition` | edit | | 是 |
| `animgraph.remove_transition` | edit | | 是 |
| `animgraph.remove_any_state_transition` | edit | | 是 |
| `animgraph.set_default_state` | edit | | 是 |
| `animgraph.set_property` | edit | | 是 |
| `animgraph.save` | edit | | 否 |

`get_graph` **只在 Surface**：`GetMaterial` / `GetGraph` 拿指针，自己列节点、引脚、状态、过渡。服务不提供该 op 的对应方法。

`set_property` 的 payload 与 Scene 相同（owner Guid、类名、path、before/after blob 或既有 JSON 值路径——与 Scene `set_property` 对齐，不另造一套字面量协议）。实施时复用 `ScenePropertyValueCodec` 若 owner 是 `MEObject`；对不上就停，不要在服务里为材质单开 parser。

选中节点/状态：本波不做 `material.select_node`。给人看用已有 `editor.activate_session`。

### 3.5 GUI

材质 `SubmitAddNode` 等改为调服务。`Apply*` 留在 `MaterialEditor`，Command 仍调它们。

动画 GUI 在对应 Command 存在后改调服务。在那之前不要让服务走另一条无撤销路径。

## 4) 备选

| 选项 | 结论 |
|------|------|
| 一个 `GraphEditorServices` 吃两种图 | 否定。节点/引脚与状态/过渡不是同一原语 |
| 动画服务直接调 `AddStateAt` | 否定。与 ED-F18「写必须进撤销栈」不一致 |
| 服务返回整图 JSON | 否定。那是 Surface |
| 新 kind `graph` | 否定。效果仍是 `edit` / `query` |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 删状态时过渡快照不全，Undo 丢边 | S03 先写往返测试再暴露 op |
| `get_graph` 被做成服务 API | 评审门禁：服务头文件无 GetGraph/GetHierarchy |
| 材质节点类名只有 GUI 知道 | Client 用 `TypeQueryServices`；不对就失败，不在服务里做短名糖（短名糖若要，放 Surface，与 Scene `add_component` 同级） |
| 编译失败被当成 op 失败却没有诊断 | `Compile` 的 Result.Message 带现有日志/诊断字符串，不新做诊断框架 |

## 6) 验收

- [x] 材质：Automation 加节点、连线、改属性、撤销，与 GUI 同一栈（同一 Command；未做运行时往返）
- [x] 动画：加状态、加过渡走 Command 快照；撤销还原逻辑在 Command 内。无独立单测（Editor 不是测试库）
- [x] 服务头文件没有整图 JSON / `GetGraph`（文档指针是 `GetMaterial` / `GetAnimationGraph`）
- [x] 无新 MCP Tool、无新 kind
- [x] `material.save` / `animgraph.save` 不走 Scene 的 SaveDocument
- [x] Editor 编译通过（`cmake --build build --target Editor`）

## 7) Status note

| 字段 | 内容 |
|------|------|
| Reason | S01–S05 已落地 |
| Impact | Host `material.*` / `animgraph.*`；GUI 拓扑改走服务 |
| Decision | 两服务；动画先补 Command；图 JSON 只在 Surface |
| Next | 无。余量见 ACTIVE_WORK |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-19 | 初稿。Status Review |
| 2026-09-19 | 实现收口。Status Done。动画撤销无独立单测 |
