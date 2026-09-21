# External / Extra Test Harness — Design Spec

## Meta
- **ID:** `TEST-F05`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-21
- **Related:**
  - [Implementation](./TEST-F05_EXTERNAL_TEST_HARNESS_IMPLEMENTATION.md)
  - [TEST_UNIFIED_DESIGN](./TEST_UNIFIED_DESIGN.md)（F01）
  - [ED-F20](../../Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)（独立；Dev 宿主）
  - 发现场景（只读）：minCraft `INFRA-F01` / `ENG-GAP-003`

## TL;DR

`minEngineTests` 的 suite 全在 `Tests/TestSuiteRegistration.cpp` 手工注册，外仓无法把领域测挂进同一 Runner（smoke/full/`MaximumEditor.exe test` 转发），只能自建 exe。本 Feature 提供 **可关闭的扩展点**：外仓测试源编进 `minEngineTests` 并注册 suite；关闭时零回归。另文档化「自建 exe 仍调 `TestRunner`」为备选路径（需导出 API 时再加薄切片）。

## Scope
- **In:**
  - 推荐路径 **A**：`MINENGINE_EXTRA_TEST_*` CMake + `RegisterExtraTestSuites` 扩展点
  - Suite ID 命名约定；额外 suite **默认不进 smoke**
  - 引擎仓内 stub extra suite 证明可挂
  - 外仓接入文档（不依赖具体游戏名）
- **Out:**
  - 强制所有游戏测进引擎 CI
  - 重写 Test 栈 / 换掉 doctest
  - 测试 DLL 热加载插件（选项 C）作为 MVP
  - 把外仓玩法源码搬进 `Tests/Suites/`

## Reader quick start
1. 本文件 — A/B 选型与契约
2. [Implementation](./TEST-F05_EXTERNAL_TEST_HARNESS_IMPLEMENTATION.md)
3. 代码入口（落地后）：`Tests/CMakeLists.txt`、`TestSuiteRegistration.cpp`、`Tests/Fixtures/ExtraTestStub/`（或等价）

---

## 1) 背景与目标

### Pain
- 游戏仓已能用引擎树里的 doctest **头文件**，但无法进入 `TestSuiteRegistry` / `test smoke|full|<id>`。
- 把测试 `.cpp` 拷进引擎违反「游戏不进引擎」。

### Success
```text
# 默认
minEngineTests test smoke   → 今日 suite 集，零回归

# 外接
cmake ... -DMINENGINE_EXTRA_TEST_SOURCES=... -DMINENGINE_EXTRA_TEST_LINK_LIBRARIES=...
minEngineTests test game.stub-extra   → 跑到外接 suite
```

---

## 2) 现状

| 项 | 状态 |
|----|------|
| Runner | `Runtime/Test/TestRunner` + `TestSuiteRegistry` + `ITestSuite` |
| 注册 | `EnsureTestSuitesRegistered()` → 巨型手工 `Register(...)` |
| 入口 | `minEngineTests.exe`；`MaximumEditor.exe test` → Forward |
| doctest | `DOCTEST_CONFIG_IMPLEMENT` 仅在 `TestMain.cpp` |
| API 导出 | `TestRunner` / Registry **无** `MINENGINE_API`（游戏自建 exe 难稳链） |

---

## 3) 方案

### 3.1 选型

| 选项 | 做法 | 优点 | 缺点 | 结论 |
|------|------|------|------|------|
| **A** | 额外 `.cpp` 编进 `minEngineTests` + 注册扩展点 | 同一 exe、同一 CLI、verify 可扩展 | 外仓路径进引擎 configure | **MVP 选用** |
| **B** | 导出 harness；游戏自建 exe | 外仓完全独立产物 | 需导出符号；双 exe 心智 | **文档化 + S03 可选做导出** |
| **C** | 测试 DLL + LoadLibrary 注册 | 最灵活 | 工作量大 | **后置** |

### 3.2 路径 A — CMake 契约

| 变量 | 含义 | 默认 |
|------|------|------|
| `MINENGINE_EXTRA_TEST_SOURCES` | 分号/列表：额外源文件（含注册 `.cpp` + suite `.cpp`） | 空 |
| `MINENGINE_EXTRA_TEST_INCLUDE_DIRS` | 额外 include | 空 |
| `MINENGINE_EXTRA_TEST_LINK_LIBRARIES` | 额外 link（通常为游戏 SHARED 目标或 IMPORTED） | 空 |
| `MINENGINE_EXTRA_TEST_DIR` | （可选便利）若设，则 `GLOB` 该目录下 `*.cpp` 追加进 SOURCES | 空 |

**启用：** `MINENGINE_EXTRA_TEST_SOURCES` 或 `MINENGINE_EXTRA_TEST_DIR` 非空 → 定义 `MINENGINE_ENABLE_EXTRA_TESTS=1`。

`Tests/CMakeLists.txt`：

```cmake
target_sources(minEngineTests PRIVATE ${MINENGINE_EXTRA_TEST_SOURCES})
# + includes / link_libraries when set
```

游戏侧若已用 `add_subdirectory` 进引擎（ED-F20），可直接把游戏 CMake 目标名放进 `MINENGINE_EXTRA_TEST_LINK_LIBRARIES`。

### 3.3 注册扩展点

引擎声明（头文件，宜放 `Tests/` 或 `Runtime/Test/`）：

```cpp
namespace minEngine
{
    // Defined by EXTRA sources when MINENGINE_ENABLE_EXTRA_TESTS=1.
    void RegisterExtraTestSuites();
}
```

`EnsureTestSuitesRegistered()`：

```text
RegisterAllTestSuites();
#if MINENGINE_ENABLE_EXTRA_TESTS
    RegisterExtraTestSuites();
#endif
```

外仓提供例如 `ExtraTestRegistration.cpp`：

```cpp
void minEngine::RegisterExtraTestSuites()
{
    TestSuiteRegistry::Get().Register(MySuite::Get());
}
```

**未启用宏时不声明依赖**，避免缺符号。

### 3.4 Suite ID 约定

| 规则 | 说明 |
|------|------|
| 前缀 | 外接 suite Id **必须**以 `game.` 开头（例：`game.voxel-world`） |
| Smoke | 外接 suite 的 `InSmoke` **默认 false**（避免拖垮 `verify.ps1`） |
| Full | 可由外仓设 `InFull=true`；或仅 `test game.*` 显式跑 |
| 撞名 | 注册时若 Id 已存在 → `Register` 应失败或日志 Error（实现时检查；今日 Registry 若无检查则本 Feature 补上） |

### 3.5 Stub（引擎仓内）

`Tests/Fixtures/ExtraTestStub/`（或 `Tests/Extra/Stub/`）：

- 一个 doctest suite + `RegisterExtraTestSuites` 实现，Id = `game.stub-extra`，`InSmoke=false`
- 文档：本地验证时把该目录源文件赋给 `MINENGINE_EXTRA_TEST_SOURCES`

### 3.6 路径 B（备选，S03）

1. 给 `TestRunner` / `TestSuiteRegistry` / `EnsureTestSuitesRegistered` 加 `MINENGINE_API`（或明确「测试用静态库」`minEngineTestHarness`）。
2. 游戏仓自建 `GameTests.exe`：`DOCTEST_CONFIG_IMPLEMENT` + 注册 + `TestRunner::Run`。
3. **不**替代路径 A；服务「外仓只想独立 CI」的用户。

MVP **可不实现 B**，只在 Design/手册写清现状限制与后续 S03。

---

## 4) 与 Editor test 转发

`MaximumEditor.exe test …` 仍 Forward 到同目录 `minEngineTests.exe`。外接编进 `minEngineTests` 后，转发 **自动** 覆盖外接 suite。无需改 Forward 逻辑。

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 外接进 smoke | verify 变慢/脆 | 约定 `InSmoke=false`；stub 遵守 |
| Id 撞名 | 静默覆盖或重复 | 注册重复检测 + `game.` 前缀 |
| 游戏 DLL 未拷到 bin | 测不到 | 与 ED-F20 COPY_RUNTIME 配合；或 EXTRA link 同 build |
| doctest 双 IMPLEMENT | 链接错误 | 外仓 suite **禁止** `DOCTEST_CONFIG_IMPLEMENT`；仅引擎 `TestMain` |
| 路径 A 绑引擎 configure | 外仓 CI 复杂 | 文档提供 B 作为后续；外仓可继续自建 exe 作过渡 |

---

## 6) 验收标准

- [x] 默认：`.\scripts\verify.ps1` / `minEngineTests test smoke` 零回归
- [x] 开启 stub extra：`minEngineTests test game.stub-extra` 通过
- [x] 文档：外仓如何设 CMake 变量、如何写 `RegisterExtraTestSuites`、Id 前缀、勿 IMPLEMENT doctest
- [x] 无写死 minCraft 路径

---

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-21 | Draft：选用 A；B 为 S03；C 后置 |
| 2026-09-21 | S01–S02 Done：EXTRA_* + `RegisterExtraTestSuites` + `game.stub-extra`；S03 仍 Deferred |
