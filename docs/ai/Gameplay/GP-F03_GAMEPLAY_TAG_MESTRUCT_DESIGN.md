# GP-F03 GameplayTag as ME_STRUCT — Design Spec

## Meta
- **ID:** `GP-F03`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** 2026-10-18
- **Related:** [GP-F01](./GP-F01_GAMEPLAY_TAG_DESIGN.md)（Done；本 Feature 补持久名，不改「运行时按 index 比较」）· [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [CORE-F39](../Platform/Serialization/CORE-F39_VALUE_CODEC_REGISTRATION_DESIGN.md)（Cancelled）· UE `FGameplayTag`
- **Reference:** minCraft `DATA-F05` 需要 `ME_PROPERTY GameplayTag` 单轨

## TL;DR

`GameplayTag` 的运行时语义保持 `GP-F01`：**Init 之后用词表 index 做相等与层级匹配**，禁止用字符串比较当热路径。本 Feature 只补缺口——升为 `ME_STRUCT`，增加**仅用于存档的 `TagName`**；反序列化后必须重新 Bind 到当前 Manager 的 index。不引入 value-codec；不把「按名比较」写成身份模型。

## Problem

今天的句柄只有 `Manager*` + `index`：

- 运行时正确（快），但**没有可进磁盘的字段**，反射无法当 `ME_STRUCT` 展开，也不能当 `ME_PROPERTY`。
- minCraft 被迫再存 `std::string` 旁路。

缺的是持久层，不是换掉 index 身份。

## Requirements

- `GameplayTag` 为 `ME_STRUCT`。
- **持久字段：** `TagName`（`std::string`）可序列化。
- **运行时身份：** Bind 之后的 `index`（及绑定的 Manager）；`operator==`、`Matches`、`IsChildOf`、Container 键路径**只使用 index**（及 Manager 一致性），**不得**用 `TagName` 字符串比较实现这些 API。
- `Resolve` / `TryResolve` / Native `GetTag()` 产出的 Tag 必须已 Bind（名 + index 一致）。
- 反序列化读入 `TagName` 后，在约定时机 Bind；成功则 `IsValid()==true` 且 index 可用；失败则无效句柄（见 Failure）。
- `ME_PROPERTY() GameplayTag` 与 `vector<GameplayTag>` 走现有 struct/array 序列化。
- `test gameplay-tags` 行为回归 + 至少一条 JSON roundtrip；roundtrip 后比较仍走 index 语义（`a == b`、`Matches`）。

## Constraints

- index / Manager **不是** `ME_PROPERTY`（比 Transient 更硬）。存档只认 `TagName`。
- 同一进程一份 Manager；index 只在该 Manager 的词表世代内有意义。Shutdown / 重建词表后，未重新 Bind 的 Tag 不得当作有效句柄使用。
- 前向开发：不保留「仅名字、无 index 的合法热路径句柄」。未 Bind 的 Tag 可以短暂存在（刚反序列化完），但在 Bind 完成前不得用于 Container::Add / 玩法匹配（`IsValid()==false`）。
- 不引入引擎级 `FName` 表（本期）；持久名用 `std::string`。效率靠 Bind 后的 index，不靠字符串 intern。

## Preferences

- 对齐 UE 的分工：磁盘/作者侧是名字；运行时比较要快。UE 用 `FName`（内部已是 intern + 快速相等）；我们没有 `FName`，故显式保留 Manager index，这是对同一目标的正确映射，不是「可选优化」。
- Bind 实现放在 Manager（`BindTag` / 反序列化后钩子），Tag 上不公开「只改名不改 index」的 API。

## Assumptions

- `ME_STRUCT` + `std::string` 属性 + `Transient` 跳过序列化已可用。
- 读写含 Tag 的资产时 Manager 已 `Initialize`（与今天一致）。

## Non-Goals

- 用 `TagName` 字符串实现 `==` / `Matches`（明确拒绝）。
- `CORE-F39` value-codec。
- Container 自身序列化、Tag 拾取器、重定向表、网络 net-index。
- 业务词汇进引擎默认表。

## Current Architecture

| 部件 | 今天 | 要保留的 |
|------|------|----------|
| 运行时比较 | index + Manager* | **保留为唯一比较路径** |
| 名字 | 只在 Manager 节点里 | **复制一份到 Tag 上供存档** |
| Container | `map<index, count>` | 保留 |
| 序列化 | 不可用 | 本 Feature 解锁 |

## Proposed Architecture

### 双层身份（强制）

```text
GameplayTag (ME_STRUCT)
├── TagName : std::string     ME_PROPERTY          // disk / authoring
├── m_Index : uint32          非属性（不进反射）    // runtime identity after Bind
└── m_Manager : const*        非属性（不进反射）    // runtime binding
```

index / Manager **不用** `ME_PROPERTY(Transient)`：根本不进属性表，避免漏标 Transient。

| 层 | 字段 | 用途 |
|----|------|------|
| 持久 | `TagName` | JSON/资产；空字符串 = 空 Tag |
| 运行时 | `m_Index` + `m_Manager` | `==`、`Matches`、`IsChildOf`、`GetIndex()`、Container |

**Invariant（有效句柄）：**  
`IsValid()` 为真 ⇒ Manager 已 Init、index 在词表内、且 `Manager.GetTagName(index) == TagName`。  
任一不一致 ⇒ 无效（不得静默用字符串继续比）。

### Bind

`GameplayTagManager::Bind(GameplayTag& tag)`（名字可调整）：

1. 若 `TagName` 为空 → 清 index / Manager，返回 false（空 Tag）。
2. 若 Manager 未 Init → 清运行时绑定，返回 false。
3. 查 `TagName` → index；失败 → 清运行时绑定，保留 `TagName`（便于报错），返回 false。
4. 成功 → 写 `m_Index` / `m_Manager`，返回 true。

调用点：

- `Resolve` / `TryResolve`：构造时直接填名 + index（已 Bind）。
- 反序列化：`TagName` 为 `ME_PROPERTY(Invisible, Setter=SetTagName)`。Serializer 在有 Setter 时走 temp + `AssignProperty` → `SetTagName` → 写名并 `BindTag`。**不**做对象级反射遍历。
- 不变量：Engine / 测试在反序列化含 Tag 的资产前已 `GameplayTagManager::Initialize`（Manager 先于内容 Load）。
- 不在每次 `operator==` 里按名查表。

### 语义（相对 GP-F01）

| API | 行为 |
|-----|------|
| `IsValid()` | 已 Bind 且 Manager 仍认该 index（同今日强度） |
| `GetName()` | 已 Bind：可从 Manager 或 `TagName` 读（二者 invariant 相等）；未 Bind：返回 `TagName`（可能非空但无效） |
| `GetIndex()` | 已 Bind 返回 index；否则哨兵。**不是**「现场按名查表伪装成 get」 |
| `operator==` | 双方均有效且同一 Manager 且 index 相等；双方均无效可定为相等（空==空）。**禁止** `TagName == other.TagName` 作为实现 |
| `Matches` / `IsChildOf` | 同 GP-F01：有效性 + index 相等或沿父链（父链仍用 Manager 的 ParentIndex） |

写出序列化：只写 `TagName`（有效 Tag 写出当前名；空 Tag 写 `""`）。有效 Tag 的 `TagName` 在 Bind 时已与词表同步；若运行期禁止改名，则写出前无需再查。

### 与 UE 的对应（源码锚点）

锚点：`Engine/Source/Runtime/GameplayTags/Classes/GameplayTagContainer.h`（`FGameplayTag`）；相等见 `TagName == Other.TagName`；`FName` 相等走 `ComparisonIndex`（`NameTypes.h`）；磁盘 `Ar << TagName`；`WithPostSerialize`；层级 `MatchesTag` → `FindTagNode`（`GameplayTagContainer.cpp`）。

| UE | minEngine（本设计） | 是否字面相同 |
|----|---------------------|--------------|
| `USTRUCT` + 唯一持久 `FName TagName` | `ME_STRUCT` + 唯一持久 `std::string TagName` | 同构（无 `FName` 表） |
| `operator==` 比较 `FName`（intern 的 ComparisonIndex，**不是** Tag 树节点下标） | `operator==` 比较 Manager **词表 index** | **意图对齐、布局不同**：我们没有全局 `FName`，词表 index 承担「非字符串比较」 |
| `IsValid()` = `TagName != None`（不强制已在 Tag 字典） | `IsValid()` = 已 Bind 且在词表 | **有意更严**（匹配前必须注册，贴近 UE `MatchesTag` 对未注册 Tag 的 ensure） |
| 无 `Manager*` 在 struct 上 | 保留 `m_Manager*` 防止错 Manager | 我们多字段；可接受 |
| `PostSerialize` 重定向 | `BindTag`（本期无重定向表） | 同阶段：读名 → 固定到运行时可匹配状态 |
| 无 `FGameplayTagProperty` | 无专用 Property 子类 | 一致 |

### Container

不变：仍按 index 计数。`Add` 要求 `IsValid()`（已 Bind）。不序列化 Container 本体（本期）。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `GameplayTag` | 持有名 + 运行时绑定；**仅用 index 做比较/匹配** | 词表所有权；在比较里查字符串 |
| `GameplayTagManager` | 词表、Resolve、Bind、父链 | 把 index 写入资产 |
| Serializer | 读写 `TagName` | 调用 Bind（除非实现选择序列化钩子；默认由 Load 路径显式 Bind） |
| 宿主 Load / 测试夹具 | 反序列化后确保 Bind | 改比较语义 |

## Boundaries

- Allowed：Tags ↔ Reflection；Load 路径调用 Bind。
- Forbidden：`==` / `Matches` / `IsChildOf` 的字符串实现；序列化 index/Manager*；Core 为 Tag 写专用 codec。

## Dependencies

- Depends on：`GP-F01` 词表与匹配语义；`ME_STRUCT` / Transient；JSON 对象序列化。
- Depended on by：minCraft 去掉 string 旁路。
- CORE-F39：不依赖（已 Cancelled）。

## Ownership & Lifetime

- Tag 值可拷贝；拷贝复制名与运行时绑定。
- Manager 由 Engine 拥有。Manager Shutdown 后：现存 Tag 的运行时绑定作废；`IsValid()` false。`TagName` 字符串仍在，需重新 Init + Bind 才可用。

## Data Flow

```text
Authoring / disk:  TagName
       │
       ▼
   Bind(Manager)
       │
       ▼
Runtime hot path:  m_Index (+ m_Manager) ──► == / Matches / Container
       │
       ▼
Serialize out:     TagName only
```

## Control Flow

- Resolve：同步 Bind。
- Deserialize：读字段 →（随后）Bind → 使用。
- 无异步。

## State & Invariants

- 有效 ⇒ Bind 成功且名/index 一致。
- 热路径从不 strcmp `TagName`。
- 非法：把未 Bind 的非空 `TagName` 当作可匹配句柄；序列化 index。

## API / Interface Semantics

- 不提供公开「只设 TagName、调用方忘记 Bind」的常规 API。若反射/测试必须写名，随后必须 Bind。
- `Resolve`：未知名 — 保持 GP-F01（Error/assert）。`TryResolve`：未知 → 空 Tag。
- Bind 失败：保留 `TagName`，清 index/Manager，`IsValid()` false。

## Failure Model

| 情况 | 行为 |
|------|------|
| 反序列化未知名 | 读入 `TagName`；Bind 失败；`IsValid()` false；名字保留供日志/Validate |
| 未 Bind 就 `==` / `Matches` | 按无效句柄处理（false / 空相等规则）；不按名比较 |
| Manager 未 Init | Bind 失败；查询无效 |
| Container::Add 无效 Tag | 保持 assert |

## Alternatives & Trade-offs

### Option A（选定）— 持久 TagName + 强制运行时 index

- Pros：可序列化；热路径仍 O(1) index；与 GP-F01 精神一致；对齐 UE「名持久、比较要快」。
- Cons：反序列化后多一步 Bind；拷贝带字符串成本（可接受；热路径比的是 index）。

### Option B — 只存 TagName，比较也用字符串 / 每次查表

- Why not selected：与 GameplayTag 引入 index 的目的相反（维护者否决）。

### Option C — index-only + 外部 codec 写名

- Why not selected：名字不属于句柄，编解码别扭；已取消 CORE-F39。

## Integration Impact

- `GameplayTag` / Manager / Container / Native；GP-F02 若持 Tag。
- 序列化：Transient 字段；Load 后 Bind 挂钩点。
- 测试与（后续）minCraft 旁路删除。

## Migration / Compatibility

- 无旧引擎资产含 Tag。无磁盘迁移。
- API：比较语义保持 index；新增持久名与 Bind 义务。

## Verification Strategy

- `gameplay-tags`：Resolve、**index 相等**、Matches/IsChildOf、Container；断言实现未走字符串比较（可用「同名但未 Bind 不得匹配」或测试友元查路径，实现计划选一种可维护做法）。
- Roundtrip：写出 Tag → 读回（`SetTagName` 自动 Bind）→ `==` 与 Matches 与源有效 Tag 一致；未知名 Bind 失败且名保留。`GameplayTagListProbe` 覆盖 `vector<GameplayTag>`。
- `gameplay-events` 回归。
- **Verified 2026-10-18：** `minEngineTests.exe test gameplay-tags` / `gameplay-events` PASSED。

## Open Questions

- 无阻塞项。

## Design Review

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** Agent + maintainer direction, 2026-10-18
- **Link or summary:** UE 对齐见上表。实现锁定：`Invisible` + `SetTagName` 自动 Bind；Manager 先于内容 Load；不做 Property 树遍历。Deferred：重定向、Container 序列化、Tag 拾取器。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes`

## Acceptance Checklist

- [x] Design review complete when required by complexity/profile
- [x] Implementation plan — waived：L2 直接按 Design 落地；无独立 Impl Plan 文件
- [x] Progress log updated when status changes
- [x] Feature registry status synced
