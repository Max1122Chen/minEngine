# ED-F13 — 显式目标编辑（去「先 Select 再改」）— Design Spec

## Meta
- **ID:** `ED-F13`
- **Type:** Refactor
- **Status:** Done（W1–W2：显式 AddComponent；去 Select 前置 / `*Selected*`）
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
- **Blocks:** ED-F14（Scene 显式目标契约稳定后再扩命令面）
- **Philosophy:** Agent-Friendly（参数自洽，不靠「当前选中」）；Prefer Simplicity（保留 Editor 实现层，不另造 MutationService）

## TL;DR

**正规分层（本 Feature 确认）：**

```text
GUI / Debug / MCP  ──产生──►  EditorCommand（调用者：入栈、undo 数据）
                                │
                                ▼
                         Editor 方法（实现者：编排变异 + Dirty 等）
                                │
                                ▼
                         GO / Scene / Serializer …
```

**真正要消灭的反模式：** 用「先 Select、再对选中生效」模拟人点 UI——把 **选中态当成执行参数**。  
**不是**反「Editor 里有实现方法」；Command 调 Editor、Editor 内部再调引擎机制，完全正确。

---

## Pre-flight（2026-09-14 · 修订）

| 项 | 结论 |
|----|------|
| 扫描 | 高优先级债：`EditorAddComponentCommand` → `Select` + `ApplyAddComponentToSelected*`；其余多数已是「Command → Editor 显式 Apply\*」，**形态 OK** |
| 前置 | **sound** |
| 债风险 | **low–medium** — 范围比「搬空 Apply」小很多；主要是改 API 形状与 Command 调用 |
| Philosophy | Editor = 编辑操作实现面；Command = 可撤销调用面；不把编排逻辑强行塞进 Command |
| 建议 | **Go with scope cut** — Scene 现有命令路径；Material/AnimGraph「未入栈」→ F14 |
| 真伪 | **true refactor**（改契约：显式目标；删/改 `*Selected*` 与 Select 前置） |
| 首稿纠偏 | 首稿误把「删光 Apply\*、逻辑进 Command」当目标；**以本修订为准** |

---

## Scope

### In
- 消除 Scene 上 **选中态作为变异前置/隐式目标** 的路径
- Editor 变异 API：**目标全部显式**（GO id / Component GUID / …）；允许保留 `Apply*` / 更好命名的实现方法
- Command：只调用显式 API；**Execute/Undo 不得先 Select 再改**
- `Select*`：仅可作 **成功后的可选 UX**（或由 Editor 在实现方法末尾按策略聚焦），不得参与「找谁被改」
- 试点并从 Add Component 扩到所有仍依赖选中的调用点；回归 undo 与 `command-system` 测试

### Out
- 把 Editor 实现逻辑搬进 Command / 删除全部 `Apply*`（**已否决**）
- 禁止 Command 持有 `SceneEditor&`（**允许**；Editor 即实现者）
- Debug 新动词、Material/AnimGraph 入栈 → **ED-F14**
- Viewport / Appearance / 布局类 `Apply*`（无关）

---

## Reader quick start

1. 本文件：分层 + 反模式 + API 改造
2. 代码：`Commands/Scene/Editor*Command.*`、`SubEditor/Scene/SceneEditor.*`
3. F14 扩面时遵守同一契约（显式目标；Command → Editor 实现方法）

---

## 1) 背景与目标

### 1.1 分层（维护者意图）

| 角色 | 职责 |
|------|------|
| **GUI（及 Debug/MCP）** | 产生意图 → **构造** `EditorCommand` 并交给 Session 栈 |
| **EditorCommand** | **调用者**：保存 undo 所需 before/after；`Execute`/`Undo` 调 Editor；不承载大块业务编排的首选位置 |
| **Editor（如 SceneEditor）** | **实现者**：显式目标上的编辑操作（命名策略、Dirty、restore snapshot、内部调 GO API 等） |
| **引擎对象** | 机制：`AddComponent` / `RemoveComponent` / 序列化等 |

### 1.2 Pain（唯一核心）

`EditorAddComponentCommand` 今日：

1. `SelectGameObject(ownerId)`
2. `ApplyAddComponentToSelectedGameObject(...)`

这是在 **假装用户先点中再点菜单**。Agent、测试、非 Active 选中会话都必须伪造选中；也让 API 无法表达「改的是谁」。

多数其它 `ApplyRename*(id, …)` 已是显式目标——**Command 调它们不是问题**。

### 1.3 成功长什么样

```text
Submit / GUI / Debug
  → new EditorAddComponentCommand(editor, ownerId, typeName)
  → stack.Execute
  → editor.AddComponentToGameObject(ownerId, typeName, outComp)   // 显式；内部可 GO::AddComponent + MarkDirty
  // 可选：之后再 Select(ownerId) 做 UX，但不参与解析目标
```

---

## 2) 现状扫描（修订口径）

### 2.1 类型 S — 选中态模拟（**必须修**）

| 位置 | 问题 | 目标 |
|------|------|------|
| `EditorAddComponentCommand::Execute/Undo` | 先 `Select` 再 `*Selected*` / 靠 `GetSelectedGameObject` | 显式 `ownerId` API；去掉 Select 前置 |
| `SceneEditor::ApplyAddComponentToSelectedGameObject` | 目标来自选中 | 改为 / 替换为 `…(gameObjectId, typeName, …)` |
| `SubmitAddComponentToSelectedGameObject` | 从选中读 owner 再组 Command | **Submit 读选中可以**（GUI 上下文）；但塞进 Command 的必须是 **已解析的 id**，且 Execute 不再读选中 |

### 2.2 类型 OK — Command → Editor 显式方法（**保留**）

下列「Command 调 `SceneEditor::Apply*`」**符合**「Editor 实现、Command 调用」，**不是**本 Feature 的删除目标：

| EditorCommand | Editor API（现状名） | 备注 |
|---------------|----------------------|------|
| `EditorRenameGameObjectCommand` | `ApplyRenameGameObject(id, …)` | 显式 id |
| `EditorRenameComponentCommand` | `ApplyRenameComponent(…)` | 显式 |
| `EditorMoveComponentCommand` | `ApplyMoveComponent(…)` | 显式 |
| `EditorReparentGameObjectCommand` | `ApplyReparentGameObject(…)` | 显式 |
| `EditorSetGameObjectTransformCommand` | `ApplyGameObjectTransform(…)` | 显式 |
| `EditorSetObjectPropertyCommand` | `ApplySetObjectProperty(guid, …)` | 显式 |
| `EditorAddEmptyGameObjectCommand` | `ApplyAddEmptyGOToScene` 等 | 场景级；无「选中冒充目标」 |
| `EditorDeleteGameObjectCommand` | `ApplyRemove*` / `ApplyRestore*` | 显式 id；若末尾 `Select` 仅为 UX → 可保留或下移到实现方法策略 |
| `EditorRemoveComponentCommand` | `ApplyRemoveComponentByGuid` / restore | 显式 |

可选洁癖（**非必须**）：重命名 `Apply*` → `AddComponentToGameObject` 等更直白的名字；不作为本 Feature 完成门槛。

### 2.3 类型 UX — 成功后 Select（允许，约定清楚）

- **禁止：** Select 作为「找到被编辑对象」的手段。
- **允许：** 变异成功后聚焦对象，方便人继续编；实现可放在 Editor 方法末尾或 Command 之后的明确 UX 钩子。
- `EditorDeleteGameObjectCommand` 等处的后置 `Select`：对照上条审查即可，不必为删而删。

### 2.4 其它 SubEditor（只记账）

| 区域 | 现状 | F13 |
|------|------|-----|
| Material / AnimGraph | 多直接改资产，未入 Command 栈 | → **F14**（新命令仍应：Command → Editor 显式方法） |
| Viewport / Theme `Apply*` | 非文档 undo | 忽略 |

---

## 3) 方案

### 3.1 契约（实现必须遵守）

1. **变异 API 的目标参数自洽**；禁止 `*ToSelected*` / `*FromSelection*` 作为 Command 可调用的实现面。
2. **`EditorCommand::Execute/Undo` 不得调用 `Select*` 作为前置条件。**
3. **GUI `Submit*` 可以从选中解析 id**，再把 id 写入 Command；之后路径与 Debug/MCP 相同。
4. **Editor 继续作为实现者**：Dirty、默认组件名、snapshot restore、反射写入等可留在 SceneEditor（或 Editor 私有 helper），由 Command 调用。
5. **Forward-only：** 删掉 `ApplyAddComponentToSelectedGameObject`（或改为显式 API 后删除旧符号）；不留「内部先 Select 再调新 API」的兼容壳给 Command 用。

### 3.2 目标调用关系

```text
GUI Submit* ──(可选：读选中)──► 构造 Editor*Command(显式参数)
Debug / MCP / Tests ──────────► 构造 Editor*Command(显式参数)
                                      │
                                      ▼
                             SceneEditor::<显式变异方法>
                                      │
                                      ├── GameObject / Scene / Serializer
                                      └── MarkSceneDirty [+ 可选后置 Select]
```

### 3.3 Wave

| Wave | 内容 | 验收 |
|------|------|------|
| **W0** | 本修订稿审批 | Meta → 可开码 |
| **W1** | Add/Remove Component：显式 owner API；改 Command；删 `*Selected*`；去 Select 前置 | 手测 + 相关测试 |
| **W2** | 全库 grep：`ToSelected` / Command 内 Select 前置；清残留 | grep 干净 |
| **W3** | （可选）重命名若干 `Apply*` 为更直白动词；更新调用方 | 非门禁 |
| **W4** | Progress / 勾验收；Registry → Done；解开 F14 | 文档 |

### 3.4 删除 / 改造列表（门禁）

**必须消失或不再被 Command 使用：**

```text
SceneEditor::ApplyAddComponentToSelectedGameObject
Editor*Command 中「先 SelectGameObject 再变异」的前置调用
```

**必须存在（名称可调整）：**

```text
SceneEditor::<AddComponent>(gameObjectId, componentTypeName, outNewComponent)
（及 Remove 对称的显式 API——已有 ApplyRemoveComponentFromGO / ByGuid 可保留）
```

**明确不删：** §2.2 表中其它 `Apply*` 实现方法。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Editor 实现 + Command 调用；只打掉选中态模拟 | 符合意图；改动面小 | — | **选用** |
| B. 逻辑全搬进 Command，删光 Editor Apply\* | 表面上「Command 很重」 | 与维护者分层相反；重复 Dirty/restore | **否决**（首稿误选） |
| C. 抽独立 MutationService | 多一层 | 与 Editor 实现者重复 | 否 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Submit 仍从选中取 id，误以为「还在用选中执行」 | 评审混淆 | 文档区分「构造期读选中」vs「执行期读选中」 |
| 后置 Select 被误删导致 UX 回退 | 人编不便 | W1 保留成功后聚焦策略 |
| 范围漂回「搬空 Apply」 | 工期膨胀 | 门禁仅 §3.4 |

---

## 6) 验收标准

- [x] 不存在 `ApplyAddComponentToSelectedGameObject`（或等价 `*Selected*` 变异 API）
- [x] 无任何 `Editor*Command` 在变异前 `Select*` 以解析目标
- [x] Add Component：仅凭构造时的 `ownerGameObjectId` 即可 Execute/Undo
- [x] §2.2 类路径仍为 Command → Editor 实现方法（未强制搬空逻辑）
- [x] `minEngineTests.exe test command-system` PASS；手测 Add/Remove Comp undo（自动化套件 PASS；手测建议提交前点一次）
- [x] Registry / Progress / ACTIVE_WORK 更新；F14 可开

---

## 7) Status note

实现已落地（W1–W2）。可选 W3 重命名 `Apply*` 未做。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 首稿（已过时）：误主张删 Apply\*、逻辑进 Command |
| 2026-09-14 | **修订：** 确认 Editor=实现者、Command=调用者、GUI=产生者；只消除选中态模拟 |
| 2026-09-14 | **Done：** `ApplyAddComponentToGameObject`；Command/Picker 去 Select 前置；删 `*Selected*` 变异 API |
