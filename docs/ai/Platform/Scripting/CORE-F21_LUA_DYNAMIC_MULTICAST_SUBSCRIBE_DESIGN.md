# CORE-F21 — Lua 订阅 Dynamic Multicast（`Add(fn)` → `AddScript`）— Design Spec

## Meta
- **ID:** `CORE-F21`
- **Type:** Feature
- **Status:** **Done**
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/lua-script`
- **Related:**
  - [CORE-F20](../Core/CORE-F20_DYNAMIC_MULTICAST_DELEGATES_DESIGN.md) §3.5–3.6（Core 挂钩与分轨契约）
  - [CORE-F19](./CORE-F19_LUA_DELEGATE_AND_INVOKE_DESIGN.md)（Call Done；委托消费迁本 Feature）
  - [CORE-F02](./LUA_SCRIPT_BINDING_DESIGN.md)（ScriptBinding codegen）
  - [CORE-F01](./LUA_SCRIPTING_DESIGN.md) · [UI-F03](../UI/UI-F03_SCREENUI_BUTTON_DESIGN.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md) · [TD-006](../../TECH_DEBT.md)
- **Depends on:** `CORE-F20` Done；`CORE-F02` Done；`CORE-F19` Done
- **Blocks:** Lua 可见并订阅 `ScriptAssignable` Dynamic 多播（如 Button `OnClicked`）

## TL;DR

F20 已提供 Dynamic 多播 + `AddScript` / `CallableScriptFunction` + 反射属性；Lua 仍无法 `btn.OnClicked:Add(fn)`。  
**方案：** 扩展现有 ScriptBinding codegen——识别 **`ME_PROPERTY(ScriptAssignable)`** 且字段类型为 Dynamic 多播——生成字段露出；**一份** `DynamicMulticastDelegateBase` usertype 提供 `Add` / `Remove`；Scripting 层把 `sol::protected_function` 装进 `CallableScriptFunction`。  
**不新增 `ME_DELEGATE` 宏**（反射基础设施已够）。竖切验收：Button 0 参 click。

## Scope

### In
- header tool：`ScriptAssignable` + Dynamic 多播字段 → ScriptBinding 生成 owner 上的只读委托属性
- 手写/共享一次：`DynamicMulticastDelegateBase`（或等价薄包装）sol usertype：`Add(fn)` / `Remove(handle)` / 可选 `Clear` / `IsBound`
- Scripting（**非 Core**）：`sol` → `CallableScriptFunction`（`FromSol` 或同名工厂）
- `ButtonComponent` 标 `ME_CLASS(ScriptType)`（若尚无），使 codegen 覆盖该类
- Headless 测试：Lua `Add(fn)` → C++ `Broadcast` → 计数；Unload / Clear 不 UAF
- Lua 属性名规则（默认见 §3.3）；私有字段用 `meta=(ScriptGetter=…)`（**勿**用反射 `Getter`，Dynamic 不可拷贝）

### Out
- 新宏 `ME_DELEGATE` / 改写 `DECLARE_DYNAMIC_*`
- 域组件 `BindOnClicked(sol::function)` 或任何 UI→sol 依赖（已否决）
- Lua 主路径 `AddDynamic(self, "FuncName")`（可选后置）
- Inspector 接线、绑定列表落盘
- Native（非 Dynamic）多播进 Lua
- 本 Feature 内强制做齐 1–2 参 **参数 unpack**（usertype/`Add` 可共用；Invoke 多参另切片，见 §7）

## Reader quick start

1. §0 Pre-flight · §3 方案（codegen + Base usertype）
2. **§9 重构目标形态**（显式 `Add(self, fn)`；删 Active；Core 命名）— 维护者审阅重点
3. [CORE-F20](../Core/CORE-F20_DYNAMIC_MULTICAST_DELEGATES_DESIGN.md) §3.5–3.6
4. 代码入口：`scripts/minEngine_header_tool.py` · `Generated/ScriptBinding/` · `Runtime/Function/Scripting/` · `ButtonComponent.h`

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 前置 | F20 `AddScript` / `AddScriptErased` / `MEDynamicMulticastDelegateProperty` / Button `m_OnClicked` **齐全**；ScriptBinding 已识别 `ScriptAssignable` 字符串，但 **未**生成 lua_bind 成员 |
| 债风险 | **Medium** — 闭包寿命；header tool 回归面 |
| WIP | 本树 Primary = F21；勿并入 ED/ANIM |
| 哲学 | 机制在 Core；Lua 只消费生成绑定 — **合** Capabilities / Mechanism |
| 建议 | **Go** — codegen 扩展（非手写域 Bind）；MVP 竖切 Button 0 参 |

---

## 1) 背景与目标

**Pain：** 脚本无法订阅引擎 Dynamic 事件；F19 曾竖切 Button+sol 已撤回。  
**成功：**

```lua
local btn = go:FindComponentByClassName("ButtonComponent") -- 或后续更强 API
btn.OnClicked:Add(function()
  me.log("clicked")
end)
-- Broadcast / NotifyClicked 后回调执行
```

作者侧仍只写：

```cpp
ME_PROPERTY(ScriptAssignable)
DOnButtonClicked m_OnClicked;
```

无需手写 `*.lua_bind` 委托字段。

---

## 2) 现状（代码真源）

| 能力 | 状态 |
|------|------|
| `DynamicMulticastDelegate` + `AddScript` / `AddScriptErased` | **有**（F20） |
| `CallableScriptFunction`（Core，无 sol） | **有** |
| `ME_PROPERTY(ScriptAssignable)` + `MEDynamicMulticastDelegateProperty` | **有** |
| Button `m_OnClicked` Dynamic | **有** |
| ScriptBinding：`ScriptType` / `ScriptCallable` / `ScriptRead*` | **有** |
| ScriptBinding：`ScriptAssignable` 字段 | **无**（生成器忽略） |
| `ButtonComponent` `ScriptType` | **无**（无 `ButtonComponent.lua_bind`） |
| Lua↔Delegate | **无**（反模式已拆） |

---

## 3) 方案

### 3.1 数据流 / 模块边界

```text
Headers: ME_PROPERTY(ScriptAssignable) DynamicMulticast…
  → header tool (ScriptBinding pass)
  → owner *.lua_bind.gen.cpp 增加委托属性 getter
  → RegisterGeneratedLuaBindings

Runtime/Function/Scripting (sol 允许):
  MakeCallableScriptFunction(sol::protected_function)
  RegisterDynamicMulticastDelegateLuaUsertype(state)  // 一次

Lua: btn.OnClicked:Add(fn)
  → Base::AddScriptErased(callable)
  → DynamicMulticastDelegate::AddScript
  → Broadcast → Invoke → sol 调用
```

| 层 | 职责 | 禁止 |
|----|------|------|
| Core / Delegates | `AddScript`、寿命在槽内 | `#include <sol/...>` |
| Scripting | FromSol、usertype、Unload 解绑策略 | 改 Button 业务语义 |
| header tool | 认出 ScriptAssignable Dynamic 字段并生成 getter | 生成域 `Bind(fn)` |
| UI / Button | 继续 `Broadcast`；仅补 `ScriptType` | sol |

### 3.2 为何一份 Base usertype 足够

所有 `DECLARE_DYNAMIC_MULTICAST_DELEGATE*` 别名最终是 `DynamicMulticastDelegate<…>`，经 `DynamicMulticastDelegateBase` 暴露：

- `AddScriptErased(CallableScriptFunction)`
- `Remove(DelegateHandle)` / `Clear` / `IsBound` / `GetArity`

Lua **不**需要为每个 `DOnButtonClicked` 再注册一个 usertype。  
codegen 只负责：在 owner ScriptType 上把字段地址转成 `DynamicMulticastDelegateBase*`（或引用）交给该共享 usertype。

多参 Broadcast 时，槽内仍是 **typed** `AddScript` lambda，`Invoke(TArgs…)` 正确；Lua `Add` 路径与 arity 无关。  
**参数从 `void* args` 推入 Lua** 仅 0 参在 MVP 保证；1–2 参 unpack 见 S04 / 后续。

### 3.3 Lua 属性命名

字段真名多为 `m_OnClicked`；F20 契约示意为 `OnClicked`。

| 选项 | 说明 | 结论 |
|------|------|------|
| A. Lua 键 = 字段名 `m_OnClicked` | 与现有 ScriptRead* 一致 | 可用，体验差 |
| B. 去掉前导 `m_`（仅 ScriptAssignable Dynamic） | `m_OnClicked` → `OnClicked` | **默认** |
| C. `meta=(ScriptName="…")` | 最灵活 | 后置；需要再开工具字段 |

**默认 B。** 无 `m_` 前缀则原样。

### 3.4 API 契约

**C++（Scripting）：**

```cpp
// 示意；名以实现为准
CallableScriptFunction MakeCallableScriptFunction(sol::protected_function fn);
void RegisterDynamicMulticastDelegateLuaUsertype(sol::state& state);
```

**Lua usertype（`DynamicMulticastDelegate` 或短名 `MulticastDelegate`——实现时定一名，文档同步）：**

| 方法 | 行为 |
|------|------|
| `Add(fn)` | `fn` 须为 function；→ `AddScriptErased`；返回 `DelegateHandle`（需 sol 绑定或 userdata） |
| `Remove(handle)` | 转发 `Remove` |
| `Clear()` | 可选 MVP |
| `IsBound()` / `GetBindingCount()` | 可选调试 |

**不变量：**

- Core 不依赖 sol。
- `CallableScriptFunction` 所有权在绑定槽；`DestroyFn` 释放 sol 侧 user 数据。
- `LuaComponent` Unload / state 销毁前：须使挂在该 env 上的脚本绑定失效（Remove 或 Clear 所属委托——策略见 §3.5）。
- 禁止在 `ButtonComponent` 等域类型上新增吃 `sol::function` 的 API。

### 3.5 寿命与 Unload

| 场景 | 要求 |
|------|------|
| `Remove` / `Clear` / 委托析构 | `CallableScriptFunction` 析构 → `DestroyFn` |
| `LuaComponent::UnloadScript` | MVP：**记录本组件 `Add` 得到的 handle 列表并 Remove**；或文档约定「Unload 前脚本自 Remove」。**推荐前者**（组件持有 `vector<pair<Base*, Handle>>` 或集中 registry） |
| 对象销毁而 Lua 仍持有 Base* | 与 F02 一致：悬空风险已知；不强做弱引用（另 Feat） |

开放点 O2：Unload 自动解绑的存储位置（LuaComponent vs Scripting 全局表）。

### 3.6 header tool 行为

对每个 `ScriptType` 类的每个 property：

1. 若含 `ScriptAssignable`，且类型判定为 Dynamic 多播（与反射同一规则：`IsDynamicMulticastDelegateField` / 解析后类型名匹配 `DynamicMulticastDelegate` 或已知别名表），则：
   - 生成只读 Lua 属性（键按 §3.3），getter 返回 `&owner->field` 转为 `DynamicMulticastDelegateBase*`（或 sol 包装）。
2. 不生成 setter（赋值替换整个委托不在范围）。
3. 确保全局注册处调用一次 `RegisterDynamicMulticastDelegateLuaUsertype`（手写模块，由 `RegisterGeneratedLuaBindings` 或 `LuaScriptSystem` 在 Manual/Generated 之间调用）。

类型判定 MVP 可：解析到的 `type_name` 含 `DynamicMulticastDelegate` **或** 等于已扫描的 `DECLARE_DYNAMIC_*` 别名（若工具暂不扫 using，可对 Button 竖切硬编码别名表并逐步泛化——优先通用匹配）。

### 3.7 Button 竖切

1. `ME_CLASS(ScriptType)`（保留既有 Abstract 等与 Component 继承关系；生成 `base_classes` → Component）。
2. 现有 `ME_PROPERTY(ScriptAssignable) m_OnClicked` 无需改语义。
3. 测试：构造 Button → Lua Add → `NotifyClicked` / `OnClicked().Broadcast()` → 断言。

### 3.8 与 F19 / F20 分轨（不变）

| 能力 | Feature |
|------|---------|
| Call / TryGetFunction | F19 Done |
| Dynamic + AddScript 挂钩 | F20 Done |
| Lua `Add(fn)` + ScriptAssignable codegen | **F21（本）** |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **ScriptAssignable codegen + Base usertype** | 复用 F02/F20；无新宏；一份 Add API | 工具改动 | **选用** |
| 新宏 `ME_DELEGATE` | 声明点显式 | 与 ScriptAssignable 重复 | **拒绝**（口误已澄清） |
| 仅手写 Button usertype | 最快 | 反模式回潮；不可扩展 | **拒绝** |
| 每别名一个 usertype | 名字好看 | 无收益（同模板实例） | 拒绝 |
| 反射通用 `GetProperty` 入口 | Agent 友好 | 本 Feature 过大 | 后置 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Unload 后仍 Broadcast | UAF / 崩 | §3.5 自动 Remove；测 Unload |
| header tool 误判类型 | 错绑 / 漏绑 | 单测生成片段 + Button 竖切 |
| `DelegateHandle` 进 Lua | 绑定琐碎 | userdata 或 light userdata；不够则 MVP 只测 Add+Clear |
| Button 无 ScriptType 时漏生成 | 竖切失败 | S02 显式加 ScriptType |
| 1–2 参 Lua 收参未做 | 多参事件暂不能用脚本逻辑 | Scope 写明；S04 可选 |

---

## 6) 验收标准

- [x] `ScriptAssignable` Dynamic 字段出现在对应 `*.lua_bind.gen.cpp`（键名符合 §3.3）
- [x] Lua：`Add(fn)` 后 C++ Broadcast 触发 fn（Button 0 参）
- [x] `Remove` 或 `Clear` 后不再触发；Unload 路径无 UAF（有测）
- [x] Core 无 sol include；无 Button/域 `Bind*(sol::function)`
- [x] `minEngineTests` 相关 suite PASS（名：扩展 `lua-script-mvp` 或新 `lua-dynamic-delegates`）
- [x] Registry / ACTIVE_WORK / Progress 更新（本轮文档同步中）

---

## 7) 切片

| Slice | 内容 | Verify | 状态 |
|-------|------|--------|------|
| **S01** | Scripting：`MakeCallableScriptFunction` + Base usertype `Add`/`Remove`（可先手写注册） | 单元/夹具 mock Broadcast | **Done** |
| **S02** | header tool 生成 ScriptAssignable 字段；Button `ScriptType`；Register 钩子 | 生成物 diff + 编译 | **Done** |
| **S03** | Headless：Lua Add → NotifyClicked；Unload 解绑 | `test lua-script-mvp` PASS | **Done** |
| **S04** | （可选）1 参 Invoke→Lua unpack | 有参 Dynamic 测 | Deferred / 可选 |
| **S05** | Unload 归属：显式 `Add(host, fn)`；删 Active；Core 虚 `AddScript` override | `lua-script-mvp` PASS；无 Active API | **Done** |

Implementation Plan 可按需另文；S01–S03 Done；**S05 待按 §9 开做**。

---

## 8) Status note

**Done**（2026-09-15）。S01–S05 已落地；后续应用中再发现的问题另开 BUG / 小切片。

---

## 9) 重构目标形态（Unload 归属 — 去掉临时 ctx）

> **S05 之后**的终态。S01–S03 的 codegen / Broadcast 语义保留；改 Unload 归属 + Core 虚函数命名。

### 9.1 问题一句话

Lua `OnClicked:Add(...)` 必须知道**哪个 `LuaComponent` 订的**，Unload 才能 `Remove`。  
不用全局 Active，也不抽 `ScriptComponentBase`：`AddScriptFromLua` 已在 Lua 边界，**显式传 `self`**，类比 `AddMEObject(obj, …)`。

### 9.2 分层（终态）

```text
┌──────────────────────────────────────────────────────────┐
│ Core / Delegates                                          │
│   DynamicMulticastDelegateBase::AddScript(callable) 虚    │
│   DynamicMulticastDelegate<…>::AddScript override         │
│   —— 无 Lua / 无 LuaComponent ——                          │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │ AddScript(callable)
┌──────────────────────────────────────────────────────────┐
│ Scripting                                                 │
│   AddScriptFromLua(delegate, LuaComponent* host, fn)      │
│     // host 在前、fn 在后（对齐 AddMEObject 对象优先）      │
│     1) host != null → host->TrackScriptDelegateBinding    │
│     2) delegate.AddScript(MakeCallable(fn))               │
│   Lua:  delegate:Add(self, fn)                            │
└──────────────────────────────────────────────────────────┘
```

| 层 | 做什么 | 不做什么 |
|----|--------|----------|
| Core | 只收 `CallableScriptFunction`；基类虚函数对外名 **`AddScript`** | 不认识 `LuaComponent` |
| `LuaComponent` | 保留 Track / Clear 表；Unload 时 Clear | 不继承 ScriptComponentBase |
| `AddScriptFromLua` | `(delegate, host*, fn)`；Track + Core `AddScript` | 不读 ActiveScriptComponent |

**不引入 `ScriptComponentBase`：** 桥已是 Lua 专用；第二种脚本宿主出现再抽象。

### 9.3 Core 命名（`AddScript` / 原 `AddScriptErased`）

**现状问题：** 模板上叫 `AddScript`，基类虚函数叫 `AddScriptErased`——「Erased」易被理解成 callable 擦除，实际擦的是**委托模板/arity**（经基类多态）。

**维护者倾向：** 基类对外 = `AddScript`；模板里现在的实现体 = `AddScript_Internal`。

**推荐默认（比 `_Internal` 更干净）：合并为一个虚函数**

```cpp
// Base
virtual DelegateHandle AddScript(CallableScriptFunction callable) = 0;

// DynamicMulticastDelegate<TArgs...>
DelegateHandle AddScript(CallableScriptFunction callable) override
{
    // 原 AddScript 函数体（AddLambda + Invoke(TArgs...)）
}
```

| 方案 | 说明 | 结论 |
|------|------|------|
| **A. 合并虚 override** | 删 `AddScriptErased`；类型上与基类同名 `AddScript`；C++ 有具体类型时也调 `AddScript` | **推荐默认** |
| B. 基类 `AddScript` + 模板 `AddScript_Internal` | 表达「对外 / 对内」 | 可用；但测试/C++ 直调要写 `_Internal`，或再包一层转发，易糊 |
| 保持 `AddScript` + `AddScriptErased` | 少改 | **拒绝**（名不达意） |

S05 按 **A** 落地（已确认）。

### 9.4 Lua / 桥参数顺序（对象优先）

对齐 `AddMEObject(userObject, method)`：**宿主在前，可调用物在后**。

```cpp
// Scripting（示意）
DelegateHandle AddScriptFromLua(
    DynamicMulticastDelegateBase& delegate,  // sol 成员调用时的 receiver
    LuaComponent* host,                      // 显式 self — 对象优先
    sol::protected_function function);
```

```lua
-- colon：第一个隐式参数是委托；随后 host、fn
btn.OnClicked:Add(self, function()
  me.log("clicked")
end)
```

| 情况 | 行为 |
|------|------|
| `host != nullptr` | Track → Unload 自动 Remove |
| `host == nullptr`（测试可传 nil） | 不 Track；可手动 Remove/Clear |
| 类型不是 `LuaComponent` | 拒绑 + 打日志 |

### 9.5 删除列表（S05）

| 删除 | 说明 |
|------|------|
| `LuaScriptSystem::ActiveScriptComponent` 及 Get/Set | 临时 ctx |
| Load/Tick/Call 上对 Active 的 set/clear | |
| `AddScriptErased` 这个名字 | 改为基类虚 `AddScript`（方案 A） |
| `ScriptComponentBase` | **不引入** |

### 9.6 数据流对比

**现在（过渡）：** Active ctx → Track → `AddScriptErased`

**目标：**

```text
btn.OnClicked:Add(self, fn)
  → AddScriptFromLua(delegate, self, fn)
  → self->TrackScriptDelegateBinding(...)
  → delegate.AddScript(callable)    // 基类虚 = 原 Erased 入口
UnloadScript → ClearScriptDelegateBindings()
```

### 9.7 明确拒绝

| 做法 | 原因 |
|------|------|
| Core `AddScript` 增加 `LuaComponent*` | Core 绑死 Lua |
| 全局 / TLS Active ctx 当终态 | 已否决 |
| MVP 引入 `ScriptComponentBase` | 桥已是 Lua 边界；过早 |
| Lua `Add(fn)` 隐式抠 env 当默认 | 不如显式 `Add(self, fn)` 清晰 |

### 9.8 S05 验收

- [x] 无 `ActiveScriptComponent`
- [x] Lua：`Add(self, fn)`（host 在前）；Unload 解绑测仍绿
- [x] 跟踪仍在 `LuaComponent`（无 ScriptComponentBase）
- [x] Core：基类虚函数名 `AddScript`；无 `AddScriptErased` 对外名（方案 A）
- [x] Core `AddScript` **仍不**接收 `LuaComponent*`

---

## 10) 开放点（默认已选）

| # | 议题 | 默认 |
|---|------|------|
| **O1** | Lua 属性名 | 去前导 `m_`（§3.3 B） |
| **O2** | Unload 解绑 | **`LuaComponent` Track 表**；显式 host；删 Active |
| **O2b** | host 如何得到 | **`Add(self, fn)` 显式**（self 在前） |
| **O3** | usertype 对外名 | `DynamicMulticastDelegate` |
| **O4** | `DelegateHandle` Lua 形态 | userdata（`IsValid`） |
| **O5** | 多参 Lua 收参 | **不进 MVP**（S04） |
| **O6** | Lua `AddDynamic` | **不做** |
| **O7** | `ScriptComponentBase` | **不引入**（MVP） |
| **O8** | Core `AddScript` vs Erased 命名 | **方案 A：合并虚 `AddScript`**；B=`_Internal` 仅后备 |

---

## 11) 审阅清单

- [x] 同意不新增 `ME_DELEGATE`，只扩展 ScriptAssignable codegen
- [x] 同意共享 `DynamicMulticastDelegateBase` usertype
- [x] S01–S03 已实现
- [x] §9：显式 `Add(self, fn)`、无 ScriptComponentBase、删 Active
- [x] §9.3 命名：**A（合并虚 AddScript）** 已确认并落地
- [x] S05 开工 / Done

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | Draft：ScriptAssignable codegen + Base usertype + FromSol；Button 0 参竖切；拒绝新宏与域 Bind |
| 2026-09-15 | In Progress：S01–S03 落地；`ScriptGetter`；`lua-script-mvp` PASS |
| 2026-09-15 | 增补 §9：初稿 ScriptComponentBase + 隐式 env |
| 2026-09-15 | **§9 修订：** 无 ScriptComponentBase；显式 `Add(self, fn)` 对象优先；Core 命名 A=合并虚 `AddScript`（荐）/ B=`_Internal` |
| 2026-09-15 | **S05 Done：** 虚 `AddScript` override；删 Active；`Add(self, fn)` |
| 2026-09-15 | **Done：** 验收收口 |
