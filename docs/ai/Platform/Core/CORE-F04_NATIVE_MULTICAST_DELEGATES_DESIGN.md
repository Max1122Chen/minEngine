# Native Multicast Delegates — Design Spec

## Meta
- **ID:** `CORE-F04`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-04
- **Related:** [Implementation](./CORE-F04_NATIVE_MULTICAST_DELEGATES_IMPLEMENTATION.md), [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [TECH_DEBT.md](../../TECH_DEBT.md) **TD-006**, [PHYS-F03 placeholder](../../Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md), 旧占位 [REFLECTION_DELEGATES_DESIGN.md](../Reflection/REFLECTION_DELEGATES_DESIGN.md)

## TL;DR

引擎需要可订阅、可移除、可多播的事件（首要解锁 **PHYS-F03** Contact 玩法派发）。**本期只做类型安全的 Native Multicast Delegate**（类似 UE `TMulticastDelegate`），**不做**反射动态委托 / Inspector 绑定 / Lua 一等事件。落点 `Runtime/Core/Delegates/`；以独立测试竖切验收，再让物理/资产等子系统接入。

## Scope
- **In:**
  - 0–N 参数的 **multicast** 委托类型（模板 + 声明宏）。
  - `Add` / `Remove` / `RemoveAll` / `Clear` / `Broadcast` / `IsBound`。
  - 绑定形态：原始成员函数、`std::function`/lambda（可选）、**MEObject 存活检查的弱绑定**（推荐默认玩法路径）。
  - Broadcast **同步、同线程**；Broadcast 期间修改订阅列表的安全策略（快照或延迟）。
  - 单元测试 suite（非 smoke）。
  - 文档：付清 TD-006「未设计」部分；明确二期边界。
- **Out（明确非目标）:**
  - Dynamic / 反射委托（UE `DECLARE_DYNAMIC_*`）、函数名字符串绑定、序列化绑定图。
  - 与 `MEFunction` / `ProcessEvent` parms 缓冲共用执行路径。
  - Lua 正式事件 API（可后续用同一 Broadcast 边沿桥接，不在本期设计细节）。
  - 跨线程队列、异步派发、协程。
  - Inspector 可视化绑定、编辑器 Undo。
  - **PHYS-F03 实现本身**（本 Feature 只提供基础设施；物理派发另开/恢复 PHYS-F03）。
  - 强制立刻迁移所有现有 `Subscribe`/`std::function` 回调（可选跟进，不挡 Done）。

## Reader quick start
1. 本文件 — 两种委托拆分、MVP API、寿命与重入、验收。
2. [Implementation](./CORE-F04_NATIVE_MULTICAST_DELEGATES_IMPLEMENTATION.md) — S01–S03 切片与命令。
3. 代码入口（落地后）：`Runtime/Core/Delegates/`；测试：`Tests/Suites/DelegateTest.*`（名称以实现为准）。

---

## 1) 背景与目标

### Pain
- 玩法需要「一对多」通知（Contact Begin/End、资源变更等），但仓库只有零散 `std::function` / `Subscribe`，无统一移除、弱引用与重入约定。
- PHYS-F03 曾考虑 Collider 虚函数过渡 API，已否决：会污染组件契约；正确形态是委托订阅。
- 旧占位文档把委托绑死在「反射 ProcessEvent 完成后才能做」，与当前需求（C++ 物理派发）错位。

### Goals
- 提供 **一个** 清晰的 C++ 多播事件原语，编译期签名固定。
- **可移除、不死挂**：对象销毁后 Broadcast 不崩、不调用野指针。
- **Broadcast 行为可预测**（含重入规则），有测试钉住。
- 为 PHYS-F03、未来 AssetRegistry / 输入 / UI 事件提供同一套心智模型。

### Success
- `test delegates`（或等价 suite）覆盖 Add/Remove/Broadcast、析构后不回调、Broadcast 中途 Remove。
- TD-006 从「未设计」变为 **设计 Done + Native MVP 落地**（Lua 动态委托可另开 Feature，不再挡物理）。
- PHYS-F03 可重新写正式 Design，事件源仍为 F01 Contact 双缓冲，派发目标为本委托。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| 委托类型 / 宏 | **无** |
| `MEFunction` / Invoke MVP | 有；**不**作为本期 Broadcast 路径 |
| Lua Script\* | CORE-F01/F02 Done；事件订阅未统一 |
| `AssetManager::Subscribe` | 有专用 callback 表；可选日后迁到本 API |
| PHYS-F03 | Deferred，显式依赖本能力 |
| 旧文档 | [REFLECTION_DELEGATES_DESIGN.md](../Reflection/REFLECTION_DELEGATES_DESIGN.md) 占位 → 本文取代其「实施约束」角色 |

---

## 3) 方案

### 3.0 两种委托（分期）

| 形态 | 类比 | 本期 |
|------|------|------|
| **A. Native multicast** | UE `TMulticastDelegate<>` | **做** |
| **B. Dynamic / 反射** | UE `DECLARE_DYNAMIC_MULTICAST` | **不做**（另 Feature） |

**原则：** A 不依赖反射；B 将来可包装 A 或并行存在，但不得倒逼 A 的 API 变成字符串绑定。

### 3.1 模块边界

```text
Runtime/Core/Delegates/
  Delegate.h              // 单播（可选，若与多播共用存储可后补）
  MulticastDelegate.h     // 核心模板
  DelegateMacros.h        // DECLARE_MULTICAST_DELEGATE_* 
  DelegateHandle.h        // 移除用句柄（opaque id）
  MEObjectDelegateBindings.h  // MEObject* 弱绑定辅助（可同文件）

消费者（本期之后）:
  Physics / Asset / …     // Add + Broadcast；不拥有委托实现
```

- **不**放进 `Reflection/` 目录，避免与 MEFunction 生命周期耦合。
- 头文件以模板为主，尽量 header-only 或极薄 `.cpp`（Handle 发号器等）。

### 3.2 API 与行为契约（Draft）

命名可在实现时微调；语义以下为准。

#### 声明

```cpp
// 0 参数
DECLARE_MULTICAST_DELEGATE(FOnSomethingChanged);

// 1+ 参数（宏生成或可变模板；实现选一种，对外一致）
DECLARE_MULTICAST_DELEGATE_OneParam(FOnContactBegin, const PhysicsContactEvent& /* Event */);
```

展开为类型别名：`using FOnContactBegin = MulticastDelegate<void(const PhysicsContactEvent&)>;`（示意）。

#### 成员用法

```cpp
FOnContactBegin OnContactBegin;

DelegateHandle handle = OnContactBegin.AddRaw(listener, &Listener::OnBegin);
OnContactBegin.AddMEObject(meObject, &MyComponent::OnBegin); // 弱：Broadcast 前 IsValid/存活检查
OnContactBegin.AddLambda([](const PhysicsContactEvent& e) { /* … */ }); // 可选；Remove 靠 handle

OnContactBegin.Broadcast(event);
OnContactBegin.Remove(handle);
OnContactBegin.RemoveAll(listener); // 按实例移除该对象上所有绑定（若支持 Raw/MEObject）
OnContactBegin.Clear();
```

#### 不变量

1. **同步 Broadcast：** 调用线程上依次执行；无隐式任务队列。
2. **拷贝语义：** 委托实例 **不可随意拷贝**（删除拷贝或 =delete）；可移动（若需要）。避免双份订阅表。
3. **句柄：** `Add*` 返回 `DelegateHandle`；`Remove(handle)` 幂等（重复 Remove 安全）。
4. **寿命（MEObject 路径）：** Broadcast 时若目标已销毁则 **跳过并宜惰性剔除**；不得解引用野指针。
5. **重入：** Broadcast 期间允许 `Add`/`Remove`/`Clear`：
   - **选用：** Broadcast 开始时 **快照** 当前调用列表，只调用快照；修改作用于下一轮。  
   - （备选：延迟队列合并到 Broadcast 结束——实现更重，不作为默认。）
6. **异常：** C++ 路径假设回调不抛；若抛出则行为未定义（与引擎其它回调一致）。不引入跨回调 try/catch 框架。
7. **线程：** 非线程安全；跨线程 Subscribe/Broadcast 由调用方加锁或禁止（文档写明）。

#### 参数数量

- MVP：**0、1、2** 参数足够覆盖 Contact / 多数引擎事件；更多参数用 `const Payload&` 打包，或后续扩宏。
- 避免无限制的完美转发爆炸；优先可读宏 + 小套模板特化。

### 3.3 与 PHYS-F03 的衔接（不在本期实现）

- `PhysicsWorld`（或 System）在消费 Contact 双缓冲后 `OnContactBegin.Broadcast` / `OnContactEnd.Broadcast`。
- 组件侧 `AddMEObject(this, &T::Handler)`；组件销毁自动不再被调。
- 细节（挂在 World 还是 Collider、过滤 Trigger）留 PHYS-F03 Design。

### 3.4 与 Lua / 反射（远期备忘）

- 二期可选：`AddScript` / Dynamic Delegate 属性；或 Lua 侧显式 `bind(event, fn)` 调到 C++ `AddLambda`。
- **禁止**在本期为「将来 Lua」引入字符串方法名或强制走 `InvokeFunction`。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Native multicast（本方案） | 解锁物理；类型安全；实现可控 | 无编辑器绑函数名 | **选用** |
| B. 先做 Dynamic + MEFunction | 与旧占位一致 | 挡 PHYS-F03；复杂度高 | 拒绝作 MVP |
| C. 仅 `std::vector<std::function>` 各处复制 | 快 | 无统一 Remove/弱引用/重入 | 拒绝作平台 API |
| D. Collider 虚函数通知 | 实现快 | 污染组件 API（已否决） | 拒绝 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Broadcast 中修改列表导致迭代器失效 / 漏调 | 崩溃或逻辑错 | 快照调用；测试钉住 |
| Raw 绑定忘记 Remove | 野指针 | 文档强调优先 MEObject；测试覆盖销毁场景；可选 debug 下追踪 |
| 宏/模板膨胀 | 编译变慢 | 限制参数个数；头文件精简 |
| 与 AssetManager::Subscribe 双轨 | 两套心智 | Design 写明可选迁移，不挡 MVP Done |
| 误把 Dynamic 需求塞进 MVP | 范围爆炸 | §Scope Out + Review 门禁 |

---

## 6) 验收标准

- [x] Design Status → **Planned** 后实现；现为 **Done**。
- [x] `Runtime/Core/Delegates/` 落地；Multicast + Handle + MEObject 弱绑定。
- [x] 独立测试 suite `test delegates`：多订阅者 Broadcast、Remove(handle)、RemoveAll、对象销毁后不回调、Broadcast 中 Remove。
- [x] `TECH_DEBT` TD-006 → Done（Native MVP）；Dynamic/Lua 另议。
- [x] `PROGRESS_LOG` / ACTIVE_WORK 更新。
- [x] PHYS-F03 仍 Deferred，依赖指向 CORE-F04 Native API。

---

## 7) 拍板项（已确认）

| # | 问题 | 结论 |
|---|------|------|
| 1 | MVP 是否仅 Native multicast？ | **是** |
| 2 | 弱绑定默认用 `MEObject*` + 存活检查？ | **是**（`AddMEObject` + GUID/`FindObject`） |
| 3 | Broadcast 重入策略？ | **调用列表快照** |
| 4 | 第一个业务挂钩？ | **测试竖切**；PHYS-F03 另开 |
| 5 | 是否本期迁移 `AssetManager::Subscribe`？ | **否** |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-04 | 初稿：拆分 Native vs Dynamic；API/寿命/重入；切片见 Implementation；取代旧占位的实施约束 |
| 2026-08-04 | 拍板确认 → In Progress；S01/S02 实现 + `test delegates` PASSED → Done |
