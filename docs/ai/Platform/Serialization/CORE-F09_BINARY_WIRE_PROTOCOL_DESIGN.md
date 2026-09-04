# CORE-F09 — Binary Wire Protocol v2（Transient）

## Meta
- **ID:** `CORE-F09`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/core`
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../../ACTIVE_WORK.md) · [CORE-F08](./CORE-F08_SERIALIZATION_CLEANUP_DESIGN.md) · [SERIALIZATION_BINARY_AND_PROPERTY_API.md](./SERIALIZATION_BINARY_AND_PROPERTY_API.md) · [CORE-F10](./CORE-F10_JSON_DISK_COMPAT_DESIGN.md)（JSON 存盘宽松策略） · [Implementation](./CORE-F09_BINARY_WIRE_PROTOCOL_IMPLEMENTATION.md)
- **Tech debt:** TD-028 / TD-029 → **Done**
- **Depends on:** `CORE-F08` **Done**

## TL;DR

用 **无歧义 framing**（`fieldCount` + `bodyLength`）+ **进程内 dense ClassId/FieldId** 重做 **内存暂态 Binary（v2）**，替换不可靠的 v1 `EndObject` 字段流；严格解析；关闭 TD-028/029（PIE 改回 Binary）。**一套 wire 骨架**预留「存盘 = 名字键 + 宽松跳过」契约，**本期不实现存盘 Binary**。不读 v1。磁盘 `.mescene` 仍 JSON（宽松策略见 `CORE-F10`）。

## Scope

### In（本期实现）
- Binary **SchemaVersion = 2** wire 规范（header、object、field、value tags）
- 反射 Finalize 时分配 **dense ClassId / FieldId** + **SchemaFingerprint**
- 重写 / 替换 `BinaryWriterArchive` / `BinaryReaderArchive` 为 v2（或并行后端后删 v1）
- Serializer 暂态路径走 Id 键；**严格**错误策略
- 测试：现有 archive 测试 + **TD-028 复现**多 GO physics-mesh Scene round-trip
- `SceneDuplicator` 恢复 Binary（关 TD-029）
- 文档：本 Spec + 短 Implementation Plan

### Out
- 读 Binary v1 blob
- 存盘 Binary / `.mescene` 改 Binary（契约见 §3.2 Persistent binding）
- UE 式 `ClassCastFlags` 当 ClassId（澄清见 §3.3）
- TD-026、GC、网络复制、压缩、增量 diff
- JSON 存盘宽松 / `$schemaVersion`（→ **CORE-F10**）

## Reader quick start
1. §2 现状与 TD-028；§3 双 binding 与错误策略（拍板）
2. §4 wire 布局与数据结构；§5 接口与数据流
3. §6 案例；§7 风险；§8 验收
4. [Implementation Plan](./CORE-F09_BINARY_WIRE_PROTOCOL_IMPLEMENTATION.md)

---

## 0) Pre-flight（2026-09-04）

| 项 | 结论 |
|----|------|
| 依赖 | F08 已提供 `MEClass*` 热路径 |
| 根因 | v1 `EndObject=0x0A` 与 u16 字段名长度低字节冲突；启发式不可靠 |
| 范围 | 只做 Transient Binary v2；Persistent 写契约不实现 |
| 风险 | Medium — BinaryArchive 大改；须用 TD-028 场景回归 |
| 建议 | **Go** — Design 定稿后按切片实现 |

### 拍板摘要（维护者确认）

| # | 决定 |
|---|------|
| 1 | Framing：`fieldCount` + `bodyLength` |
| 2 | 暂态：类型/字段用 **dense 整型 Id**；存盘稳态（未来）：**类型名/字段名** |
| 3 | 暂态解析 **严格**；存盘（未来）**可跳过未知 + Warn**，framing 损坏 **Fail** |
| 4 | **不读 v1** |
| 5 | Header 含 **SchemaVersion**（+ SchemaFingerprint） |
| 6 | ClassId ≠ UE CastFlags；CastFlags 另议，不进 wire |
| 7 | 一套 framing，两套 identity/容错；F09 只实现 Transient |

---

## 1) 背景与目标

### Pain
- Undo / PIE 需要快、可靠的内存 blob；v1 Binary 在真实 Scene 上 round-trip 失败。
- PIE 被迫 JSON（TD-029），成本高且掩盖协议债。
- 字段名字符串进 wire 既慢又放大 framing 歧义面。

### Goals
- **正确优先：** 无启发式 End；边界可用 `bodyLength` 校验。
- **暂态性能：** ClassId/FieldId 替代 UTF-8 名。
- **严格产品头：** SchemaVersion + Fingerprint，错代即拒。
- **可演进：** 同一骨架将来支持 Persistent（名键 + 宽松），不推倒重来。

### Success
- TD-028 场景 Binary round-trip 通过；PIE 用 Binary；`verify` / 相关测试绿。

---

## 2) 现状（v1）

```text
Object: Tag + (u16 nameLen + typeName) + [u16 fieldNameLen + name + taggedValue]* + EndObject(0x0A)
```

`EndObject` 与长度前缀低字节冲突 → `IsObjectFieldStreamEnd` 启发式失败（TD-028）。

**保持不变：** Serializer 仍是唯一逻辑路径；Json / Binary 仍是 Archive 后端；ObjectPtr = null | GuidRef | Instanced 内联。

---

## 3) 方案总览

### 3.1 一套 framing，两套 binding

```text
┌──────────────────────────────────────────────┐
│  Wire framing（v2，共用）                      │
│  SchemaVersion · fieldCount · bodyLength     │
│  value tags · 定长 / length-delimited payload  │
└──────────────────────────────────────────────┘
           │                         │
           ▼                         ▼
   Transient binding            Persistent binding（契约 / 后做）
   ClassId + FieldId            typeName + fieldName
   SchemaFingerprint            可选 schema 表
   严格 Fail                    未知可 Skip+Warn
```

### 3.2 Identity 与容错契约

| | **Transient（本期）** | **Persistent（未来；F09 不实现）** |
|--|----------------------|-----------------------------------|
| 用途 | Undo、PIE clone、内存快照 | 将来磁盘 Binary / Prefab 候选 |
| 类键 | dense `ClassId`（Finalize 分配） | 反射类型全名字符串 |
| 字段键 | dense `FieldId`（类内 Finalize 分配） | `MEProperty::GetName()` |
| Header | `SchemaVersion` + `SchemaFingerprint` | `SchemaVersion` + 可选兼容策略标志 |
| 未知字段/类型 | **Failure** | **Skip + Warn**（framing 合法时） |
| 缺字段 | **Failure**（默认；见验收） | **保留默认值**（不 Fail） |
| 已知键、类型 tag 不符 | **Failure** | **Skip 该字段 + Warn**（默认拍板） |
| framing 损坏（越界/坏 tag/body 长度不符） | **Failure** | **Failure** |
| 同名字段重复 | **Failure** | **Failure** 或 Last-wins+Warn（实现时再定） |

> JSON 存盘路径的宽松策略与 Persistent 对齐，由 **CORE-F10** 落地（仍用 JsonArchive）。

### 3.3 ClassId 分配（非 CastFlags）

**UE `EClassCastFlags`：** 有限 bitmask，加速常见 `IsA`/`Cast`，**不是**唯一类身份，**不能**当 wire ClassId。

**Transient ClassId：**

```text
ReflectionSystem::FinalizeReflection（或其后一次性 pass）:
  for each registered MEClass in stable deterministic order:
    assign ClassId = ++counter  (0 = invalid)
    ClassId → MEClass* , MEClass* → ClassId
  for each class, for each non-Transient property in hierarchy walk order:
    assign FieldId = ++perClassCounter  (0 = invalid)
  SchemaFingerprint = hash(SchemaVersion, all (ClassId, className, FieldId, fieldName, valueTypeTag)…)
```

- 「稳定」= **同进程、同一次 Finalize 内**稳定；跨编译靠 Fingerprint 拒绝旧 blob。  
- 继承判定继续 `MEClass::IsA` / Super 链；与 ClassId 无关。  
- 显式 `meta(FieldId=N)`：**本期不要求**；落盘 Binary 再考虑。

### 3.4 SchemaVersion vs SchemaFingerprint

| 字段 | 何时变 | 作用 |
|------|--------|------|
| `SchemaVersion` | framing / tag 表 / header 布局变更 | 协议代数（v2、v3…） |
| `SchemaFingerprint` | 任意反射布局变更（加字段、改类型名等） | 同版本协议下内容是否匹配本进程 |

暂态：两者都写；任一方不匹配 → Failure（清空 Undo / 拒绝 PIE blob）。

---

## 4) 数据结构与 wire 布局

### 4.1 字节序与约定

- Little-endian；UTF-8 仅出现在 Persistent（本期无）。
- 单字段 payload 与总 blob 设上限（实现常量，防损坏撑爆内存）。

### 4.2 Blob header（Transient）

```text
offset  size  field
0       4     Magic = 'M','E','B','2'   // ME Binary v2
4       2     SchemaVersion = 2
6       2     HeaderFlags = 0（保留：endian、是否含 debug 名字表等）
8       8     SchemaFingerprint (u64)
16      …     RootValue（一条 tagged value，通常为 Object）
```

### 4.3 Value tags（`BinaryWireTag` v2）

| Tag | Value | Payload |
|-----|-------|---------|
| Null | 0 | — |
| Bool | 1 | u8 0/1 |
| Int64 | 2 | i64 |
| UInt64 | 3 | u64 |
| Double | 4 | f64 |
| String | 5 | u32 byteLen + bytes |
| Array | 6 | u32 count + N × tagged value |
| GuidRef | 7 | 16 bytes GUID |
| Object | 8 | 见 §4.4 |
| ObjectPtr | 9 | 同 Object 布局（语义在 Serializer：内联实例） |

**删除 v1 的 `EndObject`。** 对象边界由 `bodyLength` / `fieldCount` 决定。

> Tag 编号可与 v1 对齐前几项以便阅读；`EndObject=10` 不再使用。实现时可 `enum class BinaryWireTagV2`。

### 4.4 Object / ObjectPtr body

```text
Tag (Object|ObjectPtr)           // 1 byte
ClassId                          // u32
FieldCount                       // u32
BodyLength                       // u32 = 后续字段字节数（不含本 header）
Fields[FieldCount]:
  FieldId                        // u32
  Value                          // tagged value（自描述长度）
```

校验：

1. 读完 `FieldCount` 个字段后，消耗字节数 == `BodyLength`。  
2. 否则 Failure。  
3. `BodyLength` 可用于跳过整个对象（Persistent 将来跳未知嵌套时有用；Transient 主要用于校验）。

### 4.5 运行时辅助结构（概念）

```cpp
// 概念 API — 实现时可落在 ReflectionSystem 或 Serialization::SchemaTable
struct TransientSchemaTable
{
    uint16_t schemaVersion = 2;
    uint64_t fingerprint = 0;
    const MEClass* FindClass(uint32_t classId) const;
    uint32_t GetClassId(const MEClass* classInfo) const;
    const MEProperty* FindProperty(uint32_t classId, uint32_t fieldId) const;
    uint32_t GetFieldId(const MEClass* classInfo, const MEProperty& property) const;
};
```

Fingerprint 在 Finalize 后不可变，直至下次重新 Finalize。

---

## 5) 接口与数据流

### 5.1 Archive 层

保持 `WriterArchive` / `ReaderArchive` **逻辑接口**不变（BeginObject、BeginField…）。  
Binary v2 后端在内部把：

- `BeginObject` / 属性迭代 → 缓冲字段，在 `EndObject` 时写出 `ClassId + FieldCount + BodyLength + fields`（或流式两遍：先算长度再写；实现可选）。  
- Transient 模式：`BeginField(name)` 映射为 `FieldId`（查表）；写 wire 只写 Id。

可选扩展（若需避免改抽象）：

```cpp
enum class ArchiveIdentityMode { TransientIds, PersistentNames };
// SerializerOptions 或 Archive 构造参数
```

本期 Binary 固定 `TransientIds`。

### 5.2 Serializer 数据流（Transient）

```text
Serialize(MEClass*, obj)
  → fingerprint/header 由 BinaryWriter 在根写入
  → SerializeObject_IterateProps
       BeginField(name) → Archive 查 FieldId → 缓冲
       SerializeProperty → tagged value
  → EndObject → 写出 Object record

Deserialize(MEClass*, obj)
  → 读 header：Version==2 && Fingerprint==表 → 否则 Fail
  → 读 Object：ClassId 必须与期望兼容（IsA / 精确匹配策略见下）
  → 对每个 FieldId：查 MEProperty；未知 Id → Fail
  → tag 与 property 类别/codec 不符 → Fail
  → 反射表中应序列化却未出现的字段 → Fail（严格缺字段）
```

**动态 ObjectPtr：** wire 上 `ClassId` 为动态类型；须 `IsA(staticValueClass)`；创建实例后再灌字段。

### 5.3 与 PIE / Undo

| 调用方 | 今日 | F09 后 |
|--------|------|--------|
| `SceneDuplicator` | JSON | Binary v2 Transient |
| SceneEditor snapshot | Binary（v1） | Binary v2 |
| Property Undo buffer | Binary | Binary v2 |

同一次 Editor 会话内 Finalize 不变 → Fingerprint 稳定；热重载/重启后旧 Undo 栈应丢弃（已有产品语义）。

### 5.4 Persistent binding（契约示意，不实现）

```text
Object:
  Tag + typeName (u16 len + bytes) + FieldCount + BodyLength
  Field: fieldName (u16 len + bytes) + tagged value

Deserialize:
  一遍扫描或 name→payload 表
  已知名 → 写入；类型不符 → Skip+Warn
  未知名 → Skip+Warn
  缺名 → 默认值
  framing 坏 → Fail
```

与 JSON（F10）语义对齐，便于双后端行为一致。

---

## 6) 案例

### 6.1 暂态成功路径（示意）

```text
Write GameObject (ClassId=12):
  Header MEB2 ver=2 fp=0xABC…
  Object ClassId=12 FieldCount=3 BodyLength=…
    FieldId=1 (m_Name) String "Cube"
    FieldId=2 (m_Guid) … 
    FieldId=5 (m_Components) Array …

Read: fp 匹配；字段 1,2,5 均认识；无多余 Id → OK
```

### 6.2 暂态失败：未知 FieldId

```text
Blob 含 FieldId=99，本进程表无此 Id
→ Failure("unknown field id")，不部分应用
```

### 6.3 暂态失败：Fingerprint 不匹配

```text
改代码加了字段 → 新 fp
旧 Undo blob fp 旧 → Failure，Undo 项失效
```

### 6.4 TD-028 回归（验收必测）

Scene ≥2 GO，各含 StaticMesh + RigidBody + Collider（字段名长度 10 曾触发 v1 bug）  
→ `SerializeObjectToBuffer` / `DeserializeObjectFromBuffer` round-trip + ResolvePending OK。

### 6.5 Persistent（未来）对比

旧资产缺新字段 `m_Foo` → 默认值 + 可选 Warn。  
新资产多字段 `m_Bar`（旧代码不认识）→ Skip+Warn。  
JSON（F10）应对齐此行为。

---

## 7) 备选方案

| 选项 | 结论 |
|------|------|
| 仅 fieldCount、不要 bodyLength | 可用；**拒绝** — 要冗余校验与将来 skip 嵌套 |
| 两套完整 Binary 协议 | **拒绝** — 双维护；用双 binding |
| CastFlags 当 ClassId | **拒绝** — 非唯一身份 |
| 显式 meta FieldId 从第一天 | 延后到存盘 Binary；暂态用注册序 |
| 兼容读 v1 | **拒绝** |

---

## 8) 风险与缓解

| 风险 | 缓解 |
|------|------|
| EndObject 缓冲两遍写复杂 | 实现切片：先正确后优化流式 |
| FieldId 随注册序变化 | Fingerprint + 文档；不承诺跨进程 |
| PIE 仍失败 | 先 archive 测试再接 Duplicator |
| Serializer 缺字段策略过严 | 验收明确；若过痛可对「有默认值的新字段」再议（须改拍板） |

---

## 9) 验收标准

- [x] Wire Spec 与实现一致（Magic/Version/Fingerprint/Object 布局）
- [x] 无 `EndObject` 启发式；grep 无 v1 EndObject 解析路径（或 v1 代码删除）
- [x] SchemaFingerprint 不匹配 → Fail
- [x] 未知 ClassId/FieldId → Fail
- [x] TD-028 复现 Scene Binary round-trip 通过（`scene-clone` physics-stack）
- [x] `SceneDuplicator` 使用 Binary v2；TD-029 可关
- [x] `serialization-archive`、`scene-clone`、`verify.ps1` 通过
- [x] Design 中 Persistent 契约已文档化；未实现存盘 Binary
- [x] TECH_DEBT：TD-028/029 → Done

---

## 10) Status note

| 字段 | 内容 |
|------|------|
| What's next | 可选：目视 PIE；然后 `CORE-F10` JSON 存盘宽松 |
| Follow-up | CORE-F10 JSON 存盘宽松 + `$schemaVersion`；存盘 Binary 未做 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 占位 |
| 2026-09-04 | 依赖 F08；讨论拍板 |
| 2026-09-04 | **正式 Design：** framing、双 binding、ClassId、错误策略、布局、数据流、案例 |
| 2026-09-04 | **Done：** Transient Binary v2 落地；TD-028/029 Done；PIE Binary |
