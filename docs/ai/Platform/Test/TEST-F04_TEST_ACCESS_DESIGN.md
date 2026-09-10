# TestAccess\<T\> — Design Spec

## Meta
- **ID:** `TEST-F04`
- **Type:** Feature
- **Status:** Done（S00–S04）
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Related:** [TEST-F01](./TEST_UNIFIED_DESIGN.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)

## TL;DR

生产头里堆 `XxxTestScope` forward + `friend`，每加 suite 就改 Runtime → 大面积重编。改为：

1. **`Core` 自动带上** `Testing::TestAccess` 主模板前向声明（轻量一头文件）。
2. 被测类型 **只写一行** `friend class Testing::TestAccess<ThisType>;`，删掉所有 TestScope 前向/friend。
3. **`Tests/` 里按类型特化** `TestAccess<T>`，暴露 `SetInstance` 等私有入口；TestScope 改调特化 API。

新增 suite 不再碰生产头。

## Scope
- **In:**
  - Runtime：`Core/Testing/TestAccess.h`（仅主模板前向声明）并由 `Core.h` include。
  - 被测类型：单 friend 特化；删除 `*TestScope` / 临时测试 friend 列表。
  - Tests：`Tests/Access/`（或等价）下的 `TestAccess` 特化头；迁移现有 TestScope 调用点。
  - 热点迁移顺序见 §迁移。
- **Out:** 正式对外的「任意替换单例」产品 API（可另立 Feature）；gmock；改断言框架；把特化实现放进 Runtime。

## 背景与目标

### Pain
- `ObjectManager.h` / `SceneManager.h` / `PhysicsSystem.h` 等为每个 suite 前向声明 + friend。
- 新 feat 的 smoke 若要装子系统实例 → 改生产头 → include 链上 TU 全量重编。
- 读者扫生产头时看到一长串测试类型名，干扰「系统本身」的阅读。

### 成功标准
- 生产类型对测试只出现 **一行** friend（见下方契约）。
- 生产头 **零** `*TestScope` 前向声明。
- 新 suite 只改 `Tests/`；Runtime 头不变则不触发引擎侧重编连锁。
- `Core.h` 仅多一个「前向声明」头，不引入测试实现、不拉 STL 以外依赖。

---

## 方案

### 1) Core 侧：统一前向声明头（自动包含）

**路径（建议）：** `Runtime/Core/Testing/TestAccess.h`

```cpp
#pragma once

namespace minEngine::Testing
{
    /**
     * Test-only private access hook.
     * Production types friend a specialization: Testing::TestAccess<ThisType>.
     * Specializations live under Tests/ and must NOT be included by Runtime TUs.
     */
    template<typename T>
    class TestAccess;
}
```

**挂入 `Core.h`（靠后、与其它 Core 轻量头并列）：**

```cpp
#include "Testing/TestAccess.h"
```

**为什么放 Core、而不是各类型自己 forward：**
- 凡 `#include "Core.h"` 的类型头自动具备 `Testing::TestAccess` 名，无需再写 forward。
- 生产侧迁移成本降到「加一行 friend + 删一堆 TestScope」。
- 前向声明几乎零成本；**不**在 Core 里放特化、不 include 任何 `Tests/` 路径。

**阅读面：** 头文件用短注释标明「测试钩子 / 特化在 Tests」，避免被当成公共游戏 API。不在 handbook 首页展开；需要时链到本 Design。

### 2) 生产类型契约（被测类）

```cpp
class ObjectManager
{
private:
    friend class Testing::TestAccess<ObjectManager>; // 唯一测试 friend
    friend class Engine;                             // 保留既有 Engine friend

    static void SetInstance(ObjectManager* instance);
    // ...
};
```

规则：
| 做 | 不做 |
|----|------|
| `friend class Testing::TestAccess<ThisExactType>;` | `template<typename U> friend class Testing::TestAccess;`（全开） |
| 删除所有 `friend class XxxTestScope` | 在生产头 include 任何 `Tests/Access/...` |
| 删除仅为测试存在的 `class XxxTestScope;` 前向 | 把 `SetInstance` 改 public「图省事」且不改文档（另议正式 API） |

### 3) 测试侧：按类型特化（仅 Tests）

**建议布局：**

```text
minEngine/Tests/
  Access/
    ObjectManagerTestAccess.h      # template<> class TestAccess<ObjectManager>
    SceneManagerTestAccess.h
    PhysicsSystemTestAccess.h
    ...
  Suites/
    PhysicsSmokeTest.cpp         # include Access 头；TestScope 调 TestAccess
```

**特化示例：**

```cpp
// Tests/Access/ObjectManagerTestAccess.h
#pragma once

#include "Runtime/Core/Object/ObjectManager.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<ObjectManager>
    {
    public:
        static void SetInstance(ObjectManager* instance)
        {
            ObjectManager::SetInstance(instance);
        }
    };
}
```

**TestScope 调用（迁移后）：**

```cpp
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "Access/PhysicsSystemTestAccess.h"

class PhysicsSmokeTestScope
{
public:
    PhysicsSmokeTestScope()
    {
        Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
        m_ObjectManager.Initialize();

        Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
        m_SceneManager.Initialize();

        Testing::TestAccess<PhysicsSystem>::SetInstance(&m_PhysicsSystem);
        m_PhysicsSystem.Initialize();
    }

    ~PhysicsSmokeTestScope()
    {
        m_SceneManager.Shutdown();
        Testing::TestAccess<SceneManager>::SetInstance(nullptr);

        m_PhysicsSystem.Shutdown();
        Testing::TestAccess<PhysicsSystem>::SetInstance(nullptr);

        m_ObjectManager.Shutdown();
        Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
    }
    // members unchanged
};
```

可选薄封装（非必须）：`InstallObjectManager(ObjectManager*)` 自由函数包一层，仍实现在 Tests。

### 4) 约束（必须）
1. Friend **特化**，不 friend 主模板全开。
2. 特化实现 **只住 `Tests/`**；生产 TU / Runtime 公共头禁止 include。
3. TestScope 可保留；只改调用点，不再进 friend 列表。
4. 入口尽量窄：优先 `SetInstance`；不为方便掏整类 private 字段（白盒需在特化头写清理由）。
5. `Core.h` 只引入前向声明头，不引入测试宏、不依赖 `ME_WITH_TESTS`。

---

## 迁移指南（重点）

迁移按 **「类型一次改干净」** 推进：某个 `T` 一旦接入 `TestAccess<T>`，其头文件里所有测试 friend/前向应同 PR 删光，并改完所有调用 `T::SetInstance` 的 TestScope。

### 总流程（每个类型重复）

```text
1. 若尚未有 Core/Testing/TestAccess.h → 先做 S00（仅一次）
2. 新增 Tests/Access/<Type>TestAccess.h（特化 + 所需 static 包装）
3. 改所有调用 Type::私有入口 的 TestScope → TestAccess<Type>::…
4. 生产头：加 friend TestAccess<Type>；删 *TestScope 前向与 friend
5. 编译 minEngineTests；跑相关 suite
6. 确认生产头再无 TestScope 字样（rg）
```

### S00 — 基础设施（先做、可单独合）

| 步骤 | 动作 | 验收 |
|------|------|------|
| 1 | 新增 `Runtime/Core/Testing/TestAccess.h`（仅主模板前向 + 短注释） | 单独可编译 |
| 2 | `Core.h` `#include "Testing/TestAccess.h"` | 现有目标仍编过（预期几乎无逻辑变化） |
| 3 | （可选）在 `Tests/Access/README.md` 写一行「特化放这里」 | — |

此时 **尚未** 删任何 friend；行为不变。

### 迁移一个生产类型（清单）

以 `ObjectManager` 为例：

#### A. 盘点
```text
rg "ObjectManager::SetInstance|friend class .*ObjectManager|ObjectManagerTestScope|friend class .*TestScope" minEngine/
```
记录：
- 哪些 TestScope / suite 调用了 `ObjectManager::SetInstance`
- `ObjectManager.h` 里有哪些测试相关 forward / friend

#### B. 写特化头
- 路径：`Tests/Access/ObjectManagerTestAccess.h`
- 内容：`template<> class TestAccess<ObjectManager>`，至少包装 `SetInstance`
- 若还有其它 private 仅测试用入口，一并列出；**不要**预留「以后可能用」的字段访问

#### C. 改测试 TU
对每个相关 suite（例如）：
- `ObjectManagerTest.cpp`
- `SerializationArchiveTest.cpp`
- `DelegateTest.cpp`
- `MaterialIRTest.cpp`
- `AssetManagerTest.cpp` / `LuaScriptMvpTest.cpp`（若碰 OM）
- 所有 Physics* / SceneClone / Command / Audio smoke 等装 OM 的 Scope

替换模式：
```cpp
// before
ObjectManager::SetInstance(&m_Manager);
ObjectManager::SetInstance(nullptr);

// after
Testing::TestAccess<ObjectManager>::SetInstance(&m_Manager);
Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
```
并 `#include "Access/ObjectManagerTestAccess.h"`（路径以 CMake include 根为准）。

#### D. 改生产头（同一切片内）
`ObjectManager.h`：
1. 删除所有 `class XxxTestScope;` / `MaterialIRTestObjectManagerScope` 等 **仅为 friend 存在的** 前向（保留 `Engine` 等真实依赖）。
2. 删除所有 `friend class XxxTestScope`（及同类测试 friend）。
3. 增加：`friend class Testing::TestAccess<ObjectManager>;`
4. `SetInstance` 保持 private。

#### E. 验证
```text
cmake --build … --target minEngineTests
minEngineTests.exe test object-manager   # 或实际 suite-id
# 以及仍使用 OM Scope 的 smoke / physics-* / scene-clone / …
rg "friend class .*TestScope|class \w+TestScope;" ObjectManager.h   # 应无命中
```

#### F. 完成定义（该类型）
- [ ] 生产头无 TestScope 前向/friend
- [ ] 所有 `Type::SetInstance` 测试调用已改为 `TestAccess<Type>`
- [ ] 相关 suite PASS

### 迁移一个「多系统」TestScope

如 `PhysicsSmokeTestScope` 同时装 OM + SceneManager + PhysicsSystem：

1. **先**保证三个类型都已有特化头（可分 PR，但 Scope 改完前三者 friend 迁移要齐，否则会出现「OM 已删 friend、Physics 未迁」的编译窗）。
2. 实用策略：**同一切片内**迁完 Scope 依赖的全部类型，或 Scope 暂保留对未迁类型的旧调用（仅当该类型仍 friend 该 Scope —— 过渡期尽量短）。
3. 推荐切片边界：S01 只迁 OM 且只改 **仅依赖 OM** 的 Scope；多系统 Scope 放到 S02 与 Scene/Physics 一起改。

### 当前库存（迁移地图，实施时再核对）

| 生产类型 | 典型私有入口 | 已知测试 friend / 调用方（示意） |
|----------|--------------|----------------------------------|
| `ObjectManager` | `SetInstance` | 几乎所有装引擎子集的 Scope；`ObjectManagerTest` 等 |
| `SceneManager` | `SetInstance` | Physics*、SceneClone、Command、Audio、Lua… |
| `PhysicsSystem` | `SetInstance` | PhysicsSmoke/Sync/Load/Contact/LineTrace/Shapes |
| `AssetManager` | `SetInstance` | AssetManagerTest、LuaScriptMvp |
| `AudioSystem` | `SetInstance` | AudioSmokeTest |
| （其它） | 视 `rg SetInstance` / friend | 按需列入 S03+ |

实施 PR 应用 `rg` 刷新此表；上表是起点不是冻结契约。

### 切片计划

| Slice | 内容 | 退出标准 |
|-------|------|----------|
| **S00** | `TestAccess.h` + `Core.h` include | 全量编过；无行为变化 |
| **S01** | `ObjectManager` + 仅 OM 的 Scope/suite | OM 头无 TestScope；相关 suite PASS |
| **S02** | `SceneManager` + `PhysicsSystem` + 多系统 Physics/Scene Scope | 两头无 TestScope；physics-* / scene-clone PASS |
| **S03** | `AssetManager` / `AudioSystem` / 其余 | 对应 smoke PASS |
| **S04** | 清理 `SceneManager` 等「temporarily public for testing」半公开成员（若仍存在） | 测试仍只经 `TestAccess` 或正规 public API |

### 回滚 / 过渡
- 禁止长期「一半 friend TestScope、一半 TestAccess」挂在同一类型上；单个类型必须原子切完。
- 若某 PR 过大：按 **类型** 拆 PR，不要按「先改测试、生产头下一周再删 friend」拆（易漏改导致链接期才爆，或误留永久 friend）。

---

## 备选
| 方案 | 说明 |
|------|------|
| 每系统非模板 `XxxAccess` | 等价；本 Feature 用模板统一命名 |
| 不经 Core、各头自行 forward `TestAccess` | 可行但重复、易漏；已否决 |
| `#if ME_WITH_TESTS` 公开 `SetInstance` | 更直；发行配置要分清；可作后续 |
| 正式 `InstallInstance` API | 长期机制；不挡本 Feature |

## 风险
- Friend 必须对准 **特化**；特化与前向声明同属 `minEngine::Testing`。
- 特化头被 include 进 Runtime → 编译耦合回流 — Review 拒绝。
- `Core.h` 变重：本头仅为前向声明；禁止在此叠加测试工具实现。
- 多系统 Scope 跨切片迁移时的短暂双轨 — 用切片表约束，避免跨周双轨。

## 验收（Feature Done）
- [x] `Core.h` 包含 `Testing/TestAccess.h`；该头无特化体
- [x] 热点子系统（OM / SceneManager / PhysicsSystem / AssetManager / AudioSystem）friend 列表无 `*TestScope`
- [x] 对应 Tests 均经 `TestAccess<T>` 安装实例（含 Audio `InitializeWithBackend`）
- [x] 新增假想 suite 无需改 Runtime 头即可装 `ObjectManager`
- [x] 相关 smoke / physics-* / scene-clone / command / audio / asset / serialization / delegates PASS
- [x] S04：`SceneManager` / `Scene` temporarily-public 字段收回 private；`Scene::SetSceneName` 正规 API

## 开码前
- Pre-flight：确认无进行中的大改与这些头冲突；S00 可先合。
- 实现时另开 Implementation Plan 勾选切片（可选；本 Design §迁移已含操作级步骤）。
