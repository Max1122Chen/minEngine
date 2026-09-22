# Game Module Always Links Engine — Design Spec

## Meta
- **ID:** `ED-F21`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [Implementation](./ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_IMPLEMENTATION.md)
  - [ED-F20](./ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)（Editor 链游戏）
  - [TEST-F05](../Platform/Test/TEST-F05_EXTERNAL_TEST_HARNESS_DESIGN.md)（外仓测进 harness）
  - [CORE-F27](../Platform/Core/CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md)
  - 发现场景（只读）：minCraft `ENG-GAP-006-B`（双构建 / 复制 AABB）
- **Branch:** `master`

## TL;DR

外仓为「不链引擎也能跑领域 CPU 测」拆出双构建与重复数学类型，根因是缺少**文档化的 lightweight consumer 链接契约**。本 Feature **不搬玩法进 Core**；推荐「游戏 SHARED **始终** `target_link_libraries(... minEngine)`」，与 ED-F20 同一次 configure 共用同一 `minEngine` 目标；独立纯 CPU 测走 TEST-F05 或同 DLL，而不是复制引擎类型。完整 `install`/`export` SDK 若未就绪则文档写清过渡方案，不另起大工程。

## Scope
- **In:**
  - 文档化至少一种稳定方式：外仓 TOPLEVEL 或作为 ED-F20 子目录时如何拿到公开头 + 链接 minEngine
  - 明确推荐：游戏模块始终链接 minEngine；反对复制 AABB / 宏拆两套实现
  - （可选热修）若短期无 SDK：写清「应作为 ED-F20 子目录构建；ctest 经引擎 build 的游戏目标 / TEST-F05」
- **Out:**
  - 完整 CMake `install`/`export` 大工程（若未来另开 SDK Feature，勿与本 Feature 重复造）
  - 修改 minCraft 仓源码
  - LoadLibrary 热插拔

## Reader quick start
1. 本文件 — 推荐契约
2. Implementation — 切片（多半文档 + stub CMake 示例）
3. ED-F20 — Editor 侧如何 `add_subdirectory` 游戏

---

## 1) 背景与目标

### Pain
- 游戏仓独立 cmake「只测 mesher」→ 不链引擎 → 自研 `SectionMeshBounds` ≈ AABB、`#if WITH_ENGINE`。
- 引擎侧已有 ED-F20（Editor←游戏）与 TEST-F05（测←外仓源），但缺少对称的「游戏←引擎」**官方写法**。

### Success
维护者打开文档即可按一种推荐路径配置，使游戏目标：
- 编译期可见引擎公开头（含 `Math::AABB`、`PrimitiveComponent` 等）
- 链接到**同一次构建树中的** `minEngine` SHARED（或文档化的 IMPORTED 目标）
- 不再需要为「有无引擎」维护两套几何/组件实现

---

## 2) 现状

| 项 | 状态 |
|----|------|
| ED-F20 | 引擎 `add_subdirectory(GAME_DIR)`；游戏应 `PRIVATE minEngine`（Design §3.4 已写，但未强调「始终」） |
| GameModuleStub | **不**链 minEngine（仅 Marker）— 弱化了「始终链接」示范 |
| TEST-F05 | 外仓测进 `minEngineTests`；适合领域测，不必第二套无引擎 exe |
| install/export | 仓库内**无**现成 ENG-GAP-004 / SDK 前缀约定 |

---

## 3) 方案

### 3.1 推荐默认（过渡期，可立刻落地）

**唯一推荐 Dev 路径：**

```text
引擎 configure：
  -DMINENGINE_GAME_DIR=<game-cmake-root>
  -DMINENGINE_GAME_TARGET=<GameSharedTarget>

游戏 CMakeLists（被 add_subdirectory）：
  add_library(<Game> SHARED ...)
  target_link_libraries(<Game> PRIVATE minEngine)   # 始终；不要 OPTIONAL
  # 领域 CPU 测：TEST-F05 把测源编进 minEngineTests，或链接同一 <Game>/minEngine
```

- **同一 configure** → 同一 `minEngine` 目标 → 无双份 DLL、无复制 AABB。
- 独立「纯 CPU」若坚持不启动渲染：仍链接 minEngine（头+符号），测只调用数学/mesher API；或把测放进 TEST-F05。

### 3.2 备选：已构建引擎的 IMPORTED（文档化，可后置切片）

当游戏仓坚持 **独立 TOPLEVEL cmake**（不经引擎 `add_subdirectory`）：

| 变量（草案） | 含义 |
|--------------|------|
| `MINENGINE_ROOT` 或 `MINENGINE_BUILD_DIR` | 已配置的引擎源/构建树 |
| 或 `MINENGINE_SDK_DIR` | 未来 install 前缀（本期可不实现） |

文档示例：

```cmake
add_library(minEngine SHARED IMPORTED)
set_target_properties(minEngine PROPERTIES
  IMPORTED_LOCATION ".../bin/libminEngine.dll"      # 按平台调整
  IMPORTED_IMPLIB   ".../libminEngine.dll.a"
  INTERFACE_INCLUDE_DIRECTORIES ".../minEngine/src;.../minEngine/include")
target_link_libraries(MyGame PRIVATE minEngine)
```

**本期：** 以文档 + 示例片段为主；**不**强制落地完整 `find_package(minEngine)`。

### 3.3 Stub 示范升级（小切片）

将 `Examples/GameModuleStub` 改为 `PRIVATE minEngine`（即便只用公开头做一个 `#include` 或空 Prim），作为「始终链接」活样例。与 CORE-F27 / ED-F22 可共用同一 stub 扩展，注意默认 CI 仍关游戏开关。

### 3.4 明确反对

| 做法 | 态度 |
|------|------|
| 游戏仓复制引擎 AABB/变换类型 | **反对** |
| `#if WITH_ENGINE` 拆两套 Component 实现 | **反对**（过渡期仅文档迁移提示） |
| 为躲链接而维护第二套 cmake「无引擎」生产路径 | **反对**；测试用 TEST-F05 |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 文档强化 ED-F20 子目录 + 始终 link | 零大工程；立刻减摩擦 | TOPLEVEL 独立编仍靠纪律 | **选用（MVP）** |
| B. 完整 install/export SDK | 真·消费者包 | 范围大；缺现成缺口单 | **后置另 Feature** |
| C. 引擎提供 header-only math 包 | 纯 CPU 测轻 | 分裂组件头依赖；鼓励双路径 | **拒绝** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 游戏独立 build 链错另一份 minEngine | ODR / 崩溃 | 文档强调同一次 configure 或精确 IMPORTED 路径 |
| Stub 链引擎后默认开 stub 变慢 | CI | 默认仍关 `MINENGINE_GAME_*` |
| 与 CORE-F27 交错 | 范围膨胀 | B 以文档为主；stub 链引擎可放 S02 |

---

## 6) 验收标准

- [x] 文档写清推荐路径（ED-F20 子目录 + 始终 `target_link_libraries(... minEngine)`）
- [x] 文档写清 TEST-F05 作为领域测首选，而非复制类型
- [x] （可选）IMPORTED / `MINENGINE_SDK_DIR` 草案表 — Design §3.2 已有；完整 SDK Deferred
- [x] GameModuleStub 示范链接 minEngine
- [x] 无完整 install 大工程
- [x] 默认 verify smoke 不挂

---

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
| 2026-09-22 | Done：stub 始终链引擎；BOOTSTRAP 契约 |
