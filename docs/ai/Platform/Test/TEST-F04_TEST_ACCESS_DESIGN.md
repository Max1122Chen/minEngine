# TestAccess\<T\> — Design Spec

## Meta
- **ID:** `TEST-F04`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Related:** [TEST-F01](./TEST_UNIFIED_DESIGN.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)

## TL;DR

生产子系统头文件堆满 `XxxTestScope` forward + `friend`，每加 suite 就改 Runtime 头 → 大面积重编。改为：**每个被测类型只 `friend Testing::TestAccess<T>`**；由 `TestAccess<T>` 特化在测试侧暴露 `SetInstance` 等私有入口。新增 suite 不再碰生产头。

## Scope
- **In:** `Testing::TestAccess<T>` 主模板前向声明（Runtime 可见、体为空或不可实例化）；按需 `template<> class TestAccess<ObjectManager>` 等特化（仅 Tests）；迁移热点：`ObjectManager` / `SceneManager` / `PhysicsSystem` / `AssetManager` / `AudioSystem`；删除各 TestScope friend 列表。
- **Out:** 把生产 API 永久改成「随意可换单例」而不经命名约束；gmock；改测试断言框架。

## 背景与目标

### Pain
- `ObjectManager.h` / `SceneManager.h` 等为每个 suite 前向声明 + friend。
- 新增 feat 的 smoke 测试若要装子系统实例 → 改生产头 → 依赖该头的 TU 全量重编。

### 成功标准
- 生产头对测试只出现 **一次** friend：`friend class Testing::TestAccess<ThisType>;`
- 新 suite 只改 `Tests/`；Runtime 头不动则不触发引擎侧重编连锁。

## 方案（推荐）

### 形状
```cpp
// Runtime（轻量）：仅前向声明
namespace minEngine::Testing {
template<typename T>
class TestAccess;
}

class ObjectManager {
    friend class Testing::TestAccess<ObjectManager>;
    static void SetInstance(ObjectManager* instance);
    // ...
};

// Tests（特化）：只被测试 TU include
namespace minEngine::Testing {
template<>
class TestAccess<ObjectManager> {
public:
    static void SetInstance(ObjectManager* instance)
    {
        ObjectManager::SetInstance(instance);
    }
};
}
```

### 约束（必须）
1. **Friend 特化，不 friend 主模板全开** — 避免 `TestAccess<任意>` 意外拿到私有权。
2. **特化实现只住在 `Tests/`**（或明确仅测试链接的库）— 生产 TU 不 include 特化头。
3. **TestScope 继续存在**，但只调用 `TestAccess<T>::…`，不再进 friend 列表。
4. 仅暴露测试真正需要的入口（常见：`SetInstance`）；不为「方便」大面积掏 `private` 字段，除非白盒用例有明确理由。

### 迁移顺序（建议切片）
| Slice | 内容 |
|-------|------|
| S01 | `TestAccess` 前向声明约定 + `ObjectManager` 迁完并删其 TestScope friends |
| S02 | `SceneManager` + `PhysicsSystem` |
| S03 | `AssetManager` / `AudioSystem` / 其余 |
| S04 | 清理 `SceneManager` 中 `// temporarily public for testing` 一类半公开状态（若仍存在） |

## 备选
| 方案 | 说明 |
|------|------|
| 每系统非模板 `XxxAccess` | 等价，少一点模板语法；本 Feature 选模板以统一命名 |
| `#if ME_WITH_TESTS` 公开 `SetInstance` | 更直；发行配置要分清 |
| 正式 `InstallInstance` API | 长期机制；可与 F04 并存，不挡本 Feature |

## 风险
- C++ friend 只认**特化**声明与定义一致；特化必须与前向声明同命名空间。
- 若有人把特化头 include 进 Runtime → 又把测试耦合拉回生产 — Review 时拒绝。

## 验收（Feature Done 时）
- [ ] 热点子系统 friend 列表不再含 `*TestScope`
- [ ] 新增一个假想 suite 无需改 Runtime 头即可装 `ObjectManager` 实例
- [ ] 相关 smoke / physics / scene-clone / command 等 suite PASS
