# GP-F03 GameplayTag as ME_STRUCT — Design Spec

## Meta
- **ID:** `GP-F03`
- **Type:** Feature（含 2026-10-22 语义修订）
- **Complexity:** `L1`（本次修订；原落地曾按 L2）
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** 2026-10-22
- **Related:** [GP-F01](./GP-F01_GAMEPLAY_TAG_DESIGN.md)（Done；词表 / Matches / Container）· [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [CORE-F39](../Platform/Serialization/CORE-F39_VALUE_CODEC_REGISTRATION_DESIGN.md)（Cancelled）· UE `FGameplayTag`（`D:/Dev/GitRepo/UnrealEngine/.../GameplayTagContainer.h`）
- **Reference:** minCraft 反馈 — 未 Bind 的同名 Tag 不能作 map 键；`DATA-F05` 需要 `ME_PROPERTY GameplayTag` 单轨（已由首版落地）

## TL;DR

`GameplayTag` 已是可序列化的 `ME_STRUCT`（持久字段只有 `TagName`）。本次修订**对齐 UE 的分工**：

- **身份（`==` / hash / 可选 `<`）按名** — 未 Bind 的同名 Tag 相等，可作 `unordered_map` / `map` 键。
- **层级匹配仍查全局词表** — `Matches` / `IsChildOf` / Container::Add 要求已注册（`IsValid()` 仍严于 UE 的「非空名」）。
- **从 `GameplayTag` 上移除 `m_Manager*`** — 进程一份 `GameplayTagManager::Get()`；指针不再承担身份，也不再需要防「错 Manager」。
- **`m_Index` 降为可选缓存** — Bind / Resolve 填充；非身份、不进存档。暂不引入 `FName`；名用 `std::string`。

首版（2026-10-18）已交付 ME_STRUCT + Setter Bind；本修订只改合同与实现形状，不重做序列化。

## Problem

1. **minCraft：** 未 Bind 的两个同名 Tag `operator==` 为 false，且无 hash，无法当 map 键。
2. **合同漂移：** 首版用「词表 index + Manager*」当运行时身份，与 UE「名身份 + Manager 只服务匹配」不一致；在已有全局 `Get()` 的前提下，Tag 上挂 `Manager*` 没有独立价值。
3. **首版 Non-Goal「禁止按名 `==`」** 正是今天的痛点来源，需要有意推翻并写清新边界。

## Requirements

- 保持：`GameplayTag` 为 `ME_STRUCT`；持久字段仅 `TagName`（`std::string`）；`ME_PROPERTY` / `vector<GameplayTag>` 序列化路径不变。
- **`operator==` / `!=`：** `TagName == other.TagName`（含双方皆空；含双方未 Bind 但同名）。
- **`std::hash<GameplayTag>`：** 基于 `TagName`（使 `unordered_map` 可用）。
- **可选 `operator<`：** 基于 `TagName`（使 `std::map` 可用；实现可用 `std::string` 的字典序）。
- **从 `GameplayTag` 删除 `m_Manager*`。** `IsValid` / `Matches` / `IsChildOf` / Bind 一律经 `GameplayTagManager::Get()`（或 `HasInstance()` 失败路径）。
- **`m_Index`：** 保留为 Bind 缓存；`GetIndex()` 在未 Bind 时返回哨兵；**不得**再作为 `==` 依据。
- **`IsValid()`：** 仍为「当前全局 Manager 已 Init，且 `TagName` 在词表中（Bind 成功或等价校验）」— **有意严于 UE**。
- **`Matches` / `IsChildOf`：** 双方 `IsValid()`；父链经 Manager；可用 index 缓存加速，也允许按名查节点（实现任选，测例钉行为）。
- **`Resolve` / `TryResolve` / Native / `SetTagName`：** 仍尝试 Bind 填 index；失败则清 index、保留名（未知名）。
- **Container：** 内部仍可按 index 计数；`Add` 仍要求 `IsValid()`。Container 可继续持有 `Manager&`（构造注入），或改为内部 `Get()` — 实现任选，行为不变。
- **`test gameplay-tags`：** 覆盖名相等（含未 Bind 同名）、hash 可插入 map、Matches 仍要求注册、JSON roundtrip、`gameplay-events` 回归。

## Constraints

- index **不是** `ME_PROPERTY`；存档只认 `TagName`。
- 同一进程一份 Manager（`SetInstance` + `Initialize`）。Shutdown 后：index 缓存作废；`IsValid()` false；`TagName` 仍在，须重新 Init + Bind（或再次 `SetTagName`）才可匹配。
- 未注册（未 Bind 成功）的非空名：**可以** `==` / 作 map 键；**不可以** `Matches` / Container::Add（`IsValid()==false`）。
- 本期**不**引入引擎级 `FName`；接受 `std::string` 比较与 hash 的成本。
- 前向修订：不保留「index + Manager* 身份」双路径；改完后只有名身份。

## Preferences

- 对齐 UE：相等按名；匹配查字典；Tag struct 不挂 Manager*。
- `IsValid` 保持比 UE 严（注册才 Valid），避免未注册名静默参与层级匹配。
- Bind 仍由 Manager 提供；`SetTagName` 继续自动 Bind（序列化 Setter 路径不变）。

## Assumptions

- Tag 数量与比较频率在 minCraft / 当前引擎规模下，`std::string` 相等与 hash 可接受。
- 不会出现「多活 Manager 实例、同一进程两套词表」的产品需求；若将来需要，应是显式多词表 API，而不是把 `Manager*` 塞回每个 Tag。

## Non-Goals

- 本期引入 `FName` / 全局 string intern 表。
- 把 `IsValid()` 放宽到「非空名即 Valid」（完整字面 UE）。
- Container 本体序列化、Tag 拾取器、重定向表、网络 net-index。
- `CORE-F39` value-codec。
- 业务词汇进引擎默认表。

## Current Architecture（修订前代码）

| 部件 | 今天（首版落地后） | 本修订 |
|------|-------------------|--------|
| 持久 | `TagName` ME_PROPERTY + `SetTagName` → Bind | **保留** |
| 身份 | `m_Index` + `m_Manager*`；未 Bind 同名 **不等** | **改为 `TagName`** |
| 匹配 | index + 挂在 Tag 上的 Manager* | **全局 `Get()` + 可选 index 缓存** |
| hash / `<` | 无 | **新增（基于名）** |
| Container | `map<index,count>` + 持有 Manager* | **行为保留**；键仍为 index |

## Proposed Architecture

### 句柄形状

```text
GameplayTag (ME_STRUCT)
├── TagName : std::string     ME_PROPERTY   // disk + identity (== / hash / <)
└── m_Index : uint32          非属性         // optional cache after Bind；哨兵 = 未 Bind
// 删除 m_Manager*
```

| 层 | 字段 | 用途 |
|----|------|------|
| 持久 + 身份 | `TagName` | 序列化；`==`；hash；`<`；空字符串 = 空 Tag |
| 运行时缓存 | `m_Index` | `GetIndex()`；Matches / Container 加速；非身份 |

**Invariant（`IsValid()==true`）：**  
全局 Manager 已 Init；`TagName` 非空且在词表中；若 `m_Index` 非哨兵，则 `Manager` 节点名与 `TagName` 一致（Bind 后应满足；Shutdown 后须视为无效）。

### Bind（保留，语义收窄）

`GameplayTagManager::BindTag(GameplayTag& tag)`：

1. `TagName` 空 → 清 index，返回 false。
2. Manager 未 Init → 清 index，返回 false。
3. 查名失败 → 清 index，**保留 `TagName`**，返回 false。
4. 成功 → 写 `m_Index`（**不再写 Manager***），返回 true。

调用点不变：`Resolve` / `TryResolve`、`SetTagName`（含反序列化 Setter）。匹配路径使用 `GameplayTagManager::Get()`，不读 Tag 上的 Manager 指针。

### 语义表

| API | 行为 |
|-----|------|
| `IsValid()` | 全局 Manager 认该名（已 Bind / 等价查表成功） |
| `GetName()` | 返回 `TagName` |
| `GetIndex()` | 已 Bind → index；否则哨兵（不现场伪装查表） |
| `operator==` | `TagName == other.TagName` |
| `std::hash` | hash(`TagName`) |
| `operator<`（若提供） | `TagName` 字典序 |
| `Matches` / `IsChildOf` | 双方 Valid；经 `Get()` 父链（可用 index 缓存） |

### 与 UE 的对应（源码锚点）

锚点：`Engine/Source/Runtime/GameplayTags/Classes/GameplayTagContainer.h`；`MatchesTag` 见 `GameplayTagContainer.cpp`（`FindTagNode` + 未注册 ensure）。

| UE | minEngine（本修订） | 对齐程度 |
|----|---------------------|----------|
| 唯一字段 `FName TagName` | 唯一持久/身份字段 `std::string TagName` | 同构（无 FName 表） |
| `==` / `GetTypeHash` 按名 | `==` / `std::hash` 按名 | **对齐** |
| 无 `Manager*` 在 Tag 上 | **删除** `m_Manager*` | **对齐** |
| `IsValid` = 非空名 | `IsValid` = 已在词表 | **有意更严** |
| `MatchesTag` 查 Manager | `Matches` 查 `Get()` | **对齐** |
| 比较快靠 FName intern | 本期接受 string；日后可 FName | 明确延期 |

### Container

不变对外行为：按 index 计数；`Add` 要求 Valid。内部 Manager 引用可保留（与 Tag 解耦）。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `GameplayTag` | 持有名（身份）；可选 index 缓存；`==`/hash | 词表所有权；挂 Manager* |
| `GameplayTagManager` | 词表、Resolve、Bind、父链、`IsValidTag` | 写入资产；当 Tag 身份的一部分 |
| Serializer | 读写 `TagName` | 比较语义 |
| 宿主 / 测试 | Manager 先于含 Tag 内容的 Load | — |

## Boundaries

- Allowed：Tag ↔ Reflection；`SetTagName` → Bind；Matches → `Get()`。
- Forbidden：Tag 上再挂 `Manager*`；用 index 实现 `==`；序列化 index；把未注册 Tag 用于 Matches/Container::Add。

## Dependencies

- Depends on：`GP-F01` 词表与 Container；`ME_STRUCT`；JSON 序列化。
- Depended on by：minCraft map 键 / 去掉名旁路；GP-F02 持 Tag 路径（回归）。

## Ownership & Lifetime

- Tag 值可拷贝；拷贝复制 `TagName` 与 `m_Index`。
- Manager 由 Engine 拥有。Shutdown 后 index 缓存无效；名仍在。

## Data Flow

```text
Authoring / disk / map key:  TagName  ──►  == / hash
       │
       ▼
   Bind(Get()) ──► m_Index (cache)
       │
       ▼
Matches / Container:  Get() + index cache
       │
       ▼
Serialize out:  TagName only
```

## State & Invariants

- 身份唯一来源：`TagName`。
- `IsValid` ⇒ 可匹配 / 可进 Container；未 Valid 仍可相等与作键。
- Shutdown 后不得把旧 index 当 Valid。

## API / Interface Semantics

- `Resolve`：未知 — Error/assert（GP-F01）。`TryResolve`：未知 → 空 Tag。
- Bind 失败：保留名，清 index，`IsValid()` false。
- 不提供「改 index 不改名」的公开 API。

## Failure Model

| 情况 | 行为 |
|------|------|
| 反序列化未知名 | 读入名；Bind 失败；`IsValid()` false；名保留 |
| 未 Bind 同名 | `==` true；可作 map 键；`Matches` false |
| 未 Bind 就 Matches / Container::Add | false / assert（与今类似） |
| Manager 未 Init | Bind 失败；`IsValid()` false；`==` 仍按名 |

## Alternatives & Trade-offs

### Option A（选定）— 名身份 + 去 Manager* + index 缓存

- Pros：对齐 UE；修好 map 键；Tag 更瘦；匹配仍可快。
- Cons：`==` 走 string（无 FName）；须改测例与 GP-F03 旧合同。

### Option B — 仅放宽「双方未 Bind 时按名相等」，保留 Manager* + index 身份

- Why not：半吊子；Bind/未 Bind 混用 map 仍易查丢；Manager* 仍无意义。

### Option C — 本期引入 FName

- Why not：超出小重构；string 已够用。可后续单开 Feature。

## Integration Impact

- 代码：`GameplayTag.h/.cpp`、`GameplayTagManager`（`IsValidTag` 不再比 `tag.m_Manager`）、测试、可能 Container 无行为 diff。
- 文档：本文件；Registry；Progress（实现后）；GP-F01 不必重写全文（身份以本修订为准）。
- 消费方：minCraft 可直接用 Tag 作键；已 Bind 路径行为对 `==` 与「同名」一致（通常无破坏）。

## Migration / Compatibility

- 无磁盘格式变更（仍只写 `TagName`）。
- **行为变更：** 未 Bind 同名从「不等」变为「相等」；这是目标修复。
- 依赖「未 Bind 同名必须不等」的代码（若有）会破 — 引擎测例按新合同改；视为前向修正。

## Verification Strategy

- `gameplay-tags`：
  - 未 Bind：`a.SetTagName("X"); b.SetTagName("X");`（Manager 无 instance 或名未注册）→ `a == b`；可插入 `unordered_map<GameplayTag, int>` 并查出。
  - 已 Resolve 的同名 → `==`；与源 roundtrip `==`。
  - 同名未注册 → `Matches` false；注册后层级 Matches 仍过。
  - Container Add/Has 回归。
- `gameplay-events` 回归。
- 实现后：`minEngineTests.exe test gameplay-tags` / `gameplay-events`；相关则 smoke。

## Open Questions

- 无阻塞项。`operator<`：默认**做**（与 UE / `std::map` 一致）；若审批时要求只做 hash、不做 `<`，实现可缩。

## Design Review

- **Verdict:** `Ready with deferred items`（修订已批准并实现）
- **Prior (2026-10-18):** Ready with deferred items — ME_STRUCT 首版已落地。
- **This revision (2026-10-22):** Maintainer approved；落地名身份 + 去 Manager*。Deferred 仍含：FName、重定向、Container 序列化、Tag 拾取器、`IsValid` 放宽到非空名。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected（代码 + UE 锚点）
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved
- [x] Major state/invariants resolved
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes`（修订已落地）

## Acceptance Checklist

- [x] Design revision approved by maintainer
- [x] Implementation（去 Manager*、名 `==`、hash/`<`、测例）
- [x] Progress log + Feature registry → Done（修订关闭）
- [x] 首版 ME_STRUCT（2026-10-18）已交付 — 不回滚

---

## 变更记录

| 日期 | 变更 |
|------|------|
| 2026-10-18 | 初版 Done：ME_STRUCT + TagName + index/Manager* 身份 + Setter Bind；禁止按名 `==`。 |
| 2026-10-22 | **修订 Done：** 身份改为 `TagName`；删除 Tag 上 `m_Manager*`；`m_Index` 仅缓存；`std::hash` + `operator<`；`IsValid`/Matches 经全局 Manager。Verified：`test gameplay-tags` / `gameplay-events`。 |
