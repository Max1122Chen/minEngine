# minEngine MCP Server Architecture Design Guide

## 1. Agent Role

你是一名参与设计 **minEngine / Maximum MCP Server** 的架构顾问与协作开发
Agent。

你的任务不是立即编写大量代码，而是基于现有项目架构，与用户共同讨论、验证并逐步设计一套：

-   稳定
-   可扩展
-   Agent-friendly
-   Human + Agent 共存
-   面向长期演进
-   尽量复用现有基础设施

的 MCP Server 与 Editor Automation 基础设施。

在提出方案时，必须区分：

1.  已经存在的项目能力；
2.  当前需要新增的基础设施；
3.  未来可能扩展的能力；
4.  尚未确认、需要向用户提问的设计决策。

不要假设项目中不存在某个系统，也不要重复设计已有的基础设施。

------------------------------------------------------------------------

## 2. Project Background

minEngine 是一个个人开发的 C++ 游戏引擎项目，主要用于：

-   学习游戏引擎架构与底层技术；
-   构建个人作品集；
-   为游戏客户端、游戏引擎相关实习或工作做准备；
-   探索 AI-assisted game development。

编辑器产品名为 **Maximum**，寓意是：虽然 minEngine
是一个小型个人引擎，但希望帮助用户释放最大的创造力。

当前项目已经或正在建设以下系统：

-   Engine Core / Runtime
-   Maximum Editor
-   World / Scene / GameObject / Component
-   AssetManager
-   Object / MEObject
-   GUID / UID / Object Identity
-   Reflection / Property Descriptor
-   JSON 与二进制 Serialization
-   Editor Undo / Redo
-   Debug Console
-   Lua / sol2
-   Delegate / Event
-   Material Graph
-   Animation System
-   UI System
-   RHI / Renderer
-   Profiler
-   Content Browser
-   Physics
-   Editor tooling

因此，MCP Server
的设计必须优先考虑如何复用这些系统，而不是重新创造一套平行架构。

------------------------------------------------------------------------

## 3. Long-Term Goal

最终目标不是简单地让 Agent 能够"调用几个编辑器命令"，而是建设一套通用的
**Agent-friendly Editor Automation Infrastructure**。

未来 Agent
应能够通过稳定、结构化、可验证的接口操作多种资产和编辑器对象，例如：

-   Scene / World
-   GameObject / Component
-   Material
-   Material Graph
-   Animation
-   Animation Graph
-   Prefab
-   UI
-   Project Settings
-   Import Settings
-   Editor Layout / Editor State
-   其他未来加入的资产类型

设计目标包括：

-   Agent 可以查询当前项目和编辑器状态；
-   Agent 可以准确定位对象，而不是依赖显示名称；
-   Agent 可以读取和修改属性；
-   Agent 可以创建、删除、复制、移动对象；
-   Agent 可以执行可验证的编辑操作；
-   Agent 可以获得结构化错误信息；
-   Agent 可以在修改后进行 Validation；
-   Agent 可以保存并重新查询结果；
-   人类仍然可以正常使用 Maximum；
-   Agent 的操作不会绕过 Undo/Redo、权限和一致性约束；
-   新增资产类型时，不需要重新设计整个 MCP Server。

------------------------------------------------------------------------

## 4. Core Architectural Principle

MCP Server 不应该成为引擎业务架构的中心。

MCP 是一种外部协议接入层，负责把 Agent 的请求转换成 Maximum
能理解的稳定自动化操作。

推荐的总体结构：

``` text
AI Agent
    │
    │ MCP
    ▼
MaximumMCPServer
    │
    │ Editor Automation Protocol
    ▼
Maximum.exe
    │
    ▼
Editor Automation API
    │
    ├── World / Scene
    ├── Asset System
    ├── Reflection
    ├── Serialization
    ├── Undo / Redo
    ├── Material Graph
    ├── Animation
    ├── UI
    └── Other Editor Systems
```

更完整的 MCP Server 内部结构可以是：

``` text
MaximumMCPServer
├── MCP Protocol Layer
├── Tool Registry
├── Resource Registry
├── Prompt Registry
├── Input Schema / Validation
├── Permission / Confirmation
├── Error Mapping
├── Session / Connection Management
└── Automation Client
        │
        ▼
Editor Automation Protocol
```

Maximum 内部可以是：

``` text
Maximum.exe
├── Automation Transport
├── Automation Request Dispatcher
├── Editor Automation API
├── Command / Transaction System
├── Query Services
├── Validation Services
└── Existing Editor / Engine Systems
```

------------------------------------------------------------------------

## 5. MCP Server 与 Maximum 的职责边界

### 5.1 MaximumMCPServer 应负责

-   MCP 协议处理；
-   Tool / Resource / Prompt 注册；
-   Agent-facing JSON Schema；
-   参数校验；
-   MCP 错误映射；
-   权限和确认策略；
-   会话管理；
-   与 Maximum 建立连接；
-   将 MCP 请求转换为 Automation Protocol 请求；
-   将 Maximum 返回的数据转换为 MCP 响应。

### 5.2 Maximum.exe 应负责

-   真正执行编辑器操作；
-   访问 World、Asset、Reflection、Serialization 等系统；
-   保证操作在正确的线程和编辑器上下文中执行；
-   管理事务、Undo/Redo；
-   保证对象生命周期与引用有效性；
-   执行资产和场景验证；
-   管理保存、加载和编辑器状态；
-   返回稳定、结构化、可诊断的结果。

### 5.3 不应采用的方式

不要让 MCP Server：

-   直接链接并调用 minEngine.dll 的内部 C++ ABI；
-   直接访问 SceneManager、AssetManager 等内部对象；
-   直接操作 ImGui；
-   直接修改内存；
-   依赖 C++ 类布局；
-   依赖临时变量地址；
-   通过对象名称猜测对象身份；
-   通过执行任意 C++ 代码完成所有操作。

MCP Server 应通过稳定的 Automation API 与 Maximum 通信。

------------------------------------------------------------------------

## 6. Automation API 是真正的核心

MCP Server 只是 Automation API 的一个使用者。

Automation API 未来还应服务于：

-   MCP Server；
-   Debug Console；
-   Editor UI；
-   Python Automation；
-   C++ Automation Tests；
-   批处理工具；
-   IDE 插件；
-   未来的其他 AI 工具。

因此，Automation API 不应设计成"专门为某个 MCP Tool 写的临时函数集合"。

推荐的抽象层次：

``` text
MCP Tool
    ↓
Automation Command / Query
    ↓
Editor Automation API
    ↓
Existing Systems
```

例如：

``` text
scene.set_property
    ↓
SetPropertyCommand
    ↓
Property System + Reflection + Undo
```

而不是：

``` text
MCP Tool
    ↓
直接调用某个 SceneManager 内部函数
```

------------------------------------------------------------------------

## 7. Query、Command、Validation 三类操作

应明确区分三种不同性质的接口。

### 7.1 Query

Query 用于读取信息，不应该产生副作用。

示例：

-   `project.get_info`
-   `editor.get_state`
-   `scene.get_active_scene`
-   `scene.get_hierarchy`
-   `scene.get_object`
-   `scene.get_component`
-   `scene.get_property`
-   `asset.get_info`
-   `asset.get_dependencies`
-   `material.get_graph`
-   `animation.get_graph`

Query 的特点：

-   只读；
-   结果结构化；
-   可以被缓存或重复调用；
-   不应修改场景或资产；
-   应明确返回数据版本或对象状态信息。

### 7.2 Command

Command 用于修改项目、场景、资产或编辑器状态。

示例：

-   `scene.create_game_object`
-   `scene.delete_game_object`
-   `scene.add_component`
-   `scene.remove_component`
-   `scene.set_property`
-   `scene.reparent`
-   `scene.duplicate_object`
-   `asset.create`
-   `asset.rename`
-   `asset.move`
-   `material.create_node`
-   `material.connect_nodes`
-   `animation.set_parameter`
-   `editor.open_asset_editor`
-   `editor.focus_object`
-   `editor.set_active_tab`

Command 应具备：

-   明确输入；
-   明确目标对象；
-   明确副作用；
-   结构化结果；
-   失败原因；
-   可选事务边界；
-   Undo/Redo 支持；
-   尽量幂等，或明确说明非幂等性。

### 7.3 Validation

Validation 用于检查操作结果和项目状态。

示例：

-   `scene.validate`
-   `asset.validate`
-   `material.validate_graph`
-   `animation.validate_graph`
-   `project.validate`
-   `editor.validate_state`

Validation 结果应尽量结构化，例如：

``` json
{
  "valid": false,
  "issues": [
    {
      "severity": "error",
      "code": "MissingReference",
      "message": "Component references an unavailable asset.",
      "objectId": "guid://...",
      "propertyPath": "Mesh"
    }
  ]
}
```

推荐的 Agent 工作流：

``` text
Query
  ↓
Plan
  ↓
Command
  ↓
Validation
  ↓
Save
  ↓
Query Again
```

------------------------------------------------------------------------

## 8. Object Identity

Agent 不应该依赖对象名称作为唯一身份。

名称可能：

-   重复；
-   被修改；
-   只在局部作用域内唯一；
-   因为重命名而失效；
-   无法区分不同实例。

应使用稳定的对象标识：

-   `ObjectId`
-   `GameObjectId`
-   `ComponentId`
-   `AssetId`
-   `NodeId`
-   `PropertyId`

例如：

``` text
guid://550e8400-e29b-41d4-a716-446655440000
```

名称只应作为展示信息或辅助搜索条件。

推荐的对象引用结构：

``` json
{
  "objectId": "guid://...",
  "name": "Player",
  "type": "GameObject"
}
```

如果对象存在层级关系，也应同时返回：

-   父对象 ID；
-   子对象 ID；
-   所属 Scene/World ID；
-   所属 Asset ID；
-   对象类型；
-   生命周期状态；
-   是否为 Prefab 实例；
-   是否存在 Override。

------------------------------------------------------------------------

## 9. Property Path 与 Reflection

Agent 不应依赖 C++ 内存布局或内部成员变量。

属性访问应基于稳定的反射和属性描述系统。

推荐使用类似以下形式的 Property Path：

``` text
Transform.Position
Transform.Rotation
Transform.Scale
Mesh
MaterialSlots[0]
Light.Intensity
Camera.FieldOfView
```

Property Path 应与以下系统协同：

-   Reflection；
-   Property Descriptor；
-   Serialization；
-   Undo/Redo；
-   Validation；
-   Type Conversion；
-   Editor Property UI。

理想情况下，同一个属性描述应能同时服务于：

-   Inspector；
-   JSON Serialization；
-   Binary Serialization；
-   Undo/Redo；
-   Automation API；
-   MCP Tool Schema；
-   Agent Query/Modify；
-   Validation。

例如：

``` text
Reflection Property Descriptor
        ├── Inspector
        ├── Serialization
        ├── Undo / Redo
        ├── Automation API
        └── MCP Schema
```

应考虑支持：

-   基础类型；
-   枚举；
-   向量；
-   颜色；
-   Asset Reference；
-   Object Reference；
-   数组；
-   Map；
-   Struct；
-   Nested Property；
-   Read-only Property；
-   Transient Property；
-   Editor-only Property；
-   Runtime-only Property。

------------------------------------------------------------------------

## 10. Tool 设计原则

Tool 粒度应处于合适的领域层级。

### 10.1 不要过低层

不推荐：

-   `write_memory`
-   `call_internal_function`
-   `set_raw_pointer`
-   `execute_arbitrary_cpp`
-   `modify_binary_offset`

这些接口会破坏稳定性、安全性和可验证性。

### 10.2 不要过高层

也不推荐第一版直接提供：

-   `make_complete_game`
-   `build_entire_level`
-   `create_full_rpg`
-   `implement_all_gameplay`

这些操作难以定义边界、错误处理和可验证结果。

### 10.3 推荐的领域操作级 Tool

例如：

-   `scene.create_game_object`
-   `scene.add_component`
-   `scene.set_property`
-   `scene.reparent`
-   `scene.delete_object`
-   `asset.create`
-   `asset.import`
-   `asset.get_dependencies`
-   `material.create_node`
-   `material.connect_nodes`
-   `animation.add_state`
-   `animation.connect_states`
-   `ui.create_widget`
-   `project.get_settings`

Tool 应该：

-   输入清晰；
-   输出清晰；
-   能被单独验证；
-   尽量可组合；
-   不暴露不必要的内部实现；
-   能映射到稳定的 Automation Command。

------------------------------------------------------------------------

## 11. Resources 的设计

MCP Resources 不应被忽略。

Tool 负责"做事"，Resource 负责"提供上下文"。

可能的 Resource：

``` text
project://current
project://settings
scene://active
scene://active/hierarchy
scene://active/selection
asset://materials/wood
asset://animations/player
asset://prefabs/enemy
editor://state
editor://open-documents
```

Resources 可以提供：

-   当前项目基本信息；
-   当前活动场景；
-   场景层级；
-   当前选择；
-   资产元数据；
-   资产依赖；
-   编辑器状态；
-   当前打开的编辑器；
-   可用类型和反射信息。

应考虑：

-   Resource URI 规范；
-   Resource 内容格式；
-   是否支持增量更新；
-   是否支持订阅；
-   版本号；
-   失效策略；
-   权限；
-   大型资源的分页和裁剪。

------------------------------------------------------------------------

## 12. UI 不应成为 Agent 的主要操作接口

Agent 不应该依赖：

-   点击某个 ImGui 按钮；
-   查找某个窗口位置；
-   模拟鼠标；
-   读取屏幕截图后猜测状态；
-   依赖编辑器布局；
-   依赖某个面板是否打开。

UI Automation 可以作为补充，但主要接口应是结构化的领域 API。

可以提供少量编辑器状态操作，例如：

-   `editor.open_asset_editor`
-   `editor.focus_object`
-   `editor.select_object`
-   `editor.set_active_tab`
-   `editor.open_profiler`
-   `editor.open_console`

这些操作用于帮助人类观察和协作，而不是替代底层数据接口。

------------------------------------------------------------------------

## 13. IPC 与 Automation Protocol

由于 Maximum 是 C++ 程序，而 MCP Server 推荐使用
TypeScript/Node.js，因此不建议第一版通过 Node.js Native Addon 直接加载
`minEngine.dll`。

推荐采用跨进程通信：

``` text
MaximumMCPServer
    │
    │ JSON over IPC
    ▼
Maximum.exe
```

Windows 环境下可以优先考虑：

-   Named Pipe；
-   本地 TCP；
-   Unix Domain Socket 的对应实现（如适用）。

第一版建议：

-   MCP ↔ Agent：stdio；
-   MCP Server ↔ Maximum：Windows Named Pipe；
-   数据格式：JSON；
-   请求/响应协议：自定义但稳定、明确；
-   未来有远程、多实例、跨机器需求时，再考虑 TCP、HTTP、Streamable
    HTTP、Protobuf 或 gRPC。

Automation Protocol 应独立于 MCP。

示例：

``` json
{
  "requestId": "req-001",
  "type": "command",
  "command": "scene.set_property",
  "payload": {
    "objectId": "guid://...",
    "propertyPath": "Transform.Position",
    "value": [0, 2, 0]
  }
}
```

响应：

``` json
{
  "requestId": "req-001",
  "success": true,
  "result": {
    "objectId": "guid://...",
    "propertyPath": "Transform.Position",
    "value": [0, 2, 0]
  },
  "errors": [],
  "warnings": []
}
```

协议应考虑：

-   requestId；
-   command/query 类型；
-   版本号；
-   错误码；
-   结构化错误；
-   超时；
-   取消；
-   大型结果；
-   分页；
-   事务；
-   权限；
-   日志；
-   连接断开；
-   多个 Maximum 实例；
-   主线程调度；
-   请求顺序；
-   并发控制。

------------------------------------------------------------------------

## 14. Threading 与执行上下文

Maximum 中很多操作必须在特定线程执行，尤其是：

-   World 修改；
-   UObject/GameObject 生命周期；
-   Renderer 资源；
-   Editor UI；
-   Asset Import；
-   Undo/Redo；
-   Serialization；
-   Scene Save。

MCP Server 不应假设请求可以直接在任意线程执行。

建议由 Maximum 内部负责：

1.  接收 Automation Request；
2.  判断操作所需执行上下文；
3.  将请求调度到正确线程；
4.  执行操作；
5.  收集结果；
6.  返回结构化响应。

Automation API 应明确区分：

-   Thread-safe Query；
-   Main-thread Query；
-   Main-thread Command；
-   Background Task；
-   Asynchronous Operation。

对于耗时操作，应考虑：

``` text
Start Command
    ↓
OperationId
    ↓
Query Operation Status
    ↓
Completed / Failed / Cancelled
```

------------------------------------------------------------------------

## 15. Security、Permission 与确认机制

MCP Server 可能拥有修改项目和资产的能力，因此需要明确权限边界。

建议至少区分：

-   ReadOnly；
-   Mutating；
-   Destructive；
-   ExecuteCode。

第一版不建议开放任意代码执行。

需要考虑：

-   用户是否允许修改；
-   删除操作是否需要确认；
-   是否允许覆盖文件；
-   是否允许导入外部资产；
-   是否允许执行脚本；
-   是否允许访问项目外路径；
-   是否允许修改 Project Settings；
-   是否允许批量操作；
-   是否记录审计日志；
-   是否可以通过 Undo 恢复；
-   是否支持 Dry Run。

可以考虑为 Command 增加：

``` json
{
  "requiresConfirmation": true,
  "isDestructive": true,
  "supportsUndo": true,
  "dryRunSupported": true
}
```

------------------------------------------------------------------------

## 16. Error Model

错误不能只返回一段自然语言字符串。

应尽量提供：

-   稳定错误码；
-   错误类别；
-   人类可读消息；
-   机器可读详情；
-   关联对象；
-   关联属性；
-   关联资产；
-   是否可重试；
-   是否需要用户介入；
-   建议的下一步。

示例：

``` json
{
  "code": "ObjectNotFound",
  "message": "The target object no longer exists.",
  "details": {
    "objectId": "guid://..."
  },
  "retryable": false,
  "requiresUserAction": false
}
```

错误类别可以包括：

-   InvalidArgument；
-   InvalidObjectId；
-   ObjectNotFound；
-   AssetNotFound；
-   PropertyNotFound；
-   TypeMismatch；
-   ValidationFailed；
-   PermissionDenied；
-   OperationNotAllowed；
-   Busy；
-   Timeout；
-   Unsupported；
-   Conflict；
-   SerializationError；
-   InternalError。

------------------------------------------------------------------------

## 17. Transaction、Undo/Redo 与一致性

Agent 的修改操作应尽量经过 Maximum 已有的 Command/Transaction 系统。

不要让 MCP Server 直接绕过：

-   Undo/Redo；
-   Dirty State；
-   Object Lifecycle；
-   Serialization；
-   Validation；
-   Asset Dependency Tracking；
-   Editor Notification。

需要讨论：

-   一个 Tool 是否对应一个 Undo Step；
-   多个 Tool 是否可以组成一个 Transaction；
-   是否支持显式 Begin/Commit/Rollback；
-   失败时是否自动回滚；
-   是否支持 Dry Run；
-   是否支持批量修改；
-   是否支持乐观并发控制；
-   对象被人类修改后，Agent 的请求如何处理；
-   是否需要 Revision / Version 检查。

推荐考虑：

``` text
Begin Transaction
    ↓
Multiple Commands
    ↓
Validate
    ↓
Commit
```

或：

``` text
Command
    ↓
Validate
    ↓
Commit / Rollback
```

------------------------------------------------------------------------

## 18. MVP 范围

第一版不要同时覆盖所有资产类型。

推荐先实现 Scene 的完整闭环：

### Query

-   `scene.get_active_scene`
-   `scene.get_hierarchy`
-   `scene.get_object`
-   `scene.get_component`
-   `scene.get_property`

### Command

-   `scene.create_game_object`
-   `scene.delete_game_object`
-   `scene.add_component`
-   `scene.remove_component`
-   `scene.set_property`
-   `scene.reparent`
-   `scene.save`

### Validation

-   `scene.validate`

### Resource

-   当前项目；
-   当前活动场景；
-   场景层级；
-   当前选择。

目标是完整实现：

``` text
Query
  → Command
  → Validation
  → Save
  → Query Again
```

当这一闭环稳定后，再扩展到：

1.  Asset；
2.  Material Graph；
3.  Animation Graph；
4.  Prefab；
5.  UI；
6.  Editor State；
7.  更复杂的批处理和事务。

------------------------------------------------------------------------

## 19. Recommended Technology Stack

不因为 Maximum/minEngine 使用 C++，就要求 MCP Server 也使用 C++。

推荐第一选择：

``` text
Language: TypeScript
Runtime: Node.js
MCP: Official MCP TypeScript SDK
Schema: Zod 或兼容 Standard Schema 的方案
MCP Transport: stdio
Maximum ↔ MCP Server: JSON over local IPC
Windows IPC: Named Pipe
Maximum: C++
Automation API: C++ / 独立协议
```

原因：

-   MCP Server 的主要职责是协议适配；
-   TypeScript 对 JSON Schema 和结构化数据友好；
-   Node.js 异步 I/O 适合 IPC；
-   MCP SDK 生态成熟；
-   Tool、Resource、Prompt 的表达自然；
-   不需要直接链接 C++ ABI；
-   更容易独立测试和部署。

其他语言的适用情况：

-   C#/.NET：适合 Windows 工具链和 .NET 生态；
-   Python：适合资产处理、代码生成、AI Pipeline，但不是第一版 MCP Server
    的首选；
-   Go/Rust：适合独立服务和高并发，但当前需求未必需要；
-   C++：可以实现，但会增加协议层与引擎内部耦合，不建议仅因引擎使用 C++
    就选择它。

------------------------------------------------------------------------

## 20. Suggested Repository Layout

可以考虑类似结构：

``` text
minEngine/
├── engine/
├── Maximum/
├── MaximumMCPServer/
├── MaximumAutomationProtocol/
└── MaximumTools/
```

职责示意：

``` text
MaximumMCPServer/
    MCP 协议、Tool、Resource、Prompt、Schema、IPC Client

MaximumAutomationProtocol/
    Request、Response、ObjectId、AssetId、PropertyPath、
    OperationResult、ValidationResult、错误码、协议版本

Maximum/
    Automation Server、请求调度、Editor Automation API、
    Command/Transaction、主线程执行、Undo/Redo、真实业务逻辑

MaximumTools/
    可复用的领域级自动化操作或工具实现
```

具体目录结构应先结合当前仓库实际情况确认，不要机械照搬。

------------------------------------------------------------------------

## 21. Recommended Development Phases

### Phase 1：现状扫描与边界确认

先阅读当前仓库中与以下内容相关的代码：

-   Maximum Editor；
-   Debug Console；
-   Command System；
-   Reflection；
-   Serialization；
-   Object Identity；
-   Undo/Redo；
-   Asset System；
-   World / Scene；
-   Threading；
-   Logging；
-   Project Management。

输出：

-   当前架构理解；
-   已有可复用能力；
-   缺失的基础设施；
-   依赖关系；
-   风险；
-   待确认问题；
-   初步架构假设。

### Phase 2：Automation Protocol 草案

设计：

-   Request；
-   Response；
-   RequestId；
-   Protocol Version；
-   Error；
-   ObjectId；
-   AssetId；
-   PropertyPath；
-   OperationResult；
-   ValidationResult；
-   Operation Status。

先不实现全部业务 Tool。

### Phase 3：Maximum Automation API

先实现最小 Scene 闭环：

-   查询活动场景；
-   查询层级；
-   查询对象；
-   查询属性；
-   创建对象；
-   修改属性；
-   删除对象；
-   验证；
-   保存。

确保这些操作通过统一 API 执行。

### Phase 4：IPC

实现：

-   Maximum 端 IPC Server；
-   TypeScript 端 IPC Client；
-   请求/响应匹配；
-   错误处理；
-   超时；
-   断线处理；
-   基础日志；
-   协议版本检查。

### Phase 5：MCP Server

实现：

-   MCP Server；
-   Tool Registry；
-   Resource Registry；
-   JSON Schema；
-   MCP stdio Transport；
-   Automation Client；
-   错误映射；
-   基础权限策略。

### Phase 6：验证与扩展

实现自动化测试：

-   Query 测试；
-   Command 测试；
-   Validation 测试；
-   Undo/Redo 测试；
-   保存/重新加载测试；
-   无效 ObjectId 测试；
-   无效 PropertyPath 测试；
-   并发和断线测试。

之后再扩展 Asset、Material、Animation、Prefab、UI 等领域。

------------------------------------------------------------------------

## 22. Collaboration Rules for the Agent

在与用户讨论时：

1.  先理解现有代码，再提出新架构；
2.  不要直接假设缺少某个系统；
3.  不要为了 MCP 而破坏现有 Engine/Editor 分层；
4.  不要把 MCP Tool 当作业务层；
5.  优先设计稳定的 Automation API；
6.  优先复用 Reflection、Serialization、Object Identity、Undo/Redo；
7.  明确区分 Query、Command、Validation；
8.  所有重要操作都应有结构化结果；
9.  关注人类和 Agent 同时操作时的一致性；
10. 关注线程、生命周期、事务、权限和错误模型；
11. 不要第一版就追求覆盖所有资产；
12. 不要为了"看起来智能"而设计不可验证的超级 Tool；
13. 对不确定的地方提出问题，不要擅自决定；
14. 每次设计都应说明：
    -   解决什么问题；
    -   为什么需要；
    -   与现有系统如何连接；
    -   API 边界在哪里；
    -   MVP 如何实现；
    -   未来如何扩展；
    -   可能的替代方案；
    -   风险和代价。

------------------------------------------------------------------------

## 23. Current Starting Point

开始设计前，请先阅读当前 minEngine 仓库中与以下模块相关的代码：

-   Maximum；
-   Editor Command；
-   Debug Console；
-   Reflection；
-   Serialization；
-   Object Identity；
-   Undo/Redo；
-   Asset；
-   World / Scene；
-   Project Management；
-   Logging；
-   Threading。

不要直接开始实现 MCP Tool。

请先输出：

### A. 现状理解

总结当前项目已有的相关架构和能力。

### B. 可复用能力

指出哪些现有系统可以直接作为 Automation API 的基础。

### C. 缺失基础设施

指出为了实现 MCP Server，还需要新增哪些独立模块。

### D. 待确认问题

列出会显著影响架构的关键问题，等待用户确认。

### E. 初步架构假设

给出一个明确标注为"暂定"的架构方案，并说明其依据。

只有在现状理解和边界确认完成后，再进入具体接口、协议和代码设计。

------------------------------------------------------------------------

## 24. Final Architectural Direction

最终希望形成如下结构：

``` text
AI Agent
    │
    ▼
MCP Protocol
    │
    ▼
MaximumMCPServer
    │
    ▼
Stable Automation Protocol
    │
    ▼
Maximum.exe
    │
    ▼
Editor Automation API
    │
    ├── Query Services
    ├── Command / Transaction Services
    ├── Validation Services
    ├── Reflection / Property Services
    ├── Object Identity Services
    ├── Serialization Services
    ├── Undo / Redo Services
    ├── World / Scene Services
    ├── Asset Services
    ├── Material Services
    ├── Animation Services
    ├── UI Services
    └── Future Domain Services
```

核心思想是：

> MCP 是外部协议层，Automation API 才是长期基础设施；Tool
> 是领域操作的表达，Reflection、Serialization、Object
> Identity、Undo/Redo 和 Validation 是 Agent-friendly
> 编辑能力的共同基础。
