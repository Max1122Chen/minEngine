# minEngine Play Mode Development Guideline

## 1. Goal

为 minEngine 实现一个清晰、可靠、可扩展的 **Play Mode**。

Play Mode 不应被理解为简单的 `bool isPlaying`，而应被视为：

> **Editor World 与 Runtime World 之间的生命周期与上下文协调层。**

核心目标：

```text
Editor World
    ↓ Enter Play
Runtime World
    ↓ Runtime Tick
    ↓ Stop
Editor World
```

最重要的原则：

> **Editor State 与 Runtime State 必须隔离。Runtime 的修改不能污染 Editor Scene。**

---

## 2. Core Architecture

推荐使用两个 World：

```text
EditorWorld
    │
    │ Clone / Snapshot
    ▼
RuntimeWorld
```

Play Mode 负责协调二者，而不是拥有或实现其他 Engine Subsystem。

推荐结构：

```text
PlayMode
 ├── EditorWorld
 ├── RuntimeWorld
 ├── PlayState
 ├── EnterPlay()
 ├── Pause()
 ├── Resume()
 ├── Step()
 └── Stop()
```

PlayMode 是 **orchestration layer**。

它负责协调：

- World 生命周期
- Runtime 初始化与销毁
- Input Context 切换
- View / Camera Context 切换
- Physics 生命周期
- Audio 生命周期
- Gameplay / Script 生命周期
- Runtime Debug 状态

但不要把 Physics、Audio、Input、Renderer 等系统的具体实现塞进 PlayMode。

---

## 3. Play State

不要只使用：

```cpp
bool bPlaying;
```

至少定义明确状态：

```cpp
enum class PlayState
{
    Editing,
    Playing,
    Paused
};
```

推荐支持：

```text
Editing
Playing
Paused
```

并预留未来扩展：

```text
Stopping
Simulating
```

MVP 不需要实现复杂状态机。

---

## 4. Enter Play Lifecycle

进入 Play 时，推荐遵循：

```text
EnterPlay
    │
    ├── Validate Editor World
    │
    ├── Create Runtime World
    │
    ├── Clone / Snapshot Scene
    │
    ├── Initialize Runtime Systems
    │
    ├── Initialize Physics
    │
    ├── Initialize Gameplay / Scripts
    │
    ├── Switch Input Context
    │
    ├── Switch View Context
    │
    └── BeginPlay
```

其中 Runtime World 必须是独立实例。

不要直接让 Editor World 开始 Tick，除非未来明确实现类似 SIE（Simulate In Editor）的独立模式。

---

## 5. Scene Snapshot / World Clone

Scene Snapshot 是 Play Mode 的核心基础设施之一。

需要保证：

```text
Editor World
    ↓ Clone
Runtime World
```

Runtime 修改：

```text
Runtime Transform
Runtime HP
Runtime Physics
Runtime Gameplay State
```

都不会修改 Editor World。

### Copy

通常复制：

- Entity / Object
- Transform
- Components
- Component properties
- Hierarchy
- Script / Gameplay state

### Share

通常共享：

- Mesh
- Texture
- Material
- Shader
- Animation Asset
- Audio Asset
- 其他只读 Resource

原则：

> **Copy mutable World State, share immutable Assets.**

---

## 6. Entity Reference Remapping

Clone World 时必须考虑对象引用。

例如：

```text
Enemy.Target → Player
```

如果 Editor World：

```text
Player = Entity 10
Enemy.Target = Entity 10
```

Clone 后 Runtime World 中 Entity ID 可能发生变化。

因此 Clone 系统应支持：

```text
Editor Entity ID
       ↓
Entity Remapping
       ↓
Runtime Entity ID
```

例如：

```cpp
std::unordered_map<EntityID, EntityID> remap;
```

然后修复：

- Entity references
- Component references
- Script references
- Object references

禁止 Runtime World 长期持有指向 Editor World runtime object 的引用。

---

## 7. Camera / View Context

Editor Camera 和 Game Camera 应当分离。

### Editor

```text
Editor Viewport
    ↓
Editor Camera
```

### Play

```text
Game Viewport
    ↓
Active Game Camera
```

不要让 Renderer 到处判断：

```cpp
if (IsPlaying())
    useGameCamera();
else
    useEditorCamera();
```

推荐抽象：

```cpp
ViewContext
{
    Camera
    ViewMatrix
    ProjectionMatrix
    Viewport
}
```

Renderer 只消费 `ViewContext`。

PlayMode 负责切换：

```text
Editor View Context
        ↓ Enter Play
Game View Context
```

Stop 后恢复 Editor View Context。

### Editor Camera State

进入 Play 前应保留：

- Position
- Rotation
- FOV 等 Editor Camera 状态

Stop 后恢复 Editor Camera。

不要让 Runtime Camera 修改 Editor Camera。

---

## 8. Input Context

Editor Input 与 Runtime Input 应当分离。

### Editor

```text
Mouse / Keyboard
    ↓
Editor
    ├── Camera
    ├── Gizmo
    ├── Selection
    └── UI
```

### Play

```text
Mouse / Keyboard
    ↓
Game Input
    ├── Player
    ├── Camera
    └── Gameplay
```

因此 PlayMode 需要协调 Input Context：

```text
Enter Play
    ↓
Game Input Context

Stop
    ↓
Editor Input Context
```

不要让 Editor Camera、Game Camera、Gameplay 同时抢占同一套输入。

---

## 9. Runtime Lifecycle

Runtime 系统应有清晰生命周期。

推荐：

```text
Initialize
    ↓
BeginPlay
    ↓
Tick
    ↓
EndPlay
    ↓
Shutdown
```

不同系统根据自身需要参与。

例如：

```text
World
Physics
Audio
Script
Gameplay
```

都应该有明确的初始化和销毁时机。

不要让各系统通过：

```cpp
if (IsPlaying())
```

自行猜测 Runtime 生命周期。

---

## 10. Physics

Physics World 属于 Runtime State。

进入 Play：

```text
Create Physics World
    ↓
Register Bodies
    ↓
Begin Simulation
```

运行：

```text
Physics Tick
```

Stop：

```text
Stop Simulation
    ↓
Destroy Physics World
```

Physics state 不应该作为 Editor Scene Snapshot 的持久状态。

重新 Play 时，应重新初始化 Physics。

---

## 11. Audio

Runtime Audio Instance 应属于 Runtime 生命周期。

进入 Play：

```text
Create Runtime Audio
```

运行：

```text
Play / Update / Stop
```

Stop：

```text
Stop Runtime Audio
    ↓
Release Runtime Audio Instances
```

确保 Stop Play 后不会残留：

- Playing Audio
- Runtime Audio Sources
- Runtime Audio State

Editor Audio Preview 与 Runtime Audio 应当能够区分。

---

## 12. Gameplay / Script

Runtime Gameplay / Script 应遵循明确生命周期：

```text
Create
 ↓
Initialize
 ↓
BeginPlay
 ↓
Tick
 ↓
EndPlay
 ↓
Destroy
```

不要依赖“第一次 Tick 就当作 BeginPlay”。

PlayMode 应负责触发正确生命周期，而 Gameplay System 负责执行自己的逻辑。

---

## 13. Editor UI During Play

Play Mode 不意味着 Editor 消失。

Play 时 Editor 应进入：

> **Runtime Observation / Debugging Mode**

仍然可以保留：

- Scene Hierarchy
- Inspector
- Debug Draw
- Console
- Performance information

但普通 Editor 修改不应该直接修改 Editor World。

推荐 MVP：

```text
Play Mode Inspector = Runtime Read Only
```

未来可以增加：

```text
Runtime Property Editing
Runtime Gizmo
```

但这些属于 Debug / Advanced Editor 功能。

---

## 14. Selection / Gizmo

Editor Selection 属于 Editor State。

Runtime Entity 可以被 Editor 观察，但不要让 Runtime World 直接依赖 Editor Selection。

Play Mode 下：

```text
Selection
    ↓
Runtime Entity
    ↓
Inspector / Debug
```

Transform Gizmo 在 MVP 中可以：

- 禁用
- 或仅用于观察

未来再支持 Runtime Editing。

---

## 15. Debug Drawing

Runtime Debug Draw 属于 Runtime Debug State。

例如：

```cpp
DebugDraw::Line(...)
DebugDraw::Sphere(...)
DebugDraw::Text(...)
```

这些数据可以由 Runtime World 产生，由 Renderer 显示。

Stop 时必须清理 Runtime Debug State。

避免：

```text
Stop Play
    ↓
Previous Runtime Debug Geometry still visible
```

---

## 16. Time / Tick

PlayMode 至少需要：

```text
Playing
Paused
Step Frame
```

推荐：

```cpp
PlayMode::Tick(float deltaTime);
```

Runtime World：

```cpp
RuntimeWorld->Tick(deltaTime);
```

暂停时：

```text
Editor Tick
    ↓
继续运行

Runtime Tick
    ↓
停止
```

Step：

```text
Paused
  ↓ Step
One Runtime Tick
  ↓
Paused
```

未来可扩展：

- Time Scale
- Fixed Tick
- Physics Tick
- Frame Step

但 MVP 不需要复杂时间系统。

---

## 17. Stop Lifecycle

Stop 是 PlayMode 最重要的另一条路径。

推荐：

```text
Stop
 │
 ├── EndPlay
 │
 ├── Stop Gameplay / Scripts
 │
 ├── Stop Audio
 │
 ├── Stop Physics
 │
 ├── Clear Runtime Debug State
 │
 ├── Destroy Runtime World
 │
 ├── Restore Editor View Context
 │
 ├── Restore Editor Input Context
 │
 └── Return to Editing
```

Stop 后：

```text
Runtime World = destroyed
Editor World = unchanged
Editor Camera = restored
Editor Input = restored
```

---

## 18. Dirty State / Saving

Runtime 修改绝不能自动让 Editor Scene 变 Dirty。

例如：

```text
Editor:
Enemy HP = 100

Play:
Enemy HP = 20

Stop:
Enemy HP = 100
```

Scene Asset 不应该因为 Runtime 修改而产生 Dirty State。

因此：

> **Editor World 是 Authoring State，Runtime World 是 Temporary Runtime State。**

只有明确的 Editor 操作才能修改 Authoring State。

---

## 19. Error Handling

Enter Play 失败时，不应该留下半初始化 Runtime。

推荐：

```text
EnterPlay
   ↓
Create Runtime
   ↓
Initialize
   ↓
ERROR
   ↓
Rollback / Destroy Runtime
   ↓
Return to Editing
```

PlayMode 应保证状态转换具有基本的原子性：

```text
Editing → Playing
```

要么成功进入 Playing，要么回到 Editing。

不要留下：

```text
Half Runtime World
Half Input Switch
Half Physics
```

---

## 20. Recommended API

MVP 可以保持非常简单：

```cpp
class PlayMode
{
public:
    bool EnterPlay();
    void Pause();
    void Resume();
    void Step();
    void Stop();

    void Tick(float deltaTime);

    PlayState GetState() const;
    World* GetRuntimeWorld();
};
```

内部维护：

```cpp
EditorWorld* m_editorWorld;
std::unique_ptr<World> m_runtimeWorld;
PlayState m_state;
```

不要在第一版加入过多抽象。

---

## 21. Architectural Rules

实现过程中严格遵守以下原则：

### Rule 1

**Editor World 与 Runtime World 分离。**

### Rule 2

**PlayMode 是生命周期协调器，而不是 Gameplay Manager。**

### Rule 3

**Renderer 不应该到处判断 PlayMode。**

使用 ViewContext / RenderContext 等抽象解决 Editor / Game View 差异。

### Rule 4

**Input 应使用 Context，而不是散落 `if (Playing)`。**

### Rule 5

**Runtime State 不得污染 Editor State。**

### Rule 6

**Runtime 不应持有 Editor runtime object 的生命周期引用。**

### Rule 7

**Editor UI 在 PlayMode 中主要负责观察和调试 Runtime。**

### Rule 8

**优先实现简单可靠的生命周期，再增加高级功能。**

---

## 22. MVP Scope

第一版只要求实现：

```text
✓ Editor World
✓ Runtime World
✓ Scene Clone / Snapshot
✓ Enter Play
✓ Runtime Tick
✓ Stop Play
✓ BeginPlay / EndPlay
✓ Editor Camera / Game Camera 切换
✓ Editor Input / Game Input 切换
✓ Runtime 不污染 Editor Scene
✓ Runtime World 正确销毁
```

第二阶段：

```text
✓ Pause
✓ Resume
✓ Step Frame
✓ Physics lifecycle
✓ Audio lifecycle
✓ Script lifecycle
✓ Runtime Debug Draw
✓ Runtime Inspector
```

暂时不要实现：

```text
✗ SIE / Simulate In Editor
✗ Hot Reload
✗ Runtime Transform Editing
✗ Complex Runtime Editing
✗ Network PIE
✗ Multi-World PIE
✗ Advanced Play Session Management
```

这些应在基础 PlayMode 稳定之后再考虑。

---

## 23. Design Philosophy

minEngine 的 PlayMode 应借鉴成熟引擎的共同原则，而不是复制某一个商业引擎的全部复杂性。

参考方向：

```text
Unreal
→ 学习 World / Runtime Lifecycle / PIE 的隔离思想

Unity
→ 学习简单直观的 Play / Pause / Stop 用户体验

Godot
→ 学习 Runtime Remote Inspection 和简洁的 Editor/Runtime 关系
```

最终目标：

```text
                 Editor
                    │
             Authoring World
                    │
                 Enter Play
                    │
                    ▼
             Runtime World
                    │
       ┌────────────┼────────────┐
       │            │            │
   Gameplay      Physics       Audio
       │            │            │
       └────────────┼────────────┘
                    │
                  Tick
                    │
                    ▼
                 Renderer

                 Stop
                    │
                    ▼
             Destroy Runtime
                    │
                    ▼
             Restore Editor
```

**最重要的设计目标不是“实现一个 Play 按钮”，而是建立清晰的 Authoring → Runtime → Authoring 生命周期边界。**