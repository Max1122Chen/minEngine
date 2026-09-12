# CORE-F18 — EngineVersion + Disk Schema Gate — Design Spec

## Meta
- **ID:** `CORE-F18`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-12
- **Related:**
  - [ENGINE_0_1_0_ROADMAP.md](../../ENGINE_0_1_0_ROADMAP.md) Phase F2 · 验收 D2
  - [CORE-F10 JSON disk compat](./CORE-F10_JSON_DISK_COMPAT_DESIGN.md)（**Done** — `$schemaVersion` 读写）
  - [CORE-F09 Binary wire](./CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md)（Transient；本期不扩展存盘 Binary）
  - [WF-F03 Maximum branding](../Docs/WF-F03_MAXIMUM_PRODUCT_BRANDING_DESIGN.md)（显示名；**版本数字本 Feature 提供真源**）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - Code today: `SerializationTypes.h` · `JsonArchive.*` · `Serializer.*` · `AssetMeta.h` · Resource Loaders
- **Depends on:** `CORE-F10` Done；`CORE-F17` Done（软）
- **Blocks:** Prefab A、0.1.0 Demo 资产契约（D2）、多分支 fan-out 后资产互换；WF-F03 可消费本 Feature 的版本字符串

## TL;DR

两件一并做、**字段不混用**：

1. **引擎版本身份（一等公民）** — C++ `EngineVersion{Major,Minor,Patch}` + 单一真源常量（当前 **0.0.9**）；供 CLI/`--version`、日志、资产戳、Vulkan `appInfo` 等共用。  
2. **磁盘 schema 门闸** — 沿用 F10 的 `$schemaVersion`（uint32，当前 **1**）；`read > supported` → **Fail**。

资产根另写 **`$engineVersion` 字符串**（由 `EngineVersion::ToString()` 生成，如 `"0.0.9"`）— **仅诊断，不拒载**。  
**不做**迁移表；**不**改 `.meta`/`AssetMeta`；**不**在盘上存 structured major/minor JSON。

## Scope

### In
- `Runtime/Core`：`EngineVersion` 类型 + `GetEngineVersion()` / 编译期常量（当前 **0.0.9**）
- 磁盘 schema 常量 `kDiskSchemaVersion`（与 `SerializerOptions::schemaVersion` 同源）
- JSON 根：写/读 `$engineVersion`（**string**）；写/读 `$schemaVersion`（沿用）
- `ValidateDiskSchema` + Serializer 统一接线（磁盘 JSON 路径）
- 关键盘 JSON Loader / Project / Config（若走同一根）覆盖核对
- 单测：`EngineVersion` 格式化；过高 schema Fail；缺 schema 按策略
- 样例关键资产补/重存双 meta；简短升级说明
- 可选顺手：把现有硬编码版号（如 Vulkan `VK_MAKE_VERSION`）接到 `EngineVersion`（小改，不扩 scope 到全仓清扫）

### Out / 暂缓
- 完整 migrate 表 / per-type CustomVersion（UE 式）
- 按 `$engineVersion` / major 拒载
- 资产盘上 structured `{major,minor,patch}` 对象（**已否决本期**；C++ 侧仍结构化）
- Binary 磁盘格式版本；`.meta` 镜像字段
- `Changelist` / `Branch` 字段（可后置扩展 `EngineVersion`）
- 产品显示名 **Maximum**（→ WF-F03）
- 强制重存全部历史第三方资产

## Reader quick start

1. §1.2 两层版本 · §3.1–3.3 API 与盘面约定 · §10 已拍 / 待拍  
2. CORE-F10 §3.3  
3. Roadmap D2  

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 依赖 | F10 已写读 `$schemaVersion`；无门闸、无引擎版本身份 — **本 Feature 补全** |
| 风险 | Medium — 门闸过严炸旧盘；版本身份若仍散落硬编码则假完成 |
| WIP | Primary = F18 |
| 建议 | **Go** — Core `EngineVersion` + schema 门闸；资产戳用字符串 |

---

## 1) 背景与目标

### 1.1 为什么现在做

0.1.0 窗口要同时冻结：

- **可版本化的资产契约**（fan-out / Prefab / Demo）  
- **可对外说的引擎版本**（与「叫 Maximum」的 WF-F03 分开：一个是数字身份，一个是品牌文案）

仅做 Serialization 字段而不做 `EngineVersion` 类型，会继续在多处硬编码 `"0.1.0"`，半年后再拆一次。

### 1.2 两层版本（业界对照 → 我们的映射）

大引擎普遍拆层（UE package file version vs `FEngineVersion`；Unity `fileFormatVersion` / `serializedVersion` vs Editor 产品版本）：

| 层 | 问题 | 本 Feature |
|----|------|------------|
| **Schema / file algebra** | 盘按哪套规则写的？ | `$schemaVersion` uint32 — **门闸** |
| **Engine product version** | 哪版引擎？ | C++ `EngineVersion`；盘上 `$engineVersion` **string** — **诊断** |
| Per-type CustomVersion | 某类字段第几代？ | **Out**（日后再议） |

**不**用引擎 semver 当加载主钥匙（引擎 bump 不必等于盘格式 bump）。

### 1.3 与 CORE-F10 / WF-F03

| | F10 | F18 | WF-F03 |
|--|-----|-----|--------|
| `$schemaVersion` 写读 | ✅ | 门闸 | — |
| `EngineVersion` 类型 | — | ✅ | 可读 `ToString()` 展示 |
| 产品名 Maximum | — | — | ✅ |
| 迁移表 | Out | Out | — |

### 1.4 Success

- 代码中存在唯一 `EngineVersion` 真源；`ToString()` == 资产写出的 `$engineVersion`
- 新存盘 JSON 根含 `$schemaVersion` + `$engineVersion`（string）
- `schema` 过高 → Fail；缺 schema → 按 §10；错误的/过时的 `$engineVersion` 字符串 **不** Fail
- D2 可宣称：关键资产带 schema + 引擎版本元数据

---

## 2) 现状（代码真源）

| 机制 | 今日行为 |
|------|----------|
| `SerializerOptions::writeSchemaVersion` / `schemaVersion` | 默认 true / **1**；写出 `$schemaVersion` |
| `GetReadSchemaVersion()` | 可读；**无门闸** |
| `$engineVersion` / `EngineVersion` 类型 | **不存在** |
| 版号硬编码 | 如 Vulkan `VK_MAKE_VERSION(0,1,0)` 等，与资产无关 |
| `AssetMeta` | 无版本字段 |
| 盘路径 Loader | Scene / Material / Skeleton / Anim* / … 走 JSON + Serializer |

---

## 3) 方案

### 3.1 `EngineVersion`（Core 一等公民）

```cpp
// Runtime/Core/EngineVersion.h  (path TBD; keep under Core/)
namespace minEngine
{
    struct EngineVersion
    {
        uint16_t Major = 0;
        uint16_t Minor = 0;
        uint16_t Patch = 0;

        // "Major.Minor.Patch" — asset stamp & CLI
        std::string ToString() const;

        // Optional later: Parse, operator<=> , Changelist, Branch
    };

    // Single source of truth for this engine build (0.0.9 window toward 0.1.0).
    EngineVersion GetEngineVersion();
    // or: inline constexpr EngineVersion kEngineVersion{0, 0, 9};
}
```

**约定：**

- 比较 / 分支逻辑在 C++ 用字段，不解析资产字符串做门闸。  
- 资产、日志、`--version` 展示统一走 `ToString()`。  
- **Changelist / Branch** 本期不做（避免过早复杂）；类型预留扩展注释即可。  
- **WF-F03** 只改「Maximum」等显示名，版本数字仍调用 `GetEngineVersion()`。

### 3.2 磁盘 schema 常量

```cpp
inline constexpr uint32_t kDiskSchemaVersion = 1u;
```

- `SerializerOptions::schemaVersion` 默认 = `kDiskSchemaVersion`（禁止第二套魔法数）。  
- **仅当**盘面破坏性不兼容且尚无迁移表时才 bump；文档写明「慎 bump」。

### 3.3 资产根 Meta（盘面）

#### 放在哪里？（确认）

**是的：`$schemaVersion` / `$engineVersion` 不嵌入业务对象的反射字段，也不在每个嵌套子对象上重复。**

| 层级 | 写什么 | 不写什么 |
|------|--------|----------|
| **文件 JSON 最外层根对象** | `$schemaVersion`、`$engineVersion`（本 Feature） | — |
| 根及嵌套对象（既有） | `$typeName` / `$ptr_typeName` / `$guid`（对象身份） | **不**写 schema/engine 版本 |
| `ME_PROPERTY` 业务字段 | `m_GameObjects`、`m_BlendMode`… | **不**增加 `m_SchemaVersion` 之类属性 |

实现上与 F10 一致：先按反射把根对象序列进 JSON，再对 **`m_Root` 注入** meta（`ApplyRootSchemaVersion` / 新增 engine 注入），因此它们是 **文件头式 meta**，不是 `Scene`/`Material` 类成员。

读侧：只在解析**文件根**时取这两个键；嵌套 `BeginObject` 不应依赖、也不应出现同名版本键。

#### Meta 键表

| 键 | 盘面类型 | 来源 | 门闸 |
|----|----------|------|------|
| `$schemaVersion` | uint32 | `kDiskSchemaVersion` | **是** |
| `$engineVersion` | **string** | `GetEngineVersion().ToString()` | **否** |
| `$typeName` / `$guid` / … | — | 既有（对象级） | — |

**拍板：** 资产上 **不用** JSON 对象存 major/minor；字符串足够，且与人类 diff / 手改友好。

读取：挂到 `DiskVersionInfo`；非法 `$engineVersion` 类型 → Warn + 空字符串。

#### 具体文件例子

**1）`.mescene`（示意，结构对齐现有样例；嵌套已缩短）**

今日样例往往还没有 schema 戳（read=0 → Warn）。F18 写出后根上多两行：

```json
{
  "$schemaVersion": 1,
  "$engineVersion": "0.0.9",
  "m_GameObjects": [
    {
      "$ptr_typeName": "minEngine::GameObject",
      "m_Name": "Cube",
      "m_Guid": { "High": 111, "Low": 222 },
      "m_Components": [
        {
          "$ptr_typeName": "minEngine::StaticMeshComponent",
          "m_Name": "SMC_Test",
          "m_Mesh": { "$guid": { "high": 1, "low": 2 } },
          "m_Material": { "$guid": { "high": 3, "low": 4 } }
        }
      ]
    }
  ]
}
```

注意：`GameObject` / `StaticMeshComponent` 上**没有** `$schemaVersion` / `$engineVersion`；只有文件根有。

**2）`.memtl`（示意）**

```json
{
  "$schemaVersion": 1,
  "$engineVersion": "0.0.9",
  "m_BlendMode": 0,
  "m_Graph": {
    "$ptr_typeName": "minEngine::MaterialEdGraph",
    "m_Nodes": []
  }
}
```

同样：版本只在最外层；`m_Graph` 子树只有对象 meta（`$ptr_typeName` / `$guid`），不带引擎/schema 版本。

**3）刻意对比 — 错误设计（本期不做）**

```json
{
  "m_SchemaVersion": 1,
  "m_EngineVersion": "0.1.0",
  "m_GameObjects": []
}
```

↑ 做成反射属性会污染类型，且难对「整文件」统一门闸。

```json
{
  "m_GameObjects": [
    {
      "$schemaVersion": 1,
      "$engineVersion": "0.0.9",
      "$ptr_typeName": "minEngine::GameObject"
    }
  ]
}
```

↑ 嵌在子对象上：进树前无法统一校验，同一文件多处版本易不一致。

### 3.4 加载策略

`supported = kDiskSchemaVersion`，`read = GetReadSchemaVersion()`：

| 条件 | 行为（已选默认，见 §10） |
|------|--------------------------|
| `read > supported` | **Fail**（文件更新 / 引擎过旧） |
| `read == supported` | Success |
| `0 < read < supported` | **Success**（靠 F10 缺字段默认；无迁移表） |
| `read == 0`（缺/非法当 0） | **Warn + Success** |
| `$engineVersion` 任意 | 不拒载 |

Fail message：路径（若有）、`read`、`supported`、可选附带读到的 engine 字符串。

### 3.5 API 草图（Serialization）

```cpp
namespace minEngine::Serialization
{
    struct DiskVersionInfo
    {
        uint32_t schemaVersion = 0;   // 0 = missing/legacy
        std::string engineVersion;    // stamp string; empty if missing
    };

    SerializeResult ValidateDiskSchema(
        const DiskVersionInfo& info,
        uint32_t supportedSchemaVersion = minEngine::kDiskSchemaVersion);
}
```

- **接线（已选）：** Serializer 在 JSON 根 meta 解析后统一 `ValidateDiskSchema`（避免各 Loader 漏接）。  
- **写出：** `ApplyRootDiskMeta(schema, engineString)` 或分列 `ApplyRootSchemaVersion` + `ApplyRootEngineVersion`。

### 3.6 覆盖范围

| 类别 | 本期 |
|------|------|
| 经 Serializer 的磁盘 JSON | 写双 meta + 读门闸 |
| EngineConfig / Project（同根 JSON） | 应纳入（实现时核对） |
| `.meta` / `AssetMeta` | **不纳入** |
| 二进制源文件 | 不纳入；sidecar JSON 则纳入 |
| Binary buffer / PIE | 不走磁盘门闸 |

### 3.7 消费者（EngineVersion，非仅资产）

| 消费者 | 本期期望 |
|--------|----------|
| 资产 `$engineVersion` | 必须 |
| 测试 / 单测 | 必须 |
| CLI `--version` / 启动日志一行 | **应做**（若入口已有 version 打印则改接真源） |
| Vulkan/OpenGL appInfo | **可选顺手** |
| Editor 窗口标题 | WF-F03 为主；可先留后接 |

### 3.8 数据流

```text
GetEngineVersion() ──ToString()──┐
kDiskSchemaVersion ──────────────┼─→ JsonWriter root meta → disk
                                 │
disk → JsonReader meta → DiskVersionInfo
                      → ValidateDiskSchema (schema only)
                      → deserialize (F10)
```

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 盘上 `$engineVersion` 用 `{major,minor,patch}` | **否决本期** — 维护者选字符串 |
| 用引擎 semver 拒载 | **否决** — 门闸只认 schema |
| schema 改成 `"1.0"` 字符串 | **否决** — 保持 F10 uint32 |
| 一上来 CustomVersion / Changelist | **延后** |
| `.meta` 双写 | **延后** |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 旧样例 Warn 噪声 | 重存样例；文案一次性说清 |
| 版号仍有遗漏硬编码 | S00 定真源；S01 抽查已知点；不强制全仓大扫 |
| 过早 bump schema | 文档：无迁移表时慎 +1 |
| 与 WF-F03 职责混淆 | F18 = 数字身份；F03 = 显示名 |

---

## 6) 验收

- [x] `EngineVersion` + `GetEngineVersion()`（或等价常量）真源存在；`ToString()` 稳定为 `Major.Minor.Patch`（**0.0.9**）  
- [x] `kDiskSchemaVersion` 与 Serializer 默认写出一致（**1**）  
- [x] 新存盘 JSON 含 `$schemaVersion` + **字符串** `$engineVersion`  
- [x] `read > supported` → Fail + 明确 message  
- [x] `read == 0` → Warn + 继续  
- [x] `$engineVersion` 不参与拒载  
- [x] 关键磁盘 JSON 路径走统一校验（`Serializer::FromFile` / `DeserializeObjectFromJson`）  
- [x] 单测覆盖 EngineVersion 与 schema 门闸（`engine-version` suite）  
- [x] 样例关键资产已 stamp；Docs/Registry  

---

## 7) 切片

| Slice | 内容 | 验证 |
|-------|------|------|
| **S00** | `EngineVersion` + `kDiskSchemaVersion`；写 `$engineVersion` string；`ValidateDiskSchema` + Serializer 接线 | 单测 |
| **S01** | Loader/Project/Config 覆盖核对；可选接 CLI/Vulkan 硬编码 | 负例 fixture |
| **S02** | 样例重存 + 升级说明；Feature Done 文档 | 手测打开样例 |

---

## 8) 测试计划

- `EngineVersion{0,0,9}.ToString() == "0.0.9"`  
- `schema=1` + `"0.0.9"` → Success  
- `schema=999` → Fail  
- 无 `$schemaVersion` → Warn + Success  
- `$engineVersion":"9.9.9"` 且 schema=1 → Success  

---

## 9) 哲学对照（短）

- **Mechanism over Policy：** 版本类型与加载规则是机制；不规定玩法资产语义。  
- **Prefer Simplicity：** 盘面字符串戳 + uint schema；C++ 侧才结构化。  
- **Agent-Friendly：** 同一 `ToString()` / Fail message，人与工具共用（不另做 Agent API）。

---

## 10) 开放点 — 已拍板

| # | 问题 | 结论 |
|---|------|------|
| A | 引擎版本身份 | **做** `EngineVersion` 一等公民（Major/Minor/Patch） |
| B | 资产 `$engineVersion` 形态 | **字符串**（由 `ToString()` 生成） |
| C | 加载主门闸 | **仅** `$schemaVersion`；引擎戳不拒载 |
| D | `.meta` / AssetMeta | **本期不写** |
| E | 校验接线 | **Serializer 统一** |
| F | 版本字段放置 | **仅文件 JSON 根**；不嵌入反射对象 / 不在嵌套子对象重复 |
| 1 | 缺 `$schemaVersion`（read=0） | **Warn + 继续加载** |
| 2 | `0 < read < supported` | **允许加载** |
| 3 | 头文件名 | `EngineVersion.h` + schema 常量同文件或旁邻（实现选简洁者） |

可 → **In Progress** → S00。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-12 | **实现 Done：** `EngineVersion`=0.0.9；schema=1；资产 stamp；`engine-version` 单测 PASS |
| 2026-09-12 | 明确：版本 meta **仅文件根**；补充 `.mescene` / `.memtl` 正反例 |
| 2026-09-11 | 修订：`EngineVersion` 一等公民；资产 `$engineVersion` **字符串**；业界分层说明；与 WF-F03 分工 |
| 2026-09-11 | 详设草稿：schema 门闸 + 引擎戳 |
| 2026-09-11 | Planned stub |
