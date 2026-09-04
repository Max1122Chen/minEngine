# CORE-F10 — JSON Disk Compatibility & Schema Meta

## Meta
- **ID:** `CORE-F10`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/core`
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../../ACTIVE_WORK.md) · [Implementation](./CORE-F10_JSON_DISK_COMPAT_IMPLEMENTATION.md) · [CORE-F09](./CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md)（Persistent 容错契约） · [SERIALIZATION_BINARY_AND_PROPERTY_API.md](./SERIALIZATION_BINARY_AND_PROPERTY_API.md)
- **Depends on:** `CORE-F09` Design 契约（已 Done）；不依赖存盘 Binary

## TL;DR

把 **JSON 存盘** 反序列化容错对齐 F09 **Persistent binding**：缺字段保留默认、多余键 Skip+Warn、已知键类型不符可 Skip+Warn；根对象读写 **`$schemaVersion`**。不改 Binary 暂态严格语义，不做字段 rename 迁移表。

## Scope

### In
- 与 F09 §3.2 Persistent 对齐的 JSON 错误策略（见 §3）
- 根写入/读取 `$schemaVersion`（缺省视为 `0`；本期写 `1`）
- 盘路径 Loader / Project / PathRegistry 默认走宽松选项；理清 `skipUnknownField` 与 `strictTypeCheck`
- JsonReader：对象 End 时报告未消费的非 `$` 键（Warn，不 Fail）
- 回归测试 + 文档

### Out
- Binary 磁盘格式
- 字段 rename / redirect 表 / 自动迁移脚本
- TD-026
- 强制重存全部历史资产
- 改 Transient Binary 严格策略

## Reader quick start
1. 本文件 — 契约与选项语义
2. [Implementation Plan](./CORE-F10_JSON_DISK_COMPAT_IMPLEMENTATION.md)
3. 代码：`SerializationTypes.h` · `JsonArchive.*` · `Serializer.cpp` · Loaders

---

## 0) Pre-flight（2026-09-04）

| 项 | 结论 |
|----|------|
| 依赖 | F09 Persistent 契约已文档化；JSON 仍是 `.mescene` / `.memtl` / EngineConfig 主路径 |
| 现状债 | `skipUnknownField` 只管「反射有、文件无」；多余 JSON 键静默；`strictTypeCheck` **从未被 Serializer 读取**；部分 Loader `skipUnknownField=false` 加字段即痛 |
| 风险 | Medium-Low — 改默认容错可能掩盖坏资产；用 Warn + 可选 strict 工具模式对冲 |
| 与 Primary | 不挡 `ANIM-F01`；`feat/core` 并行 |
| 建议 | **Go** — 先定契约与选项，再小切片改 Json/Serializer/Loader |

---

## 1) 背景与目标

### Pain
- 引擎加可选字段后，旧文件缺键：部分路径 `skipUnknownField=false` → Fail。
- 新文件多未知键：旧代码静默忽略，无诊断。
- 无 `$schemaVersion`，无法表达协议代数。

### Goals
- 盘路径默认 **可演进**：缺字段用默认；多余键 Warn；类型不符可 Skip+Warn。
- Framing / JSON 语法坏 / 非 object 根 → 仍 **Failure**。
- 根带 `$schemaVersion`；读缺省 = `0`。

### Success
- 旧 `.mescene` 在「新加可选反射字段」后仍可加载。
- 含未知键的 JSON 加载成功且有 Warn。
- Binary buffer / PIE 仍严格（F09 不变）。

---

## 2) 现状

| 机制 | 行为 |
|------|------|
| `skipUnknownField=true` | `EnterField` 失败 → 跳过该反射属性（缺字段） |
| `skipUnknownField=false` | 缺字段 → Failure |
| 多余 JSON 键 | 从不 Enter → **静默** |
| codec / BeginObject 失败 | 一律 Failure |
| `strictTypeCheck` | 选项存在但 **未接线** |
| `$typeName` / `$ptr_typeName` / `$guid` | 已有 meta 键 |

盘路径混用：`SceneLoader`/`PathRegistry`/`ProjectManager` 偏 true；`MaterialLoader`/`EnvironmentMapLoader`/部分 AssetManager 为 false。

---

## 3) 方案

### 3.1 对齐 F09 Persistent（JSON）

| 情况 | JSON 盘路径（默认宽松） | Strict 工具模式 |
|------|-------------------------|-----------------|
| 反射有、文件无 | 保留默认值（`skipUnknownField=true`） | Failure |
| 文件有、反射无（非 `$` meta） | Skip + **Warn** | Skip + Warn（仍不 Fail；校验工具可另计） |
| 已知键、类型/codec 不符 | Skip 该字段 + **Warn**（`strictTypeCheck=false`） | Failure（`strictTypeCheck=true`） |
| JSON 语法坏 / 根非 object | Failure | Failure |
| 同名键 | nlohmann 对象天然唯一 | — |

保留 `$` 前缀为 **meta 保留区**：`$typeName`、`$ptr_typeName`、`$guid`、`$schemaVersion`；未识别的 `$foo` → Warn（不 Fail）。

### 3.2 `SerializerOptions` 语义（接线）

| 字段 | 默认 | 含义（F10 后） |
|------|------|----------------|
| `skipUnknownField` | `true` | **缺字段**：true=跳过并保留默认；false=Fail |
| `strictTypeCheck` | `true`（全局默认） | **类型不符**：true=Fail；false=Skip+Warn 并保留默认 |
| `enumAsString` | `true` | 不变 |
| `writeObjectTypeName` | `false` | 不变 |
| `writeSchemaVersion` | **新增** `true` | 根 `BeginObject` 时写 `$schemaVersion` |
| `schemaVersion` | **新增** `1` | 写入的代数；读入仅记录，本期无迁移表 |

**盘路径推荐：** `skipUnknownField=true`，`strictTypeCheck=false`，`writeSchemaVersion=true`。  
**Binary Buffer / PIE：** 继续强制 `skipUnknownField=false`；并强制 `strictTypeCheck=true`（与 F09 Transient 一致）。

> 不重命名 `skipUnknownField`（避免大范围 churn）；文档与注释写清「缺字段」语义，不再暗示「未知键」。

### 3.3 `$schemaVersion`

```text
写入（根对象，writeSchemaVersion=true）:
  JSON object 增加 "$schemaVersion": <uint>  // 本期 = 1

读取:
  若存在且为无符号/整数 → 解析为 u32，挂在 JsonReader 或 Serializer 会话（可选查询 API）
  若不存在 → 视为 0（legacy）
  类型非法 → Warn + 视为 0（宽松）；Strict 可 Fail（与 strictTypeCheck 一致即可）
```

本期 **不做** version→迁移函数表；只保证可观测 + 向前可演进。

### 3.4 JsonReader 多余键检测

- 每个 Object 上下文维护 `consumedKeys`（`EnterField` 成功时插入）。
- `EndObject`：遍历 JSON keys；跳过已消费；`$` 保留键中已识别的不告警；其余 → `ME_CORE_WARN`（含路径/类名若可得）。
- **不**因多余键返回 false。

### 3.5 类型不符 Skip+Warn

在 `DeserializeProperty`（及必要的 BeginObject/Array 失败点）：

- 若操作失败且 `!options.strictTypeCheck` → Warn + 返回 Success（保留默认值；已部分写入的嵌套对象需谨慎——**规则：仅在进入字段后、子树失败时**，若未成功 BeginObject 则不改内存；若已 BeginObject 再失败，Strict Fail；宽松模式对 **叶子 codec 失败** 优先支持）。
- MVP 切片：**Primitive codec 读失败** + **BeginArray/BeginObject 失败（叶子/内嵌值）** 在宽松下 Skip+Warn；深度半写入对象若难以回滚 → 该路径仍 Fail 并文档化（避免静默损坏）。

### 3.6 调用点迁移

| 调用方 | 动作 |
|--------|------|
| SceneLoader / PathRegistry / ProjectManager | 确认宽松；`strictTypeCheck=false` |
| MaterialLoader / EnvironmentMapLoader | `skipUnknownField=true`，`strictTypeCheck=false` |
| AssetManager 读盘 | 同上 |
| SceneEditor / AssetManager **写后自检** 若需严格 | 显式 `strictTypeCheck=true` + `skipUnknownField=false` |
| Binary / PIE | Serializer Buffer API 已强制严格；保持 |

### 3.7 数据流

```text
Disk JSON → JsonReaderArchive
              ├─ BeginObject：可选读 $schemaVersion
              ├─ EnterField / 叶子 codec
              └─ EndObject：Warn 未消费非 meta 键
         → Serializer(options: loose disk)
```

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 新 enum `CompatibilityMode` 取代双 bool | **延后** — 两 bool 已够；减少 churn |
| 多余键也 Fail（strict 资产校验） | 本期不做独立工具；Warn 即可 |
| 立刻做 rename 表 | **Out** |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 宽松掩盖坏类型 | Warn；strict 模式保留；半写入嵌套仍 Fail |
| `$schemaVersion` 与旧工具冲突 | `$` 保留区；旧代码忽略未知键 |
| Loader 改默认破坏「故意严格」用例 | 审计调用点；Editor 自检显式 strict |

---

## 6) 验收标准

- [x] Design 与 F09 Persistent 表一致（JSON 列）
- [x] `strictTypeCheck` 已接线；盘路径默认宽松
- [x] 多余键 → Warn，不 Fail
- [x] 缺字段 + `skipUnknownField=true` → 默认值，不 Fail
- [x] 叶子类型不符 + `strictTypeCheck=false` → Warn，不 Fail
- [x] 根写入 `$schemaVersion=1`；缺省读为 0
- [x] Binary / PIE 仍严格
- [x] 测试覆盖 + `verify.ps1` / 相关 suite 绿
- [x] Registry / ACTIVE_WORK / PROGRESS_LOG 更新

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| What's next | 准备 commit；可选资产校验 CLI |
| Follow-up | 迁移表 / 资产校验 CLI（可选） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Registry 占位 |
| 2026-09-04 | **正式 Design：** 选项语义、`$schemaVersion`、Json 多余键、Loader 迁移 |
| 2026-09-04 | **Done：** 实现 + 测试；盘路径宽松；Binary 仍严格 |
