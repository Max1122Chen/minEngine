# ED-F03 — Debug Console & Unified Command System

## Meta
- **ID:** `ED-F03`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-02（§10.2 S08 Done；§10.4 S10 Scene 对象命令）
- **Branch:** `feat/editor`（Runtime Command 核心可合入 `master`；Console UI 在 Editor）
- **Depends on:** P4 Reflection · Serialization property path · `CORE-F07`（展示名，inspect 可读性）
- **Related:** [Implementation](./ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_IMPLEMENTATION.md)（待建） · [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../ACTIVE_WORK.md) · 外部参考 [Debug Console Design Guide](../../external/minEngine%20Debug%20Console%20%26%20Command%20System%20Design%20Guide.md) · [CORE-F07](../Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md)

## TL;DR

构建 **Runtime Development Interface**：统一 **Command System**（结构化描述、可发现、可校验、可补全）+ **Reflection Property 访问**（PropertyPath、`get`/`set`/`inspect`），Console ImGui 窗口只是 Frontend 之一。与现有 `IEditorCommand`（Undo 事务）**分层共存、逐步收敛**，不另起一套 `DebugCommand`。长期目标：**人类、Editor、自动化测试、AI Agent 共用同一套 Engine 控制面**——本项目差异化基础设施。

## Scope
- **In（MVP + 架构定稿）：**
  - Runtime：`CommandRegistry`、`CommandDescriptor`、参数 Schema、`CommandContext`、`CommandExecutor`、内建 meta 命令（`help`、`get`、`set`、`inspect`、`find`）
  - `PropertyPath` 解析与反射读写（优先 setter / 序列化路径，非裸 offset）
  - `CompletionService`、`ValidationService`（type-aware，MVP 可渐进）
  - `CommandHistory`（↑↓、prefix-aware）
  - Editor：`ConsoleWindow` **Command Tab**（输入、输出、补全 UI）；Output Tab 保留现有 Logger
  - Logger → Console **订阅**（展示），Logger 不依赖 Console
  - 与 `IEditorCommand` / `EditorCommandStack` 的边界与迁移策略（文档 + 首批示范命令）
  - Agent 结构化入口 **接口预留**（`discover` / `execute` JSON），MVP 可不接 UI
- **Out（本期或近端不做）：**
  - 全量 Editor 菜单/快捷键迁到 Unified Command（仅示范 + 设计钩子）
  - Dynamic 反射委托、Lua 控制台 REPL
  - Snapshot / Diff / Restore、Transaction、Command Composition（设计预留 §8）
  - Play Mode 专用 runtime 域（`CORE-F05` 之后再接 Context）
  - 网络 RPC、远程 Console
  - 强制所有 Runtime 命令 Undoable

## Reader quick start
1. 本文件 — 架构、模块边界、API 契约、MVP 切片。
2. 外部 [Design Guide](../../external/minEngine%20Debug%20Console%20%26%20Command%20System%20Design%20Guide.md) — 理念与 Agent-friendly 原则（Tier B 参考）。
3. [CORE-F07](../Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) — inspect 展示名。
4. 代码入口（落地后）：`Runtime/Core/Command/`、`Runtime/Core/PropertyPath/`、`Editor/src/UI/EditorWindows/ConsoleWindow.*`（Command Tab）。

---

## 0) Pre-flight（2026-09-02）

| 项 | 结论 |
|----|------|
| 反射 | `MEProperty`、metadata、`Serializer::SerializePropertyByPath` 已有；无通用 `PropertyPath` 模块 |
| 命令 | `IEditorCommand` + `EditorCommandStack` = **Undo 事务**；无 `CommandRegistry` |
| CLI | `CLI-F01` 统一引擎 CLI（`minEngineTests` 等）；与 Console **不同入口**，可共享 Runtime 命令注册 |
| Logger | `ME_CORE_*` / spdlog；无 Console sink |
| 债 | **medium** — PropertyPath 与 Undo 路径一致性；Completion 与 ImGui 焦点 |
| WIP | `feat/editor` 上 ED-F02 收尾；`CORE-F07` 建议先 land |
| 建议 | **Go** — 先 Runtime 竖切（Registry + PropertyPath + meta 命令 + headless 测试），再 Editor Console UI |

---

## 1) 核心定位与原则

### 1.1 不是什么

- 不是「输入作弊码的 TextBox」。
- 不是 `std::function<void(std::string)>` 字符串回调表。
- 不是与 `EditorCommand` 平行的第三套 `DebugCommand`。

### 1.2 是什么

> **Engine Runtime Control Plane** — 可发现、可描述、可校验的结构化操作与状态访问层。

```text
                    Unified Engine Interface
                              │
          ┌───────────────────┼───────────────────┐
          ↓                   ↓                   ↓
       Editor            Debug Console          Agent
    (Palette/Menu)      (Text REPL)         (JSON API)
          │                   │                   │
          └───────────────────┼───────────────────┘
                              ↓
                    Command System + PropertyPath
                              ↓
                    Reflection + Runtime
```

### 1.3 设计原则（必须遵守）

| # | 原则 | 含义 |
|---|------|------|
| P1 | **Schema-first** | 每个命令有机器可读的参数描述；Console/Agent 不靠猜 |
| P2 | **Command ≠ Property** | 动作（`spawn`）与状态（`Player.Health`）分模型，Console 同时暴露两者 |
| P3 | **Frontend 无关** | 命令不感知触发来源（菜单 / 文本 / JSON） |
| P4 | **Reflection 优先** | `get`/`set`/`inspect` 走 `MEProperty` + Serializer path，非 memory offset |
| P4b | **PropertyPath 有边界** | **不**强行让 `get`/`set` 覆盖 GO/MEObject 引擎字段（如 `m_Name`、`Invisible` 元数据）；此类操作用 **专用 Scene 命令**（`rename`、`activate`…）走 `IEditorCommand` |
| P5 | **Completion 一等公民** | 发现与补全与执行同等重要（Agent-friendly 前提） |
| P6 | **Logger 解耦** | Log 管道独立；Console 可订阅展示 |
| P7 | **渐进迁移** | 保留 `IEditorCommand` Undo 栈；Unified Command 通过 Adapter 接入，不 big-bang 重写 Scene 编辑 |

---

## 2) 现状盘点

### 2.1 已有能力

| 能力 | 状态 | 位置 |
|------|------|------|
| 反射类型/属性/函数 | 有 | `Runtime/Core/Reflection/` |
| 属性 metadata | `DisplayName`、`Category`、`ReadOnly`… | `MEProperties.h` |
| Inspector 展示/编辑 | `PropertyEditPolicy` + widgets | `Editor/src/UI/Property/` |
| 属性路径序列化 | `SerializePropertyByPath` | Serialization |
| Editor Undo 命令 | `IEditorCommand` | `EditorCommandStack` |
| Scene 编辑命令 | AddComponent、Delete、Transform… | `Editor/src/Commands/Scene/` |
| 统一 CLI | `CLI-F01` | `minEngine/bin` |

### 2.2 缺口

| 缺口 | 影响 |
|------|------|
| 无 `CommandRegistry` | 无法发现、无法 Agent `discover()` |
| 无 `PropertyPath` | Console 无法 `get Player.Transform.Position` |
| 无 Console UI | 无开发时 REPL |
| 无 Completion/Validation | 体验差、Agent 易错 |
| Editor 菜单与命令未结构化 | Command Palette 无法复用 |

### 2.3 与 `IEditorCommand` 的关系

```text
【分层 — 推荐】

Layer A: Unified Command System (ED-F03)
  - 描述：名称、参数、scope、execute 函数
  - 用于：Console、Agent、未来 Command Palette
  - 可选：Undo 钩子（声明式）

Layer B: Editor Undo Transaction (现有)
  - IEditorCommand::Execute / Undo
  - 用于：Inspector 拖拽、Scene 结构性编辑
  - 由 Layer A 的 editor.* 命令内部构造（Adapter）
```

**本期不删除 `IEditorCommand`。** 新增 **`undo` / `redo`** meta 命令（无 `editor.` 前缀）调用 `EditorCommandStack`；Scene 变更类统一命令（如 `scene.delete_entity`）内部 `Submit` 现有 Command 对象。Console 的 **`undo` / `redo` 命令**与 Editor **场景级** Ctrl+Z / Ctrl+Y（`EditorInputHub`）共用同一 `TryUndo` / `TryRedo` 接口；**Command 输入框内** Ctrl+Z / Ctrl+Y 仍为 ImGui **文本撤销/重做**（见 §10.1.3）。

---

## 3) 目标架构

### 3.1 模块与目录（建议）

```text
minEngine/minEngine/src/Runtime/Core/
  Command/
    CommandTypes.h           // CommandScope, CommandFlags, ArgType…
    CommandDescriptor.h
    CommandRegistry.h / .cpp
    CommandContext.h
    CommandExecutor.h / .cpp
    CommandParser.h / .cpp   // 文本 → InvokeRequest
    CommandHistory.h / .cpp
    CommandResult.h          // 统一返回（成功/错误/结构化 payload）
    BuiltinCommands/         // help, get, set, inspect, find
  PropertyPath/
    PropertyPath.h / .cpp    // 解析、遍历、读写
    PropertyPathTypes.h
  Console/                   // 可选：headless ConsoleService（无 ImGui）
    ConsoleService.h

minEngine/Editor/src/
  UI/EditorWindows/
    ConsoleWindow.h              // 扩展：Output | Command Tab
    CommandConsolePresenter.h    // 输出缓冲 + 渲染
    CommandConsoleStyle.h        // Kind → EditorSemanticColors 映射
  Services/
    DebugConsoleModule.h / .cpp   // 注册 Editor 命令、绑 Context
  Command/
    EditorCommandContext.h        // 填充 CommandContext（Selection、SceneEditor…）
```

**依赖方向：** Editor → Runtime.Command；Runtime **不** include Editor。

### 3.2 数据流

```text
【Console 文本输入】

User types "> set Light_0.m_Intensity 2"
    ↓
CommandParser::ParseLine
    ↓
  是注册命令？ ──yes──→ CommandExecutor::Execute(name, args, context)
    │                         ↓
    no                    CommandResult → OutputSink
    ↓
  是 property 表达式？ ──→ Builtin "set" 或 PropertyPath::Set
    ↓
  错误 → Validation 消息 + 补全建议
```

```text
【Agent JSON — 预留】

{ "op": "execute", "command": "list_go", "args": {} }
    ↓
CommandExecutor::ExecuteStructured(...)
```

---

## 4) Command System 规格

### 4.1 CommandDescriptor

每个命令一条静态或注册时构建的描述：

```cpp
enum class CommandScope : uint8_t { Editor, Runtime, Both };

enum class CommandFlags : uint32_t
{
    None       = 0,
    Undoable   = 1u << 0,  // 可提供 Undo 闭包
    Hidden     = 1u << 1,  // 不在 discover 列表
    DebugOnly  = 1u << 2,
};

enum class CommandArgType : uint8_t
{
    Bool, Int, Float, String, Enum, Guid, AssetPath,
    ObjectRef,   // 运行时对象名 / id
    StructJson,  // Agent 用
};

struct CommandArgDescriptor
{
    std::string_view Name;
    CommandArgType Type = CommandArgType::String;
    bool Required = true;
    std::string_view DefaultValue;
    std::string_view Description;
    float Min = 0.f, Max = 0.f;  // 可选
    std::span<const std::string_view> EnumValues;  // Type == Enum
    // CompletionProviderId — 扩展点
};

struct CommandDescriptor
{
    std::string_view Id;           // 稳定 ID："scene.open", "render.wireframe"
    std::string_view DisplayName;
    std::string_view Description;
    CommandScope Scope = CommandScope::Both;
    CommandFlags Flags = CommandFlags::None;
    std::span<const CommandArgDescriptor> Args;
    // Execute: (const CommandContext&, span<ParsedArg>) -> CommandResult
};
```

**命名约定：** 点分层级 `domain.verb` 或 `domain.sub.verb`（`scene.entity.delete`），便于 prefix 补全与 discover 过滤。

### 4.2 CommandRegistry

- **注册：** 静态初始化（`ME_REGISTER_COMMAND` 宏）+ 运行时 `Register(CommandDescriptor)`（插件/Editor 扩展）。
- **查询：** `Find(id)`、`List(prefix, scopeFilter)`、`Discover(query)` → 模糊 + prefix 排序。
- **线程：** MVP 单线程（Editor 主线程 / Game thread）；不承诺多线程无锁。

### 4.3 CommandContext

执行时注入环境；命令 **不得** 全局单例抓 Editor：

```cpp
struct CommandContext
{
    // 生命周期：一次 Execute 调用内有效
    World* World = nullptr;              // 可选：玩法世界
    Scene* ActiveScene = nullptr;        // SceneManager 当前场景
    class ICommandEditorBridge* Editor = nullptr;  // Editor 专用；Runtime 为 null

    // 由 Editor 实现：Selection、AssetWorkflow、CommandStack…
};
```

`ICommandEditorBridge`（Editor 侧接口，示意）：

```cpp
class ICommandEditorBridge
{
public:
    virtual EditorCommandStack& GetCommandStack() = 0;
    virtual SceneEditor* GetSceneEditor() = 0;
    virtual const std::vector<GameObject*>& GetSelectedGameObjects() = 0;
    // …
};
```

### 4.4 CommandResult 与输出行模型

统一返回，便于 Console 与 Agent 共用。**Console UI 不直接解析 `Message` 字符串上色**，而是消费 **结构化输出行**（`CommandOutputLine`），保证颜色、缩进、复制行为一致。

```cpp
enum class CommandStatus { Ok, Error, Cancelled, Warning };

enum class CommandOutputKind : uint8_t
{
  InputEcho,       // 用户输入回显：> list_go
  SuccessStatus,   // OK — 3 game objects
  Error,           // Error: unknown command …
  Warning,         // Warning: scene not saved
  Hint,            // Did you mean: list_go
  Plain,           // 普通正文
  ListItemName,    // list_go / find 结果：对象名
  ListItemMeta,    // 同行类型列：GameObject
  InspectHeader,   // inspect 根：Player : Character
  InspectSection,  // 分组：Movement
  InspectKey,      // 属性展示名（CORE-F07）
  InspectType,     // float / enum
  InspectValue,    // 1.2 / Walking
  ValueLiteral,    // get 返回值
  Path,            // PropertyPath 片段
  Muted,           // 次要说明、计数、时间戳
};

struct CommandOutputSegment
{
  CommandOutputKind Kind = CommandOutputKind::Plain;
  std::string Text;
};

struct CommandOutputLine
{
  std::vector<CommandOutputSegment> Segments;  // 单行可多段异色（SameLine 绘制）
  bool bSelectable = true;                     // 是否参与点击复制
};

struct CommandResult
{
  CommandStatus Status = CommandStatus::Ok;
  std::string Message;                         // Agent / 日志 / 无 UI 时纯文本 fallback
  std::vector<CommandOutputLine> Lines;        // Command Tab 首选渲染源
  std::optional<nlohmann::json> Data;            // 结构化（inspect 树、列表）
};
```

**约定：**

- `Execute` / Builtin 命令负责填充 `Lines`（或调用 `CommandOutputBuilder` 辅助类）；`Message` 为 `Lines` 的 plain-text 串联 fallback。
- Agent / headless 测试读 `Status` + `Data` + `Message`；**不**解析颜色。
- Copy 到剪贴板：**仅纯文本**（无 ANSI/颜色码），由 `Lines` flatten 生成。

```cpp
// 示意：list_go 输出
Lines = {
  { { { ListItemName, "Sun" }, { ListItemMeta, "GameObject" } } },
  { { { SuccessStatus, "OK — 3 game objects in scene 'test'" } } },
};
```

### 4.5 CommandParser（文本 Frontend）

- 输入：单行字符串（MVP）；引号支持字符串参数。
- 输出：`ParsedInvocation { commandId, vector<ParsedArg>, rawPropertyPath? }`。
- 语法（MVP）：
  - `command.id arg1 arg2`
  - `get <PropertyPath>`
  - `set <PropertyPath> <value>`
  - `inspect <ObjectRef>`
  - `find <query>`（`type=Character`、`name=Foo`）

**错误信息**须带：失败位置、期望类型、候选补全（对接 Validation）。

### 4.6 CommandExecutor

```cpp
class CommandExecutor
{
public:
    CommandResult Execute(std::string_view commandId,
                          std::span<const ParsedArg> args,
                          CommandContext& context);

    CommandResult ExecuteLine(std::string_view line, CommandContext& context);

    // Agent 预留
    CommandResult ExecuteStructured(const nlohmann::json& request, CommandContext& context);
};
```

执行顺序：Validation（参数）→ Scope 检查 → 用户 `Execute` → 记录 History。

### 4.7 与 Undo 的集成

| 命令类型 | Undo 策略 |
|----------|-----------|
| `set` property | 可选：捕获 before/after blob（复用 Inspector Undo 路径） |
| `scene.delete_entity` | 内部 `DeleteGameObjectCommand` + CommandStack |
| `render.wireframe` | 通常不 Undo |
| Console 纯查询 | 无 Undo |

`CommandFlags::Undoable` 表示 **Unified 层** 可提供 undo 闭包；Editor 仍可用 `IEditorCommand` 实现细粒度事务。

---

## 5) PropertyPath & Reflection 访问

### 5.1 路径语法

```text
<ObjectRef>.<member>[.<nested>…][.<component>]   // 分量：.x .y .z .r .g .b

ObjectRef MVP:
  - GameObject 名称（场景内唯一，或第一个匹配）
  - 未来：GUID、@selection（Editor bridge）
```

**逻辑名 vs 展示名：**

| 操作 | 使用 |
|------|------|
| 解析 / 序列化 / set | `MEProperty::GetName()`（`m_Intensity`） |
| inspect 输出标签 | `CORE-F07` `GetPropertyDisplayName` |

Console 输入 MVP 接受 **逻辑名**；可选后续支持展示名别名解析。

### 5.2 PropertyPath API

```cpp
class PropertyPath
{
public:
    static std::optional<PropertyPath> Parse(std::string_view text);

    bool Resolve(const CommandContext& ctx, PropertyPathResolveResult& out) const;

    CommandResult GetValue(const CommandContext& ctx) const;
    CommandResult SetValue(const CommandContext& ctx, std::string_view literal) const;

    // inspect：返回类型、展示名、当前值、子属性列表
    CommandResult Inspect(const CommandContext& ctx) const;
};
```

**实现策略：**

1. 复用 `Serialization::Serializer` 的 path 遍历（与 Undo blob 一致）。
2. Primitive：字符串 ↔ 类型转换 + metadata Min/Max 校验。
3. Object 嵌套：递归 inspect；set 仅叶子或支持 JSON 字面（后期）。
4. ObjectPtr / Asset：MVP 只读或 GUID 字符串；完整 picker 补全属 Editor。

**S08 路径扩展（§10.2）：** 显式 Component 语法 `GOName@ComponentName.FieldPath`（`@` 分隔 GO 与 Component）；短路径 `GOName.Field` 仅在无歧义时保留。

### 5.3 `find` 命令

```text
find Character
find type=SpotLightComponent
find name=Enemy_01
```

实现：遍历 `ActiveScene` GameObject / Component 反射类型名匹配；返回列表（名称、类型、GUID）供补全与 Agent。

### 5.4 PropertyPath vs Scene 对象命令（边界）

**设计决策（2026-09-02，用户拍板）：**

| 能力 | 模型 | 示例 |
|------|------|------|
| **可序列化组件/嵌套数据字段** | `get` / `set` / `inspect` + `PropertyPath` | `Sun@DirectionalLightComponent.m_Intensity` |
| **对象级语义操作**（改名、启用/禁用、删除…） | **专用 Console 命令** → 现有 `IEditorCommand` | `rename Sun Sol`、`activate Sun@PhysicsComponent` |

**不做的方向：**

- 不为 `MEObject::m_Name`（`Invisible`）、GUID 等引擎内部字段扩展 PropertyPath 解析
- 不为「继承链上所有反射成员」统一开放 `set` — 只覆盖 **Inspector 已通过 Serializer path 编辑的 data 字段**

**专用命令收益：** 语义清晰、可独立校验（重名、空名）、与 Hierarchy/Inspector 共用 Undo、Agent schema 更稳定。

详见 **§10.4 S10**。

---

## 6) Completion & Validation

### 6.1 CompletionService

```cpp
struct CompletionItem
{
    std::string Label;
    std::string InsertText;
    std::string Description;
    CompletionKind Kind;  // Command, Argument, Property, EnumValue, ObjectRef
};

class CompletionService
{
public:
    std::vector<CompletionItem> Complete(std::string_view line,
                                         size_t cursorOffset,
                                         const CommandContext& ctx);
};
```

**阶段：**

| 光标位置 | 行为 |
|----------|------|
| 行首 / 命令名 | `CommandRegistry::List(prefix)` + fuzzy |
| 第一个参数 | 按 `CommandArgDescriptor.Type` + EnumValues |
| PropertyPath | 对象名列表 → 属性名 → 嵌套字段；**属性候选 `Description` = 反射类型名**（§10.2.2） |
| `find` query | `type=` 后接 Component 类名列表 |

与 **Command Palette**（未来）共用 `CompletionService` + `CommandRegistry`。

### 6.2 Completion 触发与刷新（Frontend 契约）

`CompletionService` 只负责 **「给定行文本 + 光标 → 候选列表」**；何时刷新、如何展示、如何把选中项写回输入框，由 **Editor `CommandConsolePresenter`** 按 §8.3 状态机实现。

| 触发 | 行为 |
|------|------|
| **每帧 / 每次输入变更** | 输入框内容或光标变化时调用 `Complete(line, cursor, ctx)`；有候选则进入 **CompletionOpen** |
| **Tab** | **接受当前高亮候选**（`InsertText` 写回输入框最后一个 token）；**不**用 Tab 轮询候选 |
| **Ctrl+Space**（可选） | 强制刷新并打开补全；无候选时静默 |
| **Esc** | 关闭补全会话（见 §8.3）；保留已输入文本 |

**刷新规则：**

- 有候选 → 保持 `CompletionOpen`；默认选中 index `0`（或保持用户已移动的 index，若仍有效）。
- 候选变空 → 退回 `Normal`；清空建议列表。
- 执行命令（Enter）→ 清空补全会话。

**写回输入框（硬性要求）：**

- 接受补全 **必须** 在 `ImGuiInputText` **Callback** 内通过 `DeleteChars` / `InsertChars` 修改 buffer（或等价 API），保证 ImGui 内部状态与 `m_InputBuffer` 同步。
- **禁止** 仅更新 Presenter 侧 `char[]` 而不同步 ImGui —— 否则会出现「popup 出现但输入框不填入」的断裂体验（当前实现缺陷，见 §8.3.5）。

### 6.3 输入模式状态机（History vs Completion）

Console 输入框同一组 **↑ / ↓** 键在不同模式下语义不同；实现须用显式模式区分，**不得**让 history 与 completion 同时响应。

```text
                    输入变更（有候选）
         ┌──────────────────────────────────────┐
         │                                      │
         ▼                                      │
    ┌─────────┐   Esc / 候选空   ┌──────────────┐
    │ Normal  │◄─────────────────│ CompletionOpen│
    └─────────┘                  └──────────────┘
         │                              │
         │ ↑ / ↓                        │ ↑ / ↓
         ▼                              ▼
    ┌──────────────┐              移动选中 index
    │ HistoryBrowse│              （不改变输入文本）
    └──────────────┘
         │ 任意输入 / Esc
         ▼
       Normal
```

| 模式 | 进入条件 | ↑ / ↓ | Tab | Enter |
|------|----------|-------|-----|-------|
| **Normal** | 默认；补全关闭 | 进入 **HistoryBrowse**，浏览 `CommandHistory` | 若有缓存候选则接受第 0 条；否则刷新后接受第 0 条 | 执行 |
| **CompletionOpen** | `Complete()` 返回非空 | 在候选列表内移动 **选中 index**（循环） | **接受当前选中项**写回输入框；保持 CompletionOpen 并刷新 | 执行（补全关闭） |
| **HistoryBrowse** | Normal 下按 ↑ 或 ↓ | 在 history 条目间移动；**替换整行输入** | 退出 HistoryBrowse → Normal（不插入补全） | 执行当前草稿 |

**优先级（必须遵守）：**

1. `CompletionOpen` 时，↑↓ **只** 操作补全选中项，**不** 触发 history。
2. 仅当 **非** `CompletionOpen` 时，↑↓ 才浏览 history。
3. 用户继续打字 → 退出 `HistoryBrowse`；若仍有候选则进入 `CompletionOpen`。

**History prefix-aware（保留）：** `HistoryBrowse` 时，若当前行有非空 prefix（如 `render.`），仅匹配以该 prefix 开头的历史项（§7.1）。

### 6.4 ValidationService

- 执行前：参数个数、类型、enum 范围、Required。
- `set`：目标是否 ReadOnly、类型是否可解析、Min/Max。
- 错误消息 **机器友好**：`expected float, got 'foo'` + `suggestions: [...]`。

---

## 7) CommandHistory & Logger

### 7.1 History

- 环形缓冲最近 N 条（默认 256）。
- ↑↓ 浏览；可选 prefix filter（输入 `render.` 后 ↑ 只搜 `render.*`）。
- MVP：内存 only；持久化 defer。

### 7.2 Logger 集成

```text
ME_CORE_* / spdlog
        ↓ (sink interface)
ConsoleLogSink ──→ DebugConsoleWindow 输出区
```

- Logger **不** `#include` Console。
- Console 可过滤 Severity / Category。
- Headless test 注册 `NullSink` 或 `VectorSink` 断言。

---

## 8) Editor Frontend — Debug Console UI

### 8.0 与现有 `ConsoleWindow` 的关系

仓库已有 **Output Console**（`Editor/src/UI/EditorWindows/ConsoleWindow.h`）：订阅 `LogConsoleStorage`，带 Level/Source 过滤、Pause、AutoScroll、搜索。

**推荐：不新建独立 dock 窗口，而是在同一 **「Console」** 面板内增加 Tab**，避免底部再占一条 dock 带。

| Tab | 职责 | MVP |
|-----|------|-----|
| **Output** | 引擎 Logger 流（现有实现迁入/保留） | 已有 |
| **Command** | REPL 输入、命令结果、补全 | ED-F03 新增 |

Dock 位置：保持现有 `EditorDockLayout` 底部 **Console** 槽位不变；用户习惯「底部看日志 + 敲命令」。

### 8.1 推荐布局（Command Tab）

**总体：上输出、下输入**（UE / Chrome DevTools Console 同类模式）。输出区约占 70–80% 高度，输入区固定一行（可 Shift+Enter 扩多行，Post-MVP）。

```text
┌─ Console ───────────────────────────────────────────────────────────── [×] ┐
│ [ Output ]  [ Command ]                                                    │
├────────────────────────────────────────────────────────────────────────────┤
│  Toolbar: [Clear] [Copy]   [ ] Echo input   [ ] Show suggestions          │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  （滚动输出区 — 仅已执行命令的回显与结果，见 §8.4）                          │
│  › list_go                          ← InputEcho                            │
│    Sun                    GameObject ← ListItemName + ListItemMeta         │
│  ✓ OK — 3 game objects              ← SuccessStatus                        │
│                                                                            │
├─ Suggestions ──────────────────────────────────────────────────────────────┤
│  ▌ list_go          List game objects in active scene    ← 选中行高亮       │
│    list_go          …                                    ← 非选中 Muted    │
│    find             Find game objects by name, type=, …                    │
│  （输入时动态刷新；最多 ~8 行可滚动；无候选时整块隐藏）                        │
├────────────────────────────────────────────────────────────────────────────┤
│  › list█                                                                   │
└────────────────────────────────────────────────────────────────────────────┘
```

**三区职责（重要）：**

| 区域 | 内容 | 何时更新 |
|------|------|----------|
| **滚动输出区** | 已执行命令的 echo + `CommandResult.Lines` | Enter 执行后 |
| **Suggestions 条** | 当前补全候选（IDE 式列表） | **每次输入变更**（live completion） |
| **输入行** | `›` + 单行草稿 | 实时 |

补全候选 **不** 写入滚动输出区（避免与命令结果混淆、避免刷屏）；用户期望的「边输入边看到匹配列表」由 **Suggestions 条** 承担，视觉与 IDE 自动完成列表等价。

### 8.2 交互规格

| 操作 | 行为 |
|------|------|
| **输入任意字符** | Live 调用 `CompletionService`；有候选 → `CompletionOpen` + 刷新 Suggestions 条 |
| **Enter** | 执行当前行；关闭补全；清空 Suggestions |
| **Shift+Enter** | 多行输入（Post-MVP）；MVP 可忽略或禁用 |
| **↑ / ↓** | **见 §6.3 模式表**：CompletionOpen → 移动选中候选；否则 → HistoryBrowse |
| **Tab** | **接受当前高亮候选**写入输入框（替换最后一个 token）；**不**用 Tab 在候选间轮询 |
| **Ctrl+Space** | 强制刷新补全（可选，与 Tab 首次刷新二选一配置） |
| **Esc** | CompletionOpen → 关闭 Suggestions，保持输入；再按 Esc → 清空输入（可选） |
| **Ctrl+L** | Clear 输出区（与工具栏 Clear 相同） |
| **点击 Suggestions 行** | 等同 Tab：接受该行候选 |
| **点击输出行** | 选中文本；双击复制整行 |

**焦点：** Command Tab 激活且未在拖拽视口时，输入框持焦；视口 RMB 导航（ED-F02 S04）优先，不抢焦点。

### 8.3 补全 UX（IDE 式）

对标 **VS Code / IDE 自动完成**：输入即过滤、列表常驻输入上方、选中行高亮、Tab 确认插入。

#### 8.3.1 Suggestions 条（非浮动 Tooltip）

- **位置：** 滚动输出区与输入行 **之间** 的固定横条（§8.1 示意图）；**不用** 输入框上方的 `ImGuiWindowFlags_Tooltip` 浮动窗作为主要 UI。
- **可见性：** `CompletionOpen` 且候选非空时显示；否则 **完全隐藏**（不占高度）。
- **容量：** 默认最多显示 8 行；超出 `BeginChild` 纵向滚动。
- **Live 刷新：** 每帧根据 `m_InputBuffer` + 光标调用 `CompletionService::Complete`；输入变化时重置选中 index 为 `0`（除非新列表仍包含原 `InsertText` 且 index 仍合法）。

#### 8.3.2 候选项视觉（选中 vs 非选中）

| 状态 | 样式 |
|------|------|
| **选中（active）** | 背景条：`HierarchySelectionBar` 或 `CommandHighlight` 半透明底；左侧 **2px 强调竖条**；主标签 `TextPrimary` **加粗** |
| **非选中** | 无背景；主标签 `CommandMuted` 或 `TextSecondary` |
| **描述列** | 始终 `CommandMuted`；过长省略号截断 |

与 §8.4 `CommandHighlight` / `ListItemName` 色板一致；实现通过 `CommandConsoleStyle::GetCompletionRowStyle(selected)` 集中映射。

#### 8.3.3 Tab 接受补全（写回规则）

1. 取当前 **选中 index** 的 `CompletionItem.InsertText`。
2. 定位输入行 **最后一个 token**（自最后一个空格/制表符至行尾，或整行若无空格）。
3. 用 `InsertText` **替换** 该 token（保留 token 前的命令与参数，如 `get Sun.` + 补全 `m_Intensity` → `get Sun.m_Intensity`）。
4. 写回后 **立即** 再调 `Complete` 刷新（便于连续补全下一段路径）。
5. **Tab 不循环候选**；切换候选仅用 ↑↓ 或鼠标点击。

#### 8.3.4 ↑↓ 与 History 的上下文切换（用户强需求）

用户必须能 **无歧义** 感知当前 ↑↓ 在做什么：

| 用户可见线索 | CompletionOpen | HistoryBrowse / Normal |
|--------------|----------------|----------------------|
| Suggestions 条 | **可见** | **隐藏** |
| 输入框行为 | 文本不变，仅列表高亮移动 | 整行替换为 history 条目 |
| 状态栏 hint（可选） | `↑↓ select · Tab accept · Esc close` | `↑↓ history · Esc cancel` |

实现：`CommandConsolePresenter` 持有 `ConsoleInputMode` 枚举（§6.3）；`InputText` callback 内根据 mode 分发 `CallbackHistory` vs completion navigation。

#### 8.3.5 已知实现差距（2026-09-02，待 S04b 修复）

当前 `feat/editor` 落地与本文目标 **不一致**，验收前须对齐：

| 项 | 设计目标 | 当前实现 |
|----|----------|----------|
| 刷新时机 | 输入时 live | 仅 Tab 时查询 |
| 列表位置 | Suggestions 固定条 | 输入框上方 Tooltip popup |
| Tab 写回 | Callback 内 InsertChars，输入框可见变化 | 可能只改 `m_InputBuffer`，ImGui 未同步 → **popup 无填入** |
| Tab 语义 | 接受当前选中项 | 重复 Tab 轮询候选 |
| ↑↓ | 有 Suggestions 时选候选，否则 history | 始终走 `CallbackHistory` |
| 选中高亮 | 背景条 + 加粗 | 仅文字颜色略亮 |

**切片：** 将上述对齐记为 **S04b — IDE-style Completion UX**（Runtime `CompletionService` 已具备，主要改 Presenter + `ConsoleWindow` 布局）。

### 8.4 语义配色与输出行渲染（§4.4）

Command Tab 使用 **等宽字体**（`EditorTypographyRole::Monospace` 或 BodySmall + 强制 monospace 字体槽）。颜色来自 **`EditorSemanticColors` 扩展 + 活动主题 `TextPrimary` / `TextSecondary`**，Dark/Light 预设同步，**禁止硬编码 RGB**。

#### 8.4.1 扩展 `EditorSemanticColors`（建议）

在 `EditorSemanticColors.h` 增加 **Command** 分组（与 `Log*` / `Diagnostic*` 并列）：

| 字段 | 用途 | Dark 参考（实现时入 preset） |
|------|------|------------------------------|
| `CommandPrompt` | 输入行 `›` 前缀 | 青蓝 `#33D9FF` 系（≈ `LogDebug`） |
| `CommandEcho` | 回显的用户输入文本 | `TextPrimary` |
| `CommandSuccess` | `OK`、✓ 状态行 | 绿（≈ `LogInfo`） |
| `CommandError` | `Error:`、✗ | 红（≈ `LogError`） |
| `CommandWarning` | `Warning:` | 黄（≈ `LogWarn`） |
| `CommandHint` | `Did you mean`、提示 | 琥珀（≈ `DiagnosticWarning`） |
| `CommandPath` | PropertyPath、`Sun.m_Intensity` | 浅蓝（≈ `DiagnosticInfo`） |
| `CommandValue` | 数字、bool、字符串字面量 | 青（≈ `LogDebug`） |
| `CommandType` | `GameObject`、`float`、`enum` | 淡紫 / 次要蓝（新 preset 色） |
| `CommandMuted` | 类型列、箭头 `→`、说明文字 | `TextSecondary` |
| `CommandHighlight` | 补全候选、hint 内强调片段 | 加亮 `TextPrimary` 或 `HierarchySelectionBar` |

MVP 可 **先映射到现有 `Log*` / `Diagnostic*`**，不必一次加满字段；但 UI 代码通过 `CommandConsoleStyle::GetColor(Kind)` 间接取值，便于后期换 preset。

#### 8.4.2 `CommandOutputKind` → 颜色 / 前缀

| Kind | 颜色 | 前缀 / 排版 | 示例 |
|------|------|-------------|------|
| `InputEcho` | `Prompt` + `Echo` | `› ` + 原文 | `› list_go` |
| `SuccessStatus` | `CommandSuccess` | 可选 `✓ ` | `✓ OK — 3 game objects` |
| `Error` | `CommandError` | `✗ Error: ` | `✗ Error: unknown command 'lis_go'` |
| `Warning` | `CommandWarning` | `⚠ Warning: ` | `⚠ Warning: no active scene` |
| `Hint` | `CommandHint` | 缩进 2；候选用 `Highlight` | `Did you mean: `**`list_go`**`?` |
| `Plain` | `TextPrimary` | — | 帮助正文 |
| `ListItemName` | `TextPrimary` | 缩进 2 | `Sun` |
| `ListItemMeta` | `CommandMuted` | 列对齐（tab 或固定列宽） | `GameObject` |
| `InspectHeader` | `TextPrimary` **粗体** | — | `Player : Character` |
| `InspectSection` | `CommandType` | 无缩进 | `Movement` |
| `InspectKey` | `TextPrimary` | 缩进 2 | `Light Color`（CORE-F07） |
| `InspectType` | `CommandMuted` | 列对齐 | `float` |
| `InspectValue` | `CommandValue` | 列对齐 | `1.2` |
| `ValueLiteral` | `CommandValue` | — | `2.5` |
| `Path` | `CommandPath` | — | `Sun.m_Intensity` |
| `Muted` | `CommandMuted` | — | `→`、`(3 items)` |

#### 8.4.3 典型命令的多色行

**`get`：**

```text
› get Sun.m_Intensity          [InputEcho]
Sun.m_Intensity  →  1.2        [Path] [Muted] [ValueLiteral]
```

**`inspect`：**

```text
› inspect Player               [InputEcho]
Player : Character             [InspectHeader]
  Light Color    float    1.2  [InspectKey] [InspectType] [InspectValue]
  Cast Shadow    bool     true
```

**`set` 成功：**

```text
› set Sun.m_Intensity 2.5      [InputEcho]
✓ OK                           [SuccessStatus]
```

**校验失败：**

```text
› set Sun.m_Intensity foo      [InputEcho]
✗ Error: expected float, got 'foo'   [Error]
  for path Sun.m_Intensity           [Path + Muted]
```

#### 8.4.4 渲染实现要点

- `CommandConsolePresenter::AppendResult(const CommandResult&)` 将 `Lines` 追加到滚动缓冲。
- 每行：`for (segment : line.Segments) { ImGui::TextColored(style.Get(segment.Kind), "%s", seg.Text); ImGui::SameLine(0,0); }`
- **空行分隔**：每次 `Execute` 完成后追加一行 `Muted` 分隔（可选，默认可关闭）。
- **选中 / 复制**：整行 flatten 为纯文本；颜色不写入剪贴板。
- **Echo 开关**：关闭时跳过 `InputEcho` 行，仅显示结果（工具栏 checkbox）。

#### 8.4.5 与 Output Tab 的区分

| Tab | 配色体系 |
|-----|----------|
| **Output** | 沿用 `LogLevel` → `LogTrace`…`LogCritical`（现有 `ConsoleWindow`） |
| **Command** | `CommandOutputKind` → `Command*` 语义色（本节） |

两 Tab **色板同源**（`EditorSemanticColors`），但语义映射不同，避免 Command 成功行与 Log Info 行混淆。

#### 8.4.6 无障碍与主题

- 对比度：错误/成功/警告与背景对比度 ≥ WCAG AA（实现时目视 Dark + Light）。
- 色盲友好：**成功/错误除颜色外必有 ✓/✗ 前缀**；hint 用引号包裹候选命令。
- 自定义主题：Command 色跟随 preset；未来 `CustomPalette` 可暴露 Command 分组（Post-MVP）。

### 8.5 备选方案（未选用）

| 方案 | 说明 | 结论 |
|------|------|------|
| A. Tab：`Output` \| `Command` | 复用 dock、职责清晰 | **选用** |
| B. 独立 `Debug Console` 窗口 | 与 Logger 分离 | 占 dock；与现有 Console 重名混淆 |
| C. 单行 overlay（全屏底部） | 游戏内风格 | 不适合 Editor 多面板 |
| D. Command 与 Log 混排同一流 | 单列表 | 后期可加「Merged」模式；MVP 分开 |

### 8.6 菜单与快捷键

- **Window → Console**（已有）— 打开面板；记住上次 Tab。
- **Ctrl+`** 或 **F1**（实现时二选一）：聚焦 Command 输入框并切到 Command Tab。
- Output Tab 快捷键保持与现有一致。

### 8.7 DebugConsoleModule

- `OnEditorInitialize`：注册 Editor scope 命令；创建 `CommandContext` + `ICommandEditorBridge`。
- 每帧或 Command Tab 可见时：更新 Context（当前 Scene、Selection）。
- `ConsoleWindow` 扩展为持有 `CommandConsolePresenter`（输出缓冲 + **CommandConsoleStyle** + 输入状态 + Completion），或拆 `CommandConsoleWidget` 供 Tab 调用。

### 8.8 首批注册命令（示范）

| ID | Scope | 说明 |
|----|-------|------|
| `help` | Both | 列表 / `help <cmd>` |
| `get` / `set` / `inspect` / `find` | Both | Builtin meta |
| `list_go` | Editor | 列举当前场景 GameObject（名称 + 类型） |
| `undo` / `redo` | Editor | 转 `EditorCommandStack`（与场景 Ctrl+Z / Ctrl+Y 共用 `TryUndo`/`TryRedo`，§10.1） |
| `render.wireframe` | Both | 切换 debug draw（若已有 flag） |

`list_go` 示例输出：

```text
> list_go
  Sun                         GameObject
  Player                      GameObject
  Directional Light           GameObject
OK — 3 game objects
```

---

## 9) Agent-friendly 接口（设计预留，MVP 可选实现）

### 9.1 目标

Agent **不**模拟键盘、**不**读 UI；使用结构化 API：

```json
{ "op": "discover", "prefix": "scene." }
{ "op": "execute", "command": "get", "args": { "path": "Sun.m_Intensity" } }
{ "op": "inspect", "target": "Player" }
```

### 9.2 Schema 导出

`CommandRegistry::ExportSchema()` → JSON 数组（command id、args、types、descriptions）。供文档生成与 Agent system prompt。

### 9.3 与 CLI-F01 的关系

- `minEngineTests` / 引擎 CLI：**不**替代 Console；可新增 `minEngine.exe console-script file.txt` 批处理（后期）。
- 测试：直接调 `CommandExecutor` + headless `CommandContext`（无 ImGui）。

---

## 10) MVP 切片（Implementation Plan 预告）

| Slice | 内容 | 验证 |
|-------|------|------|
| **S00** | `CommandTypes` + `CommandRegistry` + `help` | 单元测试 list/find |
| **S01** | `PropertyPath` parse + `get`/`inspect`（只读） | 测试 + headless |
| **S02** | `set` + Validation（primitive） | 测试 set 后 Inspector 一致 |
| **S03** | `find` + object ref 补全 | Editor 场景内 find |
| **S04** | `CompletionService` + `CommandHistory`（Runtime + 基础 Presenter 接线） | headless + 手动 |
| **S04b** | **IDE 式补全 UX**（§6.3、§8.3）：live Suggestions 条、选中高亮、Tab 写回、↑↓ 模式分离 | Editor 目视 + 交互回归 |
| **S04c** | **Type-aware value completion**（§6.4）：`set <path> ` 后 bool/enum 补全；输入合法性整行着色（Valid/Partial/Invalid）；enum `set` | `command-system` + Editor 目视 |
| **S05** | Console `Command` Tab + `CommandConsoleStyle` 配色 | Editor 目视 Dark/Light — **验收 C 已通过**（2026-09-02） |
| **S06** | **`undo` / `redo` + Console `set` → CommandStack**（§10.1） | **Done** — `TryUndo`/`TryRedo`；`set`→`SetObjectPropertyCommand` |
| **S07** | `ExportSchema` JSON（Agent 预留） | **Deferred** — 测试 schema 快照 |
| **S08** | **PropertyPath v2** — `@` 显式 Component、歧义检测；**属性补全显示类型名**（§10.2） | **Done** — `command-system` 20 cases |
| **S09** | **Validation 增强** — Min/Max、Required、机器友好错误 + 建议（§10.3） | `command-system` + Console |
| **S10** | **Scene 对象命令** — `rename` / `activate` / `deactivate`（§10.4） | Undo 往返 + 与 Inspector 一致 |

**建议顺序：** S00 → … → **S06** ✓ → **S08** ✓ → **S09** → **S10** →（Feature 可收口）；**S07** 按需。

**注：** S10 的 `activate`/`deactivate` 依赖 **`CORE-F06` Component Enable** land 到 `feat/editor`（或 merge `master`）后再实现；`rename` 可先做。

**Deferred（非硬卡点）：** Console 极矮窗口 min-size 布局；S07 ExportSchema；Command Palette。

**前置：** `CORE-F07` S01 可与 ED-F03 S01 并行；inspect 展示名在 S05 前接入即可。

**S04 / S04b 分界：** S04 交付 `CompletionService` API 与 headless 可测逻辑；S04b 交付 Editor 侧 IDE 式交互（本节 §6.3、§8.3），**Console 补全验收以 S04b 为准**。

### 10.1 S06 — `undo` / `redo` 与 Console `set` 走 CommandStack

**状态：** Done（2026-09-02）

**用户约定（2026-09-02，修订）：**
- Console 命令名为 **`undo` / `redo`**（**不要** `editor.undo` 命名空间）
- **`undo` / `redo` 命令**与 Editor **场景级** Undo/Redo（菜单、Viewport 等 **`EditorInputHub` Ctrl+Z / Ctrl+Y**）共用 **`TryUndo` / `TryRedo`** → `EditorCommandStack`
- **Command 输入框聚焦时**：Ctrl+Z / Ctrl+Y = **文本**撤销/重做（ImGui 默认）；**不**劫持为场景 Undo
- **S06 范围包含**：Console `set` → `CommandStack`（`SetObjectPropertyCommand`）
- S07 ExportSchema **延后**；Console 目视验收 C **已通过**；极矮布局 **延后**

#### 10.1.1 命令契约

| 命令 | Scope | 行为 | 输出 |
|------|-------|------|------|
| `undo` | Editor | `CommandStack::Undo()` 若 `CanUndo()` | Ok：`Undid: <PeekUndoDescription>`；否则 Warning：`Nothing to undo` |
| `redo` | Editor | `CommandStack::Redo()` 若 `CanRedo()` | Ok：`Redid: <PeekRedoDescription>`；否则 Warning：`Nothing to redo` |

- 注册于 `EditorConsoleCommands`（与 `list_go` 同级），**不**放入 Runtime Builtin。
- `help` 列表可见；Completion 可补全命令名 `undo` / `redo`。

#### 10.1.2 场景 Undo 单实现（`TryUndo` / `TryRedo`）

抽取 Editor 侧薄封装（建议 `Editor/src/Shell/EditorUndoRedoActions.h`）：

```cpp
struct EditorUndoRedoResult { bool bPerformed; const char* Description; };

EditorUndoRedoResult TryUndo(IEditorContext& context);
EditorUndoRedoResult TryRedo(IEditorContext& context);
```

**调用方（场景级 Undo/Redo — 必须全部走此 API）：**

1. **`EditorInputHub::ProcessGlobalUndoRedoShortcuts`** — Ctrl+Z / Ctrl+Y（`!WantTextInput` 时生效）
2. **`MainMenuWindow` Undo/Redo 菜单项**
3. **Console `undo` / `redo` 命令** — 将 `TryUndo`/`TryRedo` 结果格式化为 `CommandResult`

三者行为一致：同一 `EditorCommandStack`、同一 `PeekUndoDescription` / `PeekRedoDescription`。

#### 10.1.3 Command 输入框 vs 场景快捷键（分界）

| 上下文 | Ctrl+Z / Ctrl+Y | 场景 Undo 方式 |
|--------|-----------------|----------------|
| **Command 输入框聚焦**（`WantTextInput`） | **文本**撤销/重做（ImGui `InputText` 内置） | 输入 `undo` / `redo` 命令 |
| **Viewport / Hierarchy / 非文本输入** | **场景** Undo/Redo（`EditorInputHub` → `TryUndo`/`TryRedo`） | 同上 + 快捷键 |
| **MainMenu** | — | Edit → Undo/Redo → `TryUndo`/`TryRedo` |

**实现约束：**
- **禁止**在 `CommandConsolePresenter` 中拦截 Ctrl+Z/Y 做场景 Undo。
- 保持 `EditorInputHub::ShouldProcessGlobalShortcuts` 对 `WantTextInput` 的现有判断（输入框激活时不走全局场景快捷键）。
- Console 用户若需撤销场景编辑：执行 `undo` / `redo`，或将焦点移出输入框后使用 Ctrl+Z / Ctrl+Y。

#### 10.1.4 Console `set` → `IEditorCommand`（§13 门禁）

当前 Builtin `set` 经 `PropertyPath::SetValue` **直接写场景**，**不进** Undo 栈 — §13「`undo` 可撤销 Console 触发的可 Undo 操作」未满足。

**S06 必须包含：**

- Editor 层拦截或替换 `set` 执行路径（`CommandScope::Editor` 专用 handler 或 Presenter 前置分发）：
  1. 解析 path + value（复用现有 `PropertyPath` / `SetValueValidation`）
  2. 序列化 **before** 值（与 Inspector `SetObjectPropertyCommand` 相同 buffer 格式）
  3. 写入 **after** 值
  4. `SceneEditor::SubmitSetObjectProperty` → `EditorCommandStack::Execute(SetObjectPropertyCommand)`
- Runtime headless `set`（`minEngineTests`、无 Editor）可保留直写路径，或测试仅跑 Builtin 注册前的路径 — **实现时二选一并在 Progress 记录**。

**验证：**

- Editor：`set Sun.m_Intensity 3` → `undo` → `get` 恢复原值；焦点在 Viewport 时 Ctrl+Z 等效
- Command 输入框内 Ctrl+Z 仅撤销**输入文本**，不改变场景属性
- `command-system`：至少 2 case（undo/redo 空栈 + set/undo 往返）；Editor 集成测 `undo`/`redo` 命令
- 菜单 Undo 与 Console `undo` 描述文案一致（同一 `PeekUndoDescription`）

---

### 10.2 S08 — PropertyPath v2（`GOName@Component.Field`）

**状态：** Done（2026-09-02）

**动机：** 当前 `PropertyPath` 在 GameObject 上 **Component 属性回退**（首个匹配 Component）对多 Component、同名字段可能**静默歧义** — 技术债自 S01 起已记录。

#### 10.2.1 路径语法（定稿）

**显式 Component 路径：**

```text
<GOName>@<ComponentName>.<FieldPath>
```

| 段 | 说明 | 示例 |
|----|------|------|
| `GOName` | 场景中 GameObject 名（与现有一致） | `Sun` |
| `@` | **固定分隔符** — GameObject 与 Component 之分界 | `Sun@` |
| `ComponentName` | 反射类名（短名或全名，与 `find type=` 一致） | `DirectionalLightComponent` |
| `FieldPath` | 组件内属性子路径（`.` 分段，与现有一致） | `m_Intensity` |

**完整示例：** `Sun@DirectionalLightComponent.m_Intensity`

**嵌套 struct / 对象字段：** 点在 `FieldPath` 内继续展开，例如 `Player@ReflectionSampleComponent.SampleData.EnumField`（S08 随 v1 嵌套能力一并验）。

**保留短路径（向后兼容）：** `Sun.m_Intensity` — 仅当**无歧义**（唯一 Component 含该字段）时解析成功；否则 **失败**并提示使用 `@` 显式路径。

**非目标（S08）：** 层级 GameObject 路径、`[]` 数组下标、Asset GUID、`@selection`。

#### 10.2.2 行为

| 能力 | 说明 |
|------|------|
| **解析** | `PropertyPath::Parse` 识别 `@` 段；`Resolve` 定位到指定 Component 实例 |
| **歧义检测** | 短路径多匹配 → Error + 列出 `GOName@Component.Field` 候选 |
| **补全** | 输入 `Sun@` 后补 Component 类型；`Sun@DirectionalLightComponent.` 后补字段；歧义时 Suggestions 展示完整 `@` 路径 |
| **属性类型显示** | 属性候选 `CompletionItem.Description` = **反射类型短名**（如 `float`、`bool`、`SampleEnum`、`Vector3`），**替代**固定文案 `property`；实现可复用 `SetValueValidation` / `MEProperty` 分类逻辑 |
| **get/set/inspect** | 显式路径与 Inspector / Undo buffer 使用同一 property 子路径 |

**验证：** `command-system` 多 Component 夹具；歧义错误含 ≥2 条 `@` 候选；`Sun@DirectionalLightComponent.m_Intensity` 与 Inspector 一致；Suggestions 行显示 `- float` 而非 `- property`。

---

### 10.4 S10 — Scene 对象命令（`rename` / `activate` / `deactivate`）

**状态：** Planned（S09 之后，或 `rename` 子步可提前）

**动机（2026-09-02）：** 用户期望能改 GO 名称、启用/禁用 Component，但 **不** 通过扩展 PropertyPath 暴露 `MEObject`/`Invisible` 字段。与 §5.4 一致：**专用命令 + 现有 `IEditorCommand`**。

#### 10.4.1 设计原则

| 原则 | 说明 |
|------|------|
| **命令 ≠ PropertyPath** | 对象级操作不进 `get`/`set` 路径语法 |
| **复用 Editor 命令** | Console 只做解析 + 分发，不重复 Scene 逻辑 |
| **可 Undo** | 全部 `EditorCommandStack::Execute`（与 S06 `set` 同模式） |
| **Scope** | `CommandScope::Editor`；注册于 `EditorConsoleCommands` |

#### 10.4.2 命令契约（MVP）

| 命令 | 语法 | 行为 | 现有落点 |
|------|------|------|----------|
| `rename` | `rename <GOName> <NewName>` | 场景内按名查找 GO → 改名 | `SceneEditor::SubmitRenameGameObject` → `RenameGameObjectCommand` |
| `activate` | `activate <GOName>@<ComponentName>` | 启用**指定 Component**（`@` **必填**） | **依赖 `CORE-F06`** `bEnabled` + 新/现有 Enable 命令 |
| `deactivate` | `deactivate <GOName>@<ComponentName>` | 禁用**指定 Component**（`@` **必填**） | 同上 |

**参数约定：**

- `<GOName>`：与 `find` / PropertyPath 一致，场景内名称匹配（首匹配或唯一性错误 — 与 S08 歧义策略对齐）。
- `<NewName>`：非空；空名行为与 Editor 一致（sanitize 为 `GameObject_<id>`）。
- `@<ComponentName>`：**`activate` / `deactivate` 必填**；与 S08 相同 Component 类名解析。**不提供**省略 `@` 的 GO 级一键启用/禁用（无 `activate Sun` / `deactivate Sun`）。

**输出示例：**

```text
> rename Sun Sol
Renamed 'Sun' → 'Sol'

> activate Sun@DirectionalLightComponent
Activated DirectionalLightComponent on 'Sun'
```

失败：Error + 可读原因（未找到对象、重名、缺少 `@`、Component 不存在、CORE-F06 未就绪等）。

#### 10.4.3 实现策略

1. **`EditorConsoleCommands`** 注册 `rename` / `activate` / `deactivate`（与 `list_go`、`undo` 同级）。
2. 解析层可抽 **`SceneCommandArgs`** 小工具：解析 `GOName`、`GOName@Component`（复用 S08 `@` 语法）。
3. **`rename`（S10a，无阻塞）**：可直接落地；`command-system` 不必覆盖（Editor 命令），但可加 headless 解析单测或 Editor 手动验收清单。
4. **`activate` / `deactivate`（S10b）**：**Blocked on `CORE-F06`**；Console 命令在 F06 land 后接线 `SubmitSetComponentEnabled`（或等价 `IEditorCommand`，实现时命名）。
5. **补全：** 命令名 + 第一参数 GO 名（复用 `ListGameObjectNames`）；第二参数 `rename` 不补全；`activate`/`deactivate` 参数形如 `GOName@` 后补 Component 类型（与 S08 共用，**不接受**无 `@` 的 GO 名补全）。
6. **ExportSchema（S07）**：上述命令纳入 schema 时标记 `undoable: true`。

#### 10.4.4 非目标（S10）

| 项 | 阶段 |
|----|------|
| `delete` / `duplicate` / `add_component` Console 命令 | 后续切片或 ED-F02 工作流 |
| PropertyPath 访问 `m_Name`、GUID、`Invisible` 字段 | **明确不做**（§5.4） |
| GO 级 `activate` / `deactivate`（无 `@`） | **明确不做**（§10.4.2） |

#### 10.4.5 验证

- `rename Sun Sol` → `find Sol` 命中；`undo` 恢复 `Sun`
- 菜单/Hierarchy 改名与 Console `rename` 共用 `RenameGameObjectCommand` 描述
- CORE-F06 就绪后：`deactivate Sun@X` → System 跳过该组件；`activate` 恢复；`undo` 往返
- `help` 列出三条命令；Completion 补全命令名

**建议子步：** **S10a** = `rename` only（可夹在 S08/S09 之间快速交付）；**S10b** = `activate`/`deactivate` after CORE-F06。

---

### 10.3 S09 — Validation 增强

**状态：** Planned（S08 之后或并行，视 S06 负载）

**动机：** §6.4 `ValidationService` MVP 由 `SetValueValidation` 覆盖 value 阶段；执行前校验、metadata 约束、Agent 友好错误尚未系统化。

**目标：**

| 能力 | 说明 |
|------|------|
| **执行前校验** | `CommandExecutor` 或 per-command hook：参数个数、Required、`CommandArgType` |
| **属性约束** | 反射 metadata：`ClampMin` / `ClampMax` / `ReadOnly`（已有部分在 `PropertyPath`）→ `set` 前校验并 **着色/错误消息** |
| **机器友好错误** | 统一格式：`expected float, got 'foo'` + `suggestions: [true, false]`（与 §6.4 对齐） |
| **与 S04c 整合** | `SetValueValidation` 升格为 `ValidationService` 子集或合并；避免双份类型推断逻辑 |

**非目标（S09）：** 跨字段交叉校验、自定义 Validator 插件、全命令 JSON Schema。

**验证：** `command-system` 覆盖 clamp 越界、readonly `set`、enum 非法值；Console 错误行分色与建议文本一致。

---

## 11) 非目标与远期（§24–27 对齐）

| 能力 | 阶段 |
|------|------|
| Snapshot / diff / restore | Post-MVP |
| Transaction（`begin`/`commit`） | Post-MVP |
| Command macro / composition | Post-MVP |
| Command Palette（Ctrl+P） | ED-F03 之后或 S06+ |
| 全 Editor 菜单迁 Unified Command | 渐进 |
| Lua REPL | 另 Feature |

---

## 12) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| PropertyPath 与 Undo 路径不一致 | set 破坏场景 | 复用 Serializer path；集成测试 |
| `IEditorCommand` 与 Unified 重复 | 维护负担 | Adapter 模式；文档边界；不双写 Scene 逻辑 |
| Completion 与 ImGui 焦点 / 输入模式 | 体验差、Tab 不写回 | §6.3 状态机；Tab 在 InputText Callback 内写回；Suggestions 固定条非 Tooltip |
| Scope 泄漏（Runtime 调 Editor 命令） | 崩溃 | Executor 强制 scope 检查 |
| 展示名 vs 逻辑名混淆 | set 失败 | 文档 + 错误提示列出合法成员名；对象级操作用专用命令（§5.4） |
| 体量过大 | 延期 | 严格 MVP；S05 前不追求 Palette |

---

## 13) 验收标准（Feature Done 前）

- [ ] `CommandRegistry` + ≥3 个示范命令 + Builtin meta 命令可 headless 测试
- [ ] `get`/`set`/`inspect` 对 Scene 内 GameObject Component  primitive 字段可用
- [x] Console UI：Command Tab 输入、执行；IDE 式补全（§8.3）；成功/错误/路径/值分色（§8.4）；Output Tab 日志无回归 — **目视验收 C 已通过**（2026-09-02；极矮布局延后）
- [x] **`undo` / `redo`** 可撤销 Console 触发的可 Undo 操作（与场景 Ctrl+Z / Ctrl+Y 共用 `TryUndo`/`TryRedo`）— **S06 Done**
- [ ] **Scene 对象命令** `rename`（+ CORE-F06 后 `activate`/`deactivate`）可 Undo，与 Inspector 行为一致 — **S10**
- [ ] `ExportSchema` 产出稳定 JSON — **S07 Deferred**
- [ ] `verify.ps1` 通过；`PROGRESS_LOG` 记录人工步骤

---

## 14) 参考对照表（外部 Guide → minEngine）

| 外部 Guide 概念 | minEngine 落点 |
|-----------------|----------------|
| Unified Command Registry | `Runtime/Core/Command/CommandRegistry` |
| CommandDescriptor / Schema | §4.1 |
| EditorCommand 统一 | §2.3 Adapter，保留 `IEditorCommand` |
| PropertyPath | `Runtime/Core/PropertyPath` + Serializer |
| Smart Completion | `CompletionService` |
| inspect/get/set/find/watch | MVP: get/set/inspect/find；watch Post-MVP |
| Logger 解耦 | `ConsoleLogSink` |
| Agent discover/execute | `ExportSchema` + `ExecuteStructured` 预留 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | 初版 Design（`feat/editor`）；自外部 Design Guide 适配 minEngine 现状 |
| 2026-09-02 | `list_go` 命名；§8 UI 示意图与 Tab 方案 |
| 2026-09-02 | §4.4 `CommandOutputLine`；§8.4 语义配色规范 |
| 2026-09-02 | §6.2–6.3、§8.2–8.3 修订：IDE 式 live 补全、Suggestions 条、输入模式状态机、Tab 写回契约、S04b 切片；记录当前实现差距 |
| 2026-09-02 | S04c：`SetValueValidation`、bool/enum value 补全、输入合法性整行着色、enum `set`；§10 切片表补 S04c |
| 2026-09-02 | S10：`activate`/`deactivate` **必须** `GOName@Component`（`@` 必填）；不提供 GO 级一键启用/禁用 |
| 2026-09-02 | 方案修订：§5.4 PropertyPath 边界；不扩展 `m_Name` 等引擎字段；**S10** Scene 命令 `rename`/`activate`/`deactivate`；S08 属性补全显示类型名；S10b 依赖 CORE-F06 |
| 2026-09-02 | **S06 Done：** `EditorUndoRedoActions`（`TryUndo`/`TryRedo`）；Console `undo`/`redo`；`set`→`CommandStack`（`EditorSetValue` hook + `TryBuildSetTransaction`）；`command-system` 16 cases PASS |
| 2026-09-02 | 排期修订：S06=`undo`/`redo`+CommandStack+TryUndo 与场景快捷键同接口；输入框 Ctrl+Z 仍为文本操作；S07 Deferred；S05 目视 C Done；S08=`GOName@Component.Field`；S09 Validation |
