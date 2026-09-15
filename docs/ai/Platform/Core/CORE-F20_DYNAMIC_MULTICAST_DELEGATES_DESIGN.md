# CORE-F20 — Dynamic Multicast Delegates — Design Spec

## Meta
- **ID:** `CORE-F20`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/core`
- **Related:**
  - [CORE-F04 Native Multicast](./CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)（Done — 本 Feature **包装**，不改写 Native 契约）
  - [CORE-F01](../Scripting/LUA_SCRIPTING_DESIGN.md) / [CORE-F02](../Scripting/LUA_SCRIPT_BINDING_DESIGN.md)（Script\* 白名单）
  - [CORE-F19](../Scripting/) Lua Call-by-name（`feat/lua-script`；C++→Lua；与本 Feature 正交）
  - **`CORE-F21`** Lua `Add(fn)` 桥（Draft：[Design](../Scripting/CORE-F21_LUA_DYNAMIC_MULTICAST_SUBSCRIBE_DESIGN.md)；挂钩见 §3.5–3.6）
  - [UI-F03 Button](../UI/UI-F03_SCREENUI_BUTTON_DESIGN.md)（`OnClicked` 今日为 Native；迁移可选竖切）
  - [TECH_DEBT TD-006](../../TECH_DEBT.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 哲学：[ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md) — Mechanism over Policy；Agent-friendly via shared APIs
- **Depends on:** `CORE-F04` Done；反射 `MEFunction` Invoke MVP（已有）；`MEObject` / `FindObject`
- **Blocks:** `CORE-F21` Lua 订阅；未来 Editor 接线 / 绑定序列化；Button 等事件的可发现订阅

## TL;DR

补齐与 UE **Dynamic Multicast** 同构的一层：**可进入类型系统、可按名绑定 `MEFunction`、可为 Script/Editor 发现** 的多播委托。

- **存储 B1（已拍）：** `DynamicMulticastDelegate<Args...>` **包装** 已有 `MulticastDelegate<Args...>`；Broadcast **只走 Native 一张表**。
- **本 Feature（`feat/core`）：** 类型 + 宏 + `AddDynamic` + **独立反射属性类型** + `CallableScriptFunction` / `AddScript` 挂钩 + Serializer **显式跳过** + 单测；可选 Button `OnClicked` 迁移。
- **不在本期：** Lua usertype、`btn.OnClicked:Add(fn)`、sol、Inspector 绑脚线、**绑定列表落盘** → **`CORE-F21` / 后续序列化切片**。
- **命名：** 运行时类型 **`CallableScriptFunction`**（与 `ME_FUNCTION(ScriptCallable)` specifier **刻意区分**）。

## Scope

### In（CORE-F20 / `feat/core`）

- `DynamicMulticastDelegate<TArgs...>`（B1：内持 `MulticastDelegate<TArgs...>`）
- 声明宏：`DECLARE_DYNAMIC_MULTICAST_DELEGATE*`（0 / 1 / 2 参，与 Native 宏对称）
- API：`AddDynamic` / `RemoveDynamic`（或统一 `Remove(handle)`）/ `Broadcast` / 转发常用 Native `Add*`（见 §3.2）
- **`CallableScriptFunction` + `AddScript`**：Core 侧类型擦除挂钩（**无** sol；**勿**与 `ScriptCallable` specifier 混淆）
- 反射：**`MEDynamicMulticastDelegateProperty`**（独立类型，类比 UE `FMulticastDelegateProperty`）+ Category 枚举值
- Serializer：对该 Category **显式跳过**（§3.4.1）；不落绑定表
- Specifier（名称待拍，见 §10）：属性可标「脚本/反射可赋值事件」（类比 UE `BlueprintAssignable`）
- 单测：`test dynamic-delegates`（或并入 `delegates` suite）
- 文档：付清「Native vs Dynamic」；更新 TD-006 指向 F20/F21
- **可选竖切：** `ButtonComponent::OnClicked` → Dynamic 类型（仍无 Lua）

### Out / 另轨

| 项 | 归属 |
|----|------|
| Lua `Add(fn)` / usertype / ScriptBinding codegen | **`CORE-F21`**（`feat/lua-script`） |
| `LuaComponent::Call` 完善 | **`CORE-F19`**（**Done**） |
| 域组件 `BindOnClicked(sol::function)` | **禁止**（F19 已否决） |
| 绑定图序列化进 `.mescene`（AddDynamic 列表） | **后置**（§3.4.1）；F20 只预留属性类型 + Serializer 跳过 |
| Inspector 可视化绑函数名 / Undo | 后续 Editor Feature |
| 把全部 Native 事件升 Dynamic | **否** — 仅「需要被 Script/Editor 发现」的事件 |
| 跨线程 Broadcast | 与 F04 相同：Out |
| 完整 UE `ProcessEvent` parms 缓冲复刻 | MVP 用现有 Invoke；复杂签名后置 |

## Reader quick start

1. §0 Pre-flight · §3.0–3.2 存储与 API · §3.5 Script 挂钩 · §3.6 与 F19/F21 分轨  
2. [CORE-F04](./CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) §3.0  
3. 代码现状：`Runtime/Core/Delegates/` · `ButtonComponent.h`

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 依赖 | Native F04 **sound**；`MEFunction` Invoke **partial**（够 0 参竖切；多参需核对） |
| 债风险 | **Medium** — 反射属性新类别会碰 header-tool；范围过大易拖进 Lua |
| WIP | `ED-F11` / `CORE-F19` 并行；本 Feature 钉 `feat/core`，**不**改 Lua 树 |
| 哲学 | 机制（可发现多播）进 Core；Lua 消费方式留 Script 轨 — **合** |
| 建议 | **Go with scope cut** — F20 = Dynamic + 反射 + `AddScript` 挂钩；Lua `Add(fn)` → F21 |

---

## 1) 背景与目标

### 1.1 Pain

- `MulticastDelegate`（F04）只能 C++ 订阅；**不在** `MEClass` 属性表 → Script/Editor/Agent **发现不了**。
- 为接 Lua 若在 `ButtonComponent` 上写 `BindOnClicked(sol::function)`，会把 **UI 域** 绑死脚本运行时（已否决）。
- 正确 UE 同构路径：需要 **Dynamic** 层（类型系统一等公民）+ Script 轨消费同一 `Add*` 面。

### 1.2 Goals

1. Dynamic 多播与 Native **分工清晰**（见 §3.0），且 Broadcast 语义与 F04 一致（同步、快照重入）。
2. C++ 能 `AddDynamic(meObject, "OnFoo")`，目标为 **具名 `MEFunction`**。
3. 反射能枚举「某类型上的 Dynamic 多播字段」及签名元数据（至少名字 + 参数个数/类型描述）。
4. Core 提供 **`AddScript(CallableScriptFunction)`**，供 F21 接入 Lua 闭包，**Core 零 sol 依赖**。
5. 现有纯 C++ 热路径（物理 Contact 等）可继续只用 Native，**不被强迫升级**。

### 1.3 Success

- `test dynamic-delegates`（名以实现为准）绿：AddDynamic Broadcast、目标销毁跳过、与 Native 转发共存、AddScript 可被 mock callable 触发。
- 至少一个反射可读的 Dynamic 字段（测试夹具或 Button）。
- `feat/core` 无新增 `#include <sol/...>` / ScriptBinding 对 Dynamic 的硬编码 usertype。
- F21 可仅凭本 Design §3.5 挂钩写 Lua Design，而不再改 Dynamic 核心语义。

---

## 2) 现状（代码真源）

| 项 | 状态 |
|----|------|
| `MulticastDelegate` / `DECLARE_MULTICAST_DELEGATE*` | Done（0–2 参） |
| `AddRaw` / `AddMEObject` / `AddLambda` / 快照 Broadcast | Done |
| `ButtonComponent::m_OnClicked` | **Dynamic** `DOnButtonClicked` + `ScriptAssignable` |
| `MEPropertyCategory` | Primitive / Object / ObjectPtr / Array — **无** Delegate |
| Script\* codegen | 白名单 usertype；**无**委托字段 |
| Lua | Tick + 少量 API；Call 在 `feat/lua-script`；**无**事件订阅 |

---

## 3) 方案

### 3.0 Native vs Dynamic（定稿心智）

| | **Native**（F04） | **Dynamic**（F20） |
|--|-------------------|---------------------|
| 类型 | `MulticastDelegate<Args...>` | `DynamicMulticastDelegate<Args...>` |
| 绑定 | 成员指针 / lambda / MEObject 弱绑 | + **`AddDynamic(obj, "Func")`** + **`AddScript`** |
| 反射 | 默认不可见 | **字段可进 `MEClass`** |
| 用途 | 引擎内部、热路径、任意 C++ 签名 | Script / Editor / Agent 可发现事件 |
| 依赖 | 无反射 | 反射 +（可选）CallableScriptFunction |

**原则（延续 F04）：** Dynamic **不得**倒逼 Native API 变成字符串绑定；Native 保持模板类型安全。

### 3.1 存储模型 B1（已拍）

```text
DynamicMulticastDelegate<Args...>
  └─ MulticastDelegate<Args...>  m_Native   // 唯一 listener 表
        ├─ Native 槽（AddRaw / AddMEObject / AddLambda）
        ├─ Dynamic 槽（AddDynamic → 包装为调用 MEFunction 的 lambda/MEObject 绑）
        └─ Script 槽（AddScript → 包装为调用 CallableScriptFunction 的 lambda）
              │
              ▼ Broadcast(args...)
         仅 m_Native.Broadcast — 一张表、一次快照
```

- **一个** Broadcast 入口（Dynamic 对外 `Broadcast` → 转 `m_Native`）。
- 避免「Native 表 + Dynamic 表」双列表漂移。
- C++ 仍可通过转发 API 或 `GetNative()` 使用 F04 能力（§10 拍板是否公开 `GetNative`）。

### 3.2 模块边界与 API 契约

**落点（建议）：**

```text
Runtime/Core/Delegates/
  MulticastDelegate.h          // 不变（F04）
  DynamicMulticastDelegate.h   // 新增模板
  DelegateMacros.h             // 增 DYNAMIC 宏
  CallableScriptFunction.h             // 类型擦除挂钩（无 Lua）

Runtime/Core/Reflection/
  MEProperties.h               // 新 Category 或 MEDynamicDelegateProperty
  （header-tool 增量）
```

#### 声明（示意）

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(DOnButtonClicked);
// → using DOnButtonClicked = DynamicMulticastDelegate<>;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(DOnValueChanged, float /* Value */);
```

#### 成员用法（C++）

```cpp
DOnButtonClicked OnClicked;

// Native 能力（转发到内部 m_Native）
OnClicked.AddMEObject(listener, &Listener::Handle);
OnClicked.AddLambda([]{ /* ... */ });

// Dynamic：具名 MEFunction（须 UFunction 式已注册；见 §3.3）
OnClicked.AddDynamic(this, "HandleClick"); // 或 AddDynamic(obj, &T::HandleClick) 宏糖

// Script 挂钩（F21 填入实现；F20 用 mock 测）
OnClicked.AddScript(callable);

OnClicked.Broadcast();
OnClicked.Remove(handle);
```

#### 不变量

1. Broadcast / 重入 / 线程：与 F04 **相同**（同步、快照、非线程安全）。
2. Dynamic 实例 **不可拷贝**（与 Native 一致）。
3. `AddDynamic`：找不到函数 / 签名不匹配 → 返回 Invalid + Error 日志；**不** abort。
4. 目标 `MEObject` 销毁后：Dynamic 槽与 `AddMEObject` 同策略（跳过 / 惰性剔）。
5. **Core 翻译单元不包含 sol2。**

#### 参数个数 MVP

- 与 F04 对齐：**0 / 1 / 2** 宏。
- **第一刀实现建议：** 单测 + Button 竖切先 **0 参** 跑通 `AddDynamic` + Invoke；1–2 参在 Invoke/帧布局核对后立刻补（同一 Feature，可分 Slice）。

### 3.3 `AddDynamic` 与 `MEFunction`

```text
AddDynamic(obj, "FuncName")
  → obj->GetClass()->FindFunctionByName(...)
  → 校验：实例函数；参数个数/类型与委托签名兼容（MVP 规则见下）
  → m_Native.AddMEObject / 等价弱绑：
        Broadcast 时 InvokeFunction(obj, fn, args...)
```

**MVP 签名兼容（建议默认）：**

| 规则 | 说明 |
|------|------|
| 0 参委托 | 目标 `MEFunction` 0 参（或仅忽略多余？→ **否，严格匹配**） |
| 1–2 参 | 参数类型需与反射属性类型可赋值兼容；失败则拒绑 |
| 返回值 | 忽略（与多播 void 一致） |
| static | MVP **不支持**（与 UE AddDynamic 常见路径一致） |

**糖：** 可提供 `AddDynamic(obj, &T::Method)` 宏，静态断言 Method 属于 T，并取反射名字符串（若现有反射能从成员指针取名则用之；否则宏里写字面量重载）。开放点 §10。

### 3.4 反射露出（对齐 UE：独立 Property 类型）

目标：Agent / 未来 Editor / F21 codegen **能问**：「类型 X 有哪些可订阅事件？」

UE 对照（同构学习点，非逐 API 克隆）：

| UE | minEngine（本 Feature） |
|----|-------------------------|
| `FMulticastDelegateProperty` / Inline / Sparse 等 | **`MEDynamicMulticastDelegateProperty : MEProperty`**（**独立类型**，不只是 Category 枚举加一项） |
| `UPROPERTY(BlueprintAssignable)` | `ME_PROPERTY(..., ScriptAssignable)`（specifier 名见 §10 O2） |
| 属性持有签名 / 可 Script 绑定 | 属性持有 arity、参数 `MEProperty*` 描述、指向字段的 accessor |
| Native `TMulticastDelegate` **无**对应 UProperty | Native `MulticastDelegate` **继续不进反射** |

**为何独立类型（而非仅 `MEPropertyCategory::MulticastDelegate`）：**

- Serializer / Inspector / ScriptBinding 需要 **特化行为**（跳过或日后写绑定表、露出 `Add`、不走 Primitive codec）。
- 与 `MEArrayProperty` / `MEObjectProperty` 一样：Category 可有，但 **逻辑挂在子类** 更清晰。
- 推荐：`GetCategory() → MulticastDelegate`（新枚举值）+ 真正能力在 `MEDynamicMulticastDelegateProperty`。

**属性元数据至少含：**

- 字段名（如 `OnClicked`）
- 签名：arity + 参数类型描述（`MEProperty*` 列表或等价）
- Specifier：`ScriptAssignable` — 「允许脚本订阅」
- accessor → `DynamicMulticastDelegateBase*`（字段擦除）

**类型擦除基类（推荐）：**

```cpp
class DynamicMulticastDelegateBase {
public:
    virtual ~DynamicMulticastDelegateBase() = default;
    virtual int GetArity() const = 0;
    virtual DelegateHandle AddScript(CallableScriptFunction) = 0;
    // 序列化后置用：枚举/重建「可落盘」绑定（仅 AddDynamic 槽）
    // virtual void GatherPersistentBindings(...) const;
    // virtual void RestorePersistentBindings(...);
};

template<typename... TArgs>
class DynamicMulticastDelegate : public DynamicMulticastDelegateBase { /* ... */ };
```

0 参 MVP 可不实现通用 `BroadcastErased`；F21 对已知签名 usertype 直接调 typed `Add`/`Broadcast`。

**header-tool：**  
- 理想：`ME_PROPERTY(..., ScriptAssignable)` 且字段类型为 Dynamic 宏生成类型 → 生成 `MEDynamicMulticastDelegateProperty` 注册。  
- 务实切片：S01 手写测试夹具注册；S05 再接通 tool（§7）。

#### 3.4.1 序列化：现在做什么 / 以后怎么做

**UE 事实：** Dynamic 委托的绑定（`UObject*` + 函数名）**可以**随 `UPROPERTY` 序列化；Native / lambda / 非 UFunction 绑 **不能**指望落盘。BlueprintAssignable 事件在资产/关卡里能保存设计器接线，靠的就是这套。

**我们能落盘的 / 不能落盘的：**

| 槽类型 | 可序列化？ | 原因 |
|--------|------------|------|
| `AddDynamic(obj, "Func")` | **是（将来）** | 稳定身份 = 目标 GUID（或路径）+ 函数名字符串 |
| `AddMEObject` / `AddRaw` | **否** | 成员指针 / 无反射名契约 |
| `AddLambda` | **否** | 无稳定身份 |
| `AddScript(CallableScriptFunction)` | **否（默认）** | Lua 闭包 / 运行时句柄；应运行时重订 |

**本期（F20）建议 — 已写入推荐默认（§10 O8）：**

1. **做** `MEDynamicMulticastDelegateProperty`，并在 `Serializer::SerializeProperty` 增加 **显式分支**：读写下 **跳过**（或写空占位），**禁止**落入 default 误当 Primitive。  
2. **不做** 绑定列表 JSON/Binary 真写入；场景里 Button `OnClicked` 空订阅即可。  
3. **文档预留** 未来盘面形状（非本 Feature 实现），便于 Editor / Prefab 开刀：

```json
"OnClicked": {
  "$type": "DynamicMulticast",
  "bindings": [
    { "target": "<guid-or-object-path>", "function": "HandleClick" }
  ]
}
```

- 仅持久化 **AddDynamic** 槽；加载后 `Clear` 持久槽再 `AddDynamic` 重建。  
- Native / Script 槽保持运行时-only。  
- 缺字段 / 旧资产：视为无绑定（宽松，对齐 F10）。

**为何不现在做完整绑定序列化（挑战）：**

- 尚无 Editor 接线 UI，也无「场景里预先订好 OnClicked → 某 GO 方法」的消费方。  
- PIE / 存盘路径上空跳过已够用；半吊子写入易与 GUID 修复、克隆、Prefab 纠缠。  
- Lua `Add(fn)` 更不应进盘。

**何时开绑定序列化：** Editor 可接线 **或** Prefab/场景作者需要存 Dynamic 绑定时，另开 Slice / Feature（可挂 F20 后续或 `CORE-F22`）；以 §3.4.1 预留格式为真源。

### 3.5 `CallableScriptFunction` / `AddScript`（Core 挂钩，供 F21）

**命名：** 运行时类型叫 **`CallableScriptFunction`**，避免与反射/绑定 specifier **`ScriptCallable`**（`ME_FUNCTION(ScriptCallable)`，CORE-F02）撞名。

**问题：** F21 需要 `Add(fn)`，但 F20 不能依赖 sol。

**契约（Core）：**

```cpp
// Runtime/Core/Delegates/CallableScriptFunction.h（示意）
class CallableScriptFunction {
public:
    using InvokeFn = void (*)(void* user, /* packed args — MVP 见下 */);
    using DestroyFn = void (*)(void* user);

    CallableScriptFunction() = default;
    CallableScriptFunction(void* user, InvokeFn invoke, DestroyFn destroy);
    // move-only；析构调 destroy

    bool IsValid() const;
    void Invoke(/* args */) const; // 模板或 0/1/2 重载
};
```

- F21 把 `sol::protected_function`（及必要的 state/寿命）放进 `user`，提供 `InvokeFn`。
- F20 `AddScript`：`m_Native.AddLambda([callable](Args... a){ callable.Invoke(a...); })`，并在 Remove/Clear 时正确销毁。
- **寿命：** `CallableScriptFunction` 所有权在绑定槽内；Lua 侧须保证函数在解绑前有效（F21 详述：与 `LuaComponent` env 绑定或显式 handle）。

**本 Feature 验收：** 用 C++ mock `CallableScriptFunction`（计数器）证明 Broadcast 能调到，**不**写 Lua。
### 3.6 与 F19 / F21 分轨（已拍）

```text
feat/core     ── CORE-F20 ──► Dynamic + 反射 + AddScript 挂钩
feat/lua-script
  ├─ CORE-F19 ──► Call / TryGetFunction（C++→Lua 按名）
  └─ CORE-F21 ──► 发现 OnClicked → Lua Add(fn) → AddScript
```

| 能力 | Feature |
|------|---------|
| Native 多播 | F04 Done |
| Dynamic + 反射 + AddDynamic + AddScript 挂钩 | **F20** |
| C++ `LuaComponent::Call` | F19 |
| Lua `delegate:Add(fn)` usertype / codegen | **F21** |

**F21 预期 Lua 体验（契约预告，非本 Feature 实现）：**

```lua
-- 示意；F21 Design 定稿
local btn = ... -- ButtonComponent
btn.OnClicked:Add(function()
  me.log("clicked")
end)
```

- `OnClicked` 来自 ScriptBinding 对 **ScriptAssignable Dynamic 字段** 的露出（不是手写 Button::Bind）。
- `Add` → `AddScript(CallableScriptFunction::FromSol(fn))`。
- **不**把 `AddDynamic(self, "name")` 作为 Lua 主路径（已拍：主形态 `Add(fn)`）；具名路径若需要可作为 F21 可选。

### 3.7 可选竖切：`ButtonComponent`

| 选项 | 说明 | 建议 |
|------|------|------|
| A. F20 内迁移 `OnClicked` → Dynamic | 真源事件可反射；C++ 订阅 API 微调 | **默认推荐** |
| B. 仅测试夹具 | 风险更低；Button 仍「看不见」 | 若 header-tool 未就绪可作 S01 |
| C. 另开 UI Feature | 过碎 | 不推荐 |

迁移时：`NotifyClicked` 仍 `Broadcast()`；现有 C++ 测试改用 Dynamic 类型上的 `Add*`。**不加** Lua。

### 3.8 数据流（Click 示例）

```text
UISystem Click 边沿
  → ButtonComponent::NotifyClicked
  → Dynamic OnClicked.Broadcast()
  → m_Native 快照
       → C++ AddMEObject 监听者
       → AddDynamic → MEFunction::Invoke
       → AddScript → CallableScriptFunction::Invoke   // F21 填 Lua
```

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **B1 Dynamic 包装 Native** | 单表；Broadcast 简单；与讨论一致 | 多一层类型 | **选用** |
| B2 只扩展 Native + 反射描述 | 少类型 | 「Dynamic」语义糊；字符串绑易污染 F04 | 拒绝 |
| 双表（Native ∥ Dynamic） | 隔离狠 | 双 Broadcast / 漂移 | 拒绝 |
| 先做 Lua Bind 竖切 | 出活快 | 域耦合；已否决 | 拒绝 |
| Dynamic 完全走独立 ProcessEvent 总线 | 更像远古 UE | 重；重复造派发 | MVP 拒绝；槽内 Invoke 即可 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| header-tool 延期 | 反射竖切卡住 | S01 手写夹具；S02 再接通 |
| `AddDynamic` 多参 Invoke 不熟 | 绑定失败/坏内存 | 先 0 参；1–2 参单独 Slice + 测试 |
| CallableScriptFunction 寿命与 Lua env | UAF（F21） | F20 只定所有权在槽内；F21 绑 env/句柄 |
| 误在 Core 引入 sol | 分层崩溃 | Code review / include 门禁；验收标准写明 |
| 全量事件升 Dynamic | 噪音与开销 | 文档：按需升级；Contact 等保持 Native |
| 与 F19 ID/文档分叉 | 合并混乱 | Registry 已并列 F19/F20/F21 |

---

## 6) 验收标准

- [x] Design 拍板 → **In Progress** → **Done**
- [x] `DynamicMulticastDelegate` + DYNAMIC 宏；包装 F04
- [x] `AddDynamic` 0 参竖切 + 对象销毁安全（`test delegates`）
- [x] `MEDynamicMulticastDelegateProperty`；Serializer IterateProps **显式跳过**
- [x] `CallableScriptFunction` + `AddScript` mock 单测（无 Lua）
- [x] Button `OnClicked` → Dynamic + `ME_PROPERTY(ScriptAssignable)` 进反射
- [x] `CreatePropertyByType` 识别 Dynamic 字段（`IsDynamicMulticastDelegateField`；无 Reflection↔Delegates include 环）
- [x] Core 无 sol 依赖
- [x] 绑定列表不落盘（O8）
- [x] `test delegates` / `screen-ui-button` PASS（收口 2026-09-15）

---

## 7) 切片

| Slice | 内容 | 验证 | 状态 |
|-------|------|------|------|
| **S00** | Design 拍板 | §10 默认采纳 | **Done** |
| **S01** | Dynamic + 宏 + AddScript | `test delegates` | **Done** |
| **S02** | AddDynamic 0 参 | 同上 | **Done** |
| **S03** | MEDynamicMulticastDelegateProperty + Serializer skip | 属性单测 | **Done** |
| **S04** | Button OnClicked → Dynamic | `test screen-ui-button` | **Done** |
| **S05** | 反射生成 Dynamic 属性 | Button `m_OnClicked` 反射测 | **Done** |
| **S06** | 1–2 参宏 / AddScript 一参；AddDynamic 多参路径 | `test delegates` | **Done**（多参 AddDynamic 靠 InvokeFunctionTyped；无独立多参 Dynamic 测） |

**F21（另分支）：** ScriptBinding + `Add(fn)` → `AddScript`。

---

## 8) Status note

**Done**（2026-09-15）。后续：CORE-F21 Lua `Add(fn)`；绑定列表落盘待 Editor/Prefab。

---

## 10) 开放点（已拍默认）

| # | 议题 | 结论 |
|---|------|------|
| **O1** | GetNative / 转发 | **转发 + public GetNative** |
| **O2** | Specifier | **`ScriptAssignable`** |
| **O3** | AddDynamic 糖 | 字符串 + `ME_ADD_DYNAMIC` |
| **O4** | Button 迁移 | **已做** |
| **O5** | 手写夹具 | **允许**；S05 以 CreatePropertyByType + codegen 完成 |
| **O6** | CallableScriptFunction 目录 | **`Delegates/`** |
| **O7** | 1–2 参 | 宏 + 路径已有；AddScript 一参有测 |
| **O8** | 绑定序列化 | **跳过落盘** |

---

## 11) 审阅清单

（已确认并实现；Feature Done。）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 初稿：B1、Add(fn)→F21、feat/core 仅基建；登记 CORE-F20；并列 F19/F21 |
| 2026-09-14 | 修订：`CallableScriptFunction`；`MEDynamicMulticastDelegateProperty`；§3.4.1 / O8 |
| 2026-09-14 | **实现：** S01–S04；`test delegates` 11/11；`screen-ui-button` 4/4 |
| 2026-09-15 | **S05 收口：** `DynamicMulticastDelegateBase` + type trait；Button `ScriptAssignable`；反射测；Status → **Done** |
| 2026-09-15 | 命名约定：委托类型别名改用 **D** 前缀（`DOn*`），避免与 UE `F` 撞风格 |
