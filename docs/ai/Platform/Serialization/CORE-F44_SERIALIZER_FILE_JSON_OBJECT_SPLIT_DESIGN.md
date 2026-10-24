# CORE-F44 — Serializer File / Json / Object Pipeline Split Design Spec

## Meta
- **ID:** `CORE-F44`
- **Type:** `Feature`（行为保持的 API 拆分 / 组合重构）
- **Complexity:** `L1`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`（S00–S01 已合入；`serialization-archive` / `data-asset` / `prefab` 绿）
- **Related:**
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - [CORE-F08 Serialization Cleanup](./CORE-F08_SERIALIZATION_CLEANUP_DESIGN.md)（Done）
  - [CORE-F10 JSON Disk Compat](./CORE-F10_JSON_DISK_COMPAT_DESIGN.md)（Done）
  - 解锁：[ASSET-F09 Async DataAsset](../../Asset/ASSET-F09_ASYNC_DATAASSET_LOAD_DESIGN.md)
- **Depends on:** 无（现有 `Serializer` / `JsonReaderArchive` / `ResolvePendingObjectRefs`）
- **Branch:** `feat/core`（序列化属 Core；勿用 ID 命名分支）

## TL;DR

把今日「一步到位」的 `Serializer::FromFile`（File → Json → Object + 引用解析）拆成可单独调用的两段，再让 `FromFile` 复用两者：

1. **File → Json**（读盘 + parse + 既有 disk meta 校验）
2. **Json → Object + ResolvePendingObjectRefs**（填字段 + 同步解析挂起引用）

行为与今日 `FromFile` / `AssetSerializeHelper::ReadObjectFromFile` **等价**。目的是让 ASSET-F09 Worker 只调第 1 段、游戏线程组装调第 2 段，避免再绕一遍完整 File 路径或私自拼 archive。

## Problem

1. `FromFile` 把读盘、反序列化、`ResolvePendingObjectRefs` 绑死；调用方无法只要内存 `Json`。
2. `DeserializeObjectFromJson` **已存在**，但只填字段、**不** Resolve；与 `FromFile` 语义不对称，调用方容易误以为「FromJson 一键等同 FromFile」。
3. `AssetSerializeHelper` 只有 `ReadObjectFromFile`；F09 / 测试无法用公开 Helper 做「内存 Json → 对象（含引用）」。
4. 若不先拆管道，F09 会在 Asset 层再发明一套读盘/parse，与 Serializer 分叉。

## Intent playback（设计输入）

| 项 | 内容 |
|----|------|
| 用户目标 | File→Json 与 Json→Object(+Resolve) 可单独调用；合起来 = 今日 FromFile |
| 成功 | 公开 API 两段可测；`FromFile` / Helper 冷路径改为组合；磁盘格式与引用语义不变 |
| 非目标 | 异步引用图、改 wire、Binary 对称拆分（可后置）、改 `ResolvePendingAssetRef` 策略 |
| 约束 | 行为保持；游戏线程 Resolve（同步 `LoadAssetByGUID`）与今日一致 |

## Requirements

### Must

1. **公开 File→Json：** 例如 `Serializer::LoadJsonFromFile(path, outJson, …)`（名可微调）。成功时 `outJson` 与今日 `FromFile` 内部读到的根等价；含现有 `SerializerDiskMeta::ValidateReader`（或等价）失败路径。
2. **公开 Json→Object(+Resolve)：** 例如 `Serializer::FromJson(rootClass, outObject, json, options)` = `DeserializeObjectFromJson` + `ResolvePendingObjectRefs`。成功语义对齐今日 `FromFile` 末尾（含同步递归资产引用加载）。
3. **保留 fill-only：** `DeserializeObjectFromJson` 仍只填字段并输出 `outUnresolvedRefs`；**不**自动 Resolve。需要自定义 Resolve 的调用方继续用它。
4. **`FromFile` 改为组合：** `LoadJsonFromFile` → `FromJson`（或等价内部步骤）；对外错误信息可整理，但成功/失败类别与今日一致（IO、版本、反序列化、引用）。
5. **Helper 对齐：** `AssetSerializeHelper` 增加 `ReadJsonFromFile` / `ReadObjectFromJson`；`ReadObjectFromFile` = 两者串联（或直接调 `FromFile`）。
6. **验证：** 同一夹具文件：`FromFile` vs `LoadJsonFromFile`+`FromJson` 属性与引用结果一致；既有序列化 / Prefab / DataAsset 相关 suite 不回归。

### Should

7. 命名与现有 `ToFile` / `FromFile` / `SerializeObjectToJson` 家族一致，避免再引入第三套别名。
8. 文档/注释说明：`FromJson` 含 Resolve；`DeserializeObjectFromJson` 不含。

### Must not

9. 改变磁盘 JSON schema、disk meta 版本规则、PendingObjectRef 形状。
10. 在本 Feature 引入异步 Resolve / 依赖图 / 「Sync vs Async 递归」开关。
11. Worker 线程调用 `FromJson` / `NewObject`（本 Feature 不强制 Worker；契约留给 F09）。

## Constraints

- Capabilities not Opinions：只提供装载机制分段；不规定资产异步策略。
- `ResolvePendingAssetRef` 今日走同步 `AssetManager::LoadAssetByGUID` — **本 Feature 不改**。
- 源码注释不含 Feature/Slice/Bug/ADR ID。
- 新公开符号 `MINENGINE_API`；不扩 `.def`。

## Preferences

- 优先薄包装 + 组合，少搬逻辑。
- Binary `FromFile` 若与 Json 共用同一入口：Json 路径拆分优先；Binary 可仍走 archive 一体路径，或对称拆「File→buffer」后置（见 Non-Goals）。

## Assumptions

- DataAsset / Prefab 等 JSON 资产是 F09 与本 Feature 的主要客户；Binary wire 本波不挡。
- `Json`（`nlohmann::json`）可在线程间移交所有权（F09 假设；本 Feature 只保证 API 可产出独立 `Json` 值）。

## Non-Goals

- ASSET-F09 实现（本 Feature 只解锁）。
- 异步递归引用解析。
- 改 `ResolvePendingObjectRefs` 失败策略（今日 Warn + remaining 等行为保持）。
- 删除 `DeserializeObjectFromJson` 或强制所有调用方改走 `FromJson`。

## Current Architecture

```text
Serializer::FromFile(path, class, object, archive)
  → archive.ReadFromFile(path)
  → (Json) SerializerDiskMeta::ValidateReader
  → Deserialize(..., unresolvedRefs)
  → ResolvePendingObjectRefs(unresolvedRefs)
        → ResolvePendingAssetRef → LoadAssetByGUID（同步）

Serializer::DeserializeObjectFromJson(class, object, json, outUnresolvedRefs)
  → JsonReaderArchive(json) + ValidateReader
  → Deserialize(...);  // 不 Resolve

AssetSerializeHelper::ReadObjectFromFile → FromFile only
```

缺口：无一级 `LoadJsonFromFile`；无「Json + Resolve」对称入口；Helper 无内存 Json 路径。

## Proposed Architecture

```text
LoadJsonFromFile(path, outJson)
  → 读盘 + parse + disk meta 校验 → outJson

FromJson(class, object, json)          // 或等价名
  → DeserializeObjectFromJson(..., pending)
  → ResolvePendingObjectRefs(pending)

FromFile(...)
  → LoadJsonFromFile → FromJson
  （Json 资产路径；实现细节可仍借 JsonReaderArchive，但对调用方呈现两段）

AssetSerializeHelper:
  ReadJsonFromFile / ReadObjectFromJson / ReadObjectFromFile(=串联)
```

### API 形状（草案，实现可微调名）

| API | 输入 | 输出 | Resolve？ |
|-----|------|------|-----------|
| `LoadJsonFromFile` | path | `Json` | N/A |
| `DeserializeObjectFromJson` | object + Json | pending refs | **否** |
| `FromJson` | object + Json | 已填对象 | **是（同步）** |
| `FromFile` | path + object | 已填对象 | 是（经组合） |

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `Serializer` | 两段 API + `FromFile` 组合；disk meta；Resolve 调用 | Asset 异步、缓存 |
| `AssetSerializeHelper` | MEObject 友好薄封装 | 业务加载策略 |
| `ResolvePendingObjectRefs` | 同步绑引用（保持） | 决定是否异步 |

## Boundaries

- **Allowed：** Core Serialization 导出分段 API；Resource Helper 调用 Serializer。
- **Forbidden：** AssetManager 内复制一套 JSON parse；本 Feature 改 TaskSystem。

## Dependencies

- Depends on: 现有 Serializer / JsonArchive / AssetManager（Resolve 路径）。
- Depended on by: **ASSET-F09**（Must 依赖本 Feature Done 或同波先合入）。
- 新依赖方向：无。

## Ownership & Lifetime

- `LoadJsonFromFile` 把 `Json` 所有权交给调用方（值或 out 参数）。
- `FromJson` 不持有 Json；调用方决定何时释放。
- Not applicable beyond normal stack/`SerializeResult`。

## Data Flow

`path → Json → (optional) MEObject fields → Resolve → bound refs`

## Control Flow

- 同步、调用方线程（与今日 `FromFile` 相同；通常游戏线程）。
- Resolve 内可触发嵌套同步 `LoadAssetByGUID`（今日行为；F09 MVP 依赖此点）。

## State & Invariants

- `FromFile` 成功 ⟹ 与 `LoadJsonFromFile`+`FromJson` 成功对同一文件在属性与硬引用上等价（测试断言）。
- `DeserializeObjectFromJson` 成功后 pending 可能非空；`FromJson` 成功后按今日 Resolve 规则处理（含 Warn 未解析项，若今日如此）。

## Failure Model

| Failure | Behavior |
|---------|----------|
| 文件缺失 / 读失败 | `LoadJsonFromFile` 失败；不产出可用 Json |
| disk meta / 版本不符 | 与今日 `FromFile` 一致 |
| 反射装盘失败 | `FromJson` / `DeserializeObjectFromJson` 失败 |
| 引用解析失败 | 对齐今日 `ResolvePendingObjectRefs`（不借机改严/改松） |

## Alternatives & Trade-offs

### Option A（推荐）— Serializer 一级两段 + FromFile 组合

- Pros：机制在序列化层；F09 / Helper / 测试共用；对称清晰。
- Cons：多 1～2 个公开符号。

### Option B — 仅 Helper 拆分，Serializer 不动

- Why not：Worker/测试仍要摸 archive 细节；机制沉在 Resource，方向反了。

### Option C — 只文档「请用 Deserialize + 手调 Resolve」，不抽 File→Json

- Why not：File→Json 仍不可单独测/调；F09 仍会复制读盘。

## Integration Impact

- `Serializer.h/.cpp`：新 API；`FromFile` 重构为组合。
- `AssetSerializeHelper`：新方法；旧 `ReadObjectFromFile` 实现改组合。
- Tests：序列化等价夹具；Prefab / DataAsset 回归。
- ASSET-F09：Worker 调 `LoadJsonFromFile`（或 Helper）；组装调 `FromJson` / `ReadObjectFromJson`。

## Migration / Compatibility

- 现有 `FromFile` / `ReadObjectFromFile` 调用方**无需**改代码。
- 已手写 `DeserializeObjectFromJson` 的调用方保持 fill-only；若需要旧 FromFile 语义，改调 `FromJson`。

## Verification Strategy

- 单元：同一 `.json` / `.medata` — `FromFile` vs 两段组合，字段 + 资产引用指针一致。
- 回归：`prefab`、`data-asset`、既有 serialization 相关 smoke。
- 不要求性能门闩。

## Open Questions

| # | Question | Default | Blocking? |
|---|----------|---------|-----------|
| Q1 | `FromJson` 最终函数名？ | **`FromJson`**（与 `FromFile` 成对） | No |
| Q2 | Binary `FromFile` 是否本波对称拆？ | **否**（后置） | No |
| Q3 | Resolve 失败是否比今日更严（硬失败）？ | **否，保持今日** | No |

## Decisions

| # | 决定 |
|---|------|
| 1 | 公开 **File→Json** 与 **Json→Object+同步 Resolve**；`FromFile` = 两者串联 |
| 2 | 保留 `DeserializeObjectFromJson` 为 fill-only |
| 3 | Helper 提供对称薄封装 |
| 4 | **不**做异步 Resolve / 双模式递归开关（属 F09 后置） |
| 5 | 本 Feature **先于** ASSET-F09 实现合入（或同波严格先合本 Feature） |

## Design Review

- **Verdict:** `Approved`（维护者批准后直接实现）
- **Reviewer / date:** maintainer / 2026-10-24
- **Summary:** File/Json/Object 分段与 FromFile 组合；同步 Resolve 保持。

## Implementation Readiness

- [x] Requirements / Non-Goals
- [x] 与现有 API 关系写清
- [x] 维护者确认 Decisions
- [x] 实现 S00–S01 完成

**Ready for implementation?** `done`

## Acceptance Checklist

- [x] 维护者批准
- [x] Registry / ACTIVE_WORK / Progress 同步
- [x] `FromFile` vs `LoadJsonFromFile`+`FromJson` 等价测试
- [x] F09 Design 依赖指向本 Feature

## Suggested slices

| Slice | Done when | Status |
|-------|-----------|--------|
| S00 | `LoadJsonFromFile` + disk meta | **Done** |
| S01 | `FromJson`；`FromFile` 组合；Helper；等价 + 回归 | **Done** |

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | Draft：从 ASSET-F09 讨论抽出；File/Json/Object 分段；同步 Resolve 保持 |
| 2026-10-24 | Done：API + Helper + `serialization-archive` 等价用例；data-asset/prefab 回归绿 |
