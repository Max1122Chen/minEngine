# 序列化扩展 — Binary Archive 与 Property 粒度 API

Last updated: 2026-05-24  
Status: **S1–S2 已实现（`--serialization-archive-test`）**  
父文档：[Platform 路线图](../PLATFORM_ROADMAP.md)  
关联：[Editor Command / Undo](../../Editor/EDITOR_COMMAND_HISTORY.md)（E1.3+ 依赖本能力）、`Runtime/Core/Serialization/`

---

## 0) 一句话

在现有 **`WriterArchive` / `ReaderArchive` + `Serializer`（按 `MEProperty` 遍历）** 之上，新增 **`BinaryArchive`**（内存字节流）与 **公开的 Property 级序列化 API**；全量对象序列化 **复用** `SerializeProperty`，Json 与 Binary **共用同一套逻辑**。磁盘资产格式 **暂不强制** 改 Binary。

---

## 1) 背景与动机

| 需求 | 现状 | 目标 |
|------|------|------|
| Inspector / Undo 快照 | 无统一 property 字节块；设计曾考虑手写 JSON | 小、快、可逆的 **property blob** |
| 栈上大量 JSON | `JsonArchive` 分配与文本体积大 | **Binary** 内存缓冲 |
| 单属性 vs 全对象 | `SerializeProperty` 已存在但为 **private** | 对外 `SerializeProperty` / `DeserializeProperty` |
| 与存盘一致 | Scene/Material 已走 Archive 管线 | 同一反射规则，避免第二套 |

**非目标（本阶段）：**

- 将 `.mescene` / `.memtl` 默认格式改为 Binary（可后续单独里程碑）。
- 替换或删除 `Legacy/Serializer_Legacy.h`（仅标注弃用方向）。
- 网络同步、增量 diff、压缩（zlib）— 可预留扩展位。

---

## 2) 现状（代码锚点）

```text
WriterArchive / ReaderArchive     ← 抽象接口（Archive.h）
JsonWriterArchive / JsonReaderArchive
Serializer::Serialize / Deserialize
  └─ SerializeObject_IterateProps
       └─ SerializeProperty (private)   ← Primitive / Object / ObjectPtr / Array
PrimitiveCodecRegistry              ← bool/int/float/string/Vector2–4 等
PendingObjectRef + ResolvePendingObjectRefs   ← GUID 引用二次解析
```

**ObjectPtr 语义（已实现，Binary 须对齐）：**

- `nullptr` → `WriteNull`
- 非 `Instanced` 或 outer 不匹配 → **`BeginGuidRef(GUID)`**（JSON 形如 `{"$guid":{"high","low"}}`）
- `Instanced` 且 `GetOuter() == owner` → **`BeginObjectPtr` + 内联子对象字段**

**Array 语义：**

- `BeginArray(count)` → 对每个元素递归 `SerializeProperty(inner)`
- 元素可为 `WriteNull`（空元素指针）

**Legacy：** `Serializer_Legacy.h` 基于旧 `TypeInfo::fields`，与新 `MEProperty` 路径 **并行**；新功能 **只扩展 Archive + Serializer**。

---

## 3) 目标架构

```text
                    SerializerOptions
                           │
         ┌─────────────────┴─────────────────┐
         ▼                                   ▼
  SerializeProperty(owner, name, archive)   Serialize(rootClass, rootPtr, archive)
         │                                   │
         └───────────┬───────────────────────┘
                     ▼
            SerializeProperty (MEPropertyCategory 分派)
                     │
       ┌─────────────┼─────────────┬──────────────┐
       ▼             ▼             ▼              ▼
  PrimitiveCodec   Object      ObjectPtr        Array
                     │             │              │
                     └─────────────┴──────────────┘
                                   ▼
                          WriterArchive / ReaderArchive
                     ┌────────────┴────────────┐
                     ▼                         ▼
              JsonWriterArchive          BinaryWriterArchive
              (磁盘 / 调试)               (Undo / 内存快照)
```

**原则：** Property 是 **唯一** 编解码路径；全量 = 对 hierarchy 上每个 field 调一次 Property API（与今天 `SerializeObject_IterateProps` 一致）。

---

## 4) 公开 API（拟定）

### 4.1 Property 级

```cpp
namespace minEngine::Serialization
{
    // 按 owner 对象 + 属性名解析 MEProperty（含 class hierarchy）
    SerializeResult SerializeProperty(
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        const std::string& propertyName,
        WriterArchive& archive,
        const SerializerOptions& options = {});

    SerializeResult DeserializeProperty(
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        const std::string& propertyName,
        ReaderArchive& archive,
        std::vector<PendingObjectRef>& outUnresolvedRefs,
        const SerializerOptions& options = {});
}
```

**Property-only blob 约定：** _archive 根上 **不写** `BeginObject` 外壳，直接写 **该 property 的值节点**（与全量对象里 `BeginField` 之后的 payload 相同）。便于 Undo 存「一个字段」而不带类型名包装。

可选便捷：

```cpp
SerializeResult SerializePropertyToBuffer(..., std::vector<uint8_t>& out);
SerializeResult DeserializePropertyFromBuffer(..., const std::vector<uint8_t>& in, ...);
```

内部：`BinaryWriterArchive` + `TakeBuffer()`。

### 4.2 SerializerOptions 扩展（拟定）

| 字段 | 用途 |
|------|------|
| `enumAsString` | Json：字符串；Binary：建议 **定长 int32 枚举值**（更快） |
| `skipTransient` | 全量遍历时常用；Property API 由调用方指定单个 property |
| `allowObjectPtrSerialization` | 是否允许写出 GUID 引用（Editor Undo 对资产引用字段可能要 **拒绝** 或仅 Guid） |
| `resolveRefsOnDeserialize` | Undo 场景：false → 只恢复 Guid/内联，不 `ResolvePendingObjectRefs` |

---

## 5) BinaryArchive — 格式与实现要点

### 5.1 容器

- 写入：`std::vector<uint8_t>` 追加；提供 `span` / `TakeBuffer()`。
- 读取：`const uint8_t*` + size；严格边界检查，防止 Undo 损坏栈。

### 5.2 文件 / 流头（若将来写盘）

| 字段 | 说明 |
|------|------|
| Magic | 4 字节，如 `ME\x01BIN` |
| Version | `uint16_t` schema 版本 |
| Flags | 保留（压缩、endian 标记） |

**内存-only Undo blob：** 可省略文件头，或使用 **1 字节 sub-version** 前缀，便于以后升级。

### 5.3 字节序与可移植性

- **默认 little-endian**（与 x64/Windows 主目标一致），与 `GUID.High/Low` 写入顺序一致。
- 若 `Flags` 标明 big-endian，Reader 交换（低优先级，先 LE only 亦可，文档写死）。

### 5.4 与 Archive 接口的映射（逻辑类型）

Archive 接口表达 **逻辑类型**；Binary 后端负责 **物理编码**：

| Archive 调用 | Binary 编码建议 |
|--------------|-----------------|
| `WriteNull` | `Tag::Null` (1 byte) |
| `WriteBool` | `Tag::Bool` + `uint8` 0/1 |
| `WriteInt64` / `WriteUInt64` / `WriteDouble` | `Tag` + 8 字节 LE |
| `WriteString` | `Tag::String` + `uint32 length` + **UTF-8 字节**（无 NUL） |
| `BeginArray(count)` | `Tag::Array` + `uint32 count` + 连续元素 |
| `BeginGuidRef` | `Tag::GuidRef` + **16 字节** GUID（High LE + Low LE） |
| `BeginObject` / `BeginObjectPtr` | `Tag::Object` + **类型名**（见下）+ 字段序列 |
| `BeginField` / `EndField` | Binary：**字段名** 可省略（Property API 单字段）；全量对象：**`uint32 nameLen` + name** + payload |

**类型名（Object / ObjectPtr 内联）：** `uint16 nameLen` + UTF-8 class name（与 JSON `BeginObject(expectedTypeName)` 对齐）。

### 5.5 实现注意（Object → 字节）

1. **不要** 对 C++ 对象做 `memcpy` 整块结构体（padding、指针、版本漂移都会炸）。
2. **只** 通过反射 + Archive 写 **逻辑值**；与 JSON 路径一致。
3. **版本**：Binary schema `version` 升级时，Reader 对未知 `Tag` 失败并返回 `SerializeResult::Failure`。
4. **循环引用**：全量场景图可能通过 Guid 打破环；**单 property 内联 Object** 若嵌套过深，设 **最大深度**（如 32）防栈溢出。
5. **严格类型**：`strictTypeCheck` 时 `BeginObject` 的 typeName 必须与 `MEClass` 一致。
6. **Owner 生命周期**：Undo blob 反序列化时 owner 必须仍存活；GUID 定位由 Command 层保证。
7. **浮点**：默认 IEEE754 double；若需确定性可文档约定 `-ffast-math` 风险（一般 Undo 可接受）。
8. **与 GC**：反序列化 `ObjectPtr` 产生 `PendingObjectRef`；Undo **在同一 Scene 上下文** 内应能 `Resolve`；跨 Scene Load 不适用同一 blob。

---

## 6) 特殊类型处理

### 6.1 `string` / `std::string`

| 项 | 约定 |
|----|------|
| 编码 | UTF-8，**不带** NUL 终止符 |
| 长度 | `uint32` 字节数；**0 = 空串**（与 `WriteNull` 区分：Null 表示「无值/指针空」；空串是合法 string） |
| 超大串 | 可选上限（如 16MB）防恶意/误用 Undo 撑爆内存 |
| Json 对齐 | 继续用 JSON string；Binary 不走 Json 转义 |

**Inspector Undo：** 字符串属性（含 Rename 若走 property）直接适用。

### 6.2 Array（`MEArrayProperty`，含 `std::vector` 反射容器）

| 项 | 约定 |
|----|------|
| 布局 | `BeginArray(count)`：**count 写入流**（Json 当前 write 忽略 count，Binary **必须写**） |
| 元素 | 按 index 顺序递归 `SerializeProperty(inner)` |
| 空元素 | 保持现有 `WriteNull` 语义 |
| 读回 | `Resize(out, count)` 后逐元素 `DeserializeProperty` |
| 变长 | Undo 一次编辑若只改 `count` 或单元素，仍整 property 一条 blob（E2 TryMerge 再优化） |
| 嵌套 | `vector<vector<...>>`、元素为 Object → 递归同一套规则 |

**注意：** 反序列化 **整数组覆盖** owner 上该字段；Undo 前后应是完整数组快照，不做「只 diff 一个下标」除非以后另做 Delta API。

### 6.3 Ref（`MEObjectPtrProperty` — 资产引用 / 子对象指针）

三种形态，**Binary 与 JSON 语义一致**：

```text
1) null          → WriteNull
2) GuidRef       → BeginGuidRef(guid)     // 引用已有 MEObject
3) Instanced     → BeginObjectPtr + 内联子对象全部字段
```

| 场景 | 序列化 | 反序列化 / Undo |
|------|--------|------------------|
| 指向场景中已有对象（如 Material 资产） | **GuidRef** | `PendingObjectRef` → `ObjectManager::FindObject`；失败则 **保持 null 或保留旧值**（策略在 `SerializerOptions` 拍板） |
| `Instanced` 子对象（outer == owner） | **内联** 完整子树 | 重建或覆盖子对象；可能需分配新 `MEObject`（E1.4 Snapshot 细案） |
| Undo 单 property、字段为 Texture 引用 | 通常 **仅 Guid** 即可 | 不要求 blob 内嵌资产内容 |
| `allowObjectPtrSerialization == false` | 写 GuidRef 还是报错？ | 建议：**Editor Undo 默认 true（Guid only）**；内联仅 Instanced |

**禁止：** 在 GuidRef 形态下写入 **裸指针地址**（进程内地址重开无效）。

**GUID 为零：** 序列化前若对象无 Guid，现有逻辑会 `GenerateGUID()` 并写盘 — Undo blob 是否允许分配新 Guid 需拍板（建议：**仅当 Instanced 内联** 时生成；纯引用且 Guid 无效则失败并打 log）。

### 6.4 嵌套 `Object`（非指针，值对象）

- 递归 `SerializeObjectInstance`（无 field 名外壳时由 Property API 直接进入子对象 payload）。
- Binary 子对象：`Tag::Object` + typeName + 子字段序列（与 ObjectPtr 内联相同编码，区分 Tag 便于 Reader）。

### 6.5 Primitive / `Vector2–4`

- 继续走 **`PrimitiveCodecRegistry`**，不在 Binary 重复实现 float 逻辑。
- Vector 在 JSON 为 **length-2/3/4 数组**；Binary 可用 **`Tag::Array` + fixed count** 或专用 `Tag::Vector3`（实现简单选 Array + count 校验）。

---

## 7) JsonArchive 与 BinaryArchive 差异摘要

| 能力 | JsonArchive | BinaryArchive |
|------|-------------|---------------|
| 人类可读 | ✅ | ❌ |
| 字段名 | 对象内必有 | 全量有；**单 property blob 可无** |
| Array count | Write 可忽略 | **必须写入** |
| GuidRef | `$guid` 对象 | 16 字节固定 |
| 调试 | 直接打开文件 | 提供 `HexDump` / 可选 `Binary→Json` 转换工具（仅 Debug） |

---

## 8) 实施分期

| 阶段 | 内容 | 验收 |
|------|------|------|
| **S1** | `BinaryWriterArchive` / `BinaryReaderArchive`；Primitive + String + Array + GuidRef + Object 字段 | **完成** — `Editor.exe --serialization-archive-test` |
| **S2** | 公开 `SerializeProperty` / `DeserializeProperty` + `*ToBuffer` / `*FromBuffer` | **完成**（Vector3 反射类未注册时测试跳过） |
| **S3** | `SerializerOptions` 扩展；Editor E1.3 接线 | 待做 |

**建议顺序：** S1 → S2 → S3（Editor 不阻塞 S1 单测）。

---

## 9) 与 Editor Undo 的关系

| Editor 阶段 | 依赖本设计 |
|-------------|------------|
| E1.3 Inspector 属性 | `SerializePropertyToBuffer` × 2（before/after） |
| E1.4 Snapshot | 全 Component / GameObject = 根对象 `Serialize` + BinaryArchive |
| E1.5 Material | 图对象同样 Archive；可先 Json 调试再 Binary |

更新 [EDITOR_COMMAND_HISTORY.md](../../Editor/EDITOR_COMMAND_HISTORY.md)：**E1.3 实现前完成 S1+S2**（或 S1+property API 最小子集）。

---

## 10) 风险与拍板项

| # | 问题 | 建议 |
|---|------|------|
| 1 | Undo 反序列化 Guid 找不到 | 保留旧值 + `ME_CORE_WARN`；不静默写 null |
| 2 | Instanced 子对象 Undo | E1.3 仅 primitive；ObjectPtr/内联 Object **E1.4** |
| 3 | `Transient` / `Invisible` 过滤 | S2 在 `IterateProps` 统一跳过；与 Inspector 一致 |
| 4 | Binary 是否进磁盘 | 本阶段 **否** |
| 5 | Legacy Serializer | 新代码禁止依赖；逐步迁移 |

---

## 11) 测试策略

- **Round-trip：** 每种 `MEPropertyCategory` × 代表类型，Json 与 Binary 各测 Write→Read→memcmp 逻辑值。
- **GuidRef：** 注册对象 → 序列化 → 清空指针 → 反序列化 → `ResolvePendingObjectRefs` → 指针恢复。
- **Array：** 空数组、含 null 元素、嵌套 vector。
- **回归：** 现有 `SceneLoader` / `MaterialLoader` Json 路径 CI 不变。

---

## 12) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-24 | 初稿：BinaryArchive、Property API、string/array/ref 约定、分期 S1–S3 |
| 2026-05-24 | S1–S2 落地：`BinaryArchive.*`、`Serializer` Property API、`--serialization-archive-test` |
