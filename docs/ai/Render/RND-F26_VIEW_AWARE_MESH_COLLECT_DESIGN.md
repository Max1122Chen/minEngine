# RND-F26 — View-Aware Mesh Collect（Proxy 子 mesh 剔除钩子）

## Meta
- **ID:** RND-F26
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-10-24（S01/S02 + minCraft Section 细剔）
- **Related:**
  - 前置粗筛：[RND-F19](./RND-F19_CPU_MESH_VISIBILITY_DESIGN.md)（Primitive AABB × 主视锥；`Math::Geometry::Frustum`）
  - Collect 契约：[RND-F18](./RND-F18_MESH_BATCH_PASS_MDC_REFACTOR_DESIGN.md)
  - 阴影光锥（旁路，**本期不做**）：[RND-F21](./RND-F21_SHADOW_FRUSTUM_CASTER_CULLING_DESIGN.md)
  - 驱动场景：minCraft `ChunkMeshSceneProxy`（整柱 AABB + 多 Section batch）
  - 对照（只读）：UE `FPrimitiveSceneProxy::GetDynamicMeshElements` + `FHierarchicalStaticMeshSceneProxy` 内细剔
- **Worktree / branch:** `D:/Dev/GitRepo/minEngine-render` · `feat/render`

## TL;DR

**问题：** F19 只在 `BuildRenderQueue` 用 **整 Primitive AABB** 粗筛；`GetMeshBatches` 无 View，多 Section Proxy（如体素柱）一旦通过粗筛就整柱上传，主视 draw / Per-Object UBO 在低头等朝向下被放大。

**方案：** 引擎提供 **带 Collect 意图 + 可选主视锥** 的收集上下文；Proxy **有权**按子 AABB 少 emit。默认 Proxy 行为不变。体素子剔除策略留在 game Proxy（minCraft），引擎用测试 Fake Proxy 验收机制。

**状态：** In Progress — 维护者批准 Option A；实现见 [Implementation](./RND-F26_VIEW_AWARE_MESH_COLLECT_IMPLEMENTATION.md)。

## Problem

今日数据流：

```text
BuildRenderQueue
  → Frustum.Intersect(ResolveWorldBounds())   // 整柱 / 整 mesh
  → GetMeshBatches(collector)                 // 无 View；有几何的 Section 全交
  → Opaque / Translucent / ShadowDepth
```

对 minCraft：`ChunkMeshComponent::GetBoundingBox()` 是各 Section 的 **合并柱**；`ChunkMeshSceneProxy` 再按 Section 出 batch。粗筛通过 ≠ 每个 Section 在主视锥内。平视身后柱被粗筛掉；**低头**时附近整盘柱更容易通过粗筛，主视 `M` 上升，叠阴影 ring 易触 `kPerObjectRingSlots`（512）告警。

这不是 F19 算错，而是 **缺「场景粗筛之后、Proxy 声明子 batch 可见性」那一层**（类 UE HISM 在 GDME 内细剔，而非 StaticMesh 默认全吐）。

## Requirements

- Collect API 必须能把 **本帧主视锥**（及 Collect 意图）交给 Proxy。
- 默认 `PrimitiveSceneProxy` / Static / Skeletal / Sprite：**行为与今日等价**（忽略锥或无子结构可剔）。
- Proxy **可以**只 `AddMesh` 与主视锥相交的子 batch；无效子 AABB fail-open（与 F19 一致：宁多画、勿漏画）。
- **ShadowOnly Collect** 不得误用主视锥剔掉身后 caster 的子 mesh（否则阴影回退）；意图枚举必须区分 MainView vs ShadowOnly。
- 引擎侧有 **可重复单测**：Fake 多 Section Proxy + 已知 Frustum → 只 emit 相交 Section。
- 开关可关（默认 On）：Off 时上下文不带锥 / 或 Proxy 约定忽略（实现选一种，见 §API）。

## Constraints

- 保持 F19 契约：主锥 ≠ 阴影可见集；ShadowDepth **仍**可收集主锥外 Primitive。
- 不引入 Nanite / GPU Scene / BVH 通用空间索引。
- 单线程 Collect（与今日一致）。
- Forward-only：可改 `GetMeshBatches` 签名；游戏仓 override 需同步（minCraft）。
- 源码注释无 Feature/Slice ID（hard-constraints）。

## Preferences

- 命名贴近现有 `MeshBatchCollector` / F19 `Frustum`，避免第二套「VisibilityManager」。
- 学习对照 UE：场景粗筛 + Proxy emit 权威；不抄 HISM 向量化 BVH。

## Assumptions

- minCraft 将在引擎 API 落地后改 `ChunkMeshSceneProxy`（消费者仓；可与引擎切片并行或紧随）。
- 近期产品可另关 `EnableShadows` 减压；**不替代**本 Feature 的主视细剔。
- `MeshBatch.m_BoundingBox` 已是 world AABB（F19 契约）；子剔除用同一空间。

## Non-Goals

- **RND-F21** 光视锥 / 每 cascade caster 剔除。
- 遮挡剔除、LOD、距离剔除、多线程 Collect。
- 引擎内建「体素 Column」类型或强制每 Section 独立 Primitive。
- 加大 Per-Object UBO ring（治标，另议）。
- 改 minCraft 关阴影（产品策略，非本 Feature DoD）。

## Current Architecture

| 单元 | 今日 |
|------|------|
| `ForwardRenderer::BuildRenderQueue` | 主锥 × `ResolveWorldBounds`；main vs shadowOnly 分流后 `GetMeshBatches(collector)` |
| `PrimitiveSceneProxy::GetMeshBatches` | 仅 `MeshBatchCollector&` |
| `Math::Geometry::Frustum` | F19 已落地；`FromViewProjection` / `Intersect(AABB)` |
| Engine proxies | 单 batch 或无子 AABB 列表 |
| minCraft `ChunkMeshSceneProxy` | 最多 `kSectionCount`（24）slot；未测锥 |

## Proposed Architecture

### 3.1 原则

1. **Capabilities, not Opinions** — 引擎提供 Context；是否按 Section 剔是 Proxy 策略。
2. **粗筛仍在 Renderer** — 不把 Primitive 级剔除下沉进每个 Proxy。
3. **细筛可选** — 无子结构的 Proxy 零成本忽略 Context。
4. **意图显式** — MainView 可带主锥；ShadowOnly 默认不应用主锥细剔（直至 F21 另传光锥）。

### 3.2 类型（建议名）

```text
enum class MeshCollectPurpose : uint8_t
{
    MainView,     // Opaque / Translucent（及同 Primitive 进 Shadow 的 main 支路）
    ShadowOnly,   // 仅阴影候选（主锥外 caster）
};

struct MeshCollectContext
{
    MeshCollectPurpose Purpose = MeshCollectPurpose::MainView;
    // Non-owning; null = 不做子 AABB 视锥测试（开关 Off 或 ShadowOnly 默认）
    const Math::Geometry::Frustum* ViewFrustum = nullptr;
};
```

路径建议：`Runtime/Function/Render/DrawCommands/MeshCollectContext.h`（或旁挂 `MeshBatchCollector.h`）。

### 3.3 API

```text
virtual void GetMeshBatches(
    MeshBatchCollector& collector,
    const MeshCollectContext& context) const;
```

- **Renderer：**  
  - 主支路：`Purpose=MainView`，`ViewFrustum=&mainFrustum`（开关 On 且 cull 启用时）。  
  - 阴影支路：`Purpose=ShadowOnly`，`ViewFrustum=nullptr`（F26 默认）。  
- **Default Proxy：** 忽略 `context`，行为同今。  
- **细剔 Proxy：** 仅当 `context.ViewFrustum != nullptr` 时对子 world AABB 做 `Intersect`；false 则 skip 该 Section。

可选兼容：保留无 Context 的 protected 辅助，或一次性改全 override（仓内 callers 少：ForwardRenderer + 测试；**外仓** minCraft 一处）。

### 3.4 数据流（目标）

```text
BuildRenderQueue
  build mainFrustum (F19)
  for each enabled proxy:
    if !Intersect(primitiveBounds) && !wantShadow: continue
    if mainVisible:
      GetMeshBatches(mainCollector, {MainView, &mainFrustum})
    else:
      GetMeshBatches(shadowOnlyCollector, {ShadowOnly, nullptr})
  Dispatch → Opaque / Translucent / ShadowDepth
```

### 3.5 minCraft（消费者，非引擎实现阻塞）

`ChunkMeshSceneProxy`：对每个 occupied slot，world AABB = `Transform(LocalBounds, localToWorld)`；若 `ViewFrustum` 非空且不相交则 skip。线性扫 ≤24，无需 BVH。

引擎 DoD **不要求**合入 minCraft；ACTIVE_WORK / 验收可列「消费者跟进」为并行项。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `ForwardRenderer::BuildRenderQueue` | Primitive 粗筛；填充 `MeshCollectContext`；分流 Collect | 认识 Section / 体素 |
| `MeshCollectContext` | 传递 Purpose + 可选 Frustum | 持有场景或相机寿命 |
| `PrimitiveSceneProxy`（默认） | 全量 emit（兼容） | 子剔除策略 |
| 游戏 / 测试 Proxy | 可选子 AABB 策略 | 改 Renderer 粗筛 |
| `Frustum` | 数学相交 | 渲染策略 |

## Boundaries

- **Allowed:** Renderer → Proxy Collect；Proxy → `collector.AddMesh`；Proxy 只读 `context.ViewFrustum`。
- **Forbidden:** Proxy 依赖 `ForwardRenderer` / 游戏体素类型；在 ShadowOnly 路径默认套主锥细剔；把 F21 光锥塞进本 Feature 范围。

## Dependencies

- Depends on: F18 Collect、F19 `Frustum`、world `MeshBatch.m_BoundingBox` 契约。
- Depended on by: minCraft Chunk 渲染；未来其它多 batch Proxy。
- F21 可后续在 ShadowOnly 填入 light/cascade Frustum；F26 只预留 Purpose，不实现光锥。

## Ownership & Lifetime

- `MeshCollectContext`：栈上/帧局部；Frustum 指针指向 `BuildRenderQueue` 内局部 `mainFrustum`，Collect 同步完成前有效。
- Collector：帧局部，不拥有 GPU。
- `Not applicable` beyond frame-scoped pointers.

## Data Flow

`Camera ViewProj → Frustum → (primitive cull) → MeshCollectContext → GetMeshBatches → MeshBatch[] → Pass Processors → MDC / UBO`

## Control Flow

同步、单线程；与今日 Collect 相同调用栈深度 + Context 参数。

## State & Invariants

- `Purpose==ShadowOnly` ⇒ F26 下 `ViewFrustum==nullptr`（实现断言或文档契约）。
- `ViewFrustum==nullptr` ⇒ Proxy 不得假定「全不可见」；应 emit 全部子 mesh（或仅按非视锥策略过滤）。
- 子 AABB 无效 ⇒ 该子 mesh fail-open（emit）。
- Primitive 级无效 AABB ⇒ 保持 F19 fail-open（粗筛视为可见）。

## API / Interface Semantics

| API | 语义 |
|-----|------|
| `GetMeshBatches(collector, context)` | 只描述；不改 Proxy 持久状态；不提交 GPU |
| `context.ViewFrustum` | 非拥有；可空 |
| 错误 | 无抛错；skip = 不 AddMesh |
| 线程 | 仅现有渲染 Collect 线程 |

## Failure Model

| 模式 | 行为 |
|------|------|
| 空 Context / 空锥 | 全量 emit（安全默认） |
| 错误把主锥用于 ShadowOnly | **禁止**（契约 + 测试） |
| 子剔除过猛 | 画面缺块 — 保守 Intersect；开关 Off 对比 |
| 外仓未改 override | 链接/编译失败 — 迁移说明 + 一次性签名变更 |

## Alternatives & Trade-offs

### Option A（推荐）— Collect Context + 可选 Frustum

- Pros：对齐 UE 分层；默认零行为变化；测试可验收；为 F21 留 Purpose。
- Cons：签名变更波及外仓。

### Option B — 每 Section 独立 Primitive

- Pros：只靠 F19。
- Cons：代理/组件数暴涨；不符合「一柱一组件」；非本痛最小改动。

### Option C — 仅加大 UBO ring / 关阴影

- Pros：快。
- Cons：不解决多 draw；关阴影是产品刀，不是机制。

**选用 A。**

## Integration Impact

- Engine：`PrimitiveSceneProxy` 及子类、`ForwardRenderer::BuildRenderQueue`、相关测试。
- Consumer：minCraft `ChunkMeshSceneProxy`（跟进）。
- 无资产格式变更。

## Migration / Compatibility

- 一次性改虚函数签名；无长期双 API。
- 开关（建议 `ForwardRenderer` 已有 frustum 开关旁挂「子剔除启用」或复用：粗筛 Off 时 Context 不传锥）。
- 退出：所有 in-tree override 已改；文档注明外仓。

## Verification Strategy

- **单测（引擎）：** FakeMultiSectionProxy（2+ section AABB）+ 已知 Frustum → emit 计数；ShadowOnly + 身后 Primitive → 子 mesh 仍全交（`ViewFrustum==nullptr`）。
- **回归：** 现有 smoke / 相关 render suite；开关 Off ≈ 今日 Collect 规模。
- **手验（可选）：** Editor 普通场景无回归；minCraft 低头 Opaque 计数下降（消费者跟进后）。

## Open Questions

1. 开关形态：独立 bool vs 「粗筛 Off ⇒ 细剔也 Off」？ — **建议**细剔跟随 `m_bFrustumCullMainView`（少开关）；blocking? no  
2. main 支路进 Shadow 的 batch：已通过 MainView 细剔 — 身后 caster 走 ShadowOnly 全交，是否接受？ — **是**（F19 同构）；F21 再收紧。blocking? no  
3. minCraft 跟进是否写入本 Feature DoD？ — **建议否**（机制在引擎验收）；ACTIVE_WORK 列并行。blocking? no  

## Design Review

- **Verdict:** Ready
- **Reviewer / date:** maintainer 2026-10-24
- **Link or summary:** 批准 Option A；Open Questions 取默认（细剔跟随粗筛开关；minCraft 不进引擎 DoD）

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

- [x] Design review / 维护者批准 Scope 与 Option A
- [x] Implementation plan traces to design（L2）
- [ ] Progress log 在 Status 变更时更新
- [ ] Feature registry 状态同步

## 建议切片

| Slice | Goal | Verify |
|-------|------|--------|
| S01 | `MeshCollectContext` + 改 `GetMeshBatches` 签名；Renderer 填 Context；默认 Proxy 忽略 | 编译；smoke |
| S02 | 引擎 Fake Proxy 单测（MainView 细剔 + ShadowOnly 不套主锥） | `test frustum` |
| S03 |（可选）消费者跟进说明 | 笔记 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-24 | Draft：View-aware Collect；与 F19/F21 边界；待审批 |
| 2026-10-24 | 批准 Option A → In Progress；开工 S01 |
| 2026-10-24 | 批准 → In Progress；开 Implementation |
