# ED-F18 — Editor 原生能力面纠偏（`*EditorServices`）— Design Spec

## Meta
- **ID:** `ED-F18`
- **Type:** Feature（**思想纠偏 / true refactor**；代码面可控）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Branch:** `feat/mcp`
- **Depends on:** `ED-F17` Done（Host / Surface / 现协议型服务可运行）
- **Related:**
  - [ED-F17](./ED-F17_AUTOMATION_HOST_DESIGN.md)
  - [Implementation](./ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_IMPLEMENTATION.md)
  - [MCP-F01 §3.4–3.5](./MCP/MCP-F01_ARCHITECTURE_DESIGN.md)
  - [ED-F12](./ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md) · [ED-F13](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md)
- **Blocks:** 无（不挡产品竖切；挡的是长期错误同源模型）
## TL;DR

ED-F17 把域服务做成了 **Automation 协议形状**（string id、JSON、服务内拼 Hierarchy Json）。这是 **职责倒挂**。

本 Feature 纠正为：

- **`SceneEditorServices`** = Editor **中性原语**（类似指令集：暴露世界状态 + 最小操作），**不**假设 Client 要什么高级结构。
- **Surface / Debug** = Client：解析意图、组 payload、自己拼 Hierarchy 等。
- **Get/Set** = `path` + `void*` → `MEProperty` Accessor；无 typed 糖。
- **Result** = 瘦状态反馈；产物用 out。
- **不管 Active Session**（那是 GUI/体验层）。

> **警惕：** 再把 Json / 协议字符串 / 「为某 Client 预置的结构（如 GetHierarchy）」沉进 Services = 回归已否定模型。

## Scope
- **In:** 命名与契约翻转；Surface 适配；删除协议型服务 API；Debug Scene 写路径改调原生 Services（切片见 §7）
- **Out:** 新业务；MCP schema 大改；GUI 读强迁；「切换 Active 文档/编辑器」；Material/Anim；Create Prefab；字面量→void* `SetProperty` 可后置

## Reader quick start
1. §1 思想与原语观
2. §3 职责 · §4 调用
3. §5 接口形态（审批焦点）
4. **§6 DebugCommand 接入**（本 Feature 目的之一）
5. §7 迁移 · §8 切片

---

## Pre-flight

| 项 | 结论 |
|----|------|
| 性质 | 思想大、代码面中小 |
| Prerequisites | ED-F17 可跑；Command / Session / `AssignProperty` **sound** |
| Risk if wrong | 固化「协议型服务」或「服务替 Client 拼结构」 |
| Recommendation | **Go** — 本文批准后 Impl |

---

## 1) 思想：原语 vs 高级结构

### 1.1 ED-F17 错在哪里

| 症状 | 问题 |
|------|------|
| 服务吃 `string` sessionId / objectId + `Json` | 协议税沉进能力核 |
| 服务内 `BuildHierarchyNode` → Json | **假设 Client 需要 Hierarchy 树** |
| 类名 `*Automation*Services` | 宣告为对外 Automation 服务 |
| Edit 后 `ActivateSession` | 把 GUI 焦点策略塞进每次改世界 |

### 1.2 正确模型（指令集类比）

```text
SceneEditorServices ≈ 中性指令集 / 世界暴露
  - 给你 Scene*、按 GUID 找对象、对属性 void* Get/Set、拓扑原语…
  - 不提供「GetHierarchy」这种高级语言结构

Surface / Debug ≈ 高级语言 / Client
  - 自己决定：要树就 GetScene 再拼；要 Json 就自己序列化
  - Client 不是没有逻辑的傻子
```

**一句话：** Service 尽力提供中性 Editor 能力与世界状态；**具体怎么做看 Client**。Service 不为某个 Client 的偏好形状越权。

### 1.3 同源仍成立

同源 = 同一套原语 + 同一批 `EditorCommand` 入 **指定** `session.GetCommandStack()`。  
不是「大家都走 GetHierarchy / 都说 JSON」。

---

## 2) 命名

| 旧 | 新 |
|----|-----|
| `SceneAutomationServices` | **`SceneEditorServices`** |
| `ISceneAutomationServices*` | **`ISceneEditorServices*`**（Query/Edit/Verify 可拆，去 Automation） |

禁止能力核再叫 `*Automation*Service`。

---

## 3) 职责划分

| 层 | 做 | 不做 |
|----|----|------|
| **`SceneEditorServices`** | 解析引擎语义（含 property path→`MEProperty`）；`AssignProperty` / Command 入栈；返回瘦 Result + 必要 out | Json；协议 error 字符串真源；拼 Hierarchy；Activate Active Session；假设 Client UI 形态 |
| **`EditorAutomationSurface`** | Request 字段、Guid 字符串、Json↔buffer、拼协议 Hierarchy/Response、Code→ERROR_CODES | 改世界业务逻辑 |
| **DebugCommand** | 方言→原生参数；调 Services；友好文案 | 拼 AutomationRequest；直 `new Command`（迁完后） |
| **Hierarchy / Inspector** | 直接读 Scene / 现有 Submit（本波不强制） | 为「形式同源」走协议 API |
| **Active Session / 切 Tab** | GUI 与用户体验 | **本波 Services 零参与**；「帮我切到 XXX 编辑器」另能力另 Feature |

---

## 4) 调用关系

```text
                 SceneEditorServices（原语）
                    ▲         ▲
     string/JSON→native    方言→native
           │                │
  EditorAutomationSurface   DebugCommand
           │                │
           └────── 各自解析 / 组装对外形态 ──────┘

入栈：session.GetCommandStack().Execute(Command)
     （不经过、不依赖 Active Session）
```

**禁止：** GUI/Debug → AutomationRequest；Services 主 API 再吃 Json。

---

## 5) 目标接口形态（审批焦点）

> 草图可微调签名细节；下列 **不变量** 不可破。

### 5.1 瘦 Result（扩展 bool，不塞产物）

```text
enum class SceneEditorError : uint16_t {
    None = 0,
    NotFound,
    InvalidArgument,
    InvalidPropertyPath,
    PrefabConstrained,
    Internal,
    // 保持小集合；新增需评审
};

struct SceneEditorResult {
    bool Ok = false;
    SceneEditorError Code = SceneEditorError::None;
    std::string Message;   // 可选，供 Client 日志/文案
};
```

- **不**含 `GameObject*` / `Component*` / Json。  
- 成功产物：**该方法的 out 参数**；无产物则无 out。  
- 失败时：约定相关 out 置空 / 不写入。  
- MCP `ERROR_CODES` 字符串：**仅 Surface** 从 `SceneEditorError` 映射。

### 5.2 世界暴露（中性，无 GetHierarchy）

与 §5.1 同一契约：**状态走 Result，产物走 out**。禁止「直接返回指针、失败只能靠 nullptr」——Client 分不清 NotFound / 未绑文档 / Internal。

```text
// 示意 — 需要 SceneEditor 模块时由 Client 传入 IEditorContext&（非协议层）
SceneEditorResult GetScene(
    IEditorContext& context,
    EditorDocumentSession& session,
    Scene** outScene);

SceneEditorResult FindGameObject(
    Scene& scene,
    const GUID& id,
    GameObject** outObject);

SceneEditorResult FindComponent(
    Scene& scene,
    const GUID& componentId,
    Component** outComponent,
    GameObject** outOwner);            // outOwner 可选：传 nullptr 表示不要 owner
```

#### Session → Scene 绑定（本波 MVP）

| 约定 | 说明 |
|------|------|
| **Session 用途** | 选定 **Command 栈**（`session.GetCommandStack()`）；并校验 `typeId == "Scene"` |
| **Scene 指针** | 本波 = Scene 文档的 **document scene**（今日实现：`SceneEditor::GetDocumentScene()` → `SceneManager::GetEditorScene()`） |
| **不假装** | 多 Level Scene 文档并存时的精确 assetKey→Scene 映射 **后置**；本波不发明假精密绑定 |
| **Prefab Stage** | Inspecting/Stage 场景仍可由 GUI 自理；Automation/Debug 经本 Services 的写路径默认打在 **document scene** + 指定 session 栈 |

- 成功：`Ok==true`，对应 out 非空（约定写明）。  
- 失败：`Ok==false` + `Code`（如 `NotFound`）+ `Message`；相关 out 置空。  
- Client 若要 Hierarchy：先 `GetScene` 看 Result → 再遍历父子 **自己拼**。  
- **Services 不提供 `GetHierarchy`。**

#### Prefab 门禁（本波强制）

拓扑写原语（Create 顶层 / Delete / Reparent / AddComponent / RemoveComponent）在入栈前调用既有 `PrefabEditConstraints`（与 GUI 同策略）。失败 → `SceneEditorError::PrefabConstrained` + Message。
### 5.3 属性 Get/Set（签名形态 A + void*）

```text
// path 由 Service resolve → MEProperty，再 Accessor
SceneEditorResult GetProperty(
    MEObject& owner,
    std::string_view propertyPath,
    void* outValue,
    size_t outValueSize);   // size 用于校验；不足则 InvalidArgument

SceneEditorResult SetProperty(
    EditorDocumentSession& session,
    MEObject& owner,
    std::string_view propertyPath,
    const void* value,
    size_t valueSize);
```

- 底层：`PropertyPath` resolve + `GetPropertyValue` / `AssignProperty`（及现有 Undo blob/`EditorSetObjectPropertyCommand` 路径）。  
- **无** `SetInt`/`SetFloat` 等 typed API；**无** Json 入参。  
- Surface：Json → 按属性类型填 buffer → `SetProperty`；Get 后 buffer → Json。  
- Debug：字面量解析进 buffer → 同上。

### 5.4 拓扑 / 文档原语（Edit）

```text
// 示意 — 凡需 SceneEditor / document scene 的方法均带 IEditorContext&
SceneEditorResult CreateGameObject(
    IEditorContext& context,
    EditorDocumentSession& session,
    std::string_view name,
    GameObject* parent,          // nullable = 场景根
    GameObject** outCreated);    // 成功非空

SceneEditorResult DeleteGameObject(
    IEditorContext& context, EditorDocumentSession& session, GameObject& object);

SceneEditorResult AddComponent(
    IEditorContext& context,
    EditorDocumentSession& session,
    GameObject& object,
    std::string_view typeName,
    Component** outAdded);

SceneEditorResult RemoveComponent(
    IEditorContext& context, EditorDocumentSession& session, Component& component);

SceneEditorResult Reparent(
    IEditorContext& context,
    EditorDocumentSession& session,
    GameObject& object,
    GameObject* newParent);      // nullable = 根

SceneEditorResult SaveDocument(IEditorContext& context, EditorDocumentSession& session);

SceneEditorResult SetProperty(
    IEditorContext& context,
    EditorDocumentSession& session,
    MEObject& owner,
    std::string_view propertyPath,
    const void* value,
    size_t valueSize);
```

- 全部入 **`session.GetCommandStack()`**（适用时）。  
- **不调用** `DocumentHost::Activate`。  
- Prefab：见 §5.2 门禁。
### 5.5 Verify

诚实 stub 可保留；签名原生（session + 可选对象/path）；Surface 包协议响应。

### 5.6 与现状对照

| 现（删除方向） | 目标 |
|----------------|------|
| `GetHierarchy` → Json | **删除**；Client `GetScene` 自拼 |
| `SetProperty(..., Json)` | `SetProperty(..., void*, size)` |
| `SceneServiceResult` 含协议字段 + Json Result | 瘦 `SceneEditorResult`；协议只在 Surface |
| Edit 后 `ActivateSession` | **删除** |
| 字符串 objectId 主 API | `GUID` / `GameObject&` / `MEObject&` |

---

## 6) DebugCommand 接入（Client 角色）

本 Feature **本来目的之一**：让 Debug 与 Automation **共用** `SceneEditorServices` 原语，而不是 Debug 继续 `SceneEditor::Submit*`、Automation 走另一套。

Debug 是 **Client**（人话方言层），不是第二套能力核。

### 6.1 现状（要迁走）

```text
Console 方言
  → EditorSceneDebugCommands（按 GO 名查找、歧义检查）
  → SceneEditor::Submit* → context.GetCommandStack()（常绑 Active）
  → Editor*Command
```

代码枢纽：`Editor/src/Services/EditorSceneDebugCommands.cpp`（`add_go` / `delete_go` / `add_comp` / `remove_comp` / `reparent` 等）。  
属性 `set`：经 `DebugBuiltinCommands` → `EditorSetValue` 回调（最终也应落到同一 `SetProperty`）。

### 6.2 目标调用链

```text
用户输入（字符串参数）
  │
  ▼
DebugCommand 处理器（Client）
  1. 解析参数（个数、字面量）
  2. 方言解析（按名找 GO/组件；歧义 → Debug 文案错误，不进 Service）
  3. 选定 EditorDocumentSession&
       = 当前正在编辑的 Scene 文档 Session
       （从 DocumentHost / 与 ActiveScene 绑定的文档解析；
        这是 Client 选目标，不是 Service 去 Activate）
  4. 需要时 GetScene(session, &scene) 校验世界
  5. 调 SceneEditorServices 原语（指针 / void* buffer）
  6. SceneEditorResult → DebugCommandResult（Code/Message → 控制台文案）
  │
  ▼
SceneEditorServices → session.GetCommandStack().Execute(...)
```

**禁止：** Debug 拼 `AutomationRequest`；Debug 直 `new Editor*Command`（本波覆盖的 op）；Debug 调 Surface。

### 6.3 Client 职责 vs Service 职责

| 步骤 | 归属 | 说明 |
|------|------|------|
| `add_comp Cube StaticMeshComponent` 拆参数 | **Debug** | 方言 |
| 按 **名字** 找唯一 GO / 组件 | **Debug** | 人话便利；歧义报错留在 Console |
| Guid / path 合法性（引擎语义） | **Service** | 如 path resolve、Prefab 约束 |
| 选哪个 `EditorDocumentSession` | **Debug** | 通常 = 承载 `ActiveScene` 的文档；**不**要求 Service Activate |
| `CreateGameObject` / `SetProperty` / … | **Service** | 原语 |
| `Result` → 「Error: …」彩色输出 | **Debug** | Client 解析反馈 |

名字查找 **不必** 升成 Service API（那是方言糖，不是中性原语）。若将来 Agent 也要按名查找，另议，默认仍用 Guid。

### 6.4 与本波原语的命令对照

| Debug 命令（现） | 迁后 | 备注 |
|------------------|------|------|
| `add_go` | `CreateGameObject` + 可选再 `SetProperty` 改名 | 现实现先 AddEmpty 再 Rename；可收敛为一次 Create(name) |
| `delete_go` | `DeleteGameObject` | 名→GO* 在 Debug |
| `add_comp` | `AddComponent` | |
| `remove_comp` | `RemoveComponent` | |
| `reparent` | `Reparent` | `root` → parent=nullptr 在 Debug 解释 |
| 属性 `set`（Builtin） | 字面量→blob → `CommitObjectPropertyBlobs`（或日后 void* `SetProperty`） | 与 Surface Json→buffer 对等，仅入口不同 |
| `rename_comp` / `move_comp` | `SetProperty(m_Name)` / `MoveComponent` | 扩原语后 Debug 直调 Services；GUI `Submit*` 为薄壳 |

### 6.5 获取 `SceneEditorServices` 的方式

Debug 与 Surface **同一实例**（Editor 持有，经 `IEditorContext` 或等价访问）。  
禁止 Debug 自建第二份 Services。

### 6.6 验收（Debug 专条）

- [ ] `EditorSceneDebugCommands` 中 §6.4 表内「迁后」列命令不再调用对应 `Submit*`（改调 Services）
- [ ] 同一 Scene session 上：Debug `add_comp` 与 MCP `scene.add_component` 产生 **同类** Undo 条目（同栈）
- [ ] 失败时 Console 能区分「名找不到」（Debug 层）与 Service `Code`（如 PrefabConstrained）
- [ ] Debug **不**触发 Service 侧 Activate

---

## 7) 迁移面

### 7.1 必须

| 单元 | 动作 |
|------|------|
| 协议型 `*Automation*Services` | 替换为 `SceneEditorServices`；删协议主 API |
| `EditorAutomationSurface` | Guid/Json/Hierarchy 组装全在此；调原语 |
| Codec | 仅 Surface（或 Automation 适配目录） |
| Host | 持有/引用 `SceneEditorServices` |
| `EditorSceneDebugCommands` + 属性 `set` 回调 | 按 §6 改为 Client→Services |

### 7.2 本波不做

| 项 | 原因 |
|----|------|
| Hierarchy/Inspector 读改走 Services | 本就该自理 Scene |
| GUI `Submit*` 全量 | 可选后置；**S04 已迁 Scene 发 cmd 路径**（见下） |
| Active / 切编辑器 API | 另 Feature |
| MCP TS/schema | 对外行为尽量不变 |
| Create Prefab / Material / Anim Submit | 超出本波；Create Prefab 属资产 Create 族，另议 |
| Gizmo Transform | `SceneEditorServices::SetTransform` → `EditorSetGameObjectTransformCommand`（before=按下，after=松手；不走 SetProperty） |

### 7.2.1 S04 Scene GUI 发 cmd 收敛（2026-09-18）

| 类 | 决策 | 落地 |
|----|------|------|
| A：Create/Delete/Add/Remove/Reparent/SetProperty(blobs) | 迁 Services；`Submit*` 薄壳 | Done |
| B2 Rename | → `SetProperty(m_Name)`（可加 Rename API，当前用 SetProperty） | Done |
| B3 MoveComponent | 加 `MoveComponent` 原语并迁 | Done |
| B4 Instantiate | **只迁发 cmd**；文件对话框/加载留 Editor 控制流；Create Prefab 先不管 | Done |
| B1 Transform gizmo | `SetTransform(before, after)` 专用 command（非 SetProperty）；拖动仍是控制流，松手一次入栈 | Done |
| Material/Anim | 后置 | Out |

### 7.3 删除列表（Done）

- [ ] 无服务层 `GetHierarchy`  
- [ ] 无服务主路径 `Json` 入参/作为 Result 载荷  
- [ ] 无 Edit 路径 `ActivateSession`  
- [ ] 无长期双契约（协议服务 + 原生服务并存）  
- [ ] §6.4 表内必迁 Debug 命令不再直 `Submit*` / `new Command`

---

## 8) 切片

| 切片 | 内容 | 验收 |
|------|------|------|
| **S01** | `SceneEditorServices` 原语契约 + Surface 适配（对外 op 行为保持） | accept:mcp / Pipe PASS |
| **S02** | 删除旧协议型服务 API；Codec 归属厘清 | 无双契约 |
| **S03** | Debug 按 §6 接入（含属性 set） | §6.6 + 与 MCP 同栈 Undo |
| **S04** | Scene GUI 发 cmd → Services（含 Gizmo `SetTransform`；不含 CreatePrefab/Material） | Submit 薄壳；Debug rename/move/set 同栈 |

---

## 9) 风险

| 风险 | 缓解 |
|------|------|
| Surface 把 Hierarchy 逻辑写歪 | 单测/手验 get_hierarchy；逻辑只在 Surface |
| void* size 不匹配 | Service 用 `MEProperty` 存储 size 校验 |
| 有人再加回 GetHierarchy「图个方便」 | §1 否决；Review 拒绝 |
| Debug 仍调 Submit「图省事」 | §6.6 / S03 门禁；与 MCP 对照手验 |
| 无 Activate 导致「Undo 菜单对不上 Automation 刚改的文档」 | **预期**：全局 Undo 跟 Active Tab；查该 session 栈。切文档另能力 |
| Debug 选错 session | §6.2 步骤 3 写清：与 ActiveScene 绑定的文档 Session |

---

## 10) 验收

- [ ] §3–§5 落地：原语 Services；Surface 适配；无服务侧 Hierarchy/Json/Activate  
- [ ] **§6 Debug 接入完成**（§6.6）  
- [ ] 命名 `*EditorServices`  
- [ ] 对外 Automation 无回归  
- [ ] §7.3 清零  
- [ ] Progress + Registry Done

---

## 11) Status note

| 字段 | 内容 |
|------|------|
| Reason | 思想纠偏；评审后开工 |
| Impact | Impl 进行中 |
| Decision | **已批准方向；§5.2 绑定/Prefab 按评审默认落地** |
| Next | 无；余量 Create Prefab / Material / Anim / 字面量→void* |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | 挂账：GUI/Debug 事后收敛 |
| 2026-09-18 | 旧稿：GUI 迁入字符串服务（**否定**） |
| 2026-09-18 | 重写：EditorServices + Surface 协议 |
| 2026-09-18 | **再重构：** 原语观（无 GetHierarchy）；Result 瘦身 + out；Get/Set = path+void*（形态 A）；无 typed API；Service 不管 Active Session；失败小枚举 |
| 2026-09-18 | §5.2 与 §5.1 对齐：`GetScene`/`Find*` 改为 Result + out |
| 2026-09-18 | **新增 §6 DebugCommand 接入**：Client 职责、调用链、命令对照、验收；其后章节顺延 |
| 2026-09-18 | 评审修补：§5.2 Session→Scene MVP 绑定；Prefab 门禁本波强制；Status → In Progress |
| 2026-09-18 | S04：GUI 发 cmd vs 控制流拆分；Rename→SetProperty；Move/Instantiate 原语；Gizmo `SetTransform`；Status → Done |