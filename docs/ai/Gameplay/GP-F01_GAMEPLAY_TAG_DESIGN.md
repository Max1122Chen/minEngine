# GameplayTag — Design Spec

## Meta
- **ID:** `GP-F01`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Related:** [Implementation](./GP-F01_GAMEPLAY_TAG_IMPLEMENTATION.md), [GP-F02 Event](./GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md), [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [ENGINE_DESIGN_PHILOSOPHY](../ENGINE_DESIGN_PHILOSOPHY.md)
- **Reference:** CardGameDemo `CORE-F02`（`packages/core/src/tags/`）；UE `FGameplayTag` / `FGameplayTagContainer` 语义

## TL;DR

引入 **UE 风格层级 GameplayTag** 作为引擎侧通用分类/状态标记机制：`GameplayTag` handle、`GameplayTagManager`（全局注册与解析）、`GameplayTagContainer`（可叠加计数 + 层级匹配）。本期只做**基底 + 单测**，不接正式玩法、不做 Editor 拾取器、不做 ASC。Manager **暂挂 Engine 子系统**（与 `AudioSystem` 同类拥有方式），未来可迁可插拔 Module。

## Scope
- **In:**
  - `GameplayTag`（稳定 index + name；`Matches` / `IsChildOf`）
  - `GameplayTagManager`：从定义表建树、`Resolve` / `TryResolve`、隐式父节点、`list`
  - `GameplayTagContainer`：`Add`/`Remove`（refcount）、`Has`/`HasAll`/`HasAny`（查询方可更泛）
  - Engine 启动时创建/持有 Manager；对外 `GameplayTagManager::Get()`（或 `Engine` 访问器）
  - 最小引擎内置 Tag（含 Event 所需 `Channel.Default`）
  - **C++ Native 注册宏**（对齐 UE `UE_DECLARE/DEFINE_GAMEPLAY_TAG*` 用法）
  - 单元测试 suite
- **Out:**
  - `GameplayTagQuery` 布尔表达式语言
  - Tag 重定向 / 重命名迁移
  - Editor Tag Picker、表格导入管线（JSON 双源可二期）
  - 网络复制、序列化进 Scene 资产（可后续）
  - ASC / Attribute / GE / GA / Pawn 等任何 Framework 上层
  - 把业务 Tag 词汇（Combat/Dungeon 等）写进引擎默认表

## Reader quick start
1. 本文件 — 语义、所有权、API 合同
2. [Implementation](./GP-F01_GAMEPLAY_TAG_IMPLEMENTATION.md) — 切片
3. 代码落点（规划）：`Runtime/Function/GameplayFramework/Tags/`

---

## 1) 背景与目标

### Pain
- 引擎缺少「可扩展、层级化」的标记机制；布尔/枚举状态在玩法扩展时会侵入类型。
- 后续 `GP-F02` Event 通道与过滤依赖 Tag。

### Goals
- 提供与 UE 对齐的 **机制**（Capabilities，非意见）：只解决「是什么 / 现在是什么」的标记与查询。
- 全局一份注册表，保证 Tag handle 跨 Scene 可比对。
- 为未来可插拔 Module 预留迁出路径，但**本期不实现 Module 系统**。

### Success
- `minEngineTests` 有独立 suite 钉住层级匹配、refcount、未知 Tag 失败策略。
- Engine 初始化后可 `Resolve` native tags；无玩法业务依赖。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| GameplayTag / Container / Manager | **无** |
| MulticastDelegate（`CORE-F04`） | Done — **点对点类型安全事件**；与 Tag/Event 互补，不替代 |
| Engine 子系统形态 | `shared_ptr` 成员 + `XxxSystem::Get()`（无 UE `UGameInstanceSubsystem` 框架） |
| `Runtime/Function/GameplayFramework/` | **尚不存在**（本 Feature 创建） |

---

## 3) 方案

### 3.1 模块边界与所有权

```text
Engine::Initialize
    └── owns GameplayTagManager   (process / engine lifetime)
            ├── Resolve("Status.Stunned") → GameplayTag
            └── used by containers, GP-F02 channels, future callers

Scene A / Scene B (PIE)
    └── 各自持有 Tag *instances in containers*；比较用同一 Manager 的 index/name
```

| 决策 | 选择 | 说明 |
|------|------|------|
| 生命周期 | **Engine-owned** | 与 `AudioSystem`/`PhysicsSystem` 同级「暂作子系统」 |
| 全局性 | **一份 Manager** | Tag 是词汇表；不按 Scene 复制注册表 |
| 未来 Module | 文档约定可迁出 | 本期不引入插件加载器 |
| 落点 | `Runtime/Function/GameplayFramework/Tags/` | 非 `Runtime/Core` |

**哲学对齐：** Tag 是机制；引擎默认表只含基础设施 Tag（如 `Channel.Default`），不含游戏规则词汇。

### 3.2 语义（对齐 UE / CardGameDemo CORE-F02）

- 名称：`A.B.C`；`.` 分层级；加载时**隐式创建父节点** `A`、`A.B`。
- Container **只存显式 Add 的 Tag**（不自动插入父 Tag）。
- 匹配方向：`container.Has(query)` 为真，当且仅当存在 `tag` 使 `tag.Matches(query)`  
  （`tag` 等于 `query` **或** `tag` 是 `query` 的后代）。
- 例：持有 `Status.Debuff.Vulnerable` → `Has(Status.Debuff)` **true**；`Has(Status.Debuff.Heavy)` **false**。
- Stacking：同一 Tag 多次 `Add` 增加 count；`Remove` 减 count，至 0 移除。

### 3.3 API 合同（草案）

```text
GameplayTag
  - GetIndex() / GetName()
  - Matches(const GameplayTag& query) const
  - IsChildOf(const GameplayTag& ancestor) const

GameplayTagManager
  - static Get() / HasInstance()
  - Resolve(name) → tag or fail
  - TryResolve(name) → optional
  - GetParent(tag) → optional
  - IsValidTag(tag) → bool
  - ListTags() → span/list
  - InitializeFromNative(+ optional later sources)
  - Shutdown()

GameplayTagContainer
  - Add / Remove / Clear
  - GetCount(tag)
  - Has / HasAll / HasAny
  - Clone()
```

**未知 Tag：** `Resolve` 失败（assert/log + 明确错误）；禁止静默创建运行时 Tag（避免拼写污染词汇表）。MVP 以 native 编译期注册为准；JSON 双源 **Deferred**。

### 3.4 C++ Native 注册宏

对齐 UE Native Gameplay Tags 工作流，便于模块/游戏代码在 C++ 侧声明稳定 Tag 变量，而无需手写字符串散落：

| 宏 | 放置 | 作用 |
|----|------|------|
| `ME_DECLARE_GAMEPLAY_TAG_EXTERN(TagVar)` | `.h` | 声明跨 TU 可见的 `NativeGameplayTag` |
| `ME_DEFINE_GAMEPLAY_TAG(TagVar, "A.B.C")` | `.cpp` | 定义并**注册**到启动收集表；提供可转 `GameplayTag` 的变量 |
| `ME_DEFINE_GAMEPLAY_TAG_STATIC(TagVar, "A.B.C")` | `.cpp` | 仅本 TU 可见；同样注册 |

```cpp
// NativeGameplayTags.h (engine)
ME_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Channel_Default);

// NativeGameplayTags.cpp
ME_DEFINE_GAMEPLAY_TAG(TAG_Channel_Default, "Channel.Default");

// Game / module code
ME_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Status_Stunned);
// .cpp
ME_DEFINE_GAMEPLAY_TAG(TAG_Status_Stunned, "Status.Stunned");

GameplayTag stunned = TAG_Status_Stunned; // after Manager::Initialize
```

**约定：**

- 静态构造阶段只**登记名称**；`GameplayTagManager::Initialize` 建树后，`NativeGameplayTag` 才可安全解析为 `GameplayTag`。
- 引擎内置 Tag 与用户 `ME_DEFINE_*` 一并进入同一 Manager。
- 命名建议：`TAG_` 前缀 + 下划线分层（如 `TAG_Status_Debuff_Vulnerable`），字符串仍用 `.` 层级。

### 3.5 与 Component 的关系

本期 **不** 提供「挂在角色上的 TagComponent / ASC」。Container 是纯数据工具类型；谁持有（未来组件、Effect、测试）由调用方决定。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Engine 全局 Manager | 跨 Scene 一致；Event Channel Tag 稳定 | 与「一切可插拔」有张力 | **选用（暂）** |
| B. 每 Scene 一份 Manager | 隔离强 | 同名 Tag 跨 Scene 不可比；PIE 痛苦 | 否 |
| C. 仅字符串无注册表 | 实现快 | 无稳定 handle、无严格未知检测 | 否 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 「子系统」名暗示 UE Subsystem 框架 | 范围膨胀 | 文档写明：仅 Engine 持有 + `Get()`，不做 Subsystem 框架 |
| 默认 Tag 表塞入玩法词汇 | Core 意见化 | 默认表最小化；业务 Tag 由项目侧后续扩展 |
| 过早序列化进 `.mescene` | Binary/JSON 债 | 本期仅内存 + 测试 |

---

## 6) 验收标准

- [x] `GameplayTagManager` 在 Engine 启停路径正确创建/销毁
- [x] Native 宏可注册 Tag，并在 Initialize 后解析为 `GameplayTag`
- [x] 层级匹配与 count 行为有自动化测试
- [x] 未知 Tag `Resolve` 失败行为有测试
- [x] 无 ASC / 无业务 Tag 表 / 无 Editor UI
- [x] 代码位于 `Runtime/Function/GameplayFramework/`

---

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft：基底范围、Engine 所有权、落点、对齐 CardGameDemo F02 |
| 2026-09-04 | Planned：补充 `ME_DECLARE/DEFINE_GAMEPLAY_TAG*` Native 注册宏 |
| 2026-09-05 | Done：Engine 挂载 + suite `gameplay-tags` PASS |
