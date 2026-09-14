# CORE-F19 — Lua C++→Lua Call-by-name（Delegate 桥后置）— Design Spec

## Meta
- **ID:** `CORE-F19`
- **Type:** Feature
- **Status:** In Progress（**Call-by-name Done**；Lua↔Delegate **Deferred** — 待 `AddScript` + Delegate 反射/注册方案）
- **Owner:** project maintainer
- **Last updated:** 2026-09-14
- **Branch:** `feat/lua-script`
- **Related:**
  - [CORE-F01](./LUA_SCRIPTING_DESIGN.md) · [CORE-F02](./LUA_SCRIPT_BINDING_DESIGN.md) · [CORE-F04](../Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) §3.4
  - [UI-F03](../UI/UI-F03_SCREENUI_BUTTON_DESIGN.md)（Lua 订 OnClicked 仍后置）
  - [TECH_DEBT TD-006](../../TECH_DEBT.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F01` Done；`CORE-F02` Done
- **Blocks:** 稳定 C++→Lua 按名调用；**不**阻塞 native 委托

## TL;DR

本期落地：**`LuaComponent::Call` / `TryGetFunction`**（泛化硬编码 `tick`）。

**已撤回（反模式）：** 域组件 `BindOnClicked` / 旁路 `BindLua` 当主 API、Probe 手写 Bind。  
**后续方向（另议，可参考 UE）：** `MulticastDelegate::AddScript(ScriptCallable)` + `LuaCallable`；Lua **看见** C++ 类型上的 Delegate 再订 —— 需 Delegate 反射 + Lua 类型注册，**不在本切片强行竖切**。

## Scope

### In（当前保留）
- `LuaComponent::TryGetFunction` / `Call`
- Load 后缓存 `tick`（先 `m_Loaded=true` 再查找）
- Headless：`Call` 命名函数 / 缺失名

### Out / Deferred
- `BindLua` helper、Button/Probe 上的 sol `Bind*`
- `AddScript` / `ScriptCallable` / `LuaCallable`（设计中）
- Delegate 字段反射露出 + Lua usertype 注册
- Dynamic / 字符串方法名 / Inspector 绑脚本

## Reader quick start

1. §3.2 Call API  
2. §11 后续讨论提纲（AddScript + 反射）  
3. 代码：`LuaComponent.h/.cpp`

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| Call 路径 | **Go** — 已落地 |
| 委托桥 | **Defer** — 纠偏后再开；避免 UI↔Lua 耦合竖切 |

---

## 1) 背景

仅有 `tick` 不够；C++ 需要按名调脚本回调。  
委托侧曾用 `Button::BindOnClicked` 竖切，造成 **UI 组件依赖 sol**，违背「委托一等能力 + Lua 看见再订」。

---

## 2) 现状（代码）

| 能力 | 状态 |
|------|------|
| `Call` / `TryGetFunction` | **有** |
| Native `MulticastDelegate` | **有**（无 Script） |
| Lua↔Delegate | **无**（已拆除反模式） |

---

## 3) 方案（Call）

```cpp
sol::protected_function TryGetFunction(const char* name) const;
template<typename... TArgs>
bool Call(const char* name, TArgs&&... args);
```

- 仅已 Load 环境；失败打日志、不 abort。  
- **不以** `CallGlobal` 为主 API。

---

## 4) 备选（委托 — 仅记账）

| 选项 | 结论 |
|------|------|
| 域 Comp `BindXxx(sol::fn)` | **已拒绝**（耦合） |
| 旁路 `BindLua` 当主 API | **已拒绝**（非委托能力） |
| `AddScript(ScriptCallable)` + 反射露出 | **后续默认方向** |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 过早 codegen 委托反射 | 先讨论 / 对照 UE，再立切片 |
| Call 与 tick 双路径 | tick 仍缓存；查找语义与 Call 一致 |

---

## 6) 验收标准

- [x] `Call` / `TryGetFunction`；`tick` 仍工作  
- [x] 无 Button/Probe sol Bind；无 `LuaDelegateBind.h`  
- [x] `lua-script-mvp` 含 Call 用例 PASS  
- [ ]（后）`AddScript` + Delegate 可见性 — 另切片/Feature  

---

## 7) 切片

| Slice | 内容 | 状态 |
|-------|------|------|
| **S01** | Call / TryGetFunction | **Done** |
| **S02+** | AddScript + 反射/注册 | **Deferred** |

---

## 8) Status note

In Progress — Call Done；Delegate 桥等待方案讨论（可参考 UE）。

---

## 10) 开放点（委托后续）

| # | 议题 |
|---|------|
| D1 | `ScriptCallable` 放 Core 旁 vs 更薄擦除 |
| D2 | Lua 如何看见 `OnClicked`（手写 vs codegen / UE 对照） |
| D3 | `AddScript` arity 0/1/2 与宏对齐 |
| D4 | 是否新开 `CORE-F20` 专做 Script Events |

---

## 11) 后续讨论提纲

1. UE：`AddDynamic` / script delegate / UFunction 与我们规模的映射。  
2. 反射：`MulticastDelegate` 别名如何进 ScriptBinding。  
3. LuaCallable 包装与隐式 `function`→`AddScript`。  

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 初稿含 BindLua 竖切 |
| 2026-09-14 | **纠偏：** 拆除反模式 Bind；仅保留 Call；Delegate 后置讨论 |
