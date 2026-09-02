# minEngine Debug Console & Command System Design Guide

## 1. 核心定位

minEngine 的 Debug Console 不应只是传统意义上的“作弊码/调试命令窗口”，而应被设计为：

> **Runtime Development Interface / Engine Runtime Control Plane**

它同时服务于：

- Engine Programmer
- Gameplay Programmer
- Technical Artist
- Editor/Tooling
- Automated Test
- Future AI Agent

Console UI 只是 Command System 的一个 Frontend。

核心架构：

```text
Console UI
    ↓
Command System
    ↓
Reflection / Runtime Object System
    ↓
Engine Runtime
```

未来可以拥有多个 Frontend：

```text
                    Command / Runtime API
                           │
              ┌────────────┼────────────┐
              ↓            ↓            ↓
           Editor        Console       Agent
              │            │            │
              └────────────┼────────────┘
                           ↓
                    Engine Runtime
```

---

# 2. EditorCommand 与 Console Command 应统一

如果 Engine 已经存在 `EditorCommand`，不要再单独创建一个完全独立的 `DebugCommand` 系统。

两者本质上都是：

> **Command：描述一个可以被系统主动执行的操作。**

区别主要在于调用入口：

```text
EditorCommand
    ↓
Editor UI / Menu / Shortcut / Command Palette

Console Command
    ↓
文本输入 / REPL

Agent Command
    ↓
Structured API
```

因此推荐抽象为统一的：

```text
Command System
```

例如：

```cpp
CommandRegistry
CommandDescriptor
CommandParser
CommandExecutor
CommandContext
CommandHistory
CompletionSystem
ValidationSystem
```

而不是：

```text
EditorCommand
ConsoleCommand
AgentCommand
```

三套互相重复的实现。

---

# 3. Command 是正式 Engine API

Command 不应该只是：

```cpp
std::function<void(string)>
```

而应该具有结构化描述：

```cpp
struct CommandDescriptor
{
    Name;
    Description;

    Scope;
    Flags;

    Arguments;

    CompletionProvider;
    ValidationRules;

    Execute;
    Undo;
};
```

Argument Schema 至少应该能够描述：

```text
Name
Type
Required / Optional
Default
Min / Max
Enum Values
Description
Completion Provider
```

这样同一个 Command 可以被：

```text
Editor
Console
Agent
Automation
```

共同理解。

---

# 4. Command Scope

并非所有 Command 都适用于所有环境。

可以设计：

```cpp
enum class CommandScope
{
    Editor,
    Runtime,
    Both
};
```

例如：

```text
editor.save
    Scope = Editor

render.shadow.debug
    Scope = Runtime

entity.spawn
    Scope = Both
```

这样不同 Frontend 可以根据 Scope 展示合适的 Command。

---

# 5. Command Context

同一个 Command 可以在不同 Context 下执行。

例如：

```text
Delete
```

Editor：

```text
Delete Selected Entity
```

Console：

```text
delete Entity_01
```

Agent：

```text
execute("delete", { "entity": "Entity_01" })
```

Command 本身不应该关心自己是由鼠标、Console 还是 Agent 触发的。

通过：

```cpp
struct CommandContext
{
    World* World;
    EditorContext* Editor;
    RuntimeContext* Runtime;
    SelectionContext* Selection;
};
```

向 Command 提供执行环境。

---

# 6. Editor、Console、Agent 共享 Command Schema

例如：

```text
spawn.entity
    argument:
        type = EntityType
        required = true
```

Editor 可以自动生成：

```text
[ Spawn Entity ]

Entity Type:
[ Enemy ▼ ]

[ Spawn ]
```

Console：

```text
> spawn.entity E<TAB>
```

自动提示：

```text
Enemy
EnemyBoss
EnemyProjectile
```

Agent：

```json
{
    "command": "spawn.entity",
    "args": {
        "type": "Enemy"
    }
}
```

三者共享同一个 Command Descriptor。

---

# 7. Command Palette 与 Console Completion 共享基础设施

Editor Command Palette：

```text
Ctrl + P

> spawn

Spawn Entity
Spawn Light
Spawn Camera
Spawn Empty Entity
```

Console：

```text
> spawn
```

应该得到相同的 Command Discovery 结果。

底层：

```text
CommandRegistry
        │
        ├── Editor Command Palette
        ├── Debug Console Completion
        └── Agent Command Discovery
```

不要为不同 Frontend 重复维护 Command 列表。

---

# 8. Command 与 Property 是两个不同概念

不要把 Reflection Property 强行设计成 Command。

### Command

描述：

> **执行一个动作。**

例如：

```text
spawn Enemy
destroy Enemy_01
save
reload_shader
capture_frame
```

### Property

描述：

> **访问一个状态。**

例如：

```text
Player.MoveSpeed
Player.Health
Player.Transform.Position
Render.Shadow.Bias
```

Runtime Interface 可以由二者共同组成：

```text
             Runtime Interface
                    │
          ┌─────────┴─────────┐
          ↓                   ↓
      Command System      Reflection
          │                   │
        Actions            Properties
          │                   │
          └─────────┬─────────┘
                    ↓
              Debug Console
```

---

# 9. Reflection 是 Console 的核心基础设施

利用 minEngine Reflection 系统，让 Console 能够理解：

- Type
- Property
- Function
- Enum
- Nested Object
- Container
- Metadata

例如：

```cpp
class Character : public MEObject
{
    ME_PROPERTY
    float MoveSpeed;

    ME_PROPERTY
    float JumpHeight;

    ME_PROPERTY
    MovementMode MovementMode;
};
```

Console：

```text
> inspect Player
```

输出：

```text
Player : Character

Movement
    MoveSpeed       float       5.0
    JumpHeight      float       2.0
    MovementMode    enum        Walking
```

并支持：

```text
> get Player.MoveSpeed
5.0

> set Player.MoveSpeed 8
OK
```

---

# 10. Property Path

支持：

```text
Player.Transform.Position
Player.Transform.Position.x
Player.Animation.StateMachine.CurrentState
Player.Movement.MoveSpeed
```

解析结构：

```text
Object
  ↓
Property
  ↓
Property
  ↓
Property
  ↓
Value
```

这套 Property Path 应成为：

```text
Console
Editor Inspector
Serialization
Agent Interface
```

共享的基础设施。

---

# 11. Property 不应简单等价于 Memory Offset

Reflection Property 应优先支持：

```text
Getter
Setter
Metadata
```

而不是强制：

```cpp
object + offset
```

例如：

```text
set Player.Health 50
```

可以最终调用：

```cpp
Player->SetHealth(50);
```

而不是直接修改：

```cpp
Player->Health = 50;
```

因为 Setter 可能需要：

- Clamp
- Validation
- Dirty Flag
- Event
- Replication
- Gameplay Update
- Animation Update

---

# 12. Reflection Property Metadata

Property 应具有丰富的 Metadata：

```text
Editable
ReadOnly
Hidden
DebugOnly

Description
Category
DisplayName

Min
Max
Step

EnumValues
```

例如：

```cpp
ME_PROPERTY(
    Editable = true,
    Min = 0,
    Max = 20,
    Step = 0.1,
    Category = "Movement"
)
float MoveSpeed;
```

这些 Metadata 不只是 Console 使用。

应该成为：

```text
Reflection Metadata
    ├── Serialization
    ├── Console
    ├── Editor
    ├── Runtime Inspector
    └── Agent Interface
```

共享基础设施。

---

# 13. 智能补全是 Console 的第一等公民

目标：

> **用户几乎不需要记住 Command。**

只需要：

```text
输入几个字符
    ↓
Tab
    ↓
选择
    ↓
继续输入
    ↓
自动提示参数
    ↓
自动验证
```

例如：

```text
> render.
```

提示：

```text
render.debug
render.wireframe
render.stats
render.shadow
```

继续：

```text
> render.shadow.
```

提示：

```text
render.shadow.bias
render.shadow.cascade
render.shadow.debug
```

---

# 14. Completion 必须 Type-aware

Boolean：

```text
> set Player.EnableRootMotion 
```

提示：

```text
true
false
```

Enum：

```text
> set Player.MovementMode 
```

提示：

```text
Walking
Falling
Swimming
Flying
```

Vector3：

```text
> set Player.Transform.Position
```

提示：

```text
Vector3
x: float
y: float
z: float
```

Completion 不应该只是静态字符串搜索。

---

# 15. Runtime Context Completion

Completion 可以动态读取 Runtime 状态。

例如：

```text
spawn <EntityType>
```

自动提示当前注册的 Entity Types。

```text
inspect <Entity>
```

自动提示当前 World 中存在的对象：

```text
Player
Enemy_01
Enemy_02
Boss
```

因此建议：

```cpp
IConsoleCompletionProvider
```

允许 Command 根据当前 Context 动态生成 Completion。

---

# 16. Fuzzy / Prefix Matching

Console 应支持：

```text
Exact
    ↓
Prefix
    ↓
Case-insensitive
    ↓
Normalized
    ↓
Fuzzy
```

例如：

```text
> Player.MoveS
```

唯一匹配：

```text
Player.MoveSpeed
```

而：

```text
> Player.Move
```

如果存在：

```text
Player.MoveSpeed
Player.MovementMode
Player.MovementComponent
```

则应展示候选，而不是擅自选择。

**只有明确唯一匹配时才能自动解析。**

---

# 17. Pre-validation

所有输入应尽量在 Execute 前完成验证。

例如：

```text
> set Player.MoveSpeed abc
```

立即：

```text
Invalid argument.
Expected: float
Got: string
```

Range：

```text
> set Player.MoveSpeed 999
```

如果：

```text
Min = 0
Max = 20
```

则：

```text
Value out of range.
Expected: [0, 20]
```

Validation 至少支持：

```text
Type Validation
Range Validation
Enum Validation
Argument Count Validation
ReadOnly Validation
Command Schema Validation
```

---

# 18. History

Console 必须支持：

```text
↑
↓
```

访问最近执行的 Command。

建议进一步支持：

```text
Prefix-aware History
Ctrl+R / History Search
Persistent History
```

例如：

```text
> render.
↑
```

只搜索历史中的：

```text
render.shadow.debug
render.stats
render.wireframe
```

---

# 19. Runtime Inspection

基础 Command：

```text
inspect
get
set
find
watch
```

例如：

```text
> inspect Player
> get Player.Transform
> set Player.MoveSpeed 8
> find type=Character
> watch Player.Health
```

`inspect` 应类似 CLI Object Inspector。

---

# 20. Watch

支持：

```text
watch Player.Health
watch Player.Transform.Position
watch render.shadow.bias
```

输出：

```text
Player.Health = 100
Player.Health = 75
Player.Health = 50
```

这是 Runtime Debugging 的重要基础能力。

---

# 21. Find

支持 Runtime Object Discovery：

```text
find Character
find type=Character
find tag=Enemy
find name=Enemy_01
```

目的是让程序员和 Agent 能够快速定位 Runtime Object。

---

# 22. Logger 与 Console 解耦

Logger 不应该依赖 Console。

```text
Logger ──────────────┐
                     ↓
                 Console UI
                     ↑
CommandSystem ───────┘
```

Log 应具有：

```text
Timestamp
Severity
Category
Message
```

Console 可以展示 Logger，但 Headless Server / Automated Test 不需要 Console UI。

---

# 23. Undo / Redo

统一 Command System 后，Editor 与 Runtime 可以共享 Undo/Redo 基础设施。

Command 可以拥有：

```text
Execute
Undo
Redo
```

但不是所有 Runtime Command 都必须支持 Undo。

例如：

```text
spawn
destroy
set property
```

可能支持。

而：

```text
play sound
capture frame
reload shader
```

未必有意义。

因此可以通过：

```text
CommandFlags
    Undoable
    Editor
    Runtime
```

进行声明。

---

# 24. Future: Snapshot / Diff / Restore

可以进一步支持：

```text
snapshot Player
diff Player
restore Player
```

例如：

```text
Changed Properties:

Movement.MoveSpeed
    5.0 → 8.0

Movement.JumpHeight
    2.0 → 3.0
```

这对于 Runtime Debugging 和 Agent 自动调试非常有价值。

---

# 25. Future: Transaction

允许一组 Property 修改作为一个整体执行：

```text
begin

set Player.MoveSpeed 8
set Player.JumpHeight 3
set Player.Mass 80

end
```

如果其中一个操作失败：

```text
Transaction failed.
No changes applied.
```

未来 Agent 可以：

```text
Modify
    ↓
Test
    ↓
Inspect
    ↓
Rollback
```

---

# 26. Future: Command Composition

基础 Command System 稳定之后，再考虑小型 Engine-oriented DSL。

例如：

```text
;
&&
||
variables
if
for
wait
```

例如：

```text
spawn Enemy count=10
wait 1
inspect Enemy_01
```

不要为了“像 Bash”而过早实现完整 Shell。

首先确保：

```text
Command
Reflection
Completion
Validation
Inspection
```

本身足够强。

---

# 27. Agent-friendly 是长期设计目标

Agent 不应该依赖：

```text
模拟键盘
读取 UI
猜 Command
猜参数
```

而应该直接使用结构化接口：

```text
discover()
inspect()
get()
set()
execute()
watch()
```

例如：

```text
Agent:
    discover("render.shadow")

Engine:
    render.shadow.bias
        type = float
        range = [0, 10]
        description = ...

Agent:
    set render.shadow.bias 0.005

Engine:
    OK
```

因此：

> **Command Schema + Reflection + Metadata + Property Access + Runtime Inspection 是 Agent-friendly Engine 的基础设施。**

---

# 28. 推荐整体架构

```text
                           minEngine
                              │
                ┌─────────────┴─────────────┐
                │                           │
          Command System                Reflection
                │                           │
      ┌─────────┼─────────┐        ┌────────┼────────┐
      │         │         │        │        │        │
   Editor    Console    Agent    Type    Property Metadata
      │         │         │        │        │        │
      └─────────┼─────────┘        └────────┼────────┘
                │                           │
                └─────────────┬─────────────┘
                              ↓
                       Runtime / Editor
```

Command System：

```text
CommandRegistry
CommandDescriptor
CommandParser
CommandExecutor
CommandContext
CommandHistory
Completion
Validation
Undo/Redo
```

Reflection：

```text
Type
Property
Function
Enum
Metadata
Accessor
PropertyPath
```

二者共同构成：

```text
        Runtime Development Interface
                    │
          ┌─────────┴─────────┐
          ↓                   ↓
      Commands            Properties
          │                   │
          └─────────┬─────────┘
                    ↓
               Console
```

---

# 29. MVP 优先级

第一阶段：

```text
1. Unified Command Registry
2. Command Descriptor / Schema
3. Command Parser / Executor
4. Reflection Property Access
5. Property Path
6. Get / Set / Inspect / Find
7. Type-aware Validation
8. Smart Completion
9. Runtime Object Completion
10. Command History
11. Logger Integration
```

重点不是“功能数量”，而是：

> **把 Command、Reflection、Completion、Validation 这几个基础设施设计正确。**

---

# 30. Final Design Philosophy

minEngine Debug Console 最终应该达到：

```text
Developer / Agent
        │
        ▼
   Command Mode
        │
        ▼
   Auto Discovery
        │
        ▼
 Intelligent Completion
        │
        ▼
 Type-aware Validation
        │
        ▼
 Reflection Property Access
        │
        ▼
 Runtime Inspection
        │
        ▼
 Engine Runtime
```

核心目标不是：

> “做一个能输入 Debug Command 的窗口。”

而是：

> **构建一个人类、Editor、Automation 和 Agent 都可以发现、理解、检查、修改和控制 minEngine Runtime 的统一结构化接口。**

其中：

```text
Command System
        +
Reflection
        +
Metadata
        +
Smart Completion
        +
Validation
```

是整个系统的核心基础设施。

**EditorCommand、Debug Console Command 和未来 Agent Command 不应该是三个系统，而应该是同一个 Command System 的不同 Frontend。**

最终希望达到：

```text
                 Unified Engine Interface
                         │
          ┌──────────────┼──────────────┐
          ↓              ↓              ↓
       Editor          Console         Agent
          │              │              │
          └──────────────┼──────────────┘
                         ↓
                Command + Reflection
                         ↓
                   Engine Runtime
```

这使 minEngine 不仅对人类程序员非常高效，也从架构层面天然具备 **Agent-friendly / Machine-friendly** 的能力。