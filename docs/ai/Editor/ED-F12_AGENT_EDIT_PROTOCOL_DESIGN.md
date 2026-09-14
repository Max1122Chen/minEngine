# ED-F12 — Agent Edit Protocol（资产会话 × EditorCommand）— Design Spec

## Meta
- **ID:** `ED-F12`
- **Type:** Feature
- **Status:** Done（本 Feature：命名收敛 + ParseContext + Payload；**不**含 `edit`/`verify`/`invoke` — 下一 Feature）
- **Owner:** project maintainer
- **Last updated:** 2026-09-14
- **Branch:** `feat/editor`
- **Depends on:**
  - `ED-F04` Debug Console / 文本 REPL（已有；**本 Feature 将其降为 DebugCommand 前端**）
  - `ED-F11` Document Host（Session + per-Session 栈；**W0–W2 已落地**；W3 可缓）
  - `CORE-F05` Play / Inspecting Scene（Scene 域需要）
- **Related:**
  - [ED-F04 Design](./ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md)（现状；分层由本文件校准）
  - [ED-F11 Tab Host](./ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md)
  - [ENGINE_0_1_0_ROADMAP](../ENGINE_0_1_0_ROADMAP.md) §1.2 / D5
  - [ENGINE_DESIGN_PHILOSOPHY](../ENGINE_DESIGN_PHILOSOPHY.md)
  - 后续：`MCP-F01`（暴露 EditorCommand 的 AI 构造面，不另造语义）
- **Blocks:** MCP 产品化（软）；跨资产 Agent 编辑；D5（Scene 竖切）
- **Note:** 旧称「World Query–Modify–Verify」— Scene 仅为首个域竖切

## TL;DR

**核心是资产与 EditorCommand。** DebugCommand / GUI / MCP 只负责构造它。  
Debug 解析（基线语法 + Suite 方言）是偏重服务；其 **ParseContext** 不等于命令 **执行载荷**。  
写路径区分 **`set`（侵入）** 与 **`edit`（守反射 specifier）**。 Scene D5 为第一刀。

---

## 0) 维护者意图对齐（必读）

### 0.1 资产决定编辑器

```text
Asset（类型 + 身份 + 真源）
    │ 决定
    ▼
Edit Session（该资产的编辑上下文）
    │ 选用
    ▼
Type Suite UI（人类面板）  +  该域可用的 EditorCommand 集
```

多 Session 同时活动的前提：会话可寻址，而不是「当前焦点窗口」唯一。

### 0.2 Edit Session

| 能力 | 含义 |
|------|------|
| 类型 / 资产键 / title | 身份与展示 |
| Dirty | 相对保存点 |
| Command stack | **本会话** undo/redo |
| 域状态 | Scene* / Material* / Graph* … |
| 可用指令集 | 全局命令 ∪ 本类型域命令 |

人类 Active Tab = DebugCommand / GUI 的 **默认** Session；Agent/MCP 应能显式指定 Session。

### 0.3 不是三套类型——一个核心 + 多种前端

> **Command / EditorCommand / ConsoleCommand 不必是三种数据结构。**  
> 在编辑器语境下：**Command 本质上就是 EditorCommand。**

| 名称 | 角色 |
|------|------|
| **EditorCommand** | **唯一操作核心**：可执行、可（在适用时）撤销；携带完整显式参数（含目标 Session/对象引用）。例：`GetObjectPropertyCommand`、`ListGameObjectsCommand`、`SetObjectPropertyCommand`、`AddMaterialNodeCommand`。 |
| **Editor GUI** | 前端：Inspector / Hierarchy / 图节点菜单等 **直接构造** EditorCommand 并提交 Session 栈。 |
| **DebugCommand** | 前端（面向人类 GUI 编辑器里的调试 REPL）：**字符串 → 解析意图 → 构造 EditorCommand**。旧称 ConsoleCommand；**定名 DebugCommand**。 |
| **MCP Tool** | 前端：JSON/schema → **接口化构造** 同一批 EditorCommand（或工厂），不新增业务语义。 |

```text
GUI 点击/拖拽 ────────┐
DebugCommand 文本 ────┼──► 构造 EditorCommand ──► Session.CommandStack（或只读执行）
MCP tools/call ───────┘
Headless / Tests ─────（直接 new/工厂 EditorCommand，或走显式参数 API）
```

**一句话：** 只有一种「命令」；其余都是怎么把人/机的意图变成那条命令。

### 0.4 EditorCommand 能力 vs DebugCommand「猜意图」（必须分清）

| | **EditorCommand（能做什么）** | **DebugCommand 解析服务（帮用户猜什么）** |
|--|------------------------------|------------------------------------------|
| 本质 | 完整、显式参数的操作 | 把短/方言字符串 **适配** 成上述显式参数 |
| 稳定性 | 协议真源；MCP/测试/GUI 应对齐这里 | 可随 Active Suite 变重、变甜；**不**改变 EditorCommand 契约 |
| `get` 例 | 基线：`GetObjectProperty(objectGuid, propertyPath)` | 见下表方言适配 |

**同一 EditorCommand，多种 Debug 写法（解析适配，不是多种 get 语义）：**

| Debug 输入（例） | 解析适配做什么 | 最终 EditorCommand |
|------------------|----------------|--------------------|
| `get <ObjectGuid> <PropertyPath>` | 几乎直通（基线写法） | `GetObjectPropertyCommand` |
| Scene Session 下 `get Player@Controller.m_Speed` | 用 Active Scene 把 GO 名/`@Component` 解析成 Guid + 规范 path | **同一个** `GetObjectPropertyCommand` |
| Material Session 下 `get blendMode` | 用 Active Material 资产/对象补全目标 Guid + 属性 path | **同一个** `GetObjectPropertyCommand` |

- **基线形式**（Guid + PropertyPath）是跨域、可脚本、可 MCP 的稳定面。  
- Suite 方言是 **Debug 糖**：降低人类输入成本，由 **较重的解析/补全服务** 承担。  
- 禁止把「Scene 专用 get 语法」误认为另一种 EditorCommand 或另一套 get 语义。

→ DebugCommand 解析服务在本架构里是 **一等、偏重的服务**（词法、域方言、Session 解析、补全、校验），不是薄壳。

### 0.5 两种「上下文」不是同一个东西

| 名称 | 服务于谁 | 内容（例） | 生命周期 |
|------|----------|------------|----------|
| **DebugParseContext** | 补全 + **解析适配**（字符串 → EditorCommand） | Active Edit Session、Suite 类型、可选选中项、光标前 token… | 仅 Debug REPL 输入期 |
| **EditorCommand 执行上下文** | 命令 **执行/撤销** | 已解析的 Guid、PropertyPath、目标 Session 栈、Scene*/对象指针等 **显式载荷** | 随命令对象存在于栈上 |

- Debug「上下文」**只**为了把人话变成完整 EditorCommand；猜完即丢（或只留在解析诊断里）。  
- EditorCommand **不依赖**「当时 Active 是谁」才能执行——参数已在构造时钉死（除非命令故意持有 SessionId 并在 Execute 时解析，那也是命令载荷的一部分，不是 DebugParseContext）。  
- 混用这两种「上下文」会导致：以为换 Tab 会改变已入栈命令的含义，或 MCP 误去读人类 Active。

### 0.6 DebugCommand 配件一览

| 配件 | 作用 |
|------|------|
| 词法/语法 + **域方言适配** | 基线语法 ∪ Suite 糖 → 工厂参数 |
| 补全 / 校验提示 | 消费 **DebugParseContext** + 反射可见性规则（见 §0.7） |
| Active Session → 显式参数 | 填 Guid / Scene* / 资产句柄等 |

MCP / 测试：**跳过**方言与 Active 猜测，直接打基线参数或共享工厂。

### 0.7 按域管理 EditorCommand

| 范围 | 例（EditorCommand） | Debug 基线 / 糖 | 可用 Session |
|------|---------------------|-----------------|--------------|
| **全局** | `GetObjectProperty`（Guid + path） | 基线 `get <guid> <path>`；Suite 可加糖 | 任意（解析得到对象即可） |
| **Scene** | `ListGameObjects` … | `list_go`（糖：隐含 Scene） | Scene Session |
| **Material** | `AddMaterialNode` … | 域糖 | Material Session |
| **AnimGraph** | … | … | AnimGraph Session |

域标签用于发现、过滤、Debug 补全、MCP catalog——不是 per-editor 第二套命令系统。

### 0.8 与反射类型系统结合：`set` vs `edit`

Command 面应 **消费** `PropertySpecifier` / metadata（与 Inspector `PropertyEditPolicy` 同源精神），区分侵入写与编辑器政策写：

| 动词（EditorCommand / Debug） | 对 specifier 的态度 | 典型用途 |
|-------------------------------|---------------------|----------|
| **`set`** | **侵入式**：可无视（或绕过）编辑向 specifier，按引擎允许的底层赋值路径写 | 自动化、修复、Agent/测试需要强制改值；调试「硬写」 |
| **`edit`** | **尊重编辑政策**：对齐 Visible/Edit 等规则；不可见则不进 Debug **补全**；不可编辑则拒绝或只读提示 | 人类调试时模仿 Inspector 边界；「像编辑器一样改」 |

例（口径）：

- 属性仅 `VisibleAnywhere`（不可 Edit）→ **`edit` 补全不出现该属性**（或出现但标明只读且提交失败）；**`set` 仍可**在协议上允许硬写（权限/安全另议）。  
- `edit` 的补全/校验读反射 specifier；`set` 的补全可更宽（或单独 `set` 补全策略），但执行路径与 `edit` **分命令**，避免一个 `set` 有时守规有时不守。

实现提示：复用或抽取与 `PropertyEditPolicy` 一致的「是否展示 / 是否可编辑」判断，供 Debug 补全与 `EditObjectPropertyCommand` 共用；`SetObjectPropertyCommand` 走明确的 bypass 路径。

**开放细化（§9）：** `set`/`edit` 是否都入 Undo 栈、PIE 下是否允许 `set`、Agent 默认用哪个——拍板前默认：**人类 Debug 主推 `edit`；脚本/MCP/D5 硬改可用 `set`。**

---

## 1) 与当前系统设计的偏差

### 1.1 现状双核

| 现名 | 实际是什么 | 相对目标 |
|------|------------|----------|
| `IEditorCommand` + Session/`EditorCommandStack` | 可 Undo 操作对象 | **应对齐为目标核心（EditorCommand）** |
| `Command::CommandDescriptor` + `ExecuteFn(string args)` | 字符串过程：解析并直接副作用 | **实为 DebugCommand 前端（未命名、未降级）**，却被当成「Command 系统」 |

### 1.2 具体偏差

| ID | 偏差 | 说明 |
|----|------|------|
| **A** | 命名倒置 | 真核心叫 `IEditorCommand`；「Command」名字给了 Debug 过程表 |
| **B** | Debug 未「构造命令」 | 多数 Descriptor **不** new EditorCommand；`set` 仅特例桥 `EditorSetValue` |
| **C** | 两种「上下文」未分离 | `CommandContext` 既像解析又像执行；无 DebugParseContext 概念 |
| **G** | 无 `set`/`edit` 分流 | 今日 `set` 未系统对照 specifier；补全不读 Visible/Edit 政策 |
| **D** | 无域目录 | 全局一张 Registry；无 Scene/Material 域过滤；`list_go` 与未来 `AddMaterialNode` 无法按 Session 类型裁剪 |
| **E** | GUI 与 Debug 未统一工厂 | Inspector 直接 `new IEditorCommand`；Debug 走另一路 |
| **F** | 若 MCP 接 Descriptor | 会把「字符串过程」焊成 AI 语义真源——与目标相反 |

### 1.3 ED-F04 关系

ED-F04 的 REPL、补全、PropertyPath、Console UX **保留为 DebugCommand 配件与实现资产**。  
纠正：它们服务 **字符串前端**，不是编辑语义真源。真源收敛到 **EditorCommand + Session 栈**。

### 1.4 伙伴结论

| 判断 | |
|------|--|
| 采纳「单核心 EditorCommand + 多前端」 | ✅ |
| DebugCommand 定名；解析服务偏重（方言适配） | ✅ |
| DebugParseContext ≠ 执行上下文 | ✅ |
| `set`（侵入）vs `edit`（守 specifier） | ✅ 纳入；与反射/`PropertyEditPolicy` 对齐 |
| 域管理（全局 vs Type Suite） | ✅ |
| 立刻改名/重写全部命令类 | ❌ 先口径与工厂收敛 |

---

## Pre-flight

| 项 | 结论 |
|----|------|
| 扫描 | 双核；F11 Session 已有；Debug 未工厂化 |
| 债 | 模型 high；Scene 竖切 medium |
| 建议 | **Go Design** → W1 Session 上下文服务 → 写路径统一构造 EditorCommand |

---

## Scope

### In

1. 口径：资产中心；EditorCommand 核心；Debug/GUI/MCP 前端；域目录  
2. **分清** EditorCommand 基线能力 vs Debug 方言适配；**分清** DebugParseContext vs 执行载荷  
3. 反射结合：`set` / `edit` 与 specifier（补全 + 执行政策）  
4. 过渡：新逻辑禁止「只改世界不产生 EditorCommand」  
5. Scene 域竖切 D5；域注册点（Material/AnimGraph 占位）

### Out

- MCP 服务器（`MCP-F01`）  
- 一次重命名所有类  
- Material/AnimGraph 完整图编辑实现  
- 多进程真并行；Pin/Reopen  

## Reader quick start

1. §0.3–0.8  
2. **§3.9** Debug↔EditorCommand 表、**§3.10** 迁移与目录  
3. §1 偏差；§9 开放点

---

## 2) 目标

- 团队默认：「改资产 = 提交 EditorCommand 到某 Session」。  
- Debug 字符串与 MCP/GUI **共享同一批命令类型**。  
- 多类型扩展 = 注册域命令 +（可选）Type Suite UI。  

---

## 3) 方案

### 3.1 结构

```text
┌─────────────────────────────────────────────────────────┐
│  Frontends                                              │
│  · Editor GUI                                           │
│  · DebugCommand（parse + complete + 隐式 Session 显式化） │
│  · MCP Tool（结构化工厂；显式 Session/对象）               │
└───────────────────────────┬─────────────────────────────┘
                            ▼
                 EditorCommand（唯一核心）
                            │
                            ▼
              Edit Session.CommandStack / 只读执行
                            │
                            ▼
                   域状态（Scene / Material / …）
```

### 3.2 EditorCommand

- 显式参数齐全（Session 或对象引用、属性路径、值…）。  
- `Execute` / `Undo`（或标记不可撤销的查询类）。  
- 带 **域标签**（`Global` | `Scene` | `Material` | …）供目录过滤。  
- 今日 `IEditorCommand` 即此层雏形；逐步让 Debug/GUI 都经同一构造路径。

### 3.3 DebugCommand 服务（偏重）

职责：

1. 解析 **基线语法** + **Suite 方言糖** → 选定工厂与用户 token。  
2. 用 **DebugParseContext**（Active Session / Suite / 选中等）做适配与补全。  
3. 产出参数齐全的 EditorCommand（或清晰解析错误）。  
4. 呈现 Lines / Payload（人类）。  

**不负责：** 改变 EditorCommand 契约；在 ExecuteFn 里直接改资产（过渡期除外）。

### 3.4 GUI / MCP

- GUI：构造 EditorCommand → Session 栈（已走 `edit` 政策者应与 Inspector 一致）。  
- MCP：暴露 **基线参数** 的工厂/schema；默认不依赖 DebugParseContext。  

### 3.5 域目录

```text
EditorCommandCatalog
  Global:  GetObjectProperty, SetObjectProperty, EditObjectProperty, …
  Scene:   ListGameObjects, …
  Material: AddMaterialNode, …
```

### 3.6 DebugParseContext vs 执行载荷

见 §0.5。方案层约定：

- 解析管道只读 DebugParseContext。  
- 入栈/执行只看 EditorCommand 已序列化的显式字段。  

### 3.7 `set` / `edit` 与反射

- 两套 EditorCommand（或同一族带 `WritePolicy` 枚举）：`Set*` bypass specifier；`Edit*` 走与 `PropertyEditPolicy` 对齐的规则。  
- Debug：`edit` 补全过滤 Visible/Edit；`set` 补全策略可更宽（开放点）。  
- Inspector 默认对应 **`edit`** 语义。  

### 3.8 Scene 竖切（D5）

- 查询/修改/verify → EditorCommand；Debug 可提供 Scene 糖。  
- headless 打基线参数，不启动 ImGui。  

---

## 3.9) Command 目录：续用 / 新增 / Debug 映射

> 约定：表中 **EditorCommand** 为类型名（可渐进落地）；**Debug** 为 REPL 动词；**基线** = 无 Suite 猜测也能写全的形式；**糖** = 依赖 DebugParseContext 的适配。

### 3.9.1 现状盘点（代码真源）

| 今日 Debug Id | 注册处 | 今日行为概要 | 已有 IEditorCommand？ |
|---------------|--------|--------------|----------------------|
| `help` | `BuiltinCommands` | 列 Registry | 无（元命令） |
| `get` | Builtin | `PropertyPath` 单参读（GO 名/`@Comp` 方言，**非 Guid 基线**） | 无独立命令对象 |
| `set` | Builtin | 写；Edit 下桥 `SetObjectPropertyCommand` | ✅ `SetObjectPropertyCommand`（仅此桥） |
| `inspect` | Builtin | 结构浏览 | 无对象 |
| `find` | Builtin | ActiveScene 查询 | 无对象 |
| `list_go` | `EditorConsoleCommands` | 列 ActiveScene GO | 无对象 |
| `undo` / `redo` | Editor | Active Session 栈 | 操作栈本身 |
| `rename` | Editor | GO 改名 → `RenameGameObjectCommand` | ✅ |

**仅 GUI 提交、尚无 Debug 动词的 Scene EditorCommand（续用，本 Feature 不强制全部挂 Debug）：**

`AddEmptyGameObject`、`DeleteGameObject`、`AddComponent`、`RemoveComponent`、`RenameComponent`、`MoveComponent`、`ReparentGameObject`、`SetGameObjectTransform`、…

### 3.9.2 本 Feature：**继续支持**（语义收敛后）

| Debug 动词 | 域 | 基线参数 | 支持的变体 / 糖（DebugParse） | 对应 EditorCommand（目标） | 备注 |
|------------|----|----------|------------------------------|----------------------------|------|
| `help` | Global | 〔无〕 | — | *元：列出 Catalog/Debug 动词*（可不入栈） | 续用；日后按域过滤 |
| `get` | Global | `<ObjectGuid> <PropertyPath>` | **Scene 糖：** `get <GO>[@Comp].path`（现语法）；**Material 糖（后）：** `get blendMode` 等 | `GetObjectPropertyCommand`（NotUndoable） | **续用动词**；补齐 Guid 基线；糖保留兼容 |
| `set` | Global | `<ObjectGuid> <PropertyPath> <value>` | 同 get 的路径糖；可选 `=` token（现有） | `SetObjectPropertyCommand`（侵入写） | **续用**；明确 = bypass specifier；Edit 模式入 Session 栈 |
| `inspect` | Global | `<ObjectGuid>` 或 Guid+子 path | Scene 糖：GO/`@Comp` 现语法 | `InspectObjectCommand`（NotUndoable） | 续用；输出进 Payload |
| `find` | Scene | `<query>` + **显式 Scene/Session**（测试/MCP） | 糖：省略 Scene，用 Active Scene Session | `FindGameObjectsCommand` | 续用；域=Scene |
| `list_go` | Scene | 〔可选 SessionId〕 | 糖：无参 = Active Scene | `ListGameObjectsCommand` | 续用名；可另别名 `list go`（开放） |
| `rename` | Scene | `<ObjectGuid> <NewName>` | 糖：`<GOName> <NewName>`（现） | `RenameGameObjectCommand`（已有） | 续用；补 Guid 基线 |
| `undo` | Session | 〔可选 SessionId〕 | 糖：Active Session 栈 | *栈操作*（或 `UndoEditSessionCommand` 薄封装） | 续用 |
| `redo` | Session | 同上 | 同上 | 同上 | 续用 |

### 3.9.3 本 Feature：**新增**（**收窄：本 Feature 暂缓**）

> **2026-09-14 决定：** 本 Feature 以架构收敛 + 现有 Debug 动词机读/Session 上下文为主，**不新增** `edit` / `verify` / `invoke`。下表改入 **下一 Feature**。

| Debug 动词 | EditorCommand | 归属 |
|------------|---------------|------|
| `edit` | `EditObjectPropertyCommand` | 下一 Feature |
| `verify`（及子命令） | `Verify*` 族 | 下一 Feature（D5 随之） |
| `invoke` | `InvokeObjectFunctionCommand` | 下一 Feature |

原 `verify` 子命令表仍作下一 Feature 规格参考（见变更记录前设计意图）。

### 3.9.4 本 Feature **不新增 Debug、但目录保留** 的 GUI EditorCommand

继续由 Hierarchy/Inspector 构造；后续可按需加 Debug 动词（另切片）：

| EditorCommand（已有） | 建议未来 Debug（非本 Feature 必做） |
|----------------------|-------------------------------------|
| `AddEmptyGameObjectCommand` | `add_go <name>` |
| `DeleteGameObjectCommand` | `delete_go <Guid\|名>` |
| `AddComponentCommand` | `add_comp <GO> <Type>` |
| `RemoveComponentCommand` | `remove_comp …` |
| `ReparentGameObjectCommand` | `reparent …` |
| `SetGameObjectTransformCommand` | （多用 `edit`/`set` 属性） |

### 3.9.5 结果与 Payload（所有查询/verify）

- 人类：保留 `CommandOutputLine`。  
- 机读：`PayloadJson`（或等价）含 `op`、关键字段、`expected`/`actual`（verify 失败时）。  
- headless **禁止**解析 Lines。

### 3.9.6 兼容策略（Debug 用户习惯）

| 旧写法 | 新口径下 |
|--------|----------|
| `get Player@Transform.m_…` | **仍支持**（Scene 糖）→ 同一 `GetObjectPropertyCommand` |
| `set path value` / `set path = value` | **仍支持**；语义定为 **侵入 `set`**；守规请改用 `edit` |
| 无 Guid 的 get/set | 过渡期允许；文档推动 Guid 基线；测试新增 Guid 用例 |

---

## 3.10) 迁移、重构与目录布局

### 3.10.1 迁移原则

1. **先接水管，再换牌子：** 行为先「Descriptor → 构造已有/新 EditorCommand」，类名/文件夹可第二刀。  
2. **GUI 已走的 EditorCommand 不重写逻辑：** Debug/`edit`/`set` 复用 `SetObjectPropertyCommand` 等。  
3. **查询类补齐命令对象：** `Get`/`List`/`Find`/`Inspect`/`Verify` 从「ExecuteFn 直接打字」改为 NotUndoable EditorCommand（或 `IEditorQuery` 若不想进 Undo 栈——推荐仍用命令对象、`Execute` 不入栈）。  
4. **禁止**再增加「只改世界、不产生 EditorCommand」的新 Debug 处理器。

### 3.10.2 分阶段迁移（对照 Wave）

| 阶段 | 动作 | 风险控制 |
|------|------|----------|
| **M0** | 文档/注释：Registry = Debug 前端；`IEditorCommand` = 核心 | 无代码行为变 |
| **M1** | 引入 `DebugParseContext`（Active Session、TypeId）；与执行用 Command 载荷分离 | Console 填参改一处 |
| **M2** | `list_go`/`find`/`get`/`inspect` → 工厂构造查询命令；Payload | 对照现输出做单测 |
| **M3** | `set` 固定走 `SetObjectPropertyCommand`；新增 `edit` + policy | Inspector 与 `edit` 对齐抽 `PropertyEditPolicy` |
| **M4** | `verify*` + D5 套件 | |
| **M5** | Catalog 按域；Legacy ExecuteFn 删除或 Hidden | |
| **M6** | （另票）命名空间/目录改名 `DebugCommand*` | 大 diff 单独 commit |

### 3.10.3 目标目录布局（推荐）

> **2026-09-14 落地：** DebugCommand + PropertyPath **迁入 Editor**（与 EditorCommand 同仓）；Engine 不再承载调试命令面。

```text
minEngine/Editor/src/
  Commands/                          # EditorCommand 核心（按域）
    EditorCommandStack.h             # 或留 Shell/
    Scene/
      Editor*Command.*
    Global/                          # 后置
  DebugCommand/                      # Debug REPL 前端（自 Engine 迁入）
    DebugCommandRegistry.*
    DebugCommandExecutor.*
    DebugCommandContext.*
    DebugParseContext.*
    …
  PropertyPath/                      # 与 Debug get/set 绑定；随 Debug 同仓
  UI/CommandConsole/                 # Presentation（可后改名 DebugCommandConsole）
```

**依赖方向：**

```text
DebugCommand → Commands（EditorCommand）→ Session / Scene / Reflection
UI/CommandConsole → DebugCommand
MCP-F01（未来）→ Commands 工厂（不经 Debug 方言）
```

Runtime **不**依赖 Editor；若查询命令要在 headless 无 Editor 跑：  
- 方案 A（推荐 MVP）：测试在 Editor 链接下跑，或  
- 方案 B：`Get`/`Verify` 的纯数据路径放 Core，EditorCommand 做薄包装——开放点 O11。

### 3.10.4 现有文件迁移动作清单（实施时勾选）

| 现路径 | 目标 | 动作 |
|--------|------|------|
| `Editor/.../Commands/Scene/SetObjectPropertyCommand.*` | `Commands/Global/` | 上移（属性写跨域）或暂留 + using 别名 |
| `Editor/.../Commands/Scene/*` 结构性 | `Commands/Scene/` | 保持/微调 |
| `Runtime/Core/Command/BuiltinCommands.*` | `DebugCommand/Handlers/RegisterGlobal*` | 改为调用工厂 |
| `Editor/.../EditorConsoleCommands.*` | `DebugCommand/Handlers/RegisterScene*` | 拆 list/rename/undo |
| `Runtime/Core/Command/CommandRegistry.*` | `DebugCommand/*` 或保留 Core 作过渡 | M6 再搬 |
| `Runtime/Core/Command/CompletionService.*` | `DebugCommand/` | 依赖 DebugParseContext |
| `UI/CommandConsole/*` | 保留为 Presentation | 只调 Parser |

### 3.10.5 重构时明确 **不做**

- 不把 MCP schema 写进 Debug 方言层。  
- 不把补全状态塞进 EditorCommand 成员。  
- 不为一时方便再增加 `EditorSetValue` 式旁路（应内联为「解析后 Submit Set/Edit Command」）。

---

## 4) 过渡策略

细节目录与迁移动作见 **§3.9–3.10**。摘要：

| 阶段 | 做法 |
|------|------|
| T0 | 本文口径；停止称 Descriptor 为语义真源 |
| T1 | DebugParseContext；与执行载荷分离 |
| T2 | 查询类 → 工厂 + EditorCommand；Payload |
| T3 | `edit`/`set` 分路径；GUI 共享 Edit* |
| T4 | Catalog / 目录迁名；MCP 接基线工厂 |

禁止：第三套 `AgentCommand`。

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 双核共存过久 | 新代码禁令 + Wave |
| 把方言当成第二种 get | §0.4；基线测试锁定 Guid+path |
| 混用两种「上下文」 | §0.5；入栈命令自带显式载荷 |
| `set`/`edit` 混成一个 | 分命令或显式 WritePolicy |
| Agent 踩 Active 猜测 | MCP 强制基线/显式 |
| 域爆炸 | 先 Global + Scene |

---

## 6) 验收

- [ ] §3.9 续用/新增映射表与现码一致；§3.10 目录可实施  
- [ ] §0.4–0.8 口径  
- [ ] Scene D5；写路径进 Session 栈  
- [ ] 无 MCP 服务器代码

---

## 7) Status note

Draft — 待确认 §0.4–0.8 与 §9。

Status note（2026-09-14）：**Done**（本 Feature 收窄范围）。`edit`/`verify`/`invoke` 与查询 EditorCommand 对象化留给下一 Feature。

---

## 8) 波次

| Wave | 内容 |
|------|------|
| W0 | 口径确认（含少增命令） |
| **W1** | DebugParseContext 字段 + Console 从 DocumentHost 填充；与执行载荷注释分离 |
| **W2** | `CommandResult.PayloadJson`；`get` / `find` / `list_go` 机读 Payload |
| **W3** | 现有写路径整理：`set` 明确走 Session 栈上的 `SetObjectPropertyCommand`（少旁路） |
| **W4** | 查询侧开始 EditorCommand 对象化（Get/List/Find）；Catalog 雏形 |
| 后 | `edit`/`verify`/`invoke` → **下一 Feature** |

---

## 9) 开放点

| # | 议题 | 推荐默认 |
|---|------|----------|
| **O1** | 口语「Command」 | **= EditorCommand** |
| **O2** | DebugCommand 定名 | **采用** |
| **O3** | 查询是否 EditorCommand | **是**（可 NotUndoable） |
| **O4** | `get` 基线 | **Guid + PropertyPath**；GO 名 / `blendMode` 等为 Debug 糖 |
| **O5** | 域枚举 | `Global` + DocumentTypeId |
| **O6** | MCP | `MCP-F01`；基线参数 |
| **O7** | Core `Command::*` 改名 | 另票 |
| **O8** | Agent/D5 默认 `set` 还是 `edit` | **D5/自动化默认 `set`；人类 Debug 主推 `edit`** |
| **O9** | `set` 是否仍入 Undo | **是**（默认可撤销硬写）；不可撤销另标 |
| **O10** | 仅 VisibleAnywhere 在 `set` 补全中是否出现 | **可出现**（与 `edit` 补全分离） |
| **O11** | headless 查询命令放 Core 还是仅 Editor 链接测试 | **MVP：Editor 测试目标链接**；纯 Core 查询路径作后续优化 |
| **O12** | `list_go` 是否改名 `list go` | **保留 `list_go`**；可选别名 |
| **O13** | `SetObjectPropertyCommand` 是否上移 Global/ | **目标上移**；可先别名后搬文件 |

---

## 10) 哲学检查

| 问 | 答 |
|----|----|
| 机制？ | EditorCommand + 反射政策 + Session |
| Agent 双轨？ | 否 |
| Debug 方言进核心？ | 否 |
| specifier？ | `edit` 守；`set` 可侵入 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 初稿 World Query |
| 2026-09-14 | 抬升：资产中心；四层表述 |
| 2026-09-14 | **再校准：** EditorCommand 单核心；DebugCommand；域管理 |
| 2026-09-14 | **补强：** 基线 vs 方言；双上下文；`set`/`edit` |
| 2026-09-14 | **§3.9–3.10：** 续用/新增映射；迁移与目录 |
| 2026-09-14 | **收窄：** 不新增 edit/verify/invoke |
| 2026-09-14 | **Done：** DebugCommand* / EditorCommand* 命名落地；ParseContext + PayloadJson；旧 `Runtime/Core/Command` 删除 |
| 2026-09-14 | **目录：** DebugCommand + PropertyPath 迁入 `Editor/src/`；与 EditorCommand 同仓；测试编入 Editor 侧源 |
