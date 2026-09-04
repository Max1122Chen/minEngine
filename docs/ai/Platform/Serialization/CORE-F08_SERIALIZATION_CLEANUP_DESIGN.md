# CORE-F08 — Serialization System Cleanup & Optimization

## Meta
- **ID:** `CORE-F08`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/core`（自 `master`）
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../../ACTIVE_WORK.md) · [Implementation](./CORE-F08_SERIALIZATION_CLEANUP_IMPLEMENTATION.md) · [SERIALIZATION_BINARY_AND_PROPERTY_API.md](./SERIALIZATION_BINARY_AND_PROPERTY_API.md) · **Precedes** [CORE-F09](./CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md)
- **Depends on:** Reflection `MEClass` / `StaticClass()`（已有）
- **Decisions (2026-09-04):** 删除 `allowObjectPtrSerialization` 与其它死代码；**TD-026 本期不做**；范围 = 体检 **P0 + P1**
- **Verified:** `serialization-archive` · `scene-clone` · `verify.ps1` smoke · Editor build

## TL;DR

在 **不改 Binary wire**（留给 `CORE-F09`）的前提下，整理 `Serializer` 公开 API 与实现：根对象入口改为 **`MEClass*` / `StaticClass()`**；去掉无用选项与死状态；统一 `MEObject::StaticClass()` 与 `ForEachPropertyInHierarchy(MEClass*)`；合并重复的 property 查找；Deserialize 侧用 `static_cast` 对齐 Serialize。为 `CORE-F09` 的 ClassId 路径铺平 API，但不在本期引入 wire 层 ClassId。

## Scope

### In（P0 + P1）

| 优先级 | 项 | 说明 |
|--------|-----|------|
| **P0** | 根对象 `MEClass*` API | `Serialize` / `Deserialize` / `ToFile` / `FromFile` / `*ObjectToBuffer` / `*ObjectToJson` 增加 `const MEClass*` 重载；模板便捷 `Serialize<T>` 等 |
| **P0** | 调用点迁移 | 已有 `MEClass*` 的热路径（Editor snapshot、PIE Json clone、测试）改走新 API；磁盘 Loader 可先用 `T::StaticClass()` |
| **P0** | 删除死代码 | 删除 `SerializerOptions::allowObjectPtrSerialization` 及所有赋值；删除 `m_IsHandlingPtr` / `SetHandlingPtr` / `IsHandlingPtr`；清理过时 `// TODO: support object pointer later` |
| **P1** | `ForEachPropertyInHierarchy` 直传 | Serializer 内不再 `classInfo->GetName()` → `FindClass` 回环 |
| **P1** | `MEObject::StaticClass()` | 替换 `FindClass("minEngine::MEObject")` / `"MEObject"` 双查找 |
| **P1** | 合并重复查找 | `FindPropertyInHierarchy` 与 `WalkToOwningObjectByPath` 内嵌 local 合并为一处 |
| **P1** | Deserialize cast 对齐 | category 已保证时用 `static_cast`，去掉多余 `dynamic_cast` |
| **P1** | 文档同步 | 本 Feature 文档；`SERIALIZATION_BINARY_*` / Play Mode / Command History 中对已删选项的引用改为「已移除」 |

### Out

| 项 | 原因 |
|----|------|
| Binary wire / TD-028 / TD-029 | → `CORE-F09` |
| **TD-026**（`m_Owner` / Setter） | 维护者延后：与反射 Setter/Getter 统一验证一并做 |
| Legacy `Serializer_Legacy` 删除 | 仍有 Reflection bridge；另开任务 |
| `.mescene` / `.memtl` 改 Binary | 非本期 |
| `fieldPath` lazy 分配 / Binary `reserve` | P2；有 profiling 再开 |
| `Serializer.cpp` 物理拆文件 | 可选 follow-up |
| Enum property 序列化扩展 | 另开任务 |
| wire 层 ClassId / FieldId | `CORE-F09` |

## Reader quick start
1. 本文件 — 方案与边界（尤其 §3 API）
2. [Implementation Plan](./CORE-F08_SERIALIZATION_CLEANUP_IMPLEMENTATION.md) — 切片与验收
3. 代码：`Runtime/Core/Serialization/Serializer.*`、`SerializationTypes.h`

---

## 0) Pre-flight（2026-09-04）

| 项 | 结论 |
|----|------|
| 现状 | 根 API 全是 `string` + `FindClass`；Property API 已是 `MEClass*`；ObjectPtr 已实现但选项/注释仍像「未完成」 |
| 债务 | `allowObjectPtrSerialization` **从未被 Serializer 读取**；`m_IsHandlingPtr` **零调用** |
| 风险 | **low** — 行为应保持；删除无效选项不改变现有序列化结果 |
| 与 Primary | 不挡 `ANIM-F01`；`feat/core` 并行 |
| 建议 | **Go** — 先 Design 定稿 API，再按 S01→S04 开码 |

---

## 1) 背景与目标

### Pain
- 调用方已有 `MEClass*` 仍要 `GetName()` → `FindClass`，类型不安全且多余。
- `SerializerOptions` / 静态状态含死字段，误导调用方以为能 gate ObjectPtr。
- 同类查找与 cast 风格分裂，增加维护成本。

### Goals
- **类型安全入口：** 热路径用 `MEClass*` / `StaticClass()`。
- **诚实 API：** 选项只保留真正生效的字段。
- **实现干净：** 少一次查找、少一层 RTTI、少重复 helper。
- **不破坏磁盘格式：** JSON `.mescene` 等语义不变。

### Success
- 公开根 API 以 `MEClass*` 为主；`string` 仅为薄兼容（内部一次 `FindClass`）。
- grep：`allowObjectPtrSerialization`、`m_IsHandlingPtr`、过时 ObjectPtr TODO → **零**（代码与本 Feature 相关 docs）。
- `SerializationArchiveTest` + `verify.ps1` 通过；Editor Undo / PIE clone 行为目视或现有测试不回归。

---

## 2) 现状（代码锚点）

| 区域 | 行为 | 位置 |
|------|------|------|
| 根 Serialize/Deserialize | `string` → `FindClass` | `Serializer.cpp` ~165–201 |
| Property API | 已 `MEClass*` | `Serializer.h` 公开 overload |
| Object 迭代 | `ForEachPropertyInHierarchy(classInfo->GetName())` | Serialize/Deserialize IterateProps |
| MEObject 检查 | Serialize 用 `StaticClass`；Deserialize 用字符串 FindClass | `DeserializeObjectPtr` |
| 无效选项 | `allowObjectPtrSerialization` 十余处赋值，Serializer 不读 | `SerializationTypes.h` + Loaders/Editor/AssetManager |
| 死状态 | `m_IsHandlingPtr` | `Serializer.h` private |
| 重复查找 | `FindPropertyInHierarchy` vs path walk local | `Serializer.cpp` |
| 已知债（本期不做） | raw ptr assign 绕过 `SetOwner` | TD-026 |

**保持不变：**
- Property 为唯一编解码路径；Json / Binary 共用 `Serializer`。
- ObjectPtr：`null` / GuidRef / Instanced 内联语义。
- Binary wire framing（含 TD-028 脆弱性）— 不修。

---

## 3) 方案

### 3.1 数据流 / 模块边界

```text
Caller (Editor / Loader / Test)
        │
        ▼
  Serialize(MEClass*, obj, archive)     ← 新主路径
  Serialize(string, …) → FindClass → ↑  ← 薄兼容
        │
        ▼
  SerializeObjectInstance(MEClass*, …)
        │
  ForEachPropertyInHierarchy(MEClass*)  ← 不再经 name
        │
  SerializeProperty / ObjectPtr / Array
        │
  WriterArchive (Json | Binary v1 不变)
```

本 Feature **只改 Serializer 侧与调用点**；不改 `BinaryArchive` wire；`JsonArchive` 仅在文档/选项引用处顺带清理。

### 3.2 公开 API 契约

#### 新增（主路径）

```cpp
// 概念签名（实现时与现有命名对齐）
static SerializeResult Serialize(const Reflection::MEClass* rootClass,
                                 const void* rootObject,
                                 WriterArchive& archive,
                                 const SerializerOptions& options = {});

static SerializeResult Deserialize(const Reflection::MEClass* rootClass,
                                   void* outRootObject,
                                   ReaderArchive& archive,
                                   std::vector<PendingObjectRef>& outUnresolvedRefs,
                                   const SerializerOptions& options = {});

// ToFile / FromFile / SerializeObjectToBuffer / DeserializeObjectFromBuffer /
// SerializeObjectToJson / DeserializeObjectFromJson — 同样增加 MEClass* 重载

template <typename T>
static SerializeResult Serialize(const T* rootObject, WriterArchive& archive,
                                 const SerializerOptions& options = {})
{
    return Serialize(T::StaticClass(), rootObject, archive, options);
}
// Deserialize / *ToBuffer / *ToJson 对称模板（T 须有 StaticClass）
```

**约束：**
- `rootClass == nullptr` → Failure。
- 模板要求 `T::StaticClass()` 可用（反射类型）；非反射类型不提供模板。
- `string` 重载保留：内部 `FindClass`，失败信息仍带 class name；**新代码优先 MEClass***。

#### 删除

| 符号 | 动作 |
|------|------|
| `SerializerOptions::allowObjectPtrSerialization` | 删除字段；清理所有 designated-init / 赋值 |
| `Serializer::m_IsHandlingPtr` 及 Get/Set | 删除 |
| 过时 ObjectPtr TODO 注释 | 删除 |

#### `SerializerOptions` 保留字段

| 字段 | 说明 |
|------|------|
| `enumAsString` | 继续生效 |
| `strictTypeCheck` | 继续生效（Archive 侧） |
| `skipUnknownField` | 继续生效 |
| `writeObjectTypeName` | 继续生效 |

#### 内部整理（P1）

1. `SerializeObject_IterateProps` / `DeserializeObject_IterateProps` / `FindPropertyInHierarchy` / path walk → 一律 `ForEachPropertyInHierarchy(const MEClass*)`。
2. `DeserializeObjectPtr`：`MEObject::StaticClass()` 替代双字符串查找。
3. `FindPropertyInHierarchy` 单一实现；path walk 复用。
4. `DeserializeProperty` category 分支：`static_cast` 对齐 Serialize（category 错误仍 Failure）。

### 3.3 调用点迁移策略

| 类别 | 策略 |
|------|------|
| **已有 MEClass***（SceneEditor snapshot capture） | 直接传 `rootClass`；snapshot 仍可存 `rootClassName` 字符串供 restore 时 `FindClass`（或 restore 后也走 MEClass*） |
| **已知具体类型**（`Scene`、`Material` 等 Loader） | `Scene::StaticClass()` / `Material::StaticClass()` |
| **仅有类型名字符串**（极少） | 可暂留 `string` 重载 |
| **测试** | 优先改 `MEClass*` / 模板，覆盖新 API |

### 3.4 与 CORE-F09 的衔接

- F08 **不**引入 stable ClassId。
- F08 保证热路径已持有 `MEClass*`，F09 在 wire 上编码 ClassId 时可直接从 `MEClass*` 取 id，避免再发明一套字符串入口。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 只加 MEClass*，保留 string | 迁移平滑 | 短期双 API | **选用**（string = 薄兼容） |
| B. 立刻删除全部 string 根 API | 最干净 | 调用点改动面大、一次 PR 风险高 | 拒绝本期 |
| C. 恢复 `allowObjectPtrSerialization` 真 gate | 选项名诚实 | 无明确需求；改行为 | **拒绝**（YAGNI，删除） |
| D. 本期顺带修 TD-026 | 语义更正 | 与 Setter 统一验证耦合 | **拒绝**（维护者延后） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 漏删选项赋值导致编译失败 | 构建红 | S01 先删字段，全仓 grep 修编译 |
| 模板 `StaticClass` 用于非反射类型 | 编译错误 | 仅文档约定；不 SFINAE 过度设计 |
| `static_cast` 掩盖错误 category | 错误更难发现 | category switch 外仍 Failure；与 Serialize 一致 |
| 文档残留 `allowObjectPtrSerialization` | 误导 | S04 扫 docs/ai 相关引用 |

---

## 6) 验收标准

- [x] 根对象公开 API 存在 `MEClass*` 重载；至少一条测试走新路径
- [x] 热路径调用点（SceneEditor snapshot、SceneDuplicator）不再「GetName → Serialize(string)」绕圈
- [x] `allowObjectPtrSerialization` / `m_IsHandlingPtr` 代码清零；相关 docs 已注明移除
- [x] Serializer 内 property 迭代不经 `GetName()`→`FindClass` 回环
- [x] `DeserializeObjectPtr` 使用 `MEObject::StaticClass()`
- [x] `minEngineTests`：`serialization-archive`、`scene-clone` 通过
- [x] `.\scripts\verify.ps1` 通过
- [x] **不**修改 Binary wire；**不**改 TD-026 行为

---

## 7) Status note

（Done）

| 字段 | 内容 |
|------|------|
| Deferred in Feature | TD-026；P2 path/Binary 微优化；Legacy 删除 |
| Unblock TD-026 | 反射 Setter/Getter 统一验证会话 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 登记；体检 Scope 初稿 |
| 2026-09-04 | 拍板：删死代码；TD-026 延后；范围 = P0+P1；补全 Design §0–§7 |
| 2026-09-04 | **Done** — S01–S04 落地；`feat/core` |
