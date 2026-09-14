# ED-F13 — EditorCommand-first（去 Select/Apply 套壳）— Design Spec

## Meta
- **ID:** `ED-F13`
- **Type:** Refactor
- **Status:** Planned（Design 首稿；待审批后开实现）
- **Owner:** project maintainer
- **Last updated:** 2026-09-14
- **Branch:** `feat/editor`
- **Depends on:**
  - `ED-F12` Done（`EditorCommand` / `DebugCommand` 命名与目录）
  - `ED-F11` W0–W2（per-Session `EditorCommandStack`）
- **Related:**
  - [ED-F12 Agent Edit Protocol](./ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md)
  - [EDITOR_COMMAND_HISTORY](./EDITOR_COMMAND_HISTORY.md)
  - 后续：[ED-F14 命令面完备](./ED-F14_EDITOR_COMMAND_COVERAGE_DESIGN.md)
- **Blocks:** ED-F14（形态稳定后再扩命令面）
- **Philosophy:** Mechanism over Policy；Agent-Friendly（GUI / Debug / MCP 同一套 EditorCommand）；Prefer Simplicity（删中间层，不另造「CommandService」）

## TL;DR

当前 Scene 路径是 **Command 套 Editor 的 Apply\***，甚至还要先 `SelectGameObject` 再改对象。  
本 Feature 做 **真重构**：变异逻辑迁入 `Editor*Command`（直接调 Scene/GO/序列化等能力），**删除** `SceneEditor::Apply*` 中间方法；Editor 只保留会话 UI、选中态、Dirty 桥接与「组 Command 入栈」。

---

## Pre-flight（2026-09-14）

| 项 | 结论 |
|----|------|
| 扫描 | `Commands/Scene/Editor*Command` 全部依赖 `SceneEditor&` + `Apply*`；`ApplyAddComponentToSelectedGameObject` 绑选中态；Material / AnimGraph **尚无** EditorCommand 编辑路径 |
| 前置 | **sound**（栈 / 命名 / 目录已在 F12） |
| 债风险 | **medium** — Apply 体量大（尤其 snapshot restore / property set）；一次搬完易炸；需按命令切片 |
| WIP | ED-F11 W3 缓；不挡本重构 |
| Philosophy | 不把业务类型塞进 Core；Command 调 **对象机制**，不是再包一层 Editor Policy |
| 建议 | **Go with scope cut** — **本期只 Scene + 现有 `Editor*Command`**；Material/AnimGraph 的「尚无 Command」记入 F14，不在本 Feature 造半套 |
| 真伪 | **true refactor**（删 Apply 契约，禁止带迁移 shim） |

---

## Scope

### In
- 盘点并消除 **Scene** 上「Command → Editor Apply\* → 对象」反模式
- `Editor*Command` 携带 **显式目标**（GO id / Component GUID / 路径等），**禁止**依赖「当前选中」完成变异
- 从 `SceneEditor` **删除** 仅服务 Command 的 `Apply*`；必要辅助迁到 Command 侧或小型无 UI helper
- 收敛 `Submit*`：保留为「UI 组装 Command 的薄入口」或下沉到调用方；**不得**再含变异逻辑
- 回归：`minEngineTests` `command-system` + 手测 Scene undo（add/remove GO/Comp、rename、reparent、transform、property）

### Out
- 新增 Debug 动词（`edit` / `verify` / `invoke`）→ **ED-F14**
- Material / AnimGraph 首批 EditorCommand → **ED-F14**
- 查询侧对象化（GetProperty 也做成 EditorCommand）→ 另议 / F14 可选
- 拆 Editor 静态库、改 UI 文件夹名
- Viewport 相机 `ApplyLook*` / Appearance `ApplyTheme*`（非文档变异，**不是**本反模式）

---

## Reader quick start

1. 本文件：目标结构 + 反模式清单 + 删除列表
2. 实现时按 Wave 迁命令；每迁完一个删对应 `Apply*`
3. 代码入口：`Editor/src/Commands/Scene/`、`SubEditor/Scene/SceneEditor.*`

---

## 1) 背景与目标

### 1.1 Pain

- **双源真相：** 真正改 Scene 的代码在 `SceneEditor::Apply*`，Command 只是转发 → undo/redo、Debug、测试难以只测 Command。
- **选中态耦合：** `EditorAddComponentCommand` 先 `SelectGameObject` 再 `ApplyAddComponentToSelectedGameObject` → Agent/无 UI 路径必须伪造选中。
- **Editor 过重：** SceneEditor 同时管布局、选中、序列化恢复、属性写入、组件命名策略……

### 1.2 成功长什么样

```text
UI / DebugCommand / MCP / Tests
        │  构造（显式参数）
        ▼
  Editor*Command::Execute/Undo
        │  直接调用
        ▼
  Scene / GameObject / Component / Serializer / …
        │
        └──► Session Dirty（经明确桥接，非「靠 Select 副作用」）
```

`SceneEditor`：**选中、视口、面板、打开/保存编排、（可选）Submit 工厂**。  
不含「为了给 Command 用而存在的 Apply\*」。

---

## 2) 现状（反模式扫描 · 2026-09-14）

### 2.1 类型 A — Command 经 Editor Apply\* 转发（核心债）

| EditorCommand | 依赖的 SceneEditor API | 严重度 |
|---------------|------------------------|--------|
| `EditorAddComponentCommand` | `SelectGameObject` + `ApplyAddComponentToSelectedGameObject` / `ApplyRemoveComponentFromGO` | **高**（选中耦合） |
| `EditorRemoveComponentCommand` | `ApplyRemoveComponentByGuid` / `ApplyRestoreComponentFromSnapshot` | 高 |
| `EditorAddEmptyGameObjectCommand` | `ApplyAddEmptyGOToScene` / `ApplyRemoveGameObjectFromScene` | 中 |
| `EditorDeleteGameObjectCommand` | `ApplyRemove…` / `ApplyRestoreGameObjectFromSnapshot` + `SelectGameObject` | 高 |
| `EditorRenameGameObjectCommand` | `ApplyRenameGameObject` | 中 |
| `EditorRenameComponentCommand` | `ApplyRenameComponent` | 中 |
| `EditorMoveComponentCommand` | `ApplyMoveComponent` | 中 |
| `EditorReparentGameObjectCommand` | `ApplyReparentGameObject` | 中 |
| `EditorSetGameObjectTransformCommand` | `ApplyGameObjectTransform` | 中 |
| `EditorSetObjectPropertyCommand` | `ApplySetObjectProperty` | 高（反射写入体量大） |

以上 Command **均持有** `SceneEditor&`，而非 `Scene*` / 显式服务。

### 2.2 类型 B — Editor 上成对的 Submit\* + Apply\*

`SceneEditor.h` 模式：`SubmitX` 只 `CommandStack.Execute(new EditorXCommand(*this,…))`，`ApplyX` 才是真逻辑。  
→ Submit 可保留为薄工厂；**Apply 为目标删除集**。

公开 Apply 面（应迁出后删除）：

- `ApplyRenameGameObject` / `ApplyRenameComponent` / `ApplyMoveComponent`
- `ApplyReparentGameObject` / `ApplyGameObjectTransform`
- `ApplyAddComponentToSelectedGameObject` / `ApplyRemoveComponentFromGO` / `ApplyRemoveComponentByGuid`
- `ApplyAddEmptyGOToScene` / `ApplyRemoveGameObjectFromScene`
- `ApplySetObjectProperty`
- `ApplyRestoreGameObjectFromSnapshot` / `ApplyRestoreComponentFromSnapshot`

**保留（非本反模式）：** `ApplyDefaultLayout`（壳布局）。

### 2.3 类型 C — 选中态当作执行参数

- `ApplyAddComponentToSelectedGameObject`：**目标来自选中**，不是参数。
- 多处 Command / restore 结束后 `SelectGameObject`（允许作为 **UI 副作用策略**，但不得作为 **变异前置条件**）。

### 2.4 类型 D — 其它 SubEditor（本 Feature 只记账）

| 区域 | 现状 | F13 动作 |
|------|------|----------|
| MaterialEditor | 直接改 Session 材质；**无** `EditorCommand` 入栈 | 记入 **F14** |
| AnimGraph + `AnimGraphSmBridge::ApplyEditEvents` | 图编辑事件直接改资产；**无** Session 栈 | 记入 **F14**（注意：此处 `Apply*` 是桥接名，≠ Scene Apply 套壳，但同属「未走 EditorCommand」） |
| ContentBrowser `AssetTreeModel::Apply*Change` | 注册表 UI 模型同步 | **不在范围** |

### 2.5 非反模式（勿误删）

- Viewport `ApplyLookFromMouse` / `ApplyMovementFromCommands`：视口相机，不是文档 undo 单元。
- Appearance / Console `Apply*`：主题与输入控件。
- DebugCommand 校验里的 `ApplyInvalidValue`：局部 helper 命名。

---

## 3) 方案

### 3.1 目标职责

| 层 | 职责 |
|----|------|
| **EditorCommand** | 变异 + undo 数据；解析目标对象；调用引擎/资产 API；通知 Dirty |
| **SceneEditor** | 选中、Hierarchy/Inspector 编排、视口、Save/Load、可选 `Submit*` |
| **共享 helper（可选）** | 无 UI：如 `SceneEditSnapshots`（serialize/restore）、默认组件命名；**禁止**依赖 `SceneEditor` 选中态 |
| **前端** | GUI / Debug / MCP 只组 Command |

### 3.2 契约

1. **Execute/Undo 不得调用 `Select*` 作为前置**（选中只可在 Execute 成功后的可选 UX 钩子，且应由 Editor 订阅而非 Command 强绑）。
2. **Command 构造参数必须自洽**（owner id、type name、before/after…）；禁止读「当前选中」完成核心路径。
3. **优先** `GameObject::AddComponent` / `RemoveComponent` / `Rename` / `AttachToParent` / Transform setter / 反射 Serialize 等已有机制。
4. Dirty：`MarkSceneDirty` 可先经 **显式回调 / `ISceneEditSink`**，避免 Command 长期抓 `SceneEditor&`；过渡期允许 `SceneDocument*` + sink，**终态删掉对 SceneEditor 的变异依赖**。
5. **Forward-only：** 不保留 `Apply*` 弃用包装。

### 3.3 推荐迁移顺序（Wave）

| Wave | 内容 | 验收 |
|------|------|------|
| **W0** | 本设计审批；在 `SceneEditor.h` 给 Apply\* 标「F13 删除」注释清单 | 文档对齐 |
| **W1** | 试点：`EditorAddComponentCommand` / Remove 对称路径；删 `ApplyAddComponentToSelected*` | 测试 + 手测 Add/Remove Comp undo |
| **W2** | Rename / Move / Reparent / Transform | 同上 |
| **W3** | Add/Delete GO + Snapshot restore 进 helper/Command | Delete/undo 恢复 |
| **W4** | `EditorSetObjectPropertyCommand`；删 `ApplySetObjectProperty` | property undo |
| **W5** | 清扫：`SceneEditor` 不再被 Command 用于变异；收紧或内联 `Submit*`；Progress + 勾验收 | grep 无 `ApplyRename` 等 |

### 3.4 删除列表（完成时 grep 应为 0）

```text
SceneEditor::ApplyRenameGameObject
SceneEditor::ApplyRenameComponent
SceneEditor::ApplyMoveComponent
SceneEditor::ApplyReparentGameObject
SceneEditor::ApplyGameObjectTransform
SceneEditor::ApplyAddComponentToSelectedGameObject
SceneEditor::ApplyRemoveComponentFromGO
SceneEditor::ApplyRemoveComponentByGuid
SceneEditor::ApplyAddEmptyGOToScene
SceneEditor::ApplyRemoveGameObjectFromScene
SceneEditor::ApplySetObjectProperty
SceneEditor::ApplyRestoreGameObjectFromSnapshot
SceneEditor::ApplyRestoreComponentFromSnapshot
```

以及 Command 内「先 Select 再 Apply」模式。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 逻辑进 Command，删 Apply\* | 单路径；Agent/测试友好；Editor 变薄 | 搬迁工作量 | **选用** |
| B. 保留 Apply\*，Command 永远转发 | 改动小 | 债固化；选中耦合难消 | 否 |
| C. 抽 `SceneMutationService` 再给 Command/Editor 共用 | 看似干净 | 多一层与 Apply 同构的中间类型 | 否（helper 可以有，但不要第三套「服务门面」） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Snapshot / 属性写入搬迁回归 | 删 GO/Comp undo 坏 | W3/W4 单测 + 手测；先搬 helper 再删 Apply |
| Dirty / 选中 UX 变化 | 用户以为「没选中」 | Dirty 显式；选中改为可选后置钩子 |
| Command 仍抓 SceneEditor | 半吊子重构 | W5 DoD：Command 头文件不再 `#include SceneEditor.h`（或仅前向声明 sink） |
| 范围膨胀到 Material | 拖死 | 硬切 Out → F14 |

---

## 6) 验收标准

- [ ] §2.1 表中所有 Scene `Editor*Command` 不再调用任何 `SceneEditor::Apply*`
- [ ] §3.4 删除列表在代码中不存在
- [ ] 无「Select 作为变异前置」；Add Component 仅凭 owner id
- [ ] `minEngineTests.exe test command-system` PASS
- [ ] 手测：Add/Remove GO、Add/Remove Comp、Rename、Reparent、Transform、Property 的 undo/redo
- [ ] `PROGRESS_LOG` + Registry Status → Done；ACTIVE_WORK 指向 F14

---

## 7) Status note

（审批前保持 Planned。）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 首稿：反模式扫描 + Wave + 删除列表；Scene 限定 |
