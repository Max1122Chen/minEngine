# RND-F20 — Mesh Draw Sorting（Opaque 状态序 / Translucent 深度序）

## Meta
- **ID:** RND-F20
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-22（design-review 收紧契约）
- **Related:**
  - **硬前置（半透明深度）：** [RND-F19](./RND-F19_CPU_MESH_VISIBILITY_DESIGN.md) world `MeshBatch.m_BoundingBox`（实现顺序 **F19 → F20**）
  - 入队：[RND-F18](./RND-F18_MESH_BATCH_PASS_MDC_REFACTOR_DESIGN.md) Done
  - 测量：[CORE-F22](../Platform/Core/CORE-F22_PROFILER_HARNESS_DESIGN.md)
  - 代码：`MeshDrawCommand` / `MeshDrawCommandSortKey` · `BuildRenderQueue` 之后 · Pass Execute 之前

## TL;DR

**问题：** `SortKey` 未填充、队列未排序；Opaque 状态抖动，半透明无深度序。

**方案：** Processor 产出后、Pass 消费前，单点 `SortMeshDrawQueues`——Opaque 用锁定的 64-bit `m_SortKey` + `stable_sort`；Translucent 用 **view 距离远→近** 比较器（不与 Opaque 抢 key 语义）。**本期不对 ShadowCasterQueue 排序。**

**状态：** Draft（评审修订）— 待审批 → Planned（F19 后实现）。

## Scope
- **In:**
  - 锁定 Opaque `SortKey` 位布局（下表）
  - Opaque：`FillOpaqueSortKeys` + `std::stable_sort`
  - Translucent：以 world Bounds 中心（或 `LocalToWorld` 平移）相对相机距离，**远先画**；`stable_sort` 比较器
  - 单点 helper（Forward/Manual 共用）；开关默认 On；F22 Scope
  - 单测：Opaque key 序；Translucent 远近序
- **Out:**
  - Shadow 队列排序（明确 **不做**）
  - 视锥 / 实例合并 / Static MDC 缓存 / OIT
  - 玩法 render priority 层
  - 改 minCraft

## Reader quick start
1. §3.1–3.2 目标与 **锁定** key / 深度约定  
2. §3.3 落点与 API  
3. §6 验收  

---

## Pre-flight（摘要）

| 项 | 判断 |
|----|------|
| Prerequisites | F18 队列；**F19 world Bounds** → 半透明正确深度的硬前置 |
| Recommendation | **Go**；实现严格在 F19 后 |

---

## 1) 背景与目标

Opaque 减状态切换；Translucent 近似正确混合；把半成品 `SortKey` 变成可测契约。

---

## 2) 现状

`m_SortKey` 存在未用；入队后无 sort；Packet 按数组序提交。

---

## 3) 方案

### 3.0 原则

1. Pass 目标不同 → 排序策略不同。  
2. 渲染侧填 key；游戏不设 SortKey。  
3. **`std::stable_sort` 必用**（同 key / 同深度不乱跳）。  
4. Shadow **本期不排序**（去掉「可选」歧义）。

### 3.1 目标（锁定）

| 队列 | 策略 | 本期 |
|------|------|------|
| Opaque | 状态序：Material → VIL → VB → skinned 位 | **做** |
| Translucent | 距离序：远 → 近（先远后近） | **做** |
| ShadowCaster | — | **不做** |

**半透明深度定义（锁定）：**

```text
depthKey = length(worldBounds.GetCenter() - camera.Position)
// 无有效 Bounds：用 LocalToWorld 的平移分量作中心
sort: descending depthKey  // larger distance drawn first
```

不使用 NDC Z 作为本期主路径（避免与 clip 后端差异纠缠）；view 距离足够。

### 3.2 Opaque `SortKey` 位布局（锁定）

`uint64_t m_Key`，**高位优先比较**：

| Bits | 宽 | 内容 |
|------|----|------|
| 63–40 | 24 | `MaterialRenderProxy*` 地址折叠（例如 `(uintptr_t)p >> 4` 取低 24 bit）— **仅帧内**分组 |
| 39–20 | 20 | `VertexInputLayout*` 同样折叠 |
| 19–1 | 19 | `VertexBuffer*` 折叠 |
| 0 | 1 | `m_bSkinned` |

同帧内指针稳定即可；不要求跨帧/跨进程稳定。

Helper：

```cpp
MeshDrawCommandSortKey MakeOpaqueSortKey(const MeshDrawCommand& cmd);
void FillOpaqueSortKeys(std::vector<MeshDrawCommand>& cmds);
void SortOpaqueMeshDraws(std::vector<MeshDrawCommand>& cmds);      // stable_sort by m_SortKey
void SortTranslucentMeshDraws(
    std::vector<MeshDrawCommand>& cmds,
    const Vector3& cameraPosition); // stable_sort by descending distance
```

Translucent：**不**要求与 Opaque 相同的 bit 语义；比较器直接算距离。可选把量化距离写入 `m_SortKey` 仅供调试，非排序真源。

### 3.3 落点与数据流

```text
BuildRenderQueue (F18 + F19)
  → OpaqueQueue / TranslucentQueue / ShadowCasterQueue filled
  → if m_bSortMeshDraws:
       FillOpaqueSortKeys(OpaqueQueue)
       SortOpaqueMeshDraws(OpaqueQueue)
       SortTranslucentMeshDraws(TranslucentQueue, camera.Position)
  → assign queues to Passes
```

- 文件建议：`Render/DrawCommands/MeshDrawCommandSort.h|.cpp`  
- **不**在 `AddMeshBatch` 内插入排序。

### 3.4 开关

`m_bSortMeshDraws = true`（与 F19 锥开关独立）。Off：跳过填 key/排序。

### 3.5 模块边界

| 模块 | 负责 |
|------|------|
| Sort helper | key、比较器、stable_sort |
| Processor | 只接纳，不定序 |
| Pass | 假定队列已排序 |
| F19 | 提供 world Bounds |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| Opaque SortKey + Translucent 距离比较器 | **选用** |
| 统一 64-bit 挤半透明 | **否** |
| 本期 Shadow 也排序 | **否** |
| 与 F19 合并 Feature | **否** |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Bounds 非 world | **F19 硬前置** |
| 指针折叠碰撞 | 接受粗分；Scope 对比开关 |
| 小 N 排序成本 | 开关；可选 N&lt;阈值跳过（非必须） |

---

## 6) 验收标准

- [ ] Opaque `stable_sort` 后 `m_SortKey` 非降序（单测）  
- [ ] Translucent 远→近（单测构造两盒）  
- [ ] ShadowCasterQueue **未**被本 Feature 排序（文档/代码路径确认）  
- [ ] 开关 Off 可关  
- [ ] Profiler Scope：`MeshDraw.SortOpaque` / `MeshDraw.SortTranslucent`  
- [ ] Editor 手验：不透明无回归；半透明叠层更稳  

---

## 7) Status note

（Draft）实现须在 **RND-F19** Done（至少 world Bounds 契约落地）之后。

---

## 8) 建议切片

| Slice | Goal | Verify |
|-------|------|--------|
| **S01** | Design → Planned | 审批 |
| **S02** | Opaque key + sort + 单测 | tests |
| **S03** | Translucent 距离 sort + 手验 | Editor |
| **S04** | 开关 + Scope + 收口 | Progress |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft：与 F19 拆分；Opaque / Translucent 排序 |
| 2026-09-22 | **Review 修订：** 锁定 bit 布局与远→近距离；Shadow 明确不排序；`stable_sort` 必用；F19 world Bounds 硬前置；去掉未决「可选」 |
