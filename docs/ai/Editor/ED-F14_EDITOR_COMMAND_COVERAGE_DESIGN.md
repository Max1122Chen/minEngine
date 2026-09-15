# ED-F14 — 各 SubEditor 命令面完备 — Design Spec

## Meta
- **ID:** `ED-F14`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/editor`
- **Depends on:**
  - **`ED-F13` Done**（`c7b3f34`：显式目标；无「先 Select 再改」）
  - `ED-F12` Done（Debug / EditorCommand 协议骨架）
  - `ED-F11` Session 栈（已有；Material/AnimGraph 多 Session）
- **Related:**
  - [ED-F13 显式目标编辑](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md)
  - [ED-F12 Agent Edit Protocol](./ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md)（§0.7–0.8：`set`/`edit`、域命令）
  - Document types：`Editor/src/Shell/Document/EditorDocumentTypes.cpp`
- **Blocks:** MCP 富工具（软）；跨域 Agent 竖切完整度
- **Philosophy:** 同一套 EditorCommand；GUI / Debug / MCP 只构造；**实现在各 Editor**；按 Session `typeId` 裁剪命令发现面

## TL;DR

**目标不是「先做一刀 MVP」。**  
对三个已注册文档类型里 **今天已经能做的编辑操作**，本 Feature 要做到：**都能经 EditorCommand 进入该 Session 的 CommandStack**，Undo/Redo 有效；Debug（及日后 MCP）构造同一批命令。

### 表格图例（先读）

| 标记 | 含义 |
|------|------|
| **✅ 入栈** | 今日路径已是：GUI/Submit → `EditorCommand` → Session `CommandStack` → Editor 实现方法。Undo 菜单对该操作有效。 |
| **❌ 未入栈** | 今日路径是 **直接改内存/资产 + Dirty**（或只 Clear 栈），**没有**对应可撤销 `EditorCommand`。Undo **无效**。这是缺口，不是「可选」。 |
| **—** | 本来就不是文档变异（选中、Save、session-only 装饰、只读查询等），**不要求**入编辑栈。 |

Material / AnimGraph 表里大量 **❌**：表示 **现状债**，F14 要清掉，不是「以后有空再做」。

**命令分类（维护者口径）：** 反射字段一律走通用 **Set/Edit Property**（含 `Material.m_BlendMode`、`AnimState.Name` / Clip / Speed / EditorPos）。**不要**为每个字段包专用 Command。专用 Command 只留给 **拓扑**（加删节点/边、反向边、加删 GO/Component 等）。`AnimState` **保持 `ME_STRUCT`**，不上 `MEObject`。

---

## Pre-flight（2026-09-14 · 完备口径）

| 项 | 结论 |
|----|------|
| 扫描 | Scene 主路径多已 ✅；Material/AnimGraph 可持久化编辑几乎全 ❌ |
| 前置 | **sound**（F13 Done） |
| 债风险 | **high** — 图拓扑/事件全量入栈工作量大；但口径是完备，用 Wave **分期交付**，不是砍范围 |
| Philosophy | 完备 = 覆盖 **现有操作面**；不发明 Duplicate 等尚无的产品能力 |
| 建议 | **Go Design** → Wave 按域推进；每域 Done 条件 = 该域 §2 表中「应入栈」行全部 ✅ |
| 真伪 | Feature + 真重构（消灭直写双轨） |

---

## Scope

### In（完备定义）

对 **Scene / Material / AnimationGraph** 各自 **当前 UI/桥接已支持的、会 Dirty 资产的编辑**：

1. 改为 **GUI → EditorCommand → Editor 显式方法**（F13 契约）  
2. 进入 **该文档 Session** 的 `CommandStack`  
3. Undo/Redo 恢复可观察状态（含 Dirty 语义合理）  
4. Scene：补齐 Debug 覆盖 + `edit` / `verify`  
5. Material / AnimGraph：Debug 至少能驱动与 GUI 同类的主路径（基线或域方言）；可与 GUI 入栈同 Wave 或紧随其后，但 **不得** 以「只做 Debug、GUI 仍直写」交差  

**手势合并（仍算完备）：** 拖拽移动节点等连续输入，以「手势结束」为一个 Command（非每帧一条）——这是实现策略，不是少做操作种类。

### Out（明确不是「砍操作」）

- **尚不存在的产品能力**：Prefab、Duplicate/Paste、多选批量、数组属性 Inspector（今日 stub）  
- **故意非文档变异**：选中变更；Entry/AnyState **装饰**位移（session-only、不 dirty）  
- Save / 打开文档 / Clear 栈  
- `invoke`、MCP 产品化、查询全面对象化、PIE `set` 行为大改（默认保持，仅文档化）  
- **AnimState 升为 `MEObject`**（否决：保持 struct；Name 走 Set/Edit Property）  

---

## Reader quick start

1. 上文 **图例** + [ED-F13](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md)  
2. §2 矩阵：❌ = 必须修成 ✅  
3. §3 Wave = 交付顺序，不是「只做竖切就 Done」

---

## 1) 背景与目标

### 1.1 分层

```text
GUI / Debug / MCP  ──构造──►  EditorCommand  ──调用──►  *Editor 显式方法  ──►  资产/Scene
                                    │
                                    └── Session.CommandStack（Undo/Redo）
```

### 1.2 成功标准

| 文档类型 | Done 时 |
|----------|---------|
| Scene | 已有结构/属性编辑保持入栈；Debug 覆盖主要结构命令；`edit`/`verify` 可用 |
| Material | **今日能改的**（枚举字段、节点参数、加删节点/连线、移节点等）全部可 Undo |
| AnimationGraph | **今日能改的**持久化编辑（SmGraph 结构事件、Inspector 字段、Parameters 窗等）全部可 Undo；装饰位移仍不入栈 |

### 1.3 命令分类：字段 vs 拓扑

GUI / Debug **应直接 emit 通用命令**，不要 `EditorSetBlendModeCommand` / `RenameStateCommand`。

| 种类 | Command | 例 |
|------|---------|----|
| **字段** | `EditorSetObjectProperty` / `edit` | `Material.m_ShadingModel`、`m_BlendMode`；节点 def 参数；`AnimState.Clip` / `bLoop` / `Speed` / `EditorPos*`；**`AnimState.Name`** |
| **拓扑** | 专用 `Editor*Command` | 加删 GO/Component/节点/连线；加删 State/Transition；Reverse edge |
| **查询 / 断言** | get / `verify` | 不入 undo |

`AnimState` 已是 `ME_STRUCT`（`AnimationGraph.h`），**保持 struct，不升 `MEObject`。**  
Rename：GUI → Set/Edit Property（path 到该 state 的 `Name`）。

边与 Default 今日用 **名字当引用**。Editor 在 Apply 写 `Name` 后做与现 `RenameState` 相同的重定向；Undo 写回旧名同样重定向。这是实现层副作用，**不是**另一种 Command。

---

## 2) 现状覆盖矩阵（2026-09-14）

文档类型：仅 `"Scene"` / `"Material"` / `"AnimationGraph"`。

### 2.1 Scene

| 能力 | 入栈？ | Editor / Command | Debug 今日 | F14 |
|------|--------|------------------|------------|-----|
| Add empty GO | ✅ | `EditorAddEmptyGameObjectCommand` | ✅ `add_go` | 保持 |
| Delete GO + restore | ✅ | `EditorDeleteGameObjectCommand` | ✅ `delete_go` | 保持 |
| Add / Remove Component | ✅ | `EditorAdd/RemoveComponentCommand` | ✅ `add_comp` / `remove_comp` | 保持 |
| Rename GO | ✅ | `EditorRenameGameObjectCommand` | ✅ `rename` | 保持 |
| Rename / Move Comp | ✅ | 对应 Command | ✅ `rename_comp` / `move_comp` | 保持 |
| Reparent | ✅ | `EditorReparentGameObjectCommand` | ✅ `reparent` | 保持 |
| Transform | ✅ | `EditorSetGameObjectTransformCommand` | 仅 `set`/`edit` 字段 | 保持 |
| Set property（侵入） | ✅ | `EditorSetObjectPropertyCommand` | ✅ `set` | 保持 |
| Edit property（守政策） | GUI 有政策 | 同底层 Apply + 模式 | ✅ `edit` | 保持 |
| Get / find / list | —（查询） | | ✅ | 保持 |
| Undo / Redo | ✅ | 栈 | ✅ | 保持 |
| Duplicate / Paste / 多选 | —（能力不存在） | | | **Out** |
| 数组属性 | —（Inspector stub） | | | **Out** |
| PIE `set` | ❌ 直写不 Dirty | 特例 | 特例 | 文档化；默认不改 |

**半路径（应收敛，算 Scene 完备的一部分）：**

- 先写后 `Submit(..., applyOnFirstExecute=false)`：允许保留，但须保证 Undo 正确。  
- 嵌套 Object 缺 path 只 Dirty：修到可入栈或显式禁止编辑该路径（记 Progress；尽量本 Feature 内清）。

### 2.2 Material（❌ = 缺口，须全部变 ✅）

| 能力 | 今日路径 | 入栈？ | F14 |
|------|----------|--------|-----|
| ShadingModel / BlendMode | Inspector → `SubmitSetObjectProperty` + prune side-effect | ✅ | 保持；Undo 还原被 prune 的 Output 连线 |
| 节点参数（反射） | Inspector + 画布 DragFloat → 同 Command（手势结束入栈） | ✅ | 保持；Constant3 多字段一次手势分条 Submit |
| 加 / 删节点 | Inspector / 画布 → `SubmitAdd/RemoveNode` | ✅ | 保持；删节点快照 + inbound 连线 Undo |
| 加 / 删连线 | 画布 → `SubmitConnect/Disconnect` | ✅ | 保持；Connect 记录被替换的旧边 |
| 移动节点（落盘位置） | 拖拽结束 → `SubmitSetObjectProperty`（`m_EditorPos*`） | ✅ | 保持 |
| Save | `SaveActiveMaterial` | — | 保持非 Command |
| Preview 刷新 | `ApplySessionToPreview` | — | Editor 方法成功后 UX |
| Debug | `mat_*` → 同 Submit*；字段 `mat_set_shading`/`mat_set_blend` | ✅ | **W4 Done** |

### 2.3 AnimationGraph

| 能力 | 今日路径 | 入栈？ | F14 |
|------|----------|--------|-----|
| 加 / 删 State | `SubmitOwnedPropertyMutation(m_StateMachine)` | ✅ | 保持（整机 blob；因数组 path 不可走叶子 Set） |
| 加 / 删 Transition（含 AnyState / Entry→Default） | 同上 | ✅ | 保持 |
| Reverse Transition | 同上 | ✅ | 保持 |
| Rename State（改 `Name` 字段） | 同上 / Inspector mutation | ✅ | 保持；仍走 `RenameState` 重定向 |
| 状态节点位置（`EditorPos*`） | 拖拽结束 `m_StateMachine` blob | ✅ | 保持 |
| Inspector 改 clip/loop/speed/条件等 | Inspector → mutation / 手势 capture | ✅ | 保持 |
| Parameters 窗 | `m_Schema` blob mutation | ✅ | 保持 |
| Debug | `anim_*` → 同 `SubmitOwnedPropertyMutation` | ✅ | **W4 Done** |
| SelectionChanged | 仅会话选中 | — | 不入栈 |
| Entry/AnyState 装饰位移 | session-only，不 dirty | — | **不入栈**（保持） |
| Save | `SaveActiveGraph` | — | 保持 |

**`EditKind` 映射（持久化者全部入栈）：**

| `EditKind` | 入栈？ |
|------------|--------|
| `SelectionChanged` | — |
| `NodeMoved`（状态位） | ✅ 手势结束 → Set/Edit `EditorPos*` |
| `AddNodeRequested` | ✅ |
| `DeleteSelectionRequested`（节点/边） | ✅ |
| `CreateEdgeRequested` | ✅ |
| `DeleteEdgeRequested` | ✅ |
| `ReverseEdgeRequested` | ✅ |
| `RenameNodeRequested` | ✅ 映射为 Set/Edit `Name`（非专用 Rename Command） |

桥接改为：事件 → **构造 EditorCommand（显式参数）** → 栈 → Editor 方法。

### 2.4 Debug 动词

| 动词 | F14 |
|------|-----|
| `get` / `set` / `find` / `list_go` / `inspect` / `undo` / `redo` | 保持；Scene path 语法仍为主；Material/AnimGraph 以域动词发现 |
| **`edit`** | ✅ 守 `PropertyEditPolicy` |
| **`verify`** | ✅ 最小 `=` / `==` |
| Scene 结构糖（`add_go` / `add_comp` / …） | ✅ |
| Material 域（`mat_list_*` / `mat_add_node` / `mat_remove_node` / `mat_connect` / `mat_disconnect` / `mat_set_shading` / `mat_set_blend`） | ✅ → 同 GUI Submit* |
| AnimGraph 域（`anim_list_*` / `anim_add/remove/rename_state` / transition / param / `anim_set_default`） | ✅ → 同 `SubmitOwnedPropertyMutation` |
| `invoke` | Out / Deferred |

`edit` vs `set`：同底层 Apply + `EPropertyWriteMode::{Force, RespectPolicy}`；两 Debug 动词。人类主推 `edit`。

### 2.5 仍允许本 Feature 结束后不做的（仅「能力不存在 / 故意非变异」）

- Duplicate / Paste / 多选 / 数组 Inspector  
- Prefab、新文档类型、**AnimState→MEObject**  
- `invoke`、MCP、查询对象化  
- PIE `set` 入栈策略变更  
- Entry/AnyState 装饰位移入栈  

**不是**「Material 拓扑以后再说」——拓扑在 §2.2，属 In。

---

## 3) 方案

### 3.1 原则

1. F13 契约全程有效。  
2. **完备优先于「先竖切交差」**；Wave 只排期，不缩小 §2 In。  
3. 先 Editor 显式方法，再 Command，再改 GUI/桥接调用点，再挂 Debug。  
   **字段类 GUI 直接 `SubmitSet/EditObjectProperty`。**  
4. 禁止双轨：同一用户手势不得「有时直写、有时 Command」。  
5. 拖拽合并；Selection 不入栈。  
6. **禁止**为单个反射字段新增 `EditorSetBlendModeCommand` 一类包装。

### 3.2 目录

```text
Editor/src/Commands/Scene/            # 已有；属性类复用 Set/Edit
Editor/src/Commands/Material/         # 仅拓扑（加删节点/连线）
Editor/src/Commands/AnimationGraph/   # 仅拓扑（加删 State/Transition、Reverse）
```

跨域属性写入：复用 `EditorSetObjectPropertyCommand`（+ edit 模式），不要按域复制一份。

### 3.3 Wave（交付顺序 = 完备的分期，不是 MVP 停损）

| Wave | 内容 | 域 Done 条件 |
|------|------|----------------|
| **W0** | 审批本完备口径 | 可开码 |
| **W1** | Scene：`edit`/`verify` + 结构 Debug 全挂接；清半路径债 | §2.1 应入栈/应 Debug 行达标 |
| **W2** | Material：**全部** §2.2 ❌ → ✅（属性 + 拓扑 + 移动） | Material Undo 覆盖现有操作面 |
| **W3** | AnimGraph：**全部** §2.3 应入栈行 → ✅ | 同上 |
| **W4** | Debug 域补全；矩阵刷新；Registry Done | ✅ |

W2/W3 内部可再拆 PR，但 **Feature Done 不得停在「只做了 AddState」**。

### 3.4 `verify`

```text
verify <PropertyPath> [==] <value>
```

只读；`=` / `==` 可选；Payload 含 ok / expected / actual；不入栈。

### 3.5 Document Host

Dirty 仍由 Editor 方法触发；Undo 打在对应 Session 栈；Save 非 Command。

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| A. 现有操作面全部入栈（分期交付） | **选用** |
| B. 每域只做一刀 MVP 即标 Done | **否决**（维护者明确反对） |
| C. 只做 Debug、GUI 仍直写 | **否决** |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Material / AnimGraph 工作量大 | Wave + 多 PR；每 PR 清一类操作，但 Feature 门禁看全表 |
| 节点/边身份不稳定 | 设计稳定 id（guid/名称/索引约定）写进 Command 载荷 |
| 拖拽栈爆炸 | 手势结束合并 |
| Inspector 先写后提交 | Scene 可保留模式；新域优先「未写先 Command」 |
| 误把 Out 当成偷懒 | §2.5 白名单短；§2.2/2.3 不在白名单 |

---

## 6) 验收标准

- [x] §2.1–2.3 中所有「应入栈」行均为 ✅（无残留直写主路径）  
- [x] Entry/AnyState 装饰位移仍不入栈、不 dirty  
- [x] `edit` 政策拒绝 / `set` 可硬写对照用例通过  
- [x] `verify` 可用  
- [x] Scene 主要结构操作有 Debug → 同一 `Editor*Command`  
- [x] Material / AnimGraph：人手走一遍现有面板/画布操作，Undo/Redo 均还原  
- [x] 无新增「先 Select 再变异」  
- [x] W4 Debug 域动词 + Progress；Registry → **Done**  

---

## 7) Status note

**Done（2026-09-15）：** W1–W4 实现 + 人手验收；Console 按 Session Domain 裁剪；Material ObjectPtr Undo 对齐 Scene。  
**跟进（非阻塞）：** `get`/`set` 对 Material/AnimGraph 路径方言仍可另开；本 Feature 以域动词覆盖主路径。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 首稿 / F13 对齐 / 展开（曾含 MVP） |
| 2026-09-14 | 完备口径：❌=未入栈；现有操作面必须入栈 |
| 2026-09-14 | **字段走 Set/Edit；拓扑才专用 Command。** AnimState 保持 struct；Name 不升 MEObject、不用 SetObjectName |
| 2026-09-14 | **W1 Done：** `edit`/`verify`；Scene `add_go`/`delete_go`/`add_comp`/`remove_comp`/`reparent`/`rename_comp`/`move_comp` |
| 2026-09-14 | **W2-A：** 属性 Apply 解耦；Material Inspector 字段入栈；Output 连线 prune 可 Undo |
| 2026-09-14 | **W2 画布：** 节点内 DragFloat/DragFloat3 手势结束 Submit（与 Inspector 同栈） |
| 2026-09-14 | **W2 拓扑/位姿：** Material Add/Remove/Connect/Disconnect + 拖拽结束位姿 |
| 2026-09-14 | **W3：** AnimGraph `SubmitOwnedPropertyMutation(m_StateMachine|m_Schema)`；Inspector/Parameters/画布入栈 |
| 2026-09-14 | **W4：** `EditorMaterialDebugCommands` / `EditorAnimGraphDebugCommands`；挂同一 Submit/mutation |
| 2026-09-15 | **Done：** 人手验收；Session Domain 补全；Material ObjectPtr Undo |
