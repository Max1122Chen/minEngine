# Jolt Physics Bootstrap — Implementation Plan

## Meta
- **ID:** `PHYS-F01`
- **Type:** Implementation Plan
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-06-12
- **Related:** [Design](./PHYS-F01_JOLT_INTEGRATION_DESIGN.md), [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md)

## TL;DR

在 `physics` 分支分 **3 个逻辑切片（S01–S03）** 落地；S01 再拆 **4 个 landable 子步（S01-a–d）**。**S01 bootstrap Done**（2026-06-12）；下一切片：**S02**（碰撞层 + Contact）。`RigidBodyComponent` 为 **Component 物理代理**（P10），读写 GO RootComponent Transform，非 SceneComponent。每步可独立 `cmake --build`；S01-d 起挂接 `LogicalTick` 与 headless 落体测试。**刻意不碰** RHI、RenderPipeline、Editor 物理 UI。

## Scope
- **In:** `Runtime/Function/Physics/`、`Engine` 生命周期与 `LogicalTick`、`minEngineTests` physics suite、Jolt submodule、CMake
- **Out:** S02/S03 之前的碰撞通道与 LineTrace 代码；Editor Gizmo；Playground 目视（可选 bonus）

## Reader quick start
1. [Design Spec](./PHYS-F01_JOLT_INTEGRATION_DESIGN.md) — 架构、Tick、P1–P9
2. 下表 — 切片顺序与状态
3. `PROGRESS_LOG.md` — 已落地记录

---

## 1) 切片总览

| Slice ID | 标题 | 状态 | 验证 |
|----------|------|------|------|
| PHYS-F01-S01-a | Jolt submodule + CMake | **Done** | `cmake --build minEngine/build --target minEngine` |
| PHYS-F01-S01-b | `PhysicsSystem` / `PhysicsWorld` 空壳 + 生命周期 | **Done** | 编译；headless 启停不崩 |
| PHYS-F01-S01-c | `RigidBodyComponent` + `BoxColliderComponent` | **Done** | codegen + 编译 |
| PHYS-F01-S01-d | `LogicalTick` 挂接 + 落体 smoke | **Done** | `minEngineTests.exe test physics-smoke` |
| PHYS-F01-S02 | 碰撞层 + Contact Begin/End | Planned | headless contact 测试 |
| PHYS-F01-S03 | `LineTrace` | Planned | headless ray hit 测试 |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

**建议 PR 边界：**
- PR1：S01-a + S01-b
- PR2：S01-c + S01-d
- PR3：S02
- PR4：S03

---

## 2) 依赖关系

```text
S01-a → S01-b → S01-c → S01-d
                              ↘
S01-d → S02 → S03
```

`CORE-F01` 代码已 land（`physics` 分支）；S01 可与 CORE-F01-S06（registry/文档收尾）并行。

---

## 3) 切片详情

### PHYS-F01-S01-a — Jolt submodule + CMake

#### 目标
将 Jolt 以 git submodule 引入，CMake `add_subdirectory` 链接进 `minEngine` 共享库；**尚无** Runtime 物理代码。

#### 任务
- [x] 添加 `minEngine/minEngine/Third-Party/Jolt` submodule（上游 [jrouwe/JoltPhysics](https://github.com/jrouwe/JoltPhysics)）
- [x] `minEngine/minEngine/CMakeLists.txt`：`add_subdirectory(Jolt/Build)`，`target_link_libraries(minEngine PRIVATE Jolt)`
- [x] Jolt 编译选项：`INTERPROCEDURAL_OPTIMIZATION OFF`；`ENABLE_OBJECT_STREAM` / debug renderer / profiler off；嵌套 `add_subdirectory` 不构建 Samples/UnitTests
- [x] 根 `minEngine/CMakeLists.txt`：`cmake_minimum_required` 升至 **3.20**（Jolt 要求）
- [ ] worktree 若 submodule gitdir 异常：运行 `scripts/fix-worktree-submodule-gitdirs.ps1`（本 worktree 未遇到）

#### 触及文件
- `.gitmodules`
- `minEngine/minEngine/CMakeLists.txt`
- `Third-Party/Jolt/`（submodule）

#### 刻意不碰
- `Engine.cpp`、组件、测试

#### 验收
```powershell
cd D:\Dev\GitRepo\minEngine-physics\minEngine
cmake --build build --target minEngine
```

#### 风险
MinGW + Jolt 首次配置失败 → 本切片单独 land，不叠加业务代码。

---

### PHYS-F01-S01-b — PhysicsSystem / PhysicsWorld 空壳

#### 目标
引擎单例 `PhysicsSystem` 与 per-Scene `PhysicsWorld`；Jolt `RegisterTypes`、临时分配器、空 `Step`；`Scene` 切换时 create/destroy world。

#### 任务
- [x] 新建目录 `Runtime/Function/Physics/`
- [x] `PhysicsTypes.h` — `PhysicsBodyId`、`EBodyType`（Static/Dynamic/Kinematic 预留）等，**无 Jolt include**
- [x] `PhysicsConversion.h/.cpp` — 轴映射 position/quaternion（basis matrix）
- [x] `PhysicsWorld.h/.cpp` — `JPH::PhysicsSystem`、固定步长 accumulator、`Step`；`Sync*` 空实现
- [x] `PhysicsSystem.h/.cpp` — `Initialize`/`Shutdown`、`GetOrCreateWorld`、`DestroyWorld`、`SimulateActiveScene`
- [x] `Engine::StartSystems` / `ShutdownSystems` 注册 `PhysicsSystem`（Scene 之后 init，Scene 之后 shutdown）
- [x] `SceneManager`：`CreateNewScene` / `LoadSceneByPath` → `GetOrCreateWorld`；`UnloadActiveScene` → `DestroyWorld`

#### 触及文件
- `Runtime/Function/Physics/*`
- `Runtime/Engine.cpp`、`Engine.h`
- `SceneManager.cpp` 或 `Scene.cpp`（world 生命周期）

#### 刻意不碰
- 组件、`LogicalTick` 仿真调用

#### 验收
```powershell
cmake --build minEngine/build --target minEngine minEngineTests
minEngine\bin\minEngineTests.exe test smoke
```

---

### PHYS-F01-S01-c — RigidBodyComponent + BoxColliderComponent

#### 目标
反射组件类型；`RigidBodyComponent` 作物理代理（P10）；与 `PhysicsWorld` 创建/销毁 body + box shape；**尚未**要求落体正确。

#### 任务
- [x] `RigidBodyComponent` — 继承 **`Component`**（P10）；`RefreshPhysicsBody(boxColliderOverride)` 处理组件添加顺序
- [x] `BoxColliderComponent` — 继承 `Component`；`Vector3 HalfExtent`
- [x] 组件生命周期：`PhysicsWorld::RegisterRigidBody` / `UnregisterRigidBody`
- [x] Body 初始 pose 从 **RootComponent** Transform 读取（经 `PhysicsConversion`）
- [x] reflection codegen（`RigidBodyComponent` / `BoxColliderComponent` / `EBodyType`）
- [x] `PhysicsConversion` position/quaternion（落体测试间接验证）
- [x] **修复** `GameObject::AddComponent_Internal`：仅 `SceneComponent` 参与 Root/Attach（非 Scene 组件如刚体不再误 cast）

#### 组件装配约定（S01，见 Design §3.2）
- GO **RootComponent** = `SceneComponent`（或 `StaticMeshComponent` 等）— Transform 真源
- 同 GO：`RigidBodyComponent` + `BoxColliderComponent`（均非 Root）
- 测试 spawn：Static 地板 + Dynamic 盒子（S01-d 代码内创建，不必改 `.mescene`）

#### 触及文件
- `RigidBodyComponent.h/.cpp`
- `BoxColliderComponent.h/.cpp`
- `PhysicsWorld.cpp`（CreateBody/DestroyBody）
- `Generated/Reflection/*.gen.*`

#### 刻意不碰
- `Engine::LogicalTick` 仿真
- Editor Inspector 定制（用反射默认 UI 即可）

#### 验收
- 编译通过
- 手动或单元测试：创建 GO + 组件后 `PhysicsWorld` body 数量 > 0

---

### PHYS-F01-S01-d — LogicalTick 挂接 + 落体 smoke

#### 目标
完整 bootstrap 垂直切片：固定步长仿真、动态体 pull 写回、headless 测试。

#### 任务
- [x] `Engine::LogicalTick`：`SimulateActiveScene` 插在 Tick 与 `SendAllEndOfFrameUpdates` 之间
- [x] `PhysicsWorld::Step` — `1/60 s` accumulator，max 4 substeps
- [x] `PhysicsWorld::SyncBodiesToScene` — Dynamic pull → RootComponent
- [x] 默认重力 `(0, -9.81, 0)` 引擎空间
- [x] 新测试 suite `physics-smoke`：
  - 创建 Scene；每个 GO：`SceneComponent` Root + `RigidBodyComponent` + `BoxColliderComponent`
  - Static 地面 + Dynamic box 初始高度 `h0 = 10`（高度设在 **RootComponent**）
  - 仿真 `N * (1/60)` 秒（如 N=60）
  - 断言 Root `Y < h0` 且 `Y > 0`（未穿透地板）
- [x] 注册 suite：`TestSuiteRegistration.cpp`（`InFull` only；`test physics-smoke` 显式跑）

#### 触及文件
- `Engine.cpp`
- `PhysicsWorld.cpp`
- `Tests/Suites/PhysicsSmokeTest.cpp`（新）
- `Tests/TestSuiteRegistration.cpp`

#### 刻意不碰
- Editor、Playground（可选 bonus）

#### 验收
```powershell
cmake --build minEngine/build --target minEngineTests
minEngine\bin\minEngineTests.exe test physics-smoke
.\scripts\verify.ps1
```

#### S01 Done 检查（Doc DoD）
- [x] Design §7 S01 勾选项
- [x] Registry / ACTIVE_WORK / PROGRESS_LOG 更新
- [x] 非 `Physics/` 无 Jolt include（仅 `PhysicsWorld.cpp` / `PhysicsSystem.cpp`）

---

### PHYS-F01-S02 — 碰撞层 + Contact 事件

#### 目标
`ECollisionChannel`（Default / WorldStatic / Trigger）；Contact Begin/End 双缓冲事件；Trigger 不产生物理响应。

#### 任务
- [ ] `PhysicsTypes.h` — channel / response 枚举
- [ ] Jolt `ObjectLayer` / `BroadPhaseLayer` 映射
- [ ] `PhysicsWorld` contact listener → 帧末双缓冲 `TArray` 式事件列表（或 `std::vector` + swap）
- [ ] headless：两 dynamic 或 dynamic+static 碰撞；trigger overlap Begin/End 断言

#### 验收
`minEngineTests.exe test physics-contact`（或扩展现有 physics suite）

---

### PHYS-F01-S03 — LineTrace

#### 目标
`PhysicsWorld::LineTrace(Start, End, QueryParams, HitResult&)` 引擎空间 API。

#### 任务
- [ ] Jolt `NarrowPhaseQuery::CastRay`
- [ ] `FHitResult` 最小字段：`bBlockingHit`、`Location`、`Normal`、`Component`（或 GO 指针）
- [ ] headless：已知几何下 hit/miss 断言

#### 验收
`minEngineTests.exe test physics-linetrace`

---

## 4) 延后 / 取消

| Slice ID | Reason | Unblock | Next check |
|----------|--------|---------|------------|
| — | — | — | — |

---

## 5) 文件布局（计划）

```text
Runtime/Function/Physics/
  PhysicsSystem.h/.cpp      # 单例 facade
  PhysicsWorld.h/.cpp       # per-Scene，唯一 include Jolt 的业务类
  PhysicsTypes.h            # 对外类型，无 Jolt
  PhysicsConversion.h/.cpp  # 坐标转换
  RigidBodyComponent.h/.cpp
  BoxColliderComponent.h/.cpp
  # S02+: PhysicsContactEvent.h, CollisionChannels.h
```

---

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-11 | 初稿：S01-a–d + S02/S03 切片与 PR 边界 |
| 2026-06-12 | P10：`RigidBodyComponent` 改为 Component 代理 Root Transform |
| 2026-06-12 | S01-a Done：Jolt submodule + CMake link（cmake 3.20；`INTERPROCEDURAL_OPTIMIZATION OFF`） |
| 2026-06-12 | S01-b Done：`PhysicsSystem` / `PhysicsWorld` 空壳 + Scene 生命周期 |
| 2026-06-12 | **S01 Done**：组件 + LogicalTick + `physics-smoke`；`GameObject` attach 修复 |
