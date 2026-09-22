# Multi-Module Reflection Codegen (Engine + Game) — Design Spec

## Meta
- **ID:** `CORE-F27`
- **Type:** Feature（体系升级 / 真重构；非挂接热修）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [System Review](../Reflection/REFLECTION_MULTI_MODULE_CODEGEN_SYSTEM_REVIEW.md)（复盘权威）
  - [Implementation](./CORE-F27_EXTERNAL_COMPONENT_ATTACH_IMPLEMENTATION.md)（文件名暂留；内容已转向 codegen）
  - [ED-F20](../../Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md) · [ED-F21](../../Editor/ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_DESIGN.md)
  - [ED-F22](../../Editor/ED-F22_GAME_MODULE_STARTUP_DESIGN.md)（**Cancelled** — 不做 Startup 钩子）
  - [CORE-F12](../Reflection/CORE-F12_GENERATED_BODY_NO_ARG_DESIGN.md) · [CORE-F02](../../Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md)
  - 发现场景（只读）：minCraft `ENG-GAP-006-A`（根因：无项目侧 codegen，而非缺挂接 API）
- **Branch:** `master`

## TL;DR

minCraft 暴露的不是「缺一个 `AddComponentInstance`」，而是 **header tool / CMake 只服务引擎单一 `src` 根**：游戏仓 `ME_CLASS` 无法生成 `GetClassName` 与注册 TU，故 `AddComponent<T>()` 不可用。本 Feature 把 codegen **产品化为多模块管线**——引擎与项目/游戏各自 `src-root` → 各自 `out-dir` / `manifest` → 产物链入**各自 TARGET**；链接期加载后静态注册汇入同一 `ReflectionSystem`，再 `Finalize`。官方路径与引擎内类型一致。挂接热修降为可选逃生口，**不是**推荐终态。

## Scope
- **In:**
  - 可复用 CMake 封装：对任意 TARGET 跑 header tool（独立 SRC_ROOT / OUT_DIR / MANIFEST）
  - 引擎改用该封装（行为与今日等价）
  - 游戏 / GameModuleStub：示范 `ME_CLASS` Component + 自有 Generated + 链进游戏 SHARED
  - 生命周期契约：静态注册在 `FinalizeReflection` 之前；世界搭建由游戏仓/默认场景完成（**无**官方 Startup 钩子）
  - 验收：外仓类型可用 `NewObject`/`AddComponent<T>`；`GetClass()` 非空；Scene Root/Attach 与内置一致
- **Out:**
  - 重写 header tool 解析器
  - 体素 / 玩法进 Core；改 minCraft 仓（本仓只提供能力）
  - ScriptBinding 分模块（可后置切片；见 Review R05）
  - LoadLibrary 后增量 Finalize（后置）
  - 以无反射挂接 API 作为官方终态

## Reader quick start
1. [System Review](../Reflection/REFLECTION_MULTI_MODULE_CODEGEN_SYSTEM_REVIEW.md) — 为何不是热修
2. 本文件 — 目标架构与契约
3. Implementation — 切片
4. 代码入口：`scripts/minEngine_header_tool.py`；`minEngine/minEngine/CMakeLists.txt`；`ReflectionMacros.h`；`Engine::FinializeReflection`

---

## 1) 背景与目标

### 发现场景 → 引擎缺口

| 表象（游戏仓） | 引擎侧根因 |
|----------------|------------|
| 不能 `AddComponent<T>()` | 无游戏侧 `.gen` → 无 `GetClassName<T>` |
| `InsertRestoredComponent` 怪异 | `GetClass()==null`；且本就不是「添加」API |
| 双构建 / 复制 AABB | 缺「始终链引擎 + 同源反射」的官方模块路径（ED-F21 配合） |

### Success

```text
游戏头：
  ME_CLASS()
  class MyPrim : public PrimitiveComponent { ME_GENERATED_BODY(); ... };
  #include "MyPrim.gen.h"   // 来自游戏 OUT_DIR

游戏 CMake：
  minengine_add_reflection_codegen(MyGame SRC_ROOT ... OUT_DIR ...)
  target_sources(MyGame PRIVATE ${生成的 .gen.cpp})  # 或封装内完成

Dev（ED-F20）：
  Editor 链 MyGame → 进程加载 → 引擎+游戏静态注册 → Finalize
  → go->AddComponent<MyPrim>()  与引擎内置组件同路径
```

---

## 2) 现状（证据摘要）

详见 System Review §2–3。要点：

- Tool CLI **已**支持 `--src-root` / `--out-dir` / `--manifest`（非写死路径）。
- 引擎 CMake **只调用一次**，扫 `minEngine/.../src`，产物 GLOB 进 `minEngine` SHARED。
- `Ready` 后禁止 `RegisterClass` → 游戏注册必须靠 **DLL 加载期静态构造**（与引擎相同），不能默认放进 Initialize 后的 Startup。

---

## 3) 方案

### 3.1 目标结构

```text
header_tool × N（每模块一次）
  Module_engine: src=engine/src → out=engine/Generated/Reflection → link minEngine
  Module_game:   src=game/src   → out=game/Generated/Reflection   → link Game.dll

load-time: 两边 .gen.cpp 静态注册 → Collecting
Engine::Initialize: FinalizeReflection → Ready
消费：AddComponent<T> / 按名 Add / Serialize / Inspector（游戏类型在表中）
```

### 3.2 CMake 契约（推荐 API 草案）

```cmake
# 例如放在 cmake/MinEngineReflection.cmake
function(minengine_add_reflection_codegen target_name)
  # 参数：SRC_ROOT, OUT_DIR, MANIFEST（可选默认 OUT_DIR/manifest.json）
  #        [SCRIPT_BINDING_OUT_DIR] 本期可省略
  # 行为：
  #  - add_custom_command 调 minEngine_header_tool.py
  #  - add_custom_target(${target_name}_reflection_codegen)
  #  - add_dependencies(${target_name} ...)
  #  - target_include_directories(${target_name} PUBLIC ${OUT_DIR})
  #  - 将生成的 .gen.cpp 加入 ${target_name}（manifest 驱动或 GLOB OUT_DIR）
endfunction()
```

- 引擎 `CMakeLists.txt` 改为调用该函数（零行为回归）。
- 游戏 / stub 在自身 `CMakeLists.txt` 调用；**禁止**把游戏头塞进引擎 `REFLECTION_SCAN_ROOT`。

### 3.3 头文件与命名

- 保持今日：**手写** `#include` 对应 `.gen.h`（不自动改源码）。
- 每模块独立 OUT_DIR，避免扁平 stem 冲突（Review R04）。
- 类名：工具生成完全限定名（`namespace::Type`）；游戏建议自有 namespace（Open Q1）。

### 3.4 生命周期（硬契约）

| 阶段 | 允许 |
|------|------|
| DLL 加载静态构造 | `RegisterClass` / `GetClassName` 定义 |
| `FinalizeReflection` 前 | 收集完毕 |
| `Ready` 后 | **禁止**新 Register（今日实现） |
| 世界搭建 | 游戏仓 / 工程默认场景：CreateScene、Add GO、Add Comp（**不**经引擎强制 C Startup） |

若未来 LoadLibrary：另开「Re-Finalize / 模块波次」Feature；**不在本期**。

### 3.5 与 ED-F21

| Feature | 角色 |
|---------|------|
| **CORE-F27** | 反射 codegen 多模块（本主线） |
| **ED-F21** | 游戏 **始终** `target_link_libraries(... minEngine)` + 文档；codegen 需要引擎头与宏 |
| **ED-F22** | **Cancelled** — 不提供 `GameModuleStartup` |

### 3.6 可选逃生口（非目标主路径）

`AddComponentInstance` + `dynamic_cast` Scene 判定：仅当「无 ME_CLASS 的临时实验类型」需要。Design **不**将其列为官方外仓路径。若实现，标 `Transient` / 文档降级。

### 3.7 Stub 证明

扩展 `Examples/GameModuleStub`（或并列 stub）：

1. `PRIVATE minEngine`
2. 一个最小 `ME_CLASS` `StubGamePrimitiveComponent : PrimitiveComponent`
3. 本模块 codegen → 链进 stub DLL
4. 测试中：`AddComponent<StubGamePrimitiveComponent>()`，断言 `GetClass()!=nullptr`、Root/Active（场景由测试/工程创建，**不**经引擎 Startup 钩子）

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 多调用 header tool + CMake 封装 | 真解；复用现有 tool | 要理清产物归属与 stub | **选用** |
| B. 仅 `AddComponentInstance` 热修 | 快 | 反射/序列化/Inspector 永久残缺 | **拒绝作主线** |
| C. 运行时 `RegisterExternalClass` 无 codegen | 无工具链 | 无属性表；双轨 | **拒绝作主线** |
| D. 重写 header tool | — | 无证据必要 | **拒绝** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 游戏静态注册晚于 Finalize | 类型不可见 | 文档 + stub 验证；禁止 Startup 注册 |
| 同名 `.gen` 冲突 | 错链 | 每模块 OUT_DIR |
| GLOB 时序：codegen 未跑先配置 | 缺源 | 现有 custom_command + manifest DEPENDS 模式；封装复制 |
| ScriptBinding 未分模块 | 游戏 Lua 不全 | 后置；MVP 只保证 C++ 反射 |
| 与 RND-F19 并行 | 焦点 | ACTIVE_WORK：本波 foundation 优先 |

---

## 6) 验收标准

- [x] 引擎仍只扫引擎 src；默认 verify smoke 不挂
- [x] CMake 封装可被游戏目标复用（`minEngine/cmake/MinEngineReflection.cmake` + stub）
- [x] Stub 游戏类型：`AddComponent<T>()` 编译并运行；`GetClass()` 非空（`test game-module-stub`）
- [x] 生命周期契约写入 BOOTSTRAP；世界搭建归游戏仓
- [x] 无玩法源码进 Core；不改 minCraft 仓
- [x] （非必须）逃生口 API 未做

---

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | 初稿误以挂接热修为主线 |
| 2026-09-22 | **纠偏：** 主线改为多模块 reflection codegen；依据 System Review；热修降级 |
| 2026-09-22 | ED-F22 Cancelled；世界搭建归游戏仓/默认场景 |
| 2026-09-22 | **Done：** CMake 封装 + stub ME_CLASS + `test game-module-stub`；smoke PASS |
