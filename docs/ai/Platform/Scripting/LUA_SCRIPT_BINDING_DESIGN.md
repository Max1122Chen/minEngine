# Lua Script Binding Codegen — Design Spec（CORE-F02）

## Meta

- **ID:** `CORE-F02`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** —
- **Last updated:** 2026-07-31
- **Branch:** `luaScript`（与 CORE-F01 同轨；勿与 `render` 混交）
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [CORE-F01 Lua runtime](./LUA_SCRIPTING_DESIGN.md) · [函数反射](../Reflection/REFLECTION_FUNCTIONS_DESIGN.md)
- **Depends on:** `CORE-F01` Done（`LuaScriptSystem` / sol2 / header tool 反射扫描已存在）

## TL;DR

在 **opt-in Script\* specifier** 下，用 header tool 从反射标注生成 **sol2 注册代码**，把真类型与函数暴露给 Lua。生成物放在 `Generated/ScriptBinding/`，与 Reflection gen 分目录。self 模型：C++ 对象指针（寿命先不严谨）。首真类型竖切后扩面。

## Scope

### In

- Script\* **specifier** 集（对照 UE Blueprint\*，见 §3.2）；header tool 识别并进入生成
- 生成 `*.lua_bind.gen.cpp`（及必要 `.h`）于 **`src/Generated/ScriptBinding/`**
- 总入口 `RegisterGeneratedLuaBindings(sol::state&)`；`LuaScriptSystem::Initialize` 在 Manual 之后调用
- 导出 **类（ScriptType）+ 函数（ScriptCallable）+ 属性（ScriptReadOnly / ScriptReadWrite）**
- **首真类型竖切**（默认候选：`Transform` 的极小子集，见 §8）
- 专用测试（可非 smoke）：Lua 调用生成 API 可断言

### Out

- 运行时 `InvokeFunction` 作为主绑定路径
- 无 specifier 的类型自动全量进 Lua
- BlueprintImplementableEvent / 多播委托进 Lua（后期对照表）
- 严格弱引用 / GUID 句柄 GC（接受悬空；强化另 Feat）
- `package.path`、热重载（仍属 F01 后续或别 Feat）

## Reader quick start

1. 本文件：方案与边界  
2. [CORE-F01](./LUA_SCRIPTING_DESIGN.md)：runtime 已落地部分  
3. 代码入口（落地后）：`scripts/minEngine_header_tool.py` · `Generated/ScriptBinding/` · `LuaScriptSystem.cpp`

---

## 1) 背景与目标

CORE-F01 已打通脚本执行与资产；绑定仍靠手写 `LuaManualBindings`。扩 API 需要 **可重复、opt-in、与反射同工具链** 的生成路径，而不是每加一个方法手写一遍 sol2。

成功：标记 `ScriptType` 的真类型在 Lua 中可 `new`/持有指针并调用 `ScriptCallable`、读写 `ScriptRead*` 属性；未标记类型不出现。

## 2) 现状

- header tool 已解析 `ME_CLASS` / `ME_FUNCTION` / `ME_PROPERTY` 的 **specifier** 与 `meta=()`  
- 函数反射 invoke 可用，但不当作脚本热路径主桥  
- sol2 + `LuaScriptSystem` 已初始化并注册手写白名单  
- 生成 Reflection 在 `Generated/Reflection/`；**Script 绑定应分目录**，避免混杂

## 3) 方案

### 3.1 数据流 / 模块边界

```text
Headers (Script* specifiers)
    → header tool
    → Generated/ScriptBinding/*.lua_bind.gen.cpp
    → RegisterGeneratedLuaBindings(state)
    → LuaScriptSystem::Initialize（Manual 之后）
```

### 3.2 Specifier 集（起步；对照 UE）

> 均为 **specifier**（如 `EditAnywhere`），不是 `meta=(...)` 键。完整 UE 对照可后期加行；第一期只实现下表。

| Specifier | 挂在 | ≈ UE | 生成行为 |
|-----------|------|------|----------|
| `ScriptType` | `ME_CLASS` | BlueprintType | 注册 sol usertype |
| `ScriptCallable` | `ME_FUNCTION` | BlueprintCallable | 绑定为可调函数（实例/静态） |
| `ScriptPure` | `ME_FUNCTION`（可选） | BlueprintPure | 第一期可等同 Callable |
| `ScriptReadOnly` | `ME_PROPERTY` | BlueprintReadOnly | 只读 property / getter |
| `ScriptReadWrite` | `ME_PROPERTY` | BlueprintReadWrite | 读写 property |

**后期（不阻塞竖切）：** ScriptImplementableEvent、Category 进文档、权限类 specifier 等。

示例：

```cpp
ME_CLASS(ScriptType)
class Transform { ... };

ME_PROPERTY(ScriptReadWrite)
Vector3 Location; // 或现有字段名

ME_FUNCTION(ScriptCallable)
void SetLocation(const Vector3& v);
```

### 3.3 self / 寿命（已拍板）

- Lua 侧实例 **self = C++ 对象指针**（sol usertype），不强制 GUID 句柄。  
- **逻辑依据（组件脚本）：** `LuaComponent` 寿命跟随且短于 Owner；Owner 销毁路径须 `UnloadScript` / 清 env，故「该组件脚本环境」不应在主人死后仍执行。  
- **边界：** 若脚本把指针存进 **共享全局**、或持有 **非 Owner 子树** 对象，仍可能悬空——F02 **接受**；与当前简易 GC 一致。强化句柄另 Feat。  
- 生成代码不引入额外所有权包装（第一期）。

### 3.4 生成物布局

| 路径 | 职责 |
|------|------|
| `src/Generated/ScriptBinding/<Type>.lua_bind.gen.cpp` | 单类型注册 |
| `src/Generated/ScriptBinding/ScriptBindingRegister.gen.cpp`（名可调） | `RegisterGeneratedLuaBindings` 汇总 |
| **不**写入 `Generated/Reflection/` | 与反射注册分离 |

CMake：ScriptBinding 源加入引擎目标（或与 reflection_codegen 同钩子扩展）。

### 3.5 API 契约

```cpp
namespace minEngine
{
    void RegisterGeneratedLuaBindings(sol::state& state);
}
```

`LuaScriptSystem::Initialize`：`OpenStandardLibraries` → `LuaManualBindings::Register` → `RegisterGeneratedLuaBindings`。

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Specifier → sol2 codegen | 类型安全、与工具链一致 | 要维护生成模板 | **选用** |
| B. 运行时 Invoke 桥 | 零生成 | 慢、编组难 | 不做主路径 |
| C. 永久手写 | 简单 | 扩面不可持续 | 仅留 Manual 白名单 |

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 绑定面失控 | API 泄漏 | 默认无导出；仅 Script\* |
| 悬空指针 | 崩溃 | 文档约定；组件 env 清理；接受 F02 范围 |
| 生成模板复杂度 | 排期爆 | 首类型极小子集；切片扩 |
| 与 Reflection gen 耦合 | 难维护 | 目录与注册入口分离 |

## 6) 验收标准

- [x] Specifier 被 header tool 识别；无标记则不生成/不注册
- [x] `Generated/ScriptBinding/` 产出可编译；引擎 Init 注册成功
- [x] 至少一个 **真类型** 在 Lua 中可调 Callable + 读写约定属性（`Transform`：`Position` + `SetPosition`/`Translate`）
- [x] 测试覆盖生成路径（`lua-script-mvp` 增补 Transform 用例；非 smoke）
- [x] Design / Registry / Progress 更新；CORE-F01 不再承担 codegen

## 7) Status note

In Progress — 首真类型定为 `Transform` 极小子集。注意：`Transform` 非 `MEObject`，`ME_FUNCTION` 进 ScriptBinding，但 **不** 进反射 native thunk 注册（header tool 仅对 MEObject/Component 体系生成 thunk）。

---

## 8) 切片预览

| Slice | 内容 | 验证 |
|-------|------|------|
| S01 | header tool 识别 Script\*；空/探测生成管线 + CMake `ScriptBinding` | 配置通过；空注册可链接 |
| S02 | 首真类型极小导出（建议 `Transform`：`ScriptType` + 1–2 property + 1–2 Callable） | Lua 断言 |
| S03 | `RegisterGeneratedLuaBindings` 挂 `LuaScriptSystem`；文档/样例脚本 | Editor 或测试可见 |
| S04+ | 扩类型 / ScriptPure / 更多属性规则 | 按需 |

首类型若改为纯值类型（如包装 `Vector3`）须在 Status note 记录。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-07-31 | In Progress：S01–S03 落地（ScriptBinding 生成、`Transform` 子集、Init 挂钩、测试） |
| 2026-07-31 | Draft：从 CORE-F01-S06 升格；specifier 集、ScriptBinding 目录、self=指针、首真类型方向 |
