# CORE-F35 — GameInstance 会话中枢 · 项目可选派生类

## Meta
- **ID:** `CORE-F35`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-14`（S01–S03 引擎侧 Done；S04 minCraft 后置）
- **Related:**
  - [CORE-F34](./CORE-F34_GAME_INSTANCE_AND_SUBSYSTEM_LIFECYCLE_DESIGN.md)（GI / Subsystem 寿命与 Collection；**Done**）
  - [BUILD-F01](../ProductBuild/BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)（游戏模块进进程）
  - [ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md)（Mechanism over Policy；Capabilities not Opinions）
  - UE 锚点：`UGameMapsSettings::GameInstanceClass`；`UGameEngine` / `UEditorEngine::CreateInnerProcessPIEGameInstance` 按 Class `NewObject`
- **Depends on:** `CORE-F34`（已合入）
- **Blocks:** 游戏仓用派生 GI 做会话编排；避免「全塞进 Subsystem」导致的横向耦合

## TL;DR

F34 交付了 **GI + 分层 Subsystem 机制**，但创建仍写死基类 `GameInstance`，且文档易被读成「业务全靠 GI Subsystem」。本 Feature 钉死两条产品契约：**(1) GameInstance 是会话中枢（Hub），子系统是可发现扩展；(2) 项目通过设置选定派生 GI 类**（对齐 UE soft class path）。引擎不规定玩法语义；游戏在派生 GI 里编排，在子系统里承载可替换能力。

## Problem

1. **创建写死基类：** `Engine::CreateGameInstance` / `CreateGameInstanceForPIE` 使用 `NewObject<GameInstance>`；游戏模块无法换成 `MinCraftGameInstance` 等派生类。F34 设计草图已写 `class from config or default`，实现未落地。
2. **中枢层缺失的误解：** 若把所有会话逻辑都做成并列的 `GameInstanceSubsystem`，则：
   - 缺少 **单一编排入口**（Init 阶段、Play 边界、跨子系统顺序）；
   - 子系统易互相 `GetSubsystem` 形成 **网状依赖**（过耦合、难测、难替换）；
   - 「谁代表本局游戏」在调试与 Agent 查询上不清晰。
3. **UE 对照已证明中枢必要：** `UGameInstance` 持有 `SubsystemCollection`，且项目可选派生类；Init/Shutdown、LocalPlayer、Travel 等挂在 GI 虚函数 / 中枢 API 上，而不是「只有子系统、没有宿主」。

## Requirements

- **R1** `GameInstance` 明确为 **会话中枢**：拥有 GI 子系统集合；提供可覆盖的会话相位钩子；是跨 GI 子系统编排的默认归属。
- **R2** `GameInstanceSubsystem` 明确为 **中枢上的扩展能力**：按反射自动发现；通过 `InitializeDependency` / 中枢 API 协作；**禁止**把「本局游戏总控」拆成多个平级「伪中枢」子系统。
- **R3** 项目可配置 GI 类（软类名）；Standalone 与 PIE **共用同一解析规则**；无效类回退基类并打 Error。
- **R4** 基类 `GameInstance` 在无配置时行为与今日一致（回归 F34 测试）。
- **R5** 游戏仓能派生 GI、在 `.mesettings`（或等价设置）声明类名，并在 Init 中编排本游戏会话（仍不把 Inventory 等业务类型放进 Engine Core）。

## Constraints

- 不引入 UE Online / Travel / LocalPlayer 全套（LocalPlayer 仍属 F34 S07 后置）。
- 不新增第二套「GameApp / SessionManager」与 GI 并行（避免双中枢）。
- Class 解析依赖：游戏 SHARED 已进进程 + `ME_CLASS` 已注册（BUILD-F01 显式 link）。
- `ProjectSettings` 现有序列化路径须兼容（新字段默认空）。

## Preferences

- 配置形态对齐 UE：`ProjectSettings`（或嵌套 `GameMaps` 段）上的字符串类路径，而非仅 C++ `SetGameInstanceClass`（后者可作测试旁路）。
- 文档与 Sample 同时示范：**派生 GI（中枢）+ 1～2 个 GI Subsystem（扩展）**。

## Assumptions

- 反射 `FindClass` / 按名解析在引擎侧已足够支撑「字符串 → `MEClass*`」（若缺口则本 Feature 含最小解析 API）。
- `NewObject` 已支持或可短补「按 `MEClass*` 创建」路径。

## Non-Goals

- 用 Subsystem 取代 GameInstance。
- 强制每个游戏必须派生 GI（空配置 = 基类即可；小样例可只用子系统）。
- GameMode / Level 脚本体系。
- Editor 专用 Session GI（Edit 模式仍默认不创建 Play GI）。

## Current Architecture

```text
Engine
  ├─ CreateGameInstance / ForPIE  → NewObject<GameInstance>   ← 写死
  ├─ GameInstance (基类)
  │     OnInit / OnShutdown（虚，几乎无游戏覆盖入口）
  │     SubsystemCollection<GameInstanceSubsystem>  ← 反射 Populate
  └─ Scene → WorldSubsystemCollection
```

- F34：**寿命、顺序、Tickable、PIE 每实例一份 GI** 已正确。
- 缺口：**类可替换** + **中枢 vs 扩展的职责叙事与编排约定**。

## Proposed Architecture

### 分层心智（锁定）

```text
Engine                          // 进程 / 模块 / 引擎子系统
  └── GameInstance (Hub)        // ★ 本局/本 PIE 会话中枢 —— 可项目派生
        ├── 会话相位钩子         // OnInit / OnShutdown /（预留）Play 边界
        ├── 编排 API（游戏扩展） // 查询子系统、启动会话步骤、持有会话级句柄
        └── GameInstanceSubsystem…  // 可发现、可替换的能力块（非第二中枢）
              World/Scene 另有 WorldSubsystem（世界作用域，不替代 GI）
```

| 概念 | 一句话 |
|------|--------|
| **Hub（GI）** | 「这一局游戏会话」的根对象；决定顺序、持有跨能力编排；可派生。 |
| **Extension（GI Subsystem）** | 挂在中枢上的机制/服务；单一职责；经依赖声明或经中枢协调。 |
| **Policy（游戏派生 GI）** | 项目自己的编排策略（先 Content 再 UI 等）；引擎不内置玩法类型。 |

### 配置驱动创建（对齐 UE）

```text
ProjectSettings.GameInstanceClass  // string；空 = minEngine::GameInstance
        │
        ▼
Engine::ResolveGameInstanceClass()
  → 反射解析 → 必须派生自 GameInstance → 否则 Error + 回退基类
        │
        ▼
CreateGameInstance / CreateGameInstanceForPIE
  → NewObject(resolvedClass, name, outer=Engine)
```

UE 对照：

- 设置：`UGameMapsSettings::GameInstanceClass`（`FSoftClassPath`）
- Game：`GameEngine.cpp` 读设置 → `NewObject<UGameInstance>(this, GameInstanceClass)`
- PIE：`PlayLevel.cpp` **同一设置字段** → `NewObject`

### 中枢如何避免子系统网状耦合

**允许：**

1. 子系统 `Initialize` 内 `InitializeDependency<T>()`（Collection 已支持；表达硬依赖）。
2. 子系统只依赖 **稳定接口 / 只读查询**；复杂编排上移到 **派生 GI**。
3. 派生 GI 在 `OnInit`（Collection 已 Initialize 之后，或文档钉死的相对顺序）中：
   - `GetSubsystem<ContentServices>()` 等拉取扩展；
   - 按游戏策略调用启动步骤（仍不创建 Scene 玩法对象若契约禁止——见 F34 模块加载约定）。

**禁止（规范 + 审查，首期以文档/Sample 约束）：**

1. 多个「AppSubsystem / SessionDirector」平级抢编排权。
2. 子系统 A ↔ B 双向业务回调形成环（依赖环 Collection 已 Error；业务环靠设计避免）。
3. 在无 GI 的情况下假设「全局会话服务」存在。

**推荐依赖方向：**

```text
派生 GameInstance  ──编排──►  GI Subsystem（单向）
GI Subsystem       ──依赖──►  其他 GI Subsystem（仅 InitializeDependency / 窄接口）
GI Subsystem       ──不──►   直接指挥「整个会话」相位（那是 Hub 的事）
```

### 基类接触面（相对 F34 的增量）

```cpp
// ProjectSettings 增：
ME_PROPERTY()
std::string GameInstanceClass;  // e.g. "MinCraftGameInstance"；空 = 基类

// Engine：
const Reflection::MEClass* ResolveGameInstanceClass() const;
// Create* 使用 NewObject(ResolveGameInstanceClass(), ...)

// GameInstance（已有虚钩子；文档升级为「中枢扩展点」）：
virtual void OnInit();      // 默认空；派生类做会话编排
virtual void OnShutdown();
```

可选（本 Feature 可 Deferred）：`OnWorldChanged` / Play 边界转发 —— 仅当 minCraft 证明需要再加，避免一次堆满 UE 表面。

### Sample / 游戏契约

- **GameModuleStub：** 可选增加 `StubGameInstance : GameInstance` + 设置类名（或测试旁路 SetClass）；示范 OnInit 查询 Stub GI Subsystem。
- **minCraft：** `MinCraftGameInstance` 作中枢；现有 Host/Runtime 会话逻辑逐步迁入「中枢编排 + 子系统能力」，而非更多 Scene Component 伪 App。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `Engine` | 解析项目 GI 类；创建/销毁 Standalone·PIE GI；默认 Outer | 玩法编排、Content 语义 |
| `ProjectSettings` | 持久化 `GameInstanceClass` 字符串 | 加载 DLL、反射注册 |
| `GameInstance`（基类） | 会话寿命；Collection Init/Deinit；中枢虚钩子；Tick 转发 | 具体游戏系统实现 |
| 派生 `*GameInstance`（游戏） | 本项目会话编排与策略 | 引擎通用机制 |
| `GameInstanceSubsystem` | 单一会话级能力 | 充当第二中枢 / 全局 App |
| `WorldSubsystem` | 世界/Scene 作用域能力 | 跨 PIE 会话单例（那是 GI） |

## Boundaries

- **Allowed：** 项目设置 → 引擎解析 Class → `NewObject`；派生 GI ↔ 子系统单向编排；子系统间 `InitializeDependency`。
- **Forbidden：** 引擎 Core 出现 `MinCraft*` / Inventory 等业务类型；绕过 GI 另立并行 Session 单例；Edit 默认创建 Play GI（保持 F34）。

## Dependencies

- Depends on：CORE-F34；ProjectManager / ProjectSettings 序列化；Reflection 按名找 Class；BUILD-F01 模块进进程。
- Depended on by：minCraft 会话重构；未来 LocalPlayer（挂在派生 GI 更自然）。

## Ownership & Lifetime

- 与 F34 相同：Engine 拥有 Standalone / PIE GI 表；GI 拥有其 Subsystem 实例。
- 派生类实例仍由 Engine `NewObject` 创建；游戏只提供 Class，不自己 `new` 挂到引擎外。

## Data Flow

```text
.mesettings (GameInstanceClass)
  → ProjectManager 加载 ProjectSettings
  → Engine::ResolveGameInstanceClass()
  → NewObject(Class) → GI::Init → Collection.Initialize → GI::OnInit（派生编排）
```

## Control Flow

- **Standalone：** 引擎就绪路径调用 `CreateGameInstance`（解析类）。
- **PIE Enter：** `CreateGameInstanceForPIE`（同一解析）。
- **PIE Exit / Shutdown：** 与 F34 相同销毁序；先派生 `OnShutdown`，再 Collection Deinit（实现须与现序对齐并写进验收）。

## State & Invariants

- 每个 Standalone / 每个 PIE id 至多一个 GI。
- `ResolveGameInstanceClass` 结果始终非空且 `IsChildOf(GameInstance)`。
- Collection Populate 时 Outer 为该 GI（含派生类型）—— `ShouldCreateSubsystem(outer)` 可见真实动态类型。

## API / Interface Semantics

| API | 语义 |
|-----|------|
| `ProjectSettings::GameInstanceClass` | 软类名；空 = 基类；非法 = 回退 + Error |
| `Engine::ResolveGameInstanceClass()` | 纯解析；可测；不创建实例 |
| `CreateGameInstance*` | 使用 Resolve 结果；失败断言/回退策略与现 NewObject 失败一致 |
| `GameInstance::OnInit` | Collection 已 Initialize 之后调用（**钉死顺序**，便于派生编排 GetSubsystem） |

**OnInit 相对 Collection 的顺序（本设计锁定）：**

```text
GI 构造 → SetPrimaryScene / SetPIEId → Init():
  1) Collection.Initialize(this)   // 子系统起来
  2) OnInit()                      // 派生中枢编排（可安全 GetSubsystem）
Shutdown():
  1) OnShutdown()
  2) Collection.Deinitialize()
```

若今日实现顺序不同，本 Feature **纠正并对齐文档**（属兼容修正，需测）。

## Failure Model

| 失败 | 行为 |
|------|------|
| 类名无法解析 | Error 日志；使用 `GameInstance` |
| 类非 GameInstance 派生 | Error；回退基类 |
| 游戏 DLL 未进进程导致找不到类 | Error；回退；文档强调 BUILD-F01 显式 link |
| OnInit 内再 Create/Destroy GI | 禁止；Debug 可检测 |

## Alternatives & Trade-offs

### Option A（推荐）— 中枢派生 + ProjectSettings 软类名

- 对齐 UE；Agent/人可改设置；Standalone/PIE 同源。
- Pros：中枢层清晰；子系统保持可组合。
- Cons：要补按 Class 创建与设置字段。

### Option B — 仅 C++ `SetGameInstanceClass`

- Why not **solo**：不持久化、不 Agent-friendly；可作 **测试旁路** 保留。

### Option C — 不设中枢，全用 GI Subsystem +「DirectorSubsystem」

- Why not：把中枢职责藏进又一个子系统，仍是假中枢，且与 UE/`GetGameInstance()` 心智冲突；子系统横向调度更易烂。

### Option D — 新建并行 `GameSession` / `AppHost` 类型

- Why not：与 F34 GI 双中枢；违背 Minimal Core / 已选 GI 名。

## Integration Impact

- `ProjectSettings` + `.mesettings` schema；Handbook / BOOTSTRAP / 模板 `mesettings.partial.json`。
- `Engine::CreateGameInstance*`；可能 `NewObject` by class。
- 核对 `GameInstance::Init/Shutdown` 与 `OnInit` 顺序。
- Sample +（后置）minCraft 派生类。
- F34 Design：Related 链到本 Feature；不重开 F34 Status。

## Migration / Compatibility

- 默认空类名 → 行为与现网一致。
- 游戏仓自愿派生；旧 HostComponent 路径可双轨，中枢就位后再收。

## Implementation Slices

| Slice | 范围 | 验证 |
|-------|------|------|
| **S01** | 钉死 Hub/Extension 职责；Init/OnInit 顺序；本 Design Review | **Done** |
| **S02** | `ProjectSettings::GameInstanceClass` + `ResolveGameInstanceClass` + Create* 按类创建；回退 | **Done** — suite `game-instance-subsystem` |
| **S03** | Sample：`StubGameInstance` + override；OnInit 触达 Stub GI Subsystem | **Done** — suite `game-module-stub` |
| **S04** |（游戏仓）`MinCraftGameInstance` + mesettings；最小编排迁入 | **Deferred**（不挡引擎 Done） |

## Verification Strategy

- 单元：Resolve 三种输入；Create 动态类型为派生类。
- 回归：F34 suites（`game-instance-subsystem`、PIE、tickable）。
- Sample：派生 GI OnInit 能 `GetSubsystem` 到模块内子系统。
- 文档：禁止「只用子系统当 App」的写法出现在推荐路径。

## Open Questions

1. **类名格式：** — **Accepted：** 反射全名 `#TYPE`（如 `minEngine::GameInstance`、`GameModuleStub::StubGameInstance`）。
2. **设置挂在 `ProjectSettings` 根还是嵌套 `Game`/`Maps` 段？** — **Accepted：** 根字段 `GameInstanceClass`。
3. **OnInit 是否允许在 Collection 前？** — **Accepted：** 否；Collection 后。

## Design Review

- **Verdict:** `Ready`
- **Reviewer / date:** maintainer + agent 2026-10-14
- **Link or summary:** Hub vs Extension 清晰；UE soft class 路径可执行；OnInit 顺序与现实现相反须在 S02 纠正。无挡开工缺口。测试旁路保留 `SetGameInstanceClassOverride`。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected（F34 + UE GameMapsSettings / GameEngine / PlayLevel）
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved
- [x] Major state/invariants resolved
- [x] API semantics sufficiently defined（含 OnInit 顺序）
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred

**Ready for implementation?** `yes`

## Acceptance Checklist

- [x] Design review complete（L2）
- [x] Implementation plan / slices 按上表（S04 Deferred）
- [x] Progress log / Registry 同步
- [x] 引擎侧：可配置类 + 回归 F34
- [x] Sample 示范中枢+扩展；文档不再暗示「子系统替代 GI」
