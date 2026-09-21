# Editor Optional Game Module Link — Design Spec

## Meta
- **ID:** `ED-F20`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-21
- **Related:**
  - [Implementation](./ED-F20_OPTIONAL_GAME_MODULE_LINK_IMPLEMENTATION.md)
  - [TEST-F05](../Platform/Test/TEST-F05_EXTERNAL_TEST_HARNESS_DESIGN.md)（独立验收；可并行）
  - [ENGINE_DESIGN_PHILOSOPHY.md](../ENGINE_DESIGN_PHILOSOPHY.md)
  - 发现场景（只读）：minCraft `INFRA-F01` / `ENG-GAP-001`
- **Branch:** `master`（实现切片待确认后开）

## TL;DR

MaximumEditor 今天只链接 `minEngine` SHARED，外部游戏仓的 SHARED（如任意 `<Game>.dll`）无法在不改叉 Editor 的前提下进入同一进程。本 Feature 提供 **可关闭的 CMake 钩子**：可选链接外部游戏目标，关闭时行为与今日完全一致。这是 **链接期 Dev 宿主模块**，不是 LoadLibrary 热插拔。

## Scope
- **In:**
  - 文档化的 CMake 选项（路径 / 目标名可配置，不写死某个游戏）
  - 打开时：`Editor` 链接指定 SHARED 目标；约定输出目录或 POST_BUILD 拷贝
  - 引擎仓内 **最小 stub 游戏 SHARED**（证明开关，不依赖外仓）
  - 关闭开关：零回归
- **Out:**
  - 完整插件系统 / 沙箱 / 热重载 / LoadLibrary 协议（→ 后续，对应 minCraft `ENG-GAP-002`）
  - 把某个具体游戏源码默认 `add` 进引擎树
  - Release 游戏宿主 EXE 打包（游戏仓自己的 INFRA）
  - 强制游戏必须导出初始化符号（MVP **仅链接**；显式 Startup 符号作可选 S03）

## Reader quick start
1. 本文件 — 方案与验收
2. [Implementation](./ED-F20_OPTIONAL_GAME_MODULE_LINK_IMPLEMENTATION.md) — 切片
3. 代码入口（落地后）：`minEngine/CMakeLists.txt`、`minEngine/Editor/CMakeLists.txt`、`minEngine/Examples/GameModuleStub/`（或等价路径）

---

## 1) 背景与目标

### Pain
- 哲学要求：玩法在独立游戏仓；Dev 却期望 `MaximumEditor.exe + minEngine.dll + <Game>.dll` 同进程。
- 现状：`Editor/CMakeLists.txt` 只有 `target_link_libraries(Editor PRIVATE minEngine)`。
- 游戏仓不能也不应 fork Editor 源码来加一行 link。

### Success
```text
# 默认（无游戏）
cmake ...   → MaximumEditor 与今日相同

# Dev 接游戏仓
cmake ... -DMINENGINE_GAME_DIR=<abs-path-to-game-cmake-root> \
          -DMINENGINE_GAME_TARGET=<cmake-target-name>
  → Editor 链接该 SHARED；bin 下可找到游戏 DLL；进程可启动
```

---

## 2) 现状

| 项 | 状态 |
|----|------|
| `minEngine` | SHARED；`EXECUTABLE_PATH` → `bin`（或 `MINENGINE_OUTPUT_DIR`） |
| Editor | target 名 `Editor`，产物 `MaximumEditor.exe`；只链 `minEngine` |
| 入口 | `main.h` → `CreateApplication()`（Editor 侧实现）；**无**游戏模块钩子 |
| 热插拔 | 无 |

---

## 3) 方案

### 3.1 模型（链接期模块）

```text
┌─────────────────────────────────────┐
│ MaximumEditor.exe                   │
│  link: minEngine + (optional Game)  │
└──────────────┬──────────────────────┘
               │ load-time dependency
       ┌───────┴────────┐
       ▼                ▼
 minEngine.dll     <Game>.dll
```

- Windows：链接 import lib → 进程启动时加载 DLL；静态构造 / 反射自注册可在 `DllMain` 前后跑完。
- **不是** 运行时 `LoadLibrary`；改游戏代码仍需重链 / 重启 Editor（可接受，Dev 默认）。

### 3.2 CMake 契约（推荐）

| 变量 | 含义 | 默认 |
|------|------|------|
| `MINENGINE_GAME_DIR` | 外部游戏仓 **CMake 根目录**（含 `CMakeLists.txt`） | 空 = 不启用 |
| `MINENGINE_GAME_TARGET` | 要链接的 CMake **目标名**（SHARED） | 空；启用 DIR 时 **必填** |
| `MINENGINE_GAME_COPY_RUNTIME` | POST_BUILD 把游戏 RUNTIME DLL 拷到 `EXECUTABLE_PATH` | `ON`（启用游戏时） |

**启用条件：** `MINENGINE_GAME_DIR` 非空 **且** `MINENGINE_GAME_TARGET` 非空。

**行为：**

1. `add_subdirectory(${MINENGINE_GAME_DIR} ${CMAKE_BINARY_DIR}/external_game EXCLUDE_FROM_ALL)`  
   （binary dir 固定在引擎 build 下，避免污染游戏仓自己的 out-of-tree build；文档说明：游戏仓也可先独立编，再改用 Imported 路径——见备选。）
2. `if(NOT TARGET ${MINENGINE_GAME_TARGET})` → `FATAL_ERROR` 带清晰信息。
3. `target_link_libraries(Editor PRIVATE ${MINENGINE_GAME_TARGET})`
4. 若 `MINENGINE_GAME_COPY_RUNTIME`：对 Windows/MinGW `add_custom_command(POST_BUILD … copy $<TARGET_FILE:…>)` 到 `${EXECUTABLE_PATH}`。

**关闭：** 两变量皆空 → 不调用 `add_subdirectory`，不增加 link，与今日一致。

### 3.3 备选：已构建的导入库（同 Feature 文档化，可后置切片）

若游戏仓坚持独立 configure：

| 变量 | 含义 |
|------|------|
| `MINENGINE_GAME_IMPORTED_LIB` | `.dll.a` / `.lib` 或 SHARED 导入库路径 |
| `MINENGINE_GAME_IMPORTED_DLL` | 运行时 DLL 路径（拷贝用） |
| `MINENGINE_GAME_INCLUDE_DIRS` | 可选头路径（Editor 一般不需 include 游戏） |

`add_library(minengine_game_imported SHARED IMPORTED)` + `target_link_libraries(Editor PRIVATE minengine_game_imported)`。

**MVP 优先做 DIR+TARGET**（与「一次 cmake 编通 Editor+游戏」的 Dev 心智一致）；Imported 作 S02/文档备选，避免阻塞。

### 3.4 游戏侧约定（最小）

| 要求 | MVP |
|------|-----|
| 产物 | `add_library(<name> SHARED …)`，目标名与 `MINENGINE_GAME_TARGET` 一致 |
| 链引擎 | 游戏若调用引擎 API，自行 `target_link_libraries(<name> PRIVATE minEngine)`；纯数据 DLL 可不链 |
| 初始化符号 | **不要求**。DLL 随进程加载即可 |
| 源码位置 | 留在外仓；引擎只 `add_subdirectory` 引用 |

### 3.5 显式 Startup 符号（S03，可选）

仅当实践证明「仅链接不够」（例如必须在 `Application::Initialize` 之后注册系统）时再做：

```cpp
// 约定（草案，S03 定稿）
extern "C" void MinEngine_GameModuleStartup(void); // 游戏提供
// Editor 在 Initialize 末尾调用；未链接游戏模块时不编译该调用
```

用 CMake `MINENGINE_GAME_DIR` 启用时定义 `MINENGINE_HAS_GAME_MODULE=1`，Editor 源里 `#if` 调用。  
**Draft 默认：S03 Deferred**，除非评审要求 MVP 带钩子。

### 3.6 Stub（引擎仓内）

路径建议：`minEngine/Examples/GameModuleStub/`

- `add_library(GameModuleStub SHARED stub.cpp)` — 导出一个平凡符号（如 `int MinEngine_GameModuleStub_Marker()`）防止空库被裁掉。
- 文档示例：
  ```text
  -DMINENGINE_GAME_DIR=.../Examples/GameModuleStub
  -DMINENGINE_GAME_TARGET=GameModuleStub
  ```
- **不**默认打开；仅验证与手册示例。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. CMake `GAME_DIR` + `GAME_TARGET` | 通用、可关、无玩法进 Core | 外仓 CMake 进引擎 build 树需纪律 | **选用（MVP）** |
| B. 仅 Imported lib | 外仓完全独立编 | 双 configure，路径易碎 | 文档化备选 |
| C. LoadLibrary 插件 | 热更友好 | 协议/反射/生命周期大；越界 | **Out → 后续 Feature** |
| D. 游戏源码拷进引擎 | 省事 | 违反哲学 | **拒绝** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 外仓 CMake 污染引擎 cache | 难复现 | `EXCLUDE_FROM_ALL`；文档要求游戏选项带前缀；stub 作 CI/本地 smoke |
| 游戏 DLL 不在 `bin` | 启动缺 DLL | 默认 `MINENGINE_GAME_COPY_RUNTIME=ON` |
| 双份 `minEngine` 链接 | 符号混乱 | 游戏与 Editor 都链 **同一** SHARED `minEngine` 目标（同一次 configure） |
| 反射 / 静态注册未跑 | 游戏类型不可见 | 文档说明依赖 DLL load + 静态构造；不够再开 S03 |
| 误开选项 | 默认编不过 | 默认关闭；缺 TARGET 时 FATAL 信息明确 |

---

## 6) 验收标准

- [x] 默认 configure：`Editor` / `.\scripts\verify.ps1` smoke **零回归**
- [x] 指向 `GameModuleStub`：Editor 链接成功；`bin` 含 stub DLL；MaximumEditor 可启动（`--version`）
- [x] Design/手册写清变量名、启用条件、与 LoadLibrary 的边界
- [x] 无任何 minCraft 路径写死在引擎 CMake 默认值里

---

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-21 | Draft：链接期可选游戏模块；DIR+TARGET MVP；Startup 符号 S03 Deferred |
| 2026-09-21 | S01–S02 Done：CMake 钩子 + GameModuleStub + COPY_RUNTIME；S03 仍 Deferred |
