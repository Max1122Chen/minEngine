# ASSET-F09 — Async DataAsset Load Design Spec

## Meta
- **ID:** `ASSET-F09`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-24`（S00–S03 落地；`data-asset` async smoke 绿）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [CORE-F38 TaskSystem](../Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md)（池 + Drain；**已 Done**）
  - [CORE-F44 Serializer File/Json/Object Split](../Platform/Serialization/CORE-F44_SERIALIZER_FILE_JSON_OBJECT_SPLIT_DESIGN.md)（**前置 Draft**；Worker 用 File→Json，组装用 Json→Object+Resolve）
  - [ASSET-F10 Polymorphic Load](./ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md)（`RuntimeClassName`；S01/S02 Done）
  - [ASSET-F07](./ASSET-F07_DATA_ASSET_DESIGN.md) · [ASSET-F08](./ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN.md)
  - 消费动机：minCraft 启动期批量 `LoadAssetByPath` Catalog / Visual / Bank 等 `.medata`
- **Depends on:** CORE-F38 Done；**CORE-F44**（File→Json / Json→Object+Resolve 可单独调用）；ASSET-F10 同步多态 Load（S01/S02）
- **Branch:** `feat/asset`（或既有 asset 区分支；勿用 ID 命名）

## TL;DR

把 **DataAsset** 加载拆成两段：

1. **Worker：** `LoadJsonFromFile`（或等价）→ 内存 **`Json`**。**不** `NewObject`。
2. **游戏线程：** `NewObject` → **`FromJson`（填字段 + 同步 `ResolvePendingObjectRefs`）** → 写缓存 → 回调。

嵌套资产引用 **MVP 固定同步递归**（与今日 `FromFile` 一致：Resolve → `LoadAssetByGUID`）。**不做**异步依赖图，也**不提供** Sync/Async 递归双模式开关。完成通知走 `DrainCompletions`。公开 API **`AsyncLoadAsset*`**（MVP 仅 DataAsset）。同步 Load 可 `Wait` Worker，**禁止**为合流 `Drain`。

## Problem

1. minCraft（及类似项目）在启动 / 进世界时同步加载大量 DataAsset（Block/Item/Recipe Catalog、Visual Catalog、SoundBank…）。整文件 `FromFile` 堵在游戏线程，帧时间尖刺。
2. 引擎已有 `TaskSystem`（F38），但 **AssetManager 仍全同步**；没有「后台读盘、主线程建对象」的官方路径。
3. 若草率在 Worker 上 `NewObject` / 写缓存，会破坏对象模型与单线程假设。
4. 若同步 Load 为等异步结果而去调 `DrainCompletions` 或阻塞等回调，会与「Drain 只在帧挂点 / 游戏线程派发」缠死。

## Intent playback（设计输入）

| 项 | 内容 |
|----|------|
| 用户目标 | minCraft 大量 DataAsset 加载需要异步能力 |
| 成功 | 可提交一批 DataAsset 异步加载；帧内 Drain 后拿到实例；同步 Load 仍可用且不与异步互锁死 |
| 非目标 | 全资产通用 Streaming、Texture GPU 上传、**异步依赖图**、逻辑/渲染分线程、Sync/Async 递归双开关 |
| 约束 | Worker 禁 NewObject/Asset 缓存突变；可持有纯数据 `Json`；完成只经 Drain；F10 多态类身份不变；嵌套引用同步 Resolve |

## Requirements

### Must

1. **范围仅 DataAsset**（`AssetType == "DataAsset"` / `.medata`）。其它类型继续同步 Load；`AsyncLoadAsset*` 传入非 DataAsset → 拒绝并报错（名称预留扩展，MVP 行为收窄）。
2. **两段式加载（维护者模型，已锁定）：**
   - **Worker：** 调用 **CORE-F44** `LoadJsonFromFile`（或 Helper）。IO / JSON 语法 / disk meta 失败在此标记。**禁止** `NewObject`、写 `m_LoadedAssetCache`、反射装盘进 `MEObject`、RHI、Scene、Lua、`FromJson` / Resolve。
   - **游戏线程（组装）：** 按 F10 规则 `RuntimeClassName` → `NewObject` → **`FromJson`（Deserialize + 同步 ResolvePendingObjectRefs）** → `ApplyMetaIdentity` → 写缓存 → 派发完成回调。
2b. **嵌套引用（MVP，已锁定）：** 组装阶段 Resolve 走今日同步路径（`LoadAssetByGUID` 等），可递归同步加载依赖。`AsyncLoad(A)` 完成 = A 可用且硬引用已按同步语义绑好。**禁止**在 Drain/Resolve 中再 `AsyncLoad` 并等待回调（死锁）。异步依赖图 / handle-join **后置**；**不**做运行时 Sync|Async 递归选项。
3. **公开异步 API（MVP）命名：** `AsyncLoadAssetByPath` / `AsyncLoadAssetByGUID` / `AsyncLoadAssetByMeta`（见 API 节）。返回不透明 `AsyncLoadHandle`；完成回调仅在游戏线程、经 Drain 路径调用。
4. **完成回调语义：** `(std::shared_ptr<Asset> asset, bool bSucceeded, std::string_view error)`（或等价）。失败时 `asset` 为空且 `bSucceeded == false`。**不**恢复 `LoadAsset<T>` 糖衣。
5. **缓存命中：** 若 `m_LoadedAssetCache` 已有活实例 → **入队本帧 Drain** 再回调（环境一致；见 Decisions）。
6. **请求合并（coalesce）：** 同一规范化路径上多个未完成请求共享一次 Worker + 一次组装；全部回调收到同一 `shared_ptr`（成功）或同一错误。
7. **同步 Load 与 in-flight 协作（死锁禁令）：**
   - 目标为 DataAsset 且同键 in-flight：可阻塞等待 **Worker 阶段结束**（`Json` 就绪或失败；`TaskSystem::Wait`），再在 **当前游戏线程** 组装（若尚未组装）。
   - **禁止**同步 Load 为完成而调用 `DrainCompletions`，或阻塞等待「完成回调已执行」。
   - **同步 Load 仅游戏线程**；否则拒绝。
8. **Json → Object：** 组装使用 CORE-F44 **`FromJson`**（含同步 Resolve）。需要 fill-only 时才用 `DeserializeObjectFromJson`。同步冷路径统一：Worker 出 `Json`，再 `FromJson`（避免两套 IO/parse）。
9. **TaskSystem：** Worker 作业经 `Submit`；组装与用户回调经完成队列 + Drain。
10. **验证：** 异步成功/失败、缓存命中、coalesce、同步合流、Worker 不建 `MEObject`；含硬引用的 DataAsset 与同步 `LoadAssetBy*` 引用结果一致；可选批量 wall-clock（非 CI 硬倍率）。

### Should

11. 请求时校验 meta 存在、`AssetType`、非空 `RuntimeClassName`（可在提交前失败入队，不启 Worker）。
12. 诊断日志：Submit / Worker 完成 / 组装失败原因。

### Must not

14. Worker 上 `NewObject` / 把属性写入 `MEObject`（**`Json` 纯数据树可以**；这是与「只读字节」方案的明确区别）。
15. 取消令牌全家桶、优先级、**异步依赖图**、Streaming LOD、**Sync/Async 递归双模式开关**。
16. Texture2D / Mesh / Material / Prefab 异步（本 Feature）。
17. 改磁盘格式；改 F10 `RuntimeClassName` 规则；在本 Feature 内重做 Serializer 分段（属 CORE-F44）。

## Constraints

- Capabilities not Opinions：机制是「DataAsset 异步装载」；不规定 Catalog 合并策略。
- F38 Boundaries：Worker 禁表不变。
- 游戏线程 = `TaskSystem::Init` 记录的线程。
- 源码注释不含 Feature/Slice/Bug/ADR ID。
- 公开 API `MINENGINE_API`；不扩 `.def`。

## Preferences

- API 挂在 `AssetManager`；命名 **`AsyncLoadAsset*`** 强调异步（相对同步 `LoadAssetBy*`）。
- Worker 承担 **读盘 + JSON parse**，减轻游戏线程尖刺（Catalog 大文件时 parse 往往不轻）。
- 完成回调入队 Drain；同 path coalesce。
- 性能证明可用批量 `.medata`；不要求固定加速比进 CI。

## Assumptions

- DataAsset 落盘为 JSON 文本；Worker 经 F44 `LoadJsonFromFile` 产出的 `Json` 与今日 `FromFile` 内部根对象等价。
- `Json`（`nlohmann::json`）可在线程间移交所有权（`shared_ptr<Json>` / move 进完成载荷）；无引擎对象副作用。
- F10 后合法 `.medata` 有可用 `RuntimeClassName`。
- minCraft 可改用 `AsyncLoadAsset*` + 若干帧 Drain；引擎不强制改消费仓。
- 不必上二进制 Asset 格式。

## Non-Goals

- MVP 就让 `AsyncLoadAsset*` 支持全部 AssetType（名称可扩展；行为本波仅 DataAsset）。
- 资源 GC / 卸载策略变更。
- Editor 专用进度条 UI。
- 引擎级 `AsyncLoadAllUnderDirectory` 糖（后置；调用方循环即可）。
- Platform Sleep / mock 时钟。

## Current Architecture

```text
LoadAssetByMeta_Internal(meta)
  → cache hit? return
  → FindLoadHandler(AssetType)
  → DataAssetLoader::Load
        → FindClass(RuntimeClassName) → NewObject → ReadObjectFromFile(absPath) → ApplyMetaIdentity
  → ValidateOrHealRuntimeClassName → m_LoadedAssetCache[path] = asset

TaskSystem (F38): Submit(work) → worker → completion queue → DrainCompletions (Input后/Fixed前)
```

缺口：无 in-flight 表；同步/异步未合流。File→Json / Json→Object+Resolve 分段由 **CORE-F44** 提供（本 Feature 消费，不重复发明）。

## Mental model（与「只读字节」的关系）

| 方案 | Worker | 游戏线程 | 评价 |
|------|--------|----------|------|
| A. 只读字节 | `vector<uint8_t>` | parse Json + NewObject + 填 Object | 更保守；parse 仍占主线程 |
| **B. Worker 出 Json（本 Design）** | 读文件 + `Json` parse | NewObject + 用 Json 填 Object | **维护者理解；推荐** — IO 与 parse 都离开主线程 |
| C. Worker 完整 Load | NewObject + 填盘 | 只挂缓存 | **禁止** — 破坏对象模型 |

硬线不变：**Worker 不碰 `MEObject` / 缓存**；`Json` 是纯值树，可以。

## Proposed Architecture

```text
AsyncLoadAssetByPath(path, onComplete)   [game thread]
  → cache hit → enqueue completion(success)
  → else in-flight map: coalesce or create entry
  → TaskSystem.Submit(LoadJsonFromFile)
        worker: Json | IO/parse/meta error
  → completion event → Drain:
        if assemble needed: NewObject + FromJson(+同步 Resolve) + cache
        invoke all waiters' onComplete

LoadAssetByPath (sync, DataAsset, in-flight):
  → Wait(workerTask)         // NOT Drain
  → Assemble on game thread if needed
  → return shared_ptr
```

### Components

| Name | Role |
|------|------|
| `AssetManager` | `AsyncLoadAsset*`、in-flight 表、与同步 Load 合流、缓存 |
| 组装路径 | `NewObject` + `FromJson`（含同步 Resolve）+ `ApplyMetaIdentity` |
| CORE-F44 Serializer | `LoadJsonFromFile` / `FromJson`；本 Feature 只调用 |
| `TaskSystem` | 调度 Worker；不感知 Asset |
| In-flight record | path key、task handle、`shared_ptr<Json>`（或错误）、组装结果、waiter 列表 |

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `AssetManager` | 请求生命周期、coalesce、缓存、同步/异步合流 | TaskFlow 内核 |
| Worker job | 读文件、`Json` parse、IO/语法错误 | `MEObject`、反射装盘、缓存 |
| 游戏线程组装 | NewObject、`FromJson`（同步递归引用）、ApplyMetaIdentity、缓存 | 再次读盘/parse；异步依赖调度 |
| Serializer (F44) | File→Json；Json→Object+Resolve | Asset 缓存 / AsyncLoad |
| `TaskSystem` | Submit/Wait/Drain | Asset 语义 |

## Boundaries

- **Allowed on worker:** 读文件；`Json` parse；局部错误字符串；`ME_LOG`。
- **Forbidden on worker:** `NewObject`、ObjectManager、写 `m_LoadedAssetCache`、Scene、RHI、Lua、ImGui、对 `MEObject` 做 Serializer 装盘。
- **Dependency：** Resource → Core/Tasks；Tasks **不**依赖 Resource。

## Dependencies

- Depends on: CORE-F38；**CORE-F44**（分段 Serializer）；ASSET-F10 Load 规则；现有 `AssetManager` 缓存键规范化。
- Depended on by: minCraft Catalog 启动路径（消费仓迁移，非本仓 Must）。
- 新方向：Resource 使用 TaskSystem（单向，已由 F38 允许）；Resource 调用 F44 公开 API（不复制 parse）。

## Nested references（讨论结论）

```text
可行但更重：Deserialize 后收集 GUID → 对子资产再 AsyncLoad → handle-join → 再绑指针
MVP：FromJson → ResolvePendingObjectRefs → 同步 LoadAssetByGUID（可递归）
```

| 模式 | MVP？ | 说明 |
|------|-------|------|
| 同步递归 Resolve | **是** | 与今日 `FromFile` 一致；完成语义清晰；无 Drain 死锁 |
| 异步递归 + join 图 | 否（后置） | 父回调须等子图；环/失败传播；禁止在 Resolve 里 Wait 子 AsyncLoad |
| 运行时 Sync\|Async 选项 | **否** | 双路径测试矩阵与「完成」语义歧义；需要时另开 Feature / 显式 API |

Texture/Mesh 等非 DataAsset 依赖若出现在字段上：仍走同步 `LoadAssetByGUID`（今日行为）；本 Feature 不把它们改成异步。

## Ownership & Lifetime

- In-flight record 由 `AssetManager` 拥有；组装成功后结果进入 `m_LoadedAssetCache`（`weak_ptr` 缓存语义保持今日）。
- 完成回调拿到的 `shared_ptr<Asset>` 与同步 Load 同质。
- Shutdown：拒绝新 `AsyncLoadAsset*`；Wait 已接受 Worker；未派发回调 Warn 并丢弃（Decisions）。

## Data Flow

1. 输入：path / GUID / meta。
2. 处理：规范化 key → cache / in-flight / Submit。
3. 中间：`Json` 树（in-flight 持有，组装后可释放）。
4. 输出：缓存中的 `DataAsset` + 回调。

## Control Flow

- 异步：`AsyncLoadAsset*` → Worker（读+parse）→ Drain（组装+回调）。
- 同步冷路径：MVP **统一** Submit+Wait Worker（出 `Json`）再组装，避免两套 IO/parse。
- 同步热路径（有 in-flight）：`Wait(workerHandle)` → 组装 → 返回；**不** Drain。

## State & Invariants

In-flight per key：`LoadingJson | JsonReady | Assembling | Completed | Failed`（实现可合并）。

| 不变量 | 维护者 |
|--------|--------|
| 同一 key 至多一次未完成 Worker | AssetManager |
| 缓存写入只在游戏线程 | AssetManager |
| 完成回调只在游戏线程 | TaskSystem Drain + AM |
| 同步 Load 从不调用 Drain | AssetManager |
| Worker 不发布 `MEObject` | 异步路径 |

## API / Interface Semantics

### `AsyncLoadAssetByPath(path, onComplete) → AsyncLoadHandle`

- 仅游戏线程 + `TaskSystem` Running。
- 命名强调异步；与同步 `LoadAssetByPath` 成对。
- MVP：meta 必须是 DataAsset，否则失败完成（入队 Drain）。
- 空句柄：提交被拒（非 Running / 非游戏线程等）。

### `AsyncLoadAssetByGUID` / `AsyncLoadAssetByMeta`

- 薄包装：解析到 path / 校验类型后走同一 in-flight 实现。

### `AsyncLoadHandle`

- 不透明。内部可对 Worker 任务 `Wait`（供同步合流）。**不**表示回调已执行。

### 同步 `LoadAssetBy*`

- 无 in-flight：观察行为与今日等价（可实现为 Submit+Wait Json 再组装）。
- 有 in-flight：合流如上。

### 组装原语

- 输入：已构造的 `MEObject` + `const Json&`（或 `shared_ptr<Json>`）。
- 使用 CORE-F44 **`Serializer::FromJson`**（内部 Deserialize + 同步 Resolve）；失败不写缓存。
- Worker **只**使用 `LoadJsonFromFile`；不得调用 `FromJson`。

## Failure Model

| Failure | Behavior |
|---------|----------|
| 文件不存在 / 读失败 | Worker Failed；Drain 回调失败；不建对象 |
| JSON **语法**损坏 | Worker Failed（parse）；Drain 回调失败 |
| JSON 语义 / 反射装盘失败 | 游戏线程组装 Failed；回调失败；不写缓存 |
| 缺/错 RuntimeClassName | 与 F10 一致 |
| Shutdown 中 AsyncLoad | 拒绝 |
| 回调抛异常 | 对齐 F38：记日志，继续同批其余 waiter |

## Alternatives & Trade-offs

### Option A（推荐，已选）— Worker = 读文件 + `Json`；游戏线程 = NewObject + 填 Object

- Pros：IO 与 parse 都离开主线程；仍遵守「Worker 无 MEObject」；契合 minCraft 大 Catalog。
- Cons：`Json` 树峰值内存；跨线程移交需明确所有权（`shared_ptr`/`move`）。

### Option A′ — Worker 只读字节；游戏线程 parse + 装盘

- Why not now：维护者明确偏好 Json-on-worker；parse 成本留在主线程收益更小。可作实现退路（若 `Json` 线程问题），非正式默认。

### Option B — Worker 上完整 Load（含 NewObject）

- Why not：违反对象模型与 F38 Boundaries。

### Option C — MVP 就通用全类型 `AsyncLoadAsset`

- Why not：Texture/Mesh 绑 GPU；本波只 DataAsset（**API 名仍用 Asset**，行为收窄）。

### Option D — 完成不走 Drain

- Why not：与 F38 回流模型分叉。

### Option E — MVP 异步递归嵌套引用 / Sync\|Async 开关

- Why not：Drain 死锁风险；完成语义分裂；测试翻倍。后置专用 API（如依赖批加载），不做隐藏 option。

### Option F — F09 内私自 File→Json，不先做 CORE-F44

- Why not：与 Serializer 分叉；维护者已确认先切序列化分段。

## Integration Impact

- `AssetManager`：`AsyncLoadAsset*` + in-flight；同步 Load 合流。
- 组装：CORE-F44 `LoadJsonFromFile` / `FromJson`。
- `TaskSystem`：调用方，无 API 变更。
- Tests：`data-asset-async`（或扩展 `data-asset`）；含引用夹具。
- minCraft：可选迁移（非本仓 Must）。

## Migration / Compatibility

- 现有同步 Load 调用方行为保持可用。
- 无磁盘格式变更。
- 无强制 API 弃用。

## Verification Strategy

### 功能（CI）

- `AsyncLoadAssetByPath` → Drain → 成功实例且 `Cast` 到派生类；与同步 Load 属性一致。
- 读失败 / JSON 语法坏 / 装盘失败 → 回调失败、缓存无条目。
- 双 AsyncLoad 同 path → 单次 Worker + 两次回调同一实例。
- 先 AsyncLoad 再同步 `LoadAssetByPath`：不 Drain 也能拿到实例；之后 Drain 回调一致。
- 缓存命中不二次 Worker。

### 性能（可选）

- N 个 `.medata`：同步串行 vs 异步全员 + Drain；记 wall-clock（非硬门闩）。

### 回归

- `data-asset`；`task-system`；smoke。

## Open Questions

| # | Question | Default | Blocking? |
|---|----------|---------|-----------|
| Q1 | 缓存命中：立即回调 vs 入队 Drain？ | **入队 Drain** | No |
| Q2 | 同步冷路径统一 Submit+Wait Worker？ | **是** | No |
| Q3 | Worker 产出 `Json` 还是仅字节？ | **`Json`（已按维护者锁定）** | No |
| Q4 | Shutdown 与未完成 AsyncLoad？ | **Wait Worker；未派发回调 Warn 丢弃** | No |
| Q5 | MVP 含 ByGUID / ByMeta？ | **是** | No |
| Q6 | 分帧组装？ | **后置** | No |
| Q7 | 嵌套引用 Sync vs Async？ | **MVP 仅同步递归**；异步图后置；无双开关 | No（已锁定） |
| Q8 | 是否先做 Serializer 分段？ | **是（CORE-F44）** | No（已锁定） |

## Decisions（已吸收维护者反馈）

| # | 决定 |
|---|------|
| 1 | MVP 行为仅 DataAsset；API 名 `AsyncLoadAsset*` |
| 2 | Worker = `LoadJsonFromFile` → `Json`；游戏线程 = NewObject + **`FromJson`（含同步 Resolve）** + 缓存 |
| 3 | 完成经 Drain；同步 Load 可 Wait Worker，**禁止**为合流 Drain |
| 4 | 同 path coalesce |
| 5 | 缓存命中完成也入队 Drain |
| 6 | 同步冷路径统一 Submit+Wait Worker |
| 7 | 不做取消 / 本波全类型实现 / GPU 异步 |
| 8 | **嵌套引用 MVP = 同步递归 Resolve**（与今日 FromFile 一致）；异步依赖图后置；**不提供** Sync\|Async 递归选项 |
| 9 | **先落地 CORE-F44**，再实现本 Feature（或同波严格先合 F44） |

## Design Review

- **Verdict:** `Approved`（维护者批准后实现）
- **Reviewer / date:** maintainer / 2026-10-24
- **Summary:** Worker→Json；组装 FromJson+同步递归；先 F44；TaskSystem 单例访问。

## Implementation Readiness

- [x] Requirements / Non-Goals / 死锁禁令
- [x] 与 F38 / F10 / F44 边界对齐
- [x] 同步/异步合流模型
- [x] 嵌套引用 MVP 策略写清
- [x] 维护者确认 Decisions
- [x] CORE-F44 Done
- [x] S00–S03 实现完成

**Ready for implementation?** `done`

## Acceptance Checklist

- [ ] Design review when required
- [ ] Impl Plan traces to Decisions
- [ ] Registry / ACTIVE_WORK / Progress 同步
- [ ] 同步 suite 不回归；异步 suite 挂 CI 正确性

## Suggested slices

| Slice | Done when |
|-------|-----------|
| S00 | 消费 F44：Worker `LoadJsonFromFile`；游戏线程 `FromJson`；与同步 `ReadObjectFromFile` / `FromFile` 含引用结果一致 |
| S01 | `AsyncLoadAssetByPath` + in-flight + Drain 回调；成功/失败 |
| S02 | coalesce；缓存命中入队；同步 Load 合流（Wait Worker、不 Drain） |
| S03 | `AsyncLoadAssetByGUID` / ByMeta；可选批量 wall-clock；文档 Done |

## Learning path

1. F38：池 + Drain（已完成）。  
2. **F44：** Serializer File/Json/Object 分段（前置）。  
3. **F09：** DataAsset：Worker 出 `Json`，主线程 `FromJson`（同步递归引用）。  
4. 后置：异步依赖图 / 分帧组装 / 其它 AssetType / GPU / 逻辑-渲染线程。

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | Draft：minCraft 批量 DataAsset 动机；两段式；同步/异步死锁禁令；Open 默认锁定草案 |
| 2026-10-24 | 维护者澄清：Worker 产出内存 `Json`（非仅字节）；API 改名 `AsyncLoadAsset*`；更新 Mental model / Decisions |
| 2026-10-24 | 嵌套引用 MVP=同步递归；不做双模式开关；前置 CORE-F44；组装改 `FromJson` |
| 2026-10-24 | Done：`AsyncLoadAsset*` + in-flight coalesce + sync join；TaskSystem 单例；`data-asset` async smoke |
