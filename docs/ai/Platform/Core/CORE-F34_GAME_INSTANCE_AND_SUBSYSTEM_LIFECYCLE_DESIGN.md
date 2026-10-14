# CORE-F34 — GameInstance · Subsystem 作用域生命周期（引擎机制 + 游戏 App 契约）

## Meta
- **ID:** `CORE-F34`
- **Type:** Feature
- **Complexity:** `L3`
- **Status:** `In Progress`
- **Owner:** project maintainer
- **Last updated:** `2026-10-14`（S05：Tickable 相位插入 LogicalTick）
- **Related:**
  - 依赖：[CORE-F05](./CORE-F05_PLAY_MODE_DESIGN.md)、[CORE-F31](./CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md)、[CORE-F33](./CORE-F33_DEFERRED_STRUCTURAL_EDIT_DESIGN.md)、[CORE-F27](./CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md)
  - **先实施构建产品化：** [BUILD-F01](../ProductBuild/BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)（Editor+Game；退役 ED-F20）→ 再开本 Feature
  - 模块链接现状：[ED-F20](../../Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)、[ED-F21](../../Editor/ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_DESIGN.md)、[ED-F22](../../Editor/ED-F22_GAME_MODULE_STARTUP_DESIGN.md)
  - UE / Godot 锚点见正文
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) / [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F05`、`CORE-F27`、`CORE-F31`、`CORE-F33`；S06 与 BUILD-F01 协调（删 KeepAlive）
- **Blocks:** minCraft 会话/世界拆分

## TL;DR

minEngine 缺「游戏会话宿主 + 按作用域自动实例化的子系统」机制；游戏只能把逻辑挂在 Scene Component 上，导致作用域错乱与遍历期副作用。本 Feature **升格为引擎基础框架**：对齐 UE 的 **GameInstance / SubsystemCollection / 分层寿命**，提供健全的 Init·Deinit·（可选）Tick·Play 相位契约；游戏模块实现具体 `*Subsystem`，引擎不规定 Content/Inventory 等业务。

> 目标不是「最小能跑」，而是小引擎尺度下**一次设计妥当、可扩展**的生命周期骨架。切片可以大，但每刀必须可验证、可删除旧路径。

## Problem

1. **无会话宿主：** Standalone / PIE 没有「这一局游戏」对象；跨 Scene 仍要活的状态无处安放。  
2. **无作用域子系统：** 无法声明「跟 Engine / GameInstance / Scene 同生共死」的用户扩展点。  
3. **游戏侧被迫用 Component 当 App：** minCraft `MinCraftHostComponent` → `MinCraftRuntime` 拥有 Content+World+Gameplay；Edit/PIE 时序靠 if 拧；逻辑 Tick 落在 GO 遍历内。  
4. **模块入口非正式：** KeepAlive marker 只保证 DLL 加载，没有「注册会话子系统」的契约。

## Requirements

- **R1** 存在一级 **GameInstance（会话宿主）**：Standalone 一份；**每个 PIE 实例一份**（对齐 UE 头文件契约）。  
- **R2** 存在 **Subsystem 机制**：至少 `Engine` / `GameInstance` / `World(Scene)` 三层寿命；自动或半自动实例化；`Initialize` / `Deinitialize`；同集合内可声明依赖拉起顺序。  
- **R3** World 子系统可挂 **Play 相位钩子**（至少：集合 Init 后、Scene BeginPlay 前后可区分）。  
- **R4** 可选 **Tickable** World（及必要时 Session）子系统，调度点**不在** `Scene::Tick` 的 GO 定长循环内。  
- **R5** 游戏模块有正式 **注册/发现** 入口（类表或导出函数）；`GameModuleStub` 可演示 App+World 各一子系统。  
- **R6** 引擎只提供机制；具体 `MinCraft*` 业务子系统在游戏仓实现。  
- **R7** Edit 模式：Editor Scene 可有 World 子系统（预览）；**默认不创建 Play 用 GameInstance**（除非后续显式加 Editor Session——本 Feature 默认不做）。

## Constraints

- 主线程；首期不做子系统线程安全承诺。  
- 不引入 UE Online/Travel/LocalPlayer 全套；LocalPlayer 层可 **Planned 占位或后置切片**。  
- 不把现有 `PhysicsSystem` / `AssetManager` 等硬编码 Engine 系统强行改写成用户 Subsystem（可并存；长期再收敛）。  
- 哲学：Capabilities not Opinions — Core 无 Inventory/Quest。

## Preferences

- 命名贴 UE：`GameInstance`、`GameInstanceSubsystem`、`WorldSubsystem`、`EngineSubsystem`、`SubsystemCollection`。  
- 依赖：`Initialize` 内 `InitializeDependency<T>()`（同集合）；不做独立 DAG 引擎。  
- **类型发现（已锁定）：反射。** `ReflectionSystem::GetDerivedClasses<TBase>()` 枚举非 abstract 派生类；子系统基类与具体类均为 `ME_CLASS`。测试可用显式 `RegisterType` 旁路，生产路径以反射为准。  
- **Tick（已锁定方向）：默认单一 Tick 相位**，不引入 PreSim/PostSim 作为对外 API（理由见 §Tick 语义）。

## Assumptions

- `Scene` ≈ UE `UWorld` 的作用域载体（MVP 不引入第二套 World 类型）。  
- PIE 生命周期继续由现有 Play Mode（CORE-F05）驱动；本 Feature **接入**创建/销毁 GI 与 World 子系统集合，而非重做 Play Mode。  
- Content 类只读表「是否跨 PIE 共享」由**游戏**选择挂 Engine 级或 GI 级；引擎两层都提供。

## Non-Goals

- 完整复制 UE Gameplay Framework（GameMode/Pawn/Controller 强制架构）。  
- 用本 Feature 重新设计 deferred Spawn（已由 CORE-F33 覆盖）。  
- 完整热插拔 / 插件沙箱（后置；Dev 默认仍链接期模块）。  
- Godot 式 project autoload 作为主模型。  
- 网络复制 / 专用服务器多 WorldContext 全矩阵（接口预留即可）。  
- 匿名 KeepAlive / Marker —— **要删掉**，不是 Non-Goal。

## Current Architecture

```text
Engine
  ├─ 硬编码 StartSystems（ObjectManager…UISystem…）
  ├─ SceneManager（Editor Scene + PIE Scene）
  └─ LogicalTick → Scene::Tick → … → EOF → FlushDestroy

游戏模块
  └─ HostComponent → MinCraftRuntime（Content|World|Gameplay 一把抓）
```

- BeginPlay/EndPlay：Scene/GO/Component（CORE-F31）。  
- 结构变更：Spawn 即时 + Destroy 延迟（CORE-F33）。  
- 无 GameInstance；无用户 SubsystemCollection。

## Proposed Architecture

### 从 UE 学什么（采纳清单）

| UE 机制 | 源码锚点 | minEngine 采纳 |
|---------|----------|----------------|
| GI = 会话宿主；Standalone 1；PIE per-instance | `GameInstance.h` 注释；`InitializeForPlayInEditor` | **照学** |
| 分层：Engine / GI / World / (LocalPlayer) | `Subsystem.h` | **Engine+GI+World 必做**；LocalPlayer 后置 |
| Collection 拥有实例；`Initialize(Outer)` 发现派生类 | `SubsystemCollection.cpp` | **照学**（反射 `GetDerived` 或注册表等价物） |
| `ShouldCreateSubsystem(Outer)` on CDO | 同上 | **照学** |
| `InitializeDependency` 同集合内拉起 | 同上 | **照学**（不做双轨 DEPENDS_ON 宏引擎） |
| GI.Init 创建 GI 子系统，再 Init World 子系统 | PIE：`Init()` 后 `PostCreatePIEWorld` | **照学顺序** |
| World：`InitializeSubsystems` → … → `PostInitializeSubsystems` → BeginPlay 钩子 | `World.cpp` | Scene 打开流程对齐为多相位 |
| TickableWorldSubsystem ≠ Actor Tick | `WorldSubsystem.h` | 独立调度；禁止默认塞进 GO Tick |
| EngineSubsystem 可动态（模块加载） | `UDynamicSubsystem` | 后置增强；首期静态注册也可 |

### 不照抄 / 缩小

- OnlineSession、Travel、Replay、Analytics。  
- 完整 `FWorldContext` 网络矩阵 → 用轻量 `GameInstance`↔`Scene*` 绑定即可。  
- 反哺稿：把 GI 业务叫 `EngineSubsystem`、把 PostFrame 当唯一安全 Spawn —— **否决**。

### 目标结构

```text
Engine
  ├─ SubsystemCollection<EngineSubsystem>          // 进程级；少而精
  ├─ GameInstance*（Standalone | 当前主 PIE）
  │     实际：Standalone 一份；PIE 时每实例一份（由 Play 会话持有）
  │     └─ SubsystemCollection<GameInstanceSubsystem>
  └─ SceneManager
        └─ Scene
              └─ SubsystemCollection<WorldSubsystem>
                    └─ TickableWorldSubsystem → Engine/Scene 调度表

游戏模块（示例）
  ├─（可选）ProjectContentSubsystem : EngineSubsystem
  ├─ MinCraftSessionSubsystem : GameInstanceSubsystem
  └─ MinCraftWorldSubsystem : TickableWorldSubsystem
```

### 生命周期（目标契约）

**Standalone**

```text
Engine::StartSystems
  → EngineSubsystemCollection.Initialize(Engine)
  → Create GameInstance → GI.Init → GISubsystemCollection.Initialize(GI)
  → Load/Create Scene → Scene 就绪后 WorldSubsystemCollection.Initialize(Scene)
  → World PostInitialize
  → DispatchBeginPlay（现有）→ WorldSubsystem::OnWorldBeginPlay（新）
主循环：
  →（可选）Session/World Tickable 相位（见下）
  → Scene::Tick（GO）
  → … EOF → FlushDestroy
关机：逆序 Deinitialize World → EndPlay → GI.Shutdown → EngineSubsystem Deinit
```

**PIE（对齐 UE 顺序）**

```text
Enter Play
  → 创建该实例 GameInstance（Outer/所有者：Play 会话或 Engine 登记）
  → 绑定 PIE Scene
  → GI.Init（GI 子系统）
  → WorldSubsystemCollection.Initialize(PIE Scene) + PostInitialize
  → DispatchBeginPlay + OnWorldBeginPlay
Exit Play
  → OnWorldEndPlay / EndPlay
  → WorldSubsystem Deinitialize + 清空集合
  → GI.Shutdown（GI 子系统）+ 销毁该 GI
  → Editor Scene 的 World 子系统保持（若 Edit 已 Init）
```

**Tick（单相位默认）**

```text
LogicalTick:
  Input
  SceneManager::Tick（GO 定长）
  Audio / Physics / UI
  TickableWorldSubsystems / TickableGameInstanceSubsystems   // 默认：Scene GO Tick 与 Physics 之后、EOF 之前
  SendAllEndOfFrameUpdates
  FlushDeferredStructuralEdits
```

详见下一节「Tick 语义」——**不对用户暴露 PreSim/PostSim**，除非日后有真实需求再开切片加 `ESubsystemTickGroup`。

---

## Tick 语义（UE / Godot 对照 → 我们怎么定）

### 其他引擎「一半是什么」

**UE 并不是「PreSim / PostSim」两个名字**，而是一整条 **`ETickingGroup`**（`EngineBaseTypes.h`）：

| 组 | 语义（源码注释） |
|----|------------------|
| `TG_PrePhysics` | 物理模拟开始前 |
| `TG_StartPhysics` / `During` / `EndPhysics` | 物理步进边界 |
| `TG_PostPhysics` | 刚体/布料完成后 |
| `TG_PostUpdateWork` | 更新工作完成后再 Tick |
| `TG_NewlySpawned` | 本帧新生成对象的补跑 |

Actor/Component 用 `PrimaryActorTick.TickGroup` 选组；**`UTickableWorldSubsystem`** 走 `FTickableGameObject`，默认 **Conditional Tick**，**不**要求业务手写 PreSim/PostSim。需要细控时再 override tick 类型/组——那是进阶，不是子系统 API 的默认面。

**Godot** 更粗：节点常见两档——

- `_physics_process`：固定物理步  
- `_process`：帧/空闲帧  

Autoload 也是 Node，同样吃这两档，**没有**引擎级 PreSim/PostSim 子系统 API。

### 对 minEngine 的结论

当前 `LogicalTick` 本身只有一条线性链（Input → Scene GO → Audio/Physics/UI → EOF → Flush）。在此之上再发明 **PreSim/PostSim 双对外相位**：

- **收益小**：没有 UE 那么多并行物理组与 TickPrerequisite；  
- **成本高**：每个 Tickable 都要选相位，文档与误用面上升；  
- **和 F33 无关**：结构变更已有定长 Tick + Destroy Flush，不必靠「PostFrame 子系统」才能 Spawn。

**锁定：**

1. `TickableWorldSubsystem::Tick(float)` / `TickableGameInstanceSubsystem::Tick(float)` — **单函数、单默认插入点**。  
2. 默认插入点：**Physics/UI 之后、`SendAllEndOfFrameUpdates` 之前**（「本帧模拟与 GO Tick 已跑完，EOF 脏标记尚未 drain」——适合做世界逻辑汇聚、再 Mark 进本帧或下帧 EOF）。  
3. 若将来出现「必须在 GO Tick 之前」的刚需，再加可选 `GetTickGroup()`（枚举可对齐精简版 PrePhysics / PostPhysics），**不作为 S05 必做**。

---

## 引擎 DLL ↔ 游戏模块：现状逻辑、痛点、正式化方向

> 本节澄清：Stub / KeepAlive /「GameModuleHost」**不是**早已想好的正式架构，而是 Dev 赶路产物。F34 需要「游戏类型进反射表」，因此必须把链接/加载契约说清楚；完整模块系统可与 F34 并列 Design，但不得再靠 KeepAlive。

### 真实痛点（当初为什么会堆出这一串）

哲学要求：**玩法在独立游戏仓 SHARED**；Dev 又要 `MaximumEditor.exe + minEngine.dll + Game.dll` **同进程**。

| 缺口 | 若不解决会怎样 |
|------|----------------|
| Editor 只链 `minEngine` | 游戏 DLL 根本不进进程 → 游戏 `ME_CLASS` 静态注册不跑 → 反射表无游戏类型 → 无法 AddComponent / 无法发现游戏 Subsystem |
| 游戏仓不能 fork Editor 加 link | 需要引擎侧 **可开关的 CMake 钩子**（ED-F20） |
| 多模块反射 | 游戏自己 codegen（CORE-F27）；DLL 必须被加载，注册才能进表 |
| 链接器 GC/LTO | 「只 link 不引用符号」时可能裁掉游戏 DLL → 注册静默丢失 → **KeepAlive 调 Marker** 这种补丁 |

### 这一串东西各自是什么（按时间线）

```text
ED-F20（正式、Done）
  CMake: MINENGINE_GAME_DIR + MINENGINE_GAME_TARGET
  → add_subdirectory(游戏) → Editor link 游戏 SHARED → POST_BUILD 拷 DLL
  解决：Dev 同进程宿主；默认关闭零回归

GameModuleStub（引擎仓内最小 SHARED）
  解决：不依赖 minCraft 也能验证 ED-F20 / CORE-F27
  附带：Marker 导出 + 一个 Stub Component

GameModuleLinkKeepAlive（权宜）
  static 构造调 Marker → 强迫保留对 stub 的链接引用
  解决：链接器裁库；不是产品 API

ED-F21（Done）
  文档：游戏应始终 link minEngine（对称契约）

ED-F22 Startup 钩子（Cancelled）
  曾想 MinEngine_GameModuleStartup 建世界 → 否决（业务不该塞 C 钩子）
  留下的真缺口：若将来有「非场景、非玩法」的机制注册面，可重开 —— F34 子系统发现正是这类机制
```

**「GameModuleHost」** 在上一版 Draft 里只是占位名，意思是：用**可读的加载/握手对象**替换 KeepAlive，**还不是**已实现的类。正式设计应回答下面几问，而不是先造一个 Host 类名。

### 链接方向：现状「引擎拉游戏」vs 目标「项目出编辑器」

**现状（ED-F20）——依赖箭头反了：**

```text
引擎仓 CMake（根）
  → add_subdirectory(minCraft)
  → MaximumEditor link minCraft
  →（再）KeepAlive / Marker 防裁库
```

引擎 configure **知道并拉取**具体游戏；游戏 CMake 假设「已被引擎 add_subdirectory」。这是 Dev 捷径，不是「每个项目自己的编辑器」。

**UE（源码锚点）——项目 Target 组合模块：**

- `TargetType.Editor`：`UnrealEditor.exe` + `UnrealEditor*.dll` + **`GameName*.dll`**（`TargetRules.cs` 注释）  
- 例：`LyraEditor.Target.cs` → `Type = TargetType.Editor`，`ExtraModuleNames` 含 `LyraGame` / `LyraEditor`  
- 引擎二进制可 **Shared**（多项目共用 Engine/Binaries），但 **Editor 可执行目标以项目为中心声明要链哪些 Game 模块**  
- 不是 Engine 仓库 CMake 去 `add_subdirectory(Lyra)`

**理想 minEngine（北向）：**

```text
游戏仓（或「游戏+编辑器」工程）CMake
  → find/add 引擎 SDK 或 add_subdirectory(引擎)
  → 产出：MinCraftEditor.exe（或 MaximumEditor 的项目别名）
       link: minEngine + minCraft(+可选 Editor 扩展 DLL)
  → 引擎仓默认只编引擎 / 可选 Sample；不反向 add 外仓游戏
```

这需要 **Build 工具链/规则**（安装导出、Editor 目标模板、输出布局），超出 F34 子系统本体；应单开 Build/INFRA Feature。F34 S06 只要求：删 KeepAlive、类型能进表、Sample 可验；**不把「项目编辑器」整盘搬完当成 F34 DoD**。

### 正式契约应回答的问题（并列 Build Feature）

1. **谁是 CMake 根？** 目标：游戏/产品工程；引擎可 Shared 预编或源码子树。  
2. **加载模型** A 链接期（默认）/ B LoadLibrary（后置）。  
3. **类型进表** CORE-F27 + 保证模块进进程（显式 Ensure，禁 KeepAlive）。  
4. **加载时允许/禁止** 同 ED-F22 精神。  
5. **Sample vs 真游戏** 引擎仓 Sample；minCraft 外仓同契约。  

F34 **S06（窄）**：Sample 示范 GI/World 子系统；进进程依赖 **BUILD-F01** 的显式 link（删 KeepAlive）。完整「项目编辑器」→ **BUILD-F01**。

---

## ObjectManager 进程级单例 + Engine : MEObject（OQ6 / OQ7 锁定）

### 决定

1. **`ObjectManager` 提升为与 `LogSystem` / `ReflectionSystem` 同级的进程设施** —— **不由 Engine 拥有**。  
2. **`class Engine : public MEObject`** —— EngineSubsystem Outer = `Engine*`；可用 `NewObject<Engine>`（OM 已先就绪）或等价 Register。  
3. **不采用** `EngineSubsystemOuter` 代理。

### 为何 OM 不必由 Engine 拥有

| 观察 | 结论 |
|------|------|
| OM 职责是对象登记 / NewObject / GC 审计 | 与「这一局游戏」无关，是进程基础设施 |
| 测试已大量 `ObjectManager` + `TestAccess` 独立起 | 本就不依赖完整 Engine |
| Engine 拥有 OM → 鸡生蛋 | 人为复杂度；升单例后消失 |
| UE 对照 | 无「Engine 拥有 ObjectManager」；UObject 分配器是 CoreUObject 级全局设施，`GEngine` 反而是其上的对象 |

形态对齐 `LogSystem`：`Initialize` / `Shutdown` / `Get()`（内部可仍用 `SetInstance` 便于测试注入）。

### UE 对照（Engine 仍是对象）

```text
// LaunchEngineLoop.cpp
GEngine = NewObject<UEditorEngine>(GetTransientPackage(), EditorEngineClass);
// 或 GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
```

UObject 系统先于（或作为）Engine 的底座；Engine 是其上的根游戏对象之一。

### 推荐启动骨架（修订）

```text
LogSystem::Initialize（若尚未）
→ FinalizeReflection()
→ ObjectManager::Initialize()     // 进程单例；不挂在 Engine 上
→ shared_ptr<Engine> = NewObject<Engine>(...) 或 make_shared + RegisterObject
   → SetClass；标 GC root；s_Instance = this
→ InitializeEngineSubsystems()    // Outer = Engine*
→ 其余 StartSystems（SceneManager / RHI…）
Shutdown 逆序：Engine 子系统 → Engine unregister → 其它系统 → ObjectManager::Shutdown
```

测试：可只起 OM（+ Reflection），不建 Engine；测 EngineSubsystem 再起 Engine。

### 迁移注意

- 从 `Engine` 删除 `m_ObjectManager` 成员；`StartSystems`/`ShutdownSystems` 改为调用进程 `ObjectManager::Initialize/Shutdown`（或由更外层 bootstrap 调，Engine 只断言已 Init）。  
- `CollectGarbageWithEngineRoots`：根访问改为「若 `Engine::HasInstance` 则访问 Engine/Scene…」，OM 本身不持有 Engine。

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `GameInstance` | 会话寿命；持有 GI 子系统集合；绑定当前 Scene/WorldContext | 体素/UI 业务 |
| `SubsystemCollection` | 发现/创建/Init/Deinit/Get；依赖拉起 | 业务 Tick 语义 |
| `EngineSubsystem` | 进程级扩展 | 每 PIE 隔离状态 |
| `GameInstanceSubsystem` | 会话级扩展 | 代替 Scene 存世界体素 |
| `WorldSubsystem` | 每 Scene 扩展；Play 钩子 | 跨 PIE 共享可变世界 |
| `Engine` / Play Mode | 创建销毁 GI；调用集合相位 | 游戏 Content 内容 |
| 游戏模块 | 具体子系统实现与注册 | 改引擎硬编码系统列表（除非自愿迁移） |

## Boundaries

- Allowed：游戏 DLL → 注册子系统类型；`GetSubsystem<T>()` 从 GI/Scene/Engine 取。  
- Forbidden：在 `WorldSubsystem::Initialize` 内假设 BeginPlay 已发生；在 GO `Tick` 内当作唯一 App 入口长期模式（迁移后删除）。  
- Engine 硬编码系统与用户 Subsystem **双轨并存** 直至有迁移切片。

## Dependencies

- Depends on：CORE-F05、F31、F33（结构变更）。  
- Depended on by：minCraft 会话重构；未来 LocalPlayer、正式模块入口。  
- 新依赖方向：Scene → 拥有 WorldSubsystemCollection（Scene 打开时 Init）——合理。

## Ownership & Lifetime

- `GameInstance`：Engine 或 PlaySession 拥有；PIE 实例表登记。  
- 子系统实例：Outer 为对应宿主；集合 Deinit 后销毁。  
- 禁止游戏用函数内 `static` 单例代替 GI/World 集合（文档与 stub 示范禁止）。

## Control Flow

见 §Proposed Architecture 生命周期。查询：`GameInstance::GetSubsystem<T>()` / `Scene::GetSubsystem<T>()` / `Engine::GetEngineSubsystem<T>()`。

## State & Invariants

- 任一时刻：Standalone ⇒ 至多一个活动 GI；PIE ⇒ 每 PIEInstanceId 至多一个 GI。  
- World 子系统集合仅在 Scene「结构就绪」之后 Initialize；在 Scene 拆解之前 Deinitialize。  
- `InitializeDependency` 仅允许在集合 `bPopulating` 期间调用（对齐 UE）。  
- pending-destroy GO 与子系统 Tick：子系统**不要**假设「本帧 Destroy 的 GO 已从 Find 消失」（F33 契约）。

## API / Interface Semantics（展开）

### 类型与反射

| 类型 | 基类 | ME_CLASS | Outer |
|------|------|----------|-------|
| `Subsystem` | `MEObject` | Abstract | 集合 Outer |
| `EngineSubsystem` | `Subsystem` | Abstract | `Engine`（或 Engine 持有的代理 Outer） |
| `GameInstanceSubsystem` | `Subsystem` | Abstract | `GameInstance` |
| `WorldSubsystem` | `Subsystem` | Abstract | `Scene` |
| `TickableWorldSubsystem` | `WorldSubsystem` | Abstract | `Scene` |
| `TickableGameInstanceSubsystem` | `GameInstanceSubsystem` | Abstract | `GameInstance` |
| `GameInstance` | `MEObject` | 可 Blueprintable 风格（可派生） | Engine / PlaySession |

发现：`SubsystemCollection::Initialize(outer)` 内  
`ReflectionSystem::Get().GetDerivedClasses(BaseType::StaticClass())`，跳过 abstract，对每个 CDO 调 `ShouldCreateSubsystem(outer)`。

### SubsystemCollection（接触面）

```cpp
class SubsystemCollectionBase
{
public:
    void Initialize(MEObject* outer);      // 反射填充；可重入保护
    void Deinitialize();                   // 已创建实例 Deinitialize；清空
    bool IsInitialized() const;

    // 仅允许在某 Subsystem::Initialize 期间调用（bPopulating）
    Subsystem* InitializeDependency(const Reflection::MEClass* subsystemClass);
    template<typename T> T* InitializeDependency();

    Subsystem* GetSubsystem(const Reflection::MEClass* subsystemClass) const;
    template<typename T> T* GetSubsystem() const;

    void ForEachSubsystem(const std::function<void(Subsystem*)>& fn) const;
};
```

测试旁路（非生产主路径）：`RegisterTypeForTest(const MEClass*)` 或预注册列表，避免无反射环境。

### GameInstance（接触面）

```cpp
class GameInstance : public MEObject
{
public:
    void Init();                 // ReceiveInit 钩子可选 → SubsystemCollection.Initialize(this)
    void Shutdown();             // 清玩家占位 → Collection.Deinitialize → 解绑 Scene

    Scene* GetPrimaryScene() const;
    void SetPrimaryScene(Scene* scene);   // Play/Standalone 绑定时调用

    int32_t GetPIEInstanceId() const;     // 非 PIE 为 -1
    bool IsDedicatedServerInstance() const; // 首期可恒 false

    template<typename T> T* GetSubsystem() { return m_Subsystems.GetSubsystem<T>(); }

    // 可选：游戏派生覆盖
    virtual void OnInit() {}
    virtual void OnShutdown() {}

private:
    SubsystemCollection</*GameInstanceSubsystem*/> m_Subsystems;
    Scene* m_PrimaryScene = nullptr;
    int32_t m_PIEInstanceId = -1;
};
```

引擎侧访问：

- Standalone：`Engine::GetGameInstance()`  
- PIE：`Engine::GetGameInstanceForPIE(instanceId)` 或 PlaySession 持有表  
- 便捷：`GameInstance::GetSubsystem<T>(gi)` 空安全静态糖（对齐 UE）

### World / Scene 接触面

```cpp
// Scene 增：
SubsystemCollection</*WorldSubsystem*/>& GetSubsystemCollection();
template<typename T> T* GetSubsystem();

void InitializeWorldSubsystems();     // Finalize / PIE 创建后；结构就绪
void PostInitializeWorldSubsystems();
void DeinitializeWorldSubsystems();   // 卸场景 / PIE Exit 前

// 由 Scene::DispatchBeginPlay / EndPlay 转发：
//   已有 GO BeginPlay 前后，调用各 WorldSubsystem::OnWorldBeginPlay/EndPlay
```

`WorldSubsystem`：

```cpp
class WorldSubsystem : public Subsystem
{
public:
    Scene* GetScene() const;   // Cast Outer
    virtual bool ShouldCreateSubsystem(MEObject* outer) const override;
    // 默认可按 SceneType：Editor / PIE / Game 过滤（对齐 UE DoesSupportWorldType）

    virtual void PostInitialize() {}
    virtual void OnWorldBeginPlay(Scene& scene) {}
    virtual void OnWorldEndPlay(Scene& scene, EEndPlayReason reason) {}
};

class TickableWorldSubsystem : public WorldSubsystem
{
public:
    void Initialize(SubsystemCollectionBase& collection) override; // Super + 登记到 Tick 列表
    void Deinitialize() override; // 从 Tick 列表移除 + Super
    virtual void Tick(float deltaTime) {}
    virtual bool IsTickable() const { return true; }
};
```

### Engine 接触面

```cpp
// Engine：
SubsystemCollection</*EngineSubsystem*/>& GetEngineSubsystemCollection();
template<typename T> T* GetEngineSubsystem();

GameInstance* GetGameInstance();                    // Standalone 或「主」实例
GameInstance* FindGameInstance(int32_t pieId);

// 生命周期钩子（由 StartSystems / Play Mode 调用）：
void InitializeEngineSubsystems();
std::shared_ptr<GameInstance> CreateGameInstance(/* class from config or default */);
void DestroyGameInstance(GameInstance*);
```

`LogicalTick` 增：在 Physics/UI 之后调用 `TickAllTickableSubsystems(dt)`（GI + 活动 Scene 的 World）。

### 游戏模块加载（契约占位；正式方案见上文专节）

不在此过早定 `GameModuleHost` 类名。F34 需要的引擎侧最小面：

```cpp
// Engine 或独立小模块（名称待模块契约 Design 敲定）
bool EnsureGameModuleLoaded();   // 有意引用 / Load；保证反射注册已进表
bool IsGameModuleLoaded() const;
```

可选：`MinEngine_NotifyGameModuleLoaded(Engine&)` — **仅机制握手**；禁止建 Scene/GO。子系统类型仍靠 `ME_CLASS` + `GetDerivedClasses`。  
S06：删 KeepAlive/Marker；Sample 模块示范子系统；契约文档与 ED-F20 对齐升级。

### Play Mode 接触面（CORE-F05 接线清单）

| 时机 | 调用 |
|------|------|
| Standalone 引擎就绪 | `CreateGameInstance` → `Init` → 主 Scene `InitializeWorldSubsystems` → `PostInitialize` |
| PIE Enter（每实例） | 创建该实例 GI → 绑 PIE Scene → `GI.Init` → Scene `InitializeWorldSubsystems` → `PostInitialize` → 现有 `DispatchBeginPlay`（内转 OnWorldBeginPlay） |
| PIE Exit | `DispatchEndPlay`（含 OnWorldEndPlay）→ `DeinitializeWorldSubsystems` → `GI.Shutdown` → Destroy GI |
| Editor Scene 加载 | 可 `InitializeWorldSubsystems`（Edit 预览）；**不**创建 Play GI |

### 错误语义

| API | 失败 |
|-----|------|
| `GetSubsystem<T>` | 未创建 / ShouldCreate=false → `nullptr` |
| `InitializeDependency` 在非 Populate 期 | Error + nullptr |
| 依赖环（A→B→A） | Error 日志；打断该依赖链；assert Debug |
| `Initialize` 重复 | no-op 或 Error（Collection 已 Init） |
| Notify 内结构变更 | Error（检测 StructuralTraversal 或文档+审查） |

## Failure Model

| 失败 | 行为 |
|------|------|
| ShouldCreate=false | 不创建；Get 为 null |
| Init 中依赖缺失/环 | Error 日志；失败子系统不进入可用表 |
| PIE Exit 未 Deinit | Fatal/Error；测试覆盖 |
| 在 GO Tick 内 Destroy GI | 禁止；仅 Play 会话路径 |

## Alternatives & Trade-offs

### Option A（推荐）— UE 式宿主 + Collection

- 学 UE 分层与顺序；尺度缩小。  
- Pros：心智清晰；PIE 隔离自然；可扩展。  
- Cons：实现面大；需改 Play Mode 接线。

### Option B — 仅 SubsystemManager 无 GI

- 反哺稿形态。  
- Why not：缺会话宿主；Engine/App/PIE 语义糊；已否决。

### Option C — Godot autoload

- Why not：难表达 per-PIE；与现有 Scene/PIE 模型不合。

## Integration Impact

- `Engine::StartSystems` / `ShutdownSystems` / `LogicalTick`。  
- Play Mode Enter/Exit（CORE-F05）。  
- `Scene` 增加 World 集合；可能 `FinalizeLoadedScene` / PIE register 路径。  
- `GameModuleStub` 示范；minCraft 迁移在游戏仓跟进（不挡引擎切片验收）。  
- HostComponent 长期降级为锚点或删除（游戏仓）。

## Migration / Compatibility

- 双轨：旧 Host+Runtime 可暂时共存；引擎侧 Feature Done 以 stub + 测试为准。  
- 前向：不保留「Component 当 App」为推荐路径；文档标明 Deprecated。

## Implementation Slices（允许偏大，但可验证）

| Slice | 范围 | 验证 |
|-------|------|------|
| **S00** | **ObjectManager 进程单例**（脱离 Engine 拥有）；bootstrap / 测试 / Shutdown 序 | **Done** — 静态 Init/Shutdown + `s_Owned`；Engine 无 `m_ObjectManager` |
| **S01** | `Engine : MEObject` + `Subsystem`/`SubsystemCollection` + 反射发现；Engine 集合 | **Done** — Init/Deinit；`InitializeDependency`；环检测；suite `game-instance-subsystem` |
| **S02** | `GameInstance` + GI 集合；Standalone Create/Init/Shutdown | **Done** — Create/Destroy；GetSubsystem；Shutdown 逆序 |
| **S03** | Scene World 集合；Finalize/卸载；PostInitialize + BeginPlay/EndPlay 转发 | **Done** — suite `world-subsystem`；Finalize/Unload/PIE 卸；DoesSupportWorldType |
| **S04** | Play Mode：每 PIE 一 GI；顺序对齐 UE | **Done** — `CreateGameInstanceForPIE`；Enter: GI→World→BeginPlay；Exit 卸 World+GI |
| **S05** | Tickable；LogicalTick 单插入点（Physics 后、EOF 前） | **Done** — `TickTickableSubsystems`；suite `tickable-subsystem`；F33 回归 |
| **S06** | Sample 示范 GI/World 子系统；与 **BUILD-F01** 对齐删 KeepAlive | Sample suite；反射可见游戏子系统类 |
| **S07** |（可选）LocalPlayer | 后置 |
| **S08** |（游戏仓）minCraft → GI/World 子系统 | 另 Feature |

**开展顺序：** S00→S01→S02→S03→S04 主链；S05 随后；S06 与 BUILD-F01 同步。

## Verification Strategy

- 单元/集成 suite：`game-instance`、`subsystem-collection`、`world-subsystem-lifecycle`。  
- PIE：Enter 两实例（若支持）状态隔离；Exit 后无悬空集合。  
- 依赖：A InitializeDependency B 时 B 先于 A 完成 Init。  
- 回归：`begin-play`、`deferred-structural-edit`、Play Mode 冒烟。

## Open Questions

1. **Content 默认挂哪层？** 只读项目表 → Engine；会话态 → GI。 — **Accepted**  
2. **PreSim/PostSim？** 不对外双相位；单 Tick。 — **Accepted**  
3. **反射发现 + MEObject 子系统？** 是。 — **Accepted**  
4. **Edit World 子系统？** Finalize + `ShouldCreate`。 — **Accepted**  
5. **构建 / KeepAlive？** 交 **BUILD-F01**；F34 S06 对齐删除。 — **Accepted**  
6. **Engine : MEObject？** 是。 — **Accepted**  
7. **ObjectManager 进程单例（非 Engine 拥有）？** **是。** — **Accepted**

## Design Review

- **Verdict:** `Ready with deferred items`（BUILD-F01 并行；不挡 S00–S05）
- **Reviewer / date:** agent iterative review 2026-10-12
- **Link or summary:** Implementation Readiness；构建产品化显式拆出

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved（OM 单例 + Engine MEObject）
- [x] Major state/invariants resolved
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] OQ 已接受；模块契约 → BUILD-F01（Deferred 出主链）

**Ready for implementation?** `yes` — 从 **S00**（ObjectManager 进程单例）→ **S01**（Engine MEObject + Collection）。

## Acceptance Checklist

- [x] Design review（iterative；Verdict Ready with deferred）
- [ ] Implementation plan / slices 按上表执行（S00–S05 Done；S06 Sample 待做）
- [x] Progress log updated
- [x] Feature registry status synced
- [x] Suites 证明 GI/World/Engine 三层寿命
- [x] Play Mode PIE 接线无泄漏（MVP 单实例 id=0；多实例 API 已备）
- [ ] 文档明确：游戏业务子系统不进 Core
- [x] S00：Engine 不再拥有 ObjectManager
- [x] S01：Engine : MEObject + EngineSubsystem 集合
- [x] S02：GameInstance + GI 集合 Standalone Create/Init/Shutdown
- [x] S03：Scene WorldSubsystem 集合 + Begin/EndPlay 转发
- [x] S04：Play Mode 每 PIE 一 GI（顺序对齐）
- [x] S05：Tickable 相位（Physics/UI 后、EOF 前）

## 附录 A — 与反哺稿差异（显式划掉）

| 反哺稿 | 本 Design |
|--------|-----------|
| 无 GI，只有 SubsystemManager | **先有 GameInstance** |
| App 级名叫 EngineSubsystem | Engine ≠ GameInstance；业务会话用 GI |
| PostFrame 作为唯一安全 Spawn | **F33 已处理**；子系统 Tick 不绑定「才能 Create」 |
| 首期全局拓扑排序引擎 | **InitializeDependency** |
| 与 KeepAlive 强绑定 | **BUILD-F01 + S06** 删除 KeepAlive |

## 附录 B — UE 阅读地图（复习）

- `Engine/Source/Runtime/Engine/Classes/Engine/GameInstance.h` — 寿命注释  
- `Private/GameInstance.cpp` — `Init` / `Shutdown` / `InitializeForPlayInEditor`  
- `Public/Subsystems/Subsystem.h` — 五层寿命  
- `Private/Subsystems/SubsystemCollection.cpp` — `Initialize` / `AddAndInitializeSubsystem` / `InitializeDependency`  
- `Private/World.cpp` — `InitializeSubsystems` / `PostInitializeSubsystems`  
- Godot `main/main.cpp` autoload — 对照用，非主模型
