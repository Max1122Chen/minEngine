# Lua 脚本 — Design Spec（CORE-F01）

## Meta

- **ID:** `CORE-F01`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** —
- **Last updated:** 2026-07-31（MVP S01–S04 已落地）
- **Branch:** `luaScript`
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [函数反射设计](../Reflection/REFLECTION_FUNCTIONS_DESIGN.md) · [委托占位](../Reflection/REFLECTION_DELEGATES_DESIGN.md)
- **Implementation Plan:** 待补 `LUA_SCRIPTING_IMPLEMENTATION.md`（切片表）

## TL;DR

需要一条轻量脚本通路。**MVP**（S01–S04）已落地：唯一 `LuaScriptSystem`（sol2）→ 手写 `LuaBindProbe` + `me.log` → `LuaComponent` 写死 chunk + Tick → 销毁清 env。不做读文件 / `package.path` / 可配置脚本；验证 suite `lua-script-mvp`（非 smoke）。下一步：S05+ 文件/资产/codegen。分支 `luaScript`。

## Scope

### In（本 Feature 全期可排，按切片推进）

**MVP / 可行性竖切（本 Feature 当前目标）：**

- 引入 **sol2** + **`LuaScriptSystem`**（唯一 `sol::state`）
- Engine Init/Shutdown 挂接、脚本错误日志
- **手写白名单绑定**：`LuaBindProbe` + `me.log`
- **`LuaComponent`**：每实例 env；**写死一段内嵌 Lua 源码**（C++ 字符串常量）；`Tick` 调约定 `tick`
- 组件销毁时清 env / 停 Tick（最小寿命）
- **瞬时验证测试**（见 §6 / §9.10）：证明通路后可删或大幅收缩，不进长期 smoke 门禁

**后续切片（非本 MVP Done 条件；另排期）：**

- `package.path` / `require`、读 `.lua` 文件、可配置 ScriptPath/Source
- 脚本资产（如 `.melua`）、Content Browser / Inspector
- header tool 按标记生成 sol2 绑定

### Out（MVP 明确不做）

- 读 `.lua` 文件执行；`RunFile`；`ConfigurePackagePath`
- `LuaComponent` 可变脚本路径/源码字段（无 `m_ScriptPath` / 可编辑 Source）
- 运行时 `InvokeFunction` 通用桥
- 委托 / Lua 事件、热重载、调试器、多 state
- 全引擎 API、coroutine、任意 table↔struct
- 与 RND-F02 混在同一分支交付
- 把本 MVP 的瞬时用例永久钉进 `test smoke`（除非日后另开「产品化脚本」门禁）

## Reader quick start

1. 本文件：方案与边界
2. Implementation Plan（待建）：切片与验收命令
3. 代码入口（落地后，见 §9.1）：`Runtime/Function/Scripting/`；绑定生成物后期 `Generated/` 或等价路径

---

## Pre-flight（2026-07-30）

| 项 | 结论 |
|----|------|
| 前置 | P4 函数反射 invoke/frame/static 可用 → **sound**；脚本子系统与 sol2 → **missing** |
| 债务风险 | **medium** — 绑定面过大易成第二套 API；用白名单 + 后期 codegen 收敛 |
| WIP | `RND-F02` 在 `render`；本 Feature 在 **`luaScript`** → **proceed（分轨）** |
| 建议 | **Go with scope cut** — 先 System + 手写绑定 + LuaComponent 竖切，再资产化与 codegen |

---

## 1) 背景与目标

### 1.1 Pain

- Gameplay/工具逻辑全写 C++ 迭代慢；需要脚本快速试玩与挂接组件行为。
- 长期手写海量 C API 不可维护；最终应靠 **反射信息 + codegen** 导出，但验证可行性不必等自动化。

### 1.2 成功长什么样（MVP 竖切）

1. Engine 启动后存在唯一 Lua state。
2. `LuaComponent` 用**写死的**内嵌 chunk 跑起来，并能 `Tick`。
3. 脚本能调用 `me.log` + `LuaBindProbe`（证明 Lua→C++）。
4. 销毁组件后清 env，引擎继续跑不崩。
5. 验证用测试/夹具标注为 **feasibility / disposable**；本 Feature 收口后可不保留为长期门禁。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| 函数反射 P4 | 可用；slim 测试门禁已绿（`reflection-function`） |
| 脚本运行时 | 无 |
| `Component` | 有 `Tick` / `SetOwner`；无独立 `BeginPlay` |
| Engine 子系统 | 显式 Init/Shutdown 序（Object → … → Scene）；无 Script 先例 |
| 读文件 / 脚本资产 | **MVP 不做**；后期切片 |
| 委托 | 占位；不阻塞 Tick 驱动脚本 |

---

## 3) 方案

### 3.0 为何 MVP 不需要 `package.path`

`package.path` 只服务于 Lua 的 **`require` / 按路径找模块**。  
MVP **不读 `.lua` 文件、不做模块系统** → System **不必**配置 `package.path`，也**不必**提供 `RunFile`。

执行路径只有：`sol::state` 上 **`script` / `load` 内存字符串**（System 自测用 `RunString`；Component 用写死的 `constexpr`/`static` 源码）。

### 3.1 推进顺序（规范）

```text
S01  LuaScriptSystem：唯一 state + Init/Shutdown + 错误日志（无 package.path / RunFile）
S02  手写 sol2：`LuaBindProbe` + `me.log`
S03  LuaComponent：写死内嵌 chunk + env + Tick（无可配置脚本）
S04  销毁：清 env + 停 Tick
—— MVP 收口；下面为后续 Feature/切片 ——
S05  可配置脚本 / 读文件 / package.path / 资产化
S06  Header tool codegen
```

手写绑定是 **S02 显式切片**。类型与接口细节见 **§9**（§9 以 MVP 为准；文件/路径 API 标为后期）。

### 3.2 模块边界

```text
Engine
  └── LuaScriptSystem              // 拥有 sol::state；Init 时 RegisterManualBindings
        ▲
        │ GetState() / ReportLuaError() / RunString（测用）
LuaComponent                       // 写死 chunk → env；Tick → tick(dt)
        │
        ▼
LuaManualBindings：me.log + LuaBindProbe
```

- **不**再包一层「自家 Lua C API」；业务绑定直接用 sol2。
- **不**把 Transform / LogSystem / ReflectionSample 当 S02 主绑定靶；见 §9.4。
- **瞬时测试**：专测本竖切；收口后可删 suite 或降级为可选，不必进 smoke。
---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. sol2 直接绑定 + 后期 codegen | 快、类型友好、与现有 header tool 同向 | 需维护生成模板 | **选用（主路径）** |
| B. 仅运行时 InvokeFunction 桥 | 任意 `ME_FUNCTION` 无需生成 | 编组复杂、调试差、热路径慢 | 后手可选 |
| C. 纯 lua C API 手写 | 无依赖 | 绑定样板多 | 不选 |
| D. 每 World 多 state | 隔离好 | MVP 过重 | 延期 |

依赖库：**sol2** + **Lua 5.4.5**，已 vendor 于 `minEngine/Third-Party/{sol2,lua}`（见各目录 `VENDOR.md`）；CMake 自建 `minengine_lua`，不走 FetchContent。

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Lua 持有已毁 C++ 对象 | 崩溃 | S04 失效策略；测试覆盖销毁路径 |
| 绑定面失控 | 双 API 世界 | opt-in 标记；手写白名单极小；codegen 替代扩张 |
| 无 BeginPlay | 加载时机含糊 | MVP 首次 Tick 加载写死 chunk；足够 |
| 与 `render` 分支冲突 | 合并痛 | 本 Feature 仅 `luaScript`；少碰 RHI |
| codegen 低估 | 排期爆炸 | S06 独立切片；先手写证明 |

---

## 6) 验收标准

### Feature MVP 竖切（S01–S04）Done

- [x] sol2 已接入；`LuaScriptSystem` Init/Shutdown 挂 Engine（§9.2）
- [x] `LuaBindProbe` + `me.log` 可从 Lua 调用（§9.4–9.5）
- [x] `LuaComponent` 用**写死 chunk** 能 `Tick` 并产生可断言副作用（§9.6）
- [x] 销毁组件后清 env，引擎不崩（§9.8）
- [x] 存在瞬时验证（专用 suite）；**不要求**进 `test smoke`
- [x] Design / Progress / Registry 状态已更新

### 后续切片（不阻塞 MVP Done）

- [ ] 可配置脚本 / 读 `.lua` / `package.path`
- [ ] 脚本资产类型
- [ ] Header tool 生成绑定
- [ ] 产品化后的长期脚本门禁（若需要）

### 建议验证命令（落地后填实）

```bash
# MVP：瞬时 suite，不必进 smoke；收口后可删除
minEngineTests.exe test lua-script-mvp
```

---

## 7) Status note

（Draft — 无 Blocked 字段）

---

## 8) 切片预览（正式 Impl Plan 前的提纲）

| Slice | 内容 | 验证 |
|-------|------|------|
| S01 | sol2 + `LuaScriptSystem`（无 path/文件） | Init/Shutdown；`RunString("return 1+1")` |
| S02 | `LuaBindProbe` + `me.log` | Lua 调 Add/GetValue |
| S03 | `LuaComponent` 写死 chunk + Tick | 多帧副作用可断言 |
| S04 | 销毁清 env | 毁组件后不崩 |
| S05+ | 文件/资产/codegen | 另排期 |

---

## 9) 类型与接口设计（细化 · 供评审）

> 下列为 **Draft 契约**：实现时可微调命名，但行为与边界应保持一致。注释与标识符按工程习惯用英文。

### 9.1 目录与文件（落地后）

| 路径（建议） | 职责 |
|--------------|------|
| `Runtime/Function/Scripting/LuaScriptSystem.h/.cpp` | 唯一 state；Init/Shutdown；错误报告（MVP **无** package.path） |
| `Runtime/Function/Scripting/LuaManualBindings.h/.cpp` | S02 手写 `RegisterManualBindings(sol::state&)` |
| `Runtime/Function/Scripting/LuaBindProbe.h/.cpp` | 专用绑定/测试夹具（无 Component / 无反射） |
| `Runtime/Function/Framework/Components/LuaComponent.h/.cpp` | 挂 GO；MVP 内嵌写死 chunk |
| 测试 | `Tests/Suites/LuaScriptMvpTest.cpp`（**瞬时** suite，如 `lua-script-mvp`；收口可删） |

第三方：**Lua 5.4.5**（`Third-Party/lua`，`minengine_lua`）+ **sol2**（`Third-Party/sol2/include`，commit 见 `VENDOR.md`）。

### 9.2 `LuaScriptSystem`

对齐现有子系统习惯（`InputSystem` 式：`Initialize` / `Shutdown` / `Get`）。

```cpp
namespace minEngine
{
    class LuaScriptSystem
    {
    public:
        LuaScriptSystem() = default;
        ~LuaScriptSystem() = default;

        LuaScriptSystem(const LuaScriptSystem&) = delete;
        LuaScriptSystem& operator=(const LuaScriptSystem&) = delete;

        void Initialize();
        void Shutdown();

        static LuaScriptSystem& Get();
        static bool HasInstance();

        sol::state& GetState();
        const sol::state& GetState() const;

        void ReportLuaError(std::string_view context, const sol::error& error);
        void ReportLuaError(std::string_view context, std::string_view message);

        // MVP: memory string only. No RunFile / package.path.
        bool RunString(std::string_view chunk, std::string_view chunkName = "RunString");

    private:
        void OpenStandardLibraries(); // base, package, string, table, math — package lib ok, no path config

        static LuaScriptSystem* s_Instance;
        std::unique_ptr<sol::state> m_State;
        bool m_Initialized = false;
    };
}
```

**Engine 挂接（建议序）：**

- `StartSystems`：Reflection finalize **之后**，推荐 **SceneManager 之前**。
- `ShutdownSystems`：先停逻辑再 `Shutdown` state。

**MVP 明确不做：**

- `ConfigurePackagePath` / 设置 `package.path`
- `RunFile` / 任何磁盘 `.lua` 加载

（上述能力归 S05+。）

**不变量：**

- 进程内至多一个已 Init 的 `LuaScriptSystem`。
- `GetState()` 仅在已 Init 时合法。
- 不向业务层封装 sol2；需要时直接拿 `sol::state&`。

### 9.3 手动绑定入口

```cpp
namespace minEngine::Scripting
{
    // Called once from LuaScriptSystem::Initialize after state + libs ready.
    void RegisterManualBindings(sol::state& state);
}
```

`RegisterManualBindings` 内完成：

1. 全局/表：`me.log`（见 §9.5）
2. usertype：`LuaBindProbe`（见 §9.4）
3. （S03+）再增加 `LuaComponent` 向脚本暴露的只读字段，**不**在 S02 绑定 `Transform` / `GameObject` 全家桶

### 9.4 `LuaBindProbe`（S02 主靶 · 已拍板）

**目的：** 验证 sol2 注册与 Lua→C++ 调用；**编译依赖极小**，改头文件不拖 Math/GLM/Component/gen。

```cpp
namespace minEngine
{
    // No ME_CLASS / no Component / no GLM in S02.
    class LuaBindProbe
    {
    public:
        LuaBindProbe() = default;

        int32_t Add(int32_t a, int32_t b) const;
        void SetValue(int32_t value);
        int32_t GetValue() const;

        // Optional: makes tests deterministic without log scraping.
        static void ResetStaticCounter();
        static int32_t GetStaticCounter();
        static void IncrementStaticCounter();

    private:
        int32_t m_Value = 0;
        static int32_t s_StaticCounter; // defined in .cpp (DLL-safe; not inline in header)
    };
}
```

**sol2 注册草案（概念）：**

```cpp
state.new_usertype<LuaBindProbe>(
    "LuaBindProbe",
    sol::constructors<LuaBindProbe()>(),
    "Add", &LuaBindProbe::Add,
    "SetValue", &LuaBindProbe::SetValue,
    "GetValue", &LuaBindProbe::GetValue,
    "ResetStaticCounter", &LuaBindProbe::ResetStaticCounter,
    "GetStaticCounter", &LuaBindProbe::GetStaticCounter,
    "IncrementStaticCounter", &LuaBindProbe::IncrementStaticCounter);
```

**Lua 侧约定名：** 全局类型名 `LuaBindProbe`（与 C++ 一致，避免过早引入命名空间糖）。

**明确不作为 S02 主靶：**

| 类型 | 原因 |
|------|------|
| `Transform` | 头文件/gen/Math 牵动面大 |
| `LogSystem` usertype | 无必要；用自由函数即可 |
| `ReflectionSampleComponent` | 污染 P4 夹具 + Component 依赖 |

### 9.5 `me.log`（自由函数，非绑 LogSystem）

```cpp
// In LuaManualBindings.cpp — implementation detail, not a public engine type.
namespace
{
    void MeLog(std::string_view message)
    {
        ME_CORE_INFO("[Lua] {}", message);
    }
}

// Register:
sol::table me = state.create_named_table("me");
me.set_function("log", &MeLog);
```

- Lua：`me.log("hello")`
- 只 `#include` 日志头于 **Scripting 的薄 .cpp**；不修改 `LogSystem.h`，不 `usertype<LogSystem>`
- 后续可加 `me.warn` / `me.error`，仍保持自由函数

### 9.6 `LuaComponent`（MVP：写死脚本）

继承 `Component`。MVP **不要** `m_ScriptPath` / 可配置 Source / 读文件。  
反射：`ME_CLASS` 可选（想少碰 gen 可不加）。

```cpp
namespace minEngine
{
    class LuaComponent : public Component
    {
    public:
        void Tick(float deltaTime) override;

        bool IsScriptLoaded() const { return m_Loaded; }
        bool IsScriptEnabled() const { return m_ScriptEnabled; }

        bool LoadScript();   // loads the hardcoded chunk once
        void UnloadScript();

    private:
        // Hardcoded feasibility script lives in .cpp (not a public editable API).
        static const char* GetHardcodedScript();

        bool EnsureLoaded();
        bool CallTick(float deltaTime);
        void ClearLuaEnvironment();

        sol::environment m_Environment;
        sol::protected_function m_TickFn;
        bool m_Loaded = false;
        bool m_ScriptEnabled = true;
        bool m_HasLoggedTickError = false;
    };
}
```

**写死 chunk 示例（.cpp 内常量）：**

```lua
function tick(dt)
  LuaBindProbe.IncrementStaticCounter()
end
```

MVP **只认全局 `function tick(dt)`**（实现简单）。`return { tick = ... }` 留到可配置脚本阶段再考虑。

**加载时机：** 首次 `Tick` 懒加载 `GetHardcodedScript()`。  
**Unload：** 析构 / 显式 `UnloadScript`。

**后期（S05，非 MVP）：** `ScriptPath` / Source / 资产引用 —— 届时再扩接口。

### 9.7 错误与 Tick 策略

| 事件 | 行为 |
|------|------|
| 写死 chunk `load` 失败 | `ReportLuaError`；`m_Loaded=false`；`m_ScriptEnabled=false` |
| `tick` 抛错 | `ReportLuaError`；**本帧跳过**；可选：连续失败 N 次后 `m_ScriptEnabled=false`（首版：首次错误后禁用 Tick 即可） |
| System 未 Init | `LuaComponent::Tick` no-op |

### 9.8 寿命与销毁（S04 竖切最小策略 · 已收敛）

S04 **先做策略 1**（简单、够用）：

1. `UnloadScript` / 析构：`m_TickFn` 置空；`m_Environment = sol::environment{}`（或等价释放）  
2. 不再把裸 `Component*` / `GameObject*` 长期塞进会逃逸的 upvalue（S03 先少暴露）  
3. 验收：销毁带 `LuaComponent` 的 GO 后继续跑帧 → **不崩溃**

策略 2（GUID / ObjectManager 弱语义）列为 **S04 后半或独立小切片**，不阻塞竖切 Done。

### 9.9 S06 Codegen 接口预告（不实现，只定方向）

| 项 | 倾向（待最终拍板） |
|----|-------------------|
| 导出默认 | **opt-in**（避免全引擎进 Lua） |
| 标记候选 | 函数：`ME_FUNCTION(ScriptCallable)`；类：`ME_CLASS(ScriptExport)` |
| 生成物 | `TypeName.lua_bind.gen.cpp`，提供 `RegisterGeneratedLuaBindings(sol::state&)` |
| 调用点 | `LuaScriptSystem::Initialize`：`RegisterManualBindings` 之后调用 generated register |
| 首样例 | 仍可用 `LuaBindProbe` 加标记做第一个生成目标，再删对应手写注册 |

`MEObject*` 句柄模型：**竖切不定**；S06 前另开短讨论 / ADR。

### 9.10 测试面（瞬时 / disposable）

| 用例 | 断言 |
|------|------|
| System Init/Shutdown | `RunString("return 1+1")` |
| Probe | `Add` / `GetValue` / static counter |
| `me.log` | 调用不崩 |
| Component | 写死 `tick` 多帧后 static counter 增加 |
| 销毁 | Unload 后不崩 |

**策略：** suite 标注为 MVP 可行性验证；`CORE-F01` MVP 收口并确认通路后，**可以删除该 suite 或大幅收缩**。长期门禁留给产品化切片（读文件/资产/codegen）再定。  
`LuaBindProbe` 可保留为日后 codegen 样例；若仅服务瞬时测试，也可一并删。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-27 | 占位说明（P5） |
| 2026-07-30 | 升格为 `CORE-F01` Design Spec 初稿；分支 `luaScript`；明确 System → 手写绑定 → Component → 寿命 → 资产 → codegen |
| 2026-07-31 | §9 细化类型/接口：System、ManualBindings、LuaBindProbe、me.log、LuaComponent、寿命与 codegen 预告 |
