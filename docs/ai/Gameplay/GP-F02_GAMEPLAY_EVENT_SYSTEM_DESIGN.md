# GameplayEventSystem — Design Spec

## Meta
- **ID:** `GP-F02`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Related:** [Implementation](./GP-F02_GAMEPLAY_EVENT_SYSTEM_IMPLEMENTATION.md), [GP-F01 Tag](./GP-F01_GAMEPLAY_TAG_DESIGN.md), [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [CORE-F04 Delegates](../Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)
- **Reference:** CardGameDemo `CORE-F03`（`packages/core/src/events/`）；设计动机见 CardGameDemo `docs/design/systems/gameplay-framework.md` § GameplayEvent System

## TL;DR

提供 **Scene 作用域** 的 Tag 通道 **发布-订阅** 消息机制（非 MulticastDelegate）。形态为挂在 GO 上的 **`GameplayEventSystemComponent`**：一个 Scene 使用一份组件实例；Channel 用 `GameplayTag` 标识；Event 以 Tag 容器描述类型/维度。本期只做基底（同步 dispatch、过滤、优先级、重入深度）；**Payload 延后**；不做 ASC / GA 集成。

## Scope
- **In:**
  - `GameplayEvent`：携带 `GameplayTagContainer`（事件标签）；**无 payload 字段或仅占位且未使用**
  - `GameplayEventChannel`：由 `GameplayTag` 标识的通道句柄
  - `GameplayEventSystemComponent`（`Component`）：`Channel` / `Dispatch` / `Subscribe` / `Unsubscribe`
  - Default channel（native tag `Channel.Default`）
  - 监听过滤：`requiredAll` / `requiredAny`（对 event.tags）
  - Priority + 稳定注册序；`maxDispatchDepth` 防重入爆炸
  - Scene 内查找约定（见 §3.1）
  - 单元测试（可构造独立 Scene/GO 或轻量 fixture）
- **Out:**
  - Payload 设计与类型系统（另开切片/Feature）
  - 异步 / 跨线程队列
  - ASC、Granted Ability、GE Ongoing 等
  - 用 EventSystem 替换 `MulticastDelegate`
  - 全局（跨 Scene）Event bus
  - Editor 可视化订阅图、Lua 正式 API（可后续）

## Reader quick start
1. 本文件 — Scene 作用域、Component 合同、与 Delegate 分工
2. [Implementation](./GP-F02_GAMEPLAY_EVENT_SYSTEM_IMPLEMENTATION.md)
3. 代码落点（规划）：`Runtime/Function/GameplayFramework/Events/`
4. 依赖：[GP-F01](./GP-F01_GAMEPLAY_TAG_DESIGN.md) Done 或至少 Manager + Container 可用

---

## 1) 背景与目标

### Pain
- Delegate 适合**编译期已知签名**的点对点/多播；玩法侧常有「潜在多监听者 + 按语义过滤」的广播需求。
- CardGameDemo 已验证：用 Tag 表达事件维度 + Channel 分区，比枚举事件更可扩展。

### Goals
- 引擎提供 **机制级** pub-sub，作用范围绑定 **单个 Scene**（含 PIE 双 Scene 自然隔离）。
- 不引入 Gameplay Framework 上层（无 ASC）。
- Payload 明确 **Deferred**，避免过早定 C++ 类型方案。

### Success
- 同一 Engine 下两个 Scene 各有 Event 组件时，dispatch 互不串扰。
- 测试覆盖：subscribe 过滤、priority、重入深度上限、default channel。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| `MulticastDelegate` | Done — 互补，不替代 |
| `GameplayTag*` | `GP-F01`（本 Feature 硬依赖） |
| Scene / Component / PIE | CORE-F05 MVP Done — 双 Scene 已存在 |
| GameplayEvent | **无** |

---

## 3) 方案

### 3.1 所有权与 Scene 作用域（维护者确认）

```text
Engine
  └── GameplayTagManager          (全局词汇表)

Scene (Editor 或 PIE)
  └── GameObject (约定：系统/根对象之一)
        └── GameplayEventSystemComponent
              ├── channels keyed by GameplayTag
              ├── listeners
              └── Dispatch / Subscribe
```

| 决策 | 选择 | 说明 |
|------|------|------|
| 作用范围 | **一个 Scene 一份** | PIE 时 Editor Scene 与 PIE Scene 各自独立 |
| 形态 | **GO Component** | 非 Engine 全局单例；随 Scene 卸载销毁 |
| Tag | 用全局 Manager | Channel/Event tags 来自 `GP-F01` |
| Payload | **本期不做** | Event 仅 tags；扩展点另议 |

**查找约定（MVP）：**

1. 约定每个需要 Event 的 Scene **至多一个**活跃的 `GameplayEventSystemComponent`（重复时 Warn，后注册或先注册策略在实现中二选一并测）。
2. 提供 Scene 级访问器（推荐）：`Scene::GetGameplayEventSystem()` → 缓存/查找该 Component；找不到返回 `nullptr`。
3. 不在 Engine 上挂全局 EventSystem。

**明白你的意思：** Tag 是进程/引擎级词汇与解析服务；Event 是**场景上下文内的消息总线**，用 Component 挂在该 Scene 的某个 GO 上，从而天然跟 Scene 生命周期与 PIE 克隆走。

### 3.2 与 MulticastDelegate 的分工

| | Delegate | GameplayEvent |
|--|----------|---------------|
| 签名 | 编译期固定 | 运行时 Tag 描述 |
| 发现监听者 | 调用方持有委托 | 总线 + 过滤 |
| 作用域 | 任意对象字段 | **Scene Component** |
| 典型用途 | Contact、UI 控件、资源变更 | 玩法语义广播、被动监听 |

禁止：把物理 Contact 强行改走 EventSystem 仅因「有总线」——PHYS-F03 仍以 Delegate 为主路径，除非另开设计。

### 3.3 数据流

```text
Publisher:
  auto* bus = scene->GetGameplayEventSystem();
  bus->Dispatch(channelTagOrDefault, eventWithTags);

GameplayEventSystemComponent:
  snapshot event.tags
  select listeners on that channel
  filter requiredAll / requiredAny
  sort by priority desc, then registration order
  call handlers sync (depth++)

Subscriber:
  bus->Subscribe({ channel, requiredAll/Any, priority, handler })
  → listenerId ; Unsubscribe(listenerId)
```

### 3.4 API 合同（草案）

```text
GameplayEvent
  - tags: GameplayTagContainer
  - // payload: DEFERRED — 不出现在 MVP 公共 API，或 private 预留且无访问器

GameplayEventChannel
  - tag: GameplayTag  (identity)

GameplayEventSystemComponent : Component
  - Channel(tag) → channel handle
  - Dispatch(event)                    // default channel
  - Dispatch(channel, event)
  - Subscribe(options) → listenerId
  - Unsubscribe(listenerId) → bool
  - GetMaxDispatchDepth() / Set…（可选；默认 16）

SubscribeOptions
  - channel
  - priority (default 0)
  - requiredAll / requiredAny (optional tag lists)
  - handler: void(const GameplayEvent&)
  - optional listenerId
```

**Event.tags：** 允许任意已注册 Tag；不做「必须是 GameplayEvent.* 根」限制（对齐 CardGameDemo D3/D4）。

**Channel：** Tag 仅作通道名；不强制与 event.tags 命名空间相同。

### 3.5 生命周期与 PIE

- Component 随 Owner GO / Scene 销毁；listeners 清空。
- Scene 克隆进 PIE：若系统 GO 被克隆且带该 Component，PIE Scene 获得**新**总线实例（与 Editor 隔离）。若克隆策略不复制该 GO，则需在 EnterPlay 路径显式确保 PIE Scene 有一份（**实现期核对 SceneDuplicator**；若缺口，记为本 Feature 风险切片，不静默依赖全局）。

### 3.6 Payload（Deferred 说明）

正式应用尚远。MVP **不设计** `std::any` / Reflection bag / 固定字段。若实现为减少未来 diff 而留空结构，**不得**在公共头文件暴露未完成 API。恢复条件：有第一个真实消费者再开 `GP-F02` 追加切片或 `GP-F03`。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Scene Component 总线 | 作用域清晰；PIE 隔离自然 | 需查找约定 | **选用** |
| B. Engine 全局 EventSystem | API 简单 | 双 World 串扰风险 | 否 |
| C. 每 GO 一个微总线 | 局部 | 跨对象广播难 | 否（非本需求） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Scene 内 0 个或多个 Component | 静默丢事件 / 歧义 | `GetGameplayEventSystem` + Warn；测试钉住 |
| PIE 克隆未带系统 GO | Play 无总线 | S 切片核对 Duplicator；必要时自动确保 |
| 与 Delegate 概念混淆 | API 滥用 | Design 分工表；命名保留 GameplayEvent |
| 无 payload 导致「先做无用」 | 动机不足 | 接受：本阶段即基底 + 测试合同 |

---

## 6) 验收标准

- [x] 依赖 `GP-F01` 的 Tag/Manager/Container
- [x] Component 位于 `Runtime/Function/GameplayFramework/Events/`
- [x] Default channel + 过滤 + priority + depth guard 有测试
- [x] 两 Scene 隔离有测试或明确 fixture
- [x] 无 Payload 公共 API；无 ASC
- [x] 文档与 Registry 状态可升至 Planned / In Progress

---

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft：Scene Component 作用域、Payload Deferred、对齐 CardGameDemo F03 |
| 2026-09-04 | Planned：与 GP-F01 Native 宏一并推进实现 |
| 2026-09-05 | Done：Scene Component + suite `gameplay-events` PASS；PIE 克隆路径未改（S05 结论：Clone 带 Component 即隔离，无额外补齐） |
