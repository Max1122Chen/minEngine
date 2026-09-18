# MCP-F04 — Automation 扩域（control / asset / 类型查询）— Design Spec

## Meta
- **ID:** `MCP-F04`
- **Type:** Feature
- **Status:** Review
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Branch:** `feat/mcp`
- **Depends on:** `MCP-F02` Done（信封）；`MCP-F03` Done（粗 Tools）；`ED-F17` / `ED-F18` Done（Host + `SceneEditorServices`）
- **Related:**
  - [Implementation](./MCP-F04_AUTOMATION_DOMAIN_EXPANSION_IMPLEMENTATION.md)
  - [MCP-F02](./MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md)（kind 原为 query/edit/verify；本 Feature **扩展**枚举，不另起协议版本）
  - [ED-F18](../ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md)（Client 用原语；Service 不镜像 op）
  - [ENGINE_DESIGN_PHILOSOPHY.md](../../ENGINE_DESIGN_PHILOSOPHY.md) §5 Editor 域服务与 Client
- **Blocks:** 无

## TL;DR

P0 的 `query|edit|verify` 包不住「给人看」和「改资产文件」。本 Feature **只加两个 kind**：`control`、`asset`。类型目录做成 Editor 侧独立只读 Service（进程内返回 `MEClass*` / `MEProperty*`），不挂 `SceneEditorServices`。Surface 把指针译成 JSON；Client 自己解释「可添加组件」这类语义。

## Scope
- **In:**
  - kind 扩展：`control`、`asset`；`protocolVersion` 仍为 1（增量枚举）
  - 粗 Tool：`editor_control`、`editor_asset`（list 仍走已有 `editor_query`）
  - `AssetEditorServices`：列出 / 创建 / 删除资产
  - `control`：打开资产、聚焦 Session、选中对象、在 Content Browser 中定位
  - `TypeQueryServices`：按基类 + specifier 过滤，返回类或属性指针数组
  - Scene 已有原语补 op：`move_component`、`instantiate_prefab`（kind=`edit`）
- **Out:**
  - Material / Anim 图拓扑（另 Feature；本波只保证能 **打开** 这些资产）
  - Create Prefab（资产 Create 族里的特例，另议）
  - Play、读 Console、撤销/重做 op、截图
  - 字面量→void* 的 `SetProperty` 收敛
  - 为 Add Component 单开 `list_addable_components`
  - 把 `MEClass*` 放进 JSON

## Reader quick start
1. §3.1 kind 与谁用哪个
2. §3.3 `TypeQueryServices`（本轮讨论焦点）
3. §3.4–§3.5 资产与 control
4. Implementation 切片

---

## Pre-flight

| 项 | 结论 |
|----|------|
| 性质 | 协议增量 + 两个新 Editor Service；不改 Scene 写路径模型 |
| Prerequisites | Host/Surface/粗 Tools 可跑；`AssetManager::FindAssetMetas*`、`AssetWorkflowModule` 打开/创建/删除、`ReflectionSystem::GetAllClasses` **已有** |
| 债务 | `SceneEditor::m_AllComponentTypeNames` 是 Add Component 的私有过滤缓存；本 Feature 后改为 Client 调 `TypeQueryServices` |
| Go | 不新造反射系统；不把焦点塞进 `set_property` |

---

## 1) 背景与目标

Agent 今天只能改**已经打开的 Scene**。不能建/删/列出资产，不能打开文档，不能把用户的 Tab 切到刚改过的对象。反射侧，Add Component 在 `SceneEditor::InitializeComponentTypeNames` 里自己滤了一遍类；别的调用方会再写一遍。

成功时：Agent 能列出资产、创建、打开并聚焦，人立刻看到目标；Add Component 与 `reflect.*` 共用同一查询，过滤条件由 Client 传入。

## 2) 现状

| 层 | 今天 |
|----|------|
| kind | 仅 `query` / `edit` / `verify`（MCP-F02） |
| Host op | `editor.ping/get_state/get_capabilities` + Scene MVP |
| 资产 | `AssetWorkflowModule` 与 `AssetManager` 已能创建/打开/删除/按类型查找；**无** Automation op |
| 焦点 | `ActivateSession` 在文档类型注册里；Services **故意不**在每次 Edit 后 Activate（ED-F18） |
| 类型列表 | `SceneEditor` 缓存：`IsA(Component)` 且非 `Abstract` |

## 3) 方案

### 3.1 kind：效果，不是域名

`op` 仍是主键，形如 `domain.action`。`kind` 只描述这一下的效果，并与注册表一致（沿用 `KindOpMismatch`）。

| kind | 效果 | 典型 op |
|------|------|---------|
| `query` | 只读 | `asset.list`、`reflect.list_classes`、`reflect.list_properties`、既有 `scene.get_*` |
| `edit` | 改已打开文档，走该 Session 撤销栈 | 既有 `scene.*` 写；另加 `scene.move_component`、`scene.instantiate_prefab` |
| `verify` | 检查 | 不变 |
| `control` | 改人正在看什么；**不**进入场景撤销栈 | `asset.open`、`editor.activate_session`、`editor.reveal_asset`、`scene.select_object` |
| `asset` | 改资产库/文件（创建、删除）；不是 Scene command | `asset.create`、`asset.delete` |

打开是 `control` 不是 `asset`：效果是给人看，没有「撤销打开」。创建/删除是 `asset` 不是 `edit`：它们不进当前 Scene 的 Command 栈。

**不另加 kind。** 播放、日志订阅若以后塞不进这五类，再单独立项。

粗 Tool：已有 `editor_query` / `editor_edit` / `editor_verify`。新增 `editor_control`、`editor_asset`，仍是 `domain` + `action` → `op`。不为 `create_asset` 单独造 Tool。

`protocolVersion` 保持 **1**。旧 Host 拒绝未知 kind；新 Client 配新 Host。不为此升到 v2。

### 3.2 调用

```text
GUI / Debug / MCP
  → 进程内 *Services（原语）
  → Automation Surface 只做：kind/op 校验、JSON ↔ 指针/路径、Hierarchy 等 Client 结构
```

Surface 不实现第二套类型过滤。GUI 不把「可添加组件」登记成 Service API。

### 3.3 TypeQueryServices（Editor 侧，只读）

**落点：** `Editor/src/Services/TypeQuery/`（名称可实施时微调）。Editor 持有，经 `IEditorContext` 取出。**不**放进 `SceneEditor`，**不**放进 Core 反射（`ReflectionSystem` 仍是机制；本 Service 只是 Editor 语境下的结构化查询，避免每处自写 `IsA` + specifier）。

进程内契约：

```text
QueryClasses(base: MEClass* | null,
             requireAll: ClassSpecifierMask,
             excludeAny: ClassSpecifierMask) → const MEClass*[]

QueryProperties(owner: MEClass&,
                requireAll: PropertySpecifierMask,
                excludeAny: PropertySpecifierMask) → const MEProperty*[]
```

- `base == null` 且掩码为 0 → 全部类（或全部属性）。
- 指针只在本进程有效；类在反射存活期内稳定。
- **不**返回显示名、图标、默认实例名（那是 `ComponentTypeUiCatalog`）。
- **不**提供 `ListAddableComponents`。Add Component 作为 Client：`base = Component`，`excludeAny = Abstract`，再自己决定画什么。

Automation（kind=`query`）：

| op | payload | result（JSON，无指针） |
|----|---------|------------------------|
| `reflect.list_classes` | 可选 `baseClass` 名；`requireAll` / `excludeAny`（specifier **名** 数组，Surface 译成掩码） | `{ classes: [{ name, super, specifiers: string[] }] }` |
| `reflect.list_properties` | `className`；同样的 specifier 过滤 | `{ properties: [{ name, type, specifiers }] }` |

specifier 名解析失败 → 参数错误，不静默忽略。

`SceneEditor::InitializeComponentTypeNames` / `m_AllComponentTypeNames`：Add Component 改调本 Service 后删除缓存。

### 3.4 AssetEditorServices

包装已有 `AssetManager` / `AssetWorkflowModule`，不重写导入管线。

| op | kind | 行为 |
|----|------|------|
| `asset.list` | query | 可选目录、资产类型名；返回路径、Guid、类型。无过滤时不要一次倒出无关二进制；默认可限在项目 Content 下 |
| `asset.create` | asset | 类型名 + 目录 + 名。本波类型白名单与编辑器「新建」一致：Scene、Material、AnimationGraph（实施时以 `AssetWorkflowModule` 已支持的为准）。**不含** Create Prefab |
| `asset.delete` | asset | 路径。沿用现有删除/Prefab 断链规则；失败带回消息 |
| `asset.open` | **control** | 按路径打开文档 Session，并聚焦（见 §3.5） |

创建/删除的撤销若现有工作流没有 Command，本波**不发明**资产 Undo；`capabilities` 里 `undo: false`。

### 3.5 control

与 Edit 分离。Agent 可以先 `scene.set_property`，再显式 control，人这时才看到。

| op | 行为 |
|----|------|
| `editor.activate_session` | 聚焦已有 `sessionId`。不创建文档 |
| `asset.open` | 没有则打开，然后聚焦。对话框不出现（路径由 Client 给） |
| `editor.reveal_asset` | Content Browser 定位路径。不改选择中的场景对象 |
| `scene.select_object` | 在指定 Scene session 上选中 Guid。需要该 Session 已是文档；是否同时 `activate_session` 由 Client 再发一条，**不**隐含 |

### 3.6 Scene op 补齐（无新 Service）

| op | kind | Service |
|----|------|---------|
| `scene.move_component` | edit | 已有 `MoveComponent` |
| `scene.instantiate_prefab` | edit | 已有 `InstantiatePrefab`。payload 是资产路径（Surface 加载），**不**弹文件框 |

### 3.7 明确不做的镜像

| Client 说法 | 正确落点 |
|-------------|----------|
| 可添加组件列表 | Client 滤 `reflect.list_classes` |
| 打开并改材质图 | 本波只 `asset.open`；图编辑另 Feature |
| 每次 set 后自动切 Tab | 禁止；要看就 `control` |

## 4) 备选

| 选项 | 结论 |
|------|------|
| 把 Open/Create 仍塞进 `edit` | 否定：撤销语义是假的 |
| kind 名叫 `navigate` | 否定：维护者选定 **`control`** |
| 只加 domain、不加 kind | 否定：`asset.create` 与 Scene `edit` 效果不同，kind 用来挡误用 |
| 类型查询放进 `SceneEditorServices` 或 Core | 否定：过滤是 Editor 多 Client 共用的只读原语，不是场景写路径，也不是引擎机制本身 |
| 线协议返回 `MEClass*` | 否定：指针不能过 JSON |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| kind 继续膨胀 | 本 Feature 封顶五个；新手感先写进本文再改枚举 |
| `asset.create` 类型一扩就变成业务 API | 白名单=工作流已有类型；新类型另议 |
| 列出全部类过大 | 允许；Client 自滤。若实测过慢再加分页，不预先做 |
| 打开资产的控制流（多 Tab、Prefab Stage）和「发 cmd」缠在一起 | `asset.open` 只做打开+聚焦；Stage/对话框留在各自 SubEditor |

## 6) 验收

- [ ] schema / Host / MCP：kind 含 `control`、`asset`；未知组合 `KindOpMismatch`
- [ ] `editor_control`、`editor_asset` 存在；list/reflect 走 `editor_query`
- [ ] `TypeQueryServices` 与 Add Component 共用；无 `list_addable_components`；`m_AllComponentTypeNames` 删除
- [ ] Agent 路径：list → create → open（人看到对应编辑器）→ 改 Scene 属性 → `scene.select_object` + `activate_session`（人看到该对象）
- [ ] `move_component` / `instantiate_prefab` 与 GUI 同一 Service、同一撤销栈
- [ ] 线协议结果无指针

## 7) Status note

| 字段 | 内容 |
|------|------|
| Reason | 设计待维护者审阅后开工 |
| Impact | 未写代码 |
| Decision | kind=`control`+`asset`；`TypeQueryServices` 独立、返回 `MEClass*` |
| Next | 审阅通过 → S01 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 初稿：五 kind；资产/控制/类型查询；Scene 两 op 补齐。Status Review |
