# RND-F30 — DebugDraw 索引线网 + AlwaysVisible 补全

## Meta
- **ID:** `RND-F30`
- **Type:** Feature
- **Complexity:** `L1`
- **Status:** `In Progress`（S01–S02 已落地；Snapshot IndexedLines 已含；S04 便利重载已含）
- **Owner:** agent + maintainer
- **Last updated:** 2026-10-29
- **Branch:** `feat/render`（worktree `minEngine-render`）
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [RND-F11](./RND-F11_DEBUG_DRAWING_DESIGN.md)（Done，基线） · `DebugDrawService` / `DebugDrawPass` · minCraft 瞄准方块描边（**游戏侧消费**，不进引擎 Core）
- **Reference (local UE):** `Engine/Source/Runtime/Engine/Public/DrawDebugHelpers.h` — `DrawDebugLine` / `DrawDebugBox`（`DepthPriority`）· `DrawDebugMesh`（Verts+Indices，三角实体）；本 Feature **对齐「自定义顶点+索引入队 + 深度优先级」机制**，不做 UE 三角 mesh debug 全量。

## TL;DR

在 RND-F11 之上增加 **自定义顶点 + 索引的线列表入队**（调用面类似 DrawIndexed 的线拓扑），并 **补全 `AlwaysVisible` 绘制路径**（今日只建顶点、Pass 不画）。瞄准方块描边默认用 **`EDebugDepthMode::Tested`**：读 SceneDepth、不写深度，被实心几何挡住。引擎不实现选块逻辑。

## Pre-flight

| 项 | 结论 |
|----|------|
| 复杂度 | **L1**（索引命令 + 双 PSO 绘制）；若测例/导出面膨胀可按 L2 纪律写切片 |
| 前置 | RND-F11 DebugDraw MVP **Done**；`EDebugDepthMode` 与分桶顶点 **已有**；Pass 仅画 Tested = **已知缺口** |
| 技术债风险 | **低**；AlwaysVisible 半截若不补会继续误导调用方 |
| WIP | 与 `RND-F29` 同树顺序推进；不挡主树 F47 |
| 哲学 | Debug 通道是机制；体素描边形状由游戏提供索引线网 |
| 建议 | **Go** |

## Problem

1. 公开 API 只有 `Line/Point/Box/Sphere/Capsule`；无法一次提交任意折线/边集合（例如方块 12 棱的共享顶点 + 索引，或非 AABB 轮廓）。
2. `EDebugDepthMode::AlwaysVisible` 在 `BuildFrameGeometry` 分桶，但 `DebugDrawPass` **只上传并 `Draw` Tested 桶** → AlwaysVisible 名存实亡。
3. minCraft 需要瞄准方块描边：希望走 DebugDraw，且 **被前方方块挡住**（深度测试开、深度写关）——Tested 路径语义正确，缺的是灵活几何入口；AlwaysVisible 作为并列能力应一并可用。

## Requirements

- **R1：** 新增入队 API：调用方提供 **顶点数组 + 索引数组**，拓扑为 **成对索引 → LineList**（`indices.size()` 为偶数；`(i0,i1)` 画一段）。
- **R2：** 每条索引命令带 `EDebugDepthMode`（默认 **`Tested`**）。
- **R3：** `BuildFrameGeometry` 将索引命令展开进现有 `m_VerticesDepthTested` / `m_VerticesAlwaysVisible`（与 Line/Box 同一软上限策略）。
- **R4：** `DebugDrawPass` **两趟绘制**（可同一次 RenderPass 内切换 PSO）：
  - Tested：`bDepthTestEnabled=true`，`bDepthWriteEnabled=false`，Compare 与今日一致（Less）
  - AlwaysVisible：`bDepthTestEnabled=false`，`bDepthWriteEnabled=false`
- **R5：** 仍读 `kRDGSceneColor` + `kRDGSceneDepth`；不写 depth。
- **R6：** 既有 `Box/Sphere/...` 行为不变；`Box` 可继续作 AABB 描边糖衣。
- **R7：** 自动化：索引展开顶点数/线段数断言；AlwaysVisible 桶非空时 Pass 准备绘制（或服务层可测展开）；Tested 为默认。
- **R8：** 首版 **CPU 展开为 LineList 顶点流**，RHI 侧仍 `Draw`（非 `DrawIndexed`）——降低 Pass/缓冲复杂度；公开 API 对调用方仍是「顶点+索引」。

## Constraints

- **C1：** 不把 minCraft 选块 / 体素 API 放进 `Runtime/Function/Debug`。
- **C2：** 不引入 Persistent lifetime（仍 Transient / 每帧队列；与 F11 一致）。
- **C3：** 不新增第三种深度模式；不实现 GPU Occlusion Query。
- **C4：** 索引越界 → 跳过非法段 + 限频 Warn（或整命令丢弃，切片锁定一种），不崩溃。
- **C5：** 源码注释无 Feature/Slice ID。

## Preferences

- **P1：** API 名倾向 `DebugDraw::IndexedLines`（或 `Lines`）；避免与 `Line(start,end)` 混淆。
- **P2：** 索引类型首版 **`uint16_t` 或 `uint32_t` 其一**（推荐 `uint32_t` 省转换；体量小）；不混用双入口除非有证据。
- **P3：** 顶点格式继续 `DebugVertex { Position, Color }`；每顶点色，索引命令也可接受「统一色」重载（可选糖）。
- **P4：** Snapshot（`DebugDrawSnapshot`）是否纳入索引命令：**本 Feature 建议纳入**，便于游戏线程快照 → 渲染消费（与 F47 场景 debug 快照路径一致）；若实现成本高可 S02。

## Assumptions

- **A1：** 瞄准描边用量小（每帧数个方块 × 12 边），CPU 展开足够。
- **A2：** 现有 view-space bias（`DebugDraw.vert`）对 Tested 描边仍适用；不为本 Feature 改 bias 公式。
- **A3：** `DepthCompare::Less` 与场景深度布局已匹配；AlwaysVisible 关闭测试即可。

## Non-Goals

- 三角面 `DrawDebugMesh` 实体填充（UE `DrawDebugMesh`）——另开 Feature。
- GPU 侧真正的 `DrawIndexed` + 共享 VB/IB 池化（可后续优化，接口可不变）。
- Line strip / adjacency / 粗线 geometry shader。
- Editor debug 总开关、Persistent 线、联系人/trace 可视化（F11 已划出）。
- 屏幕空间 UI 线。

## Current Architecture

```text
Game/Editor → DebugDraw::* → DebugDrawService queues
  → BuildFrameGeometry → VerticesDepthTested | VerticesAlwaysVisible
  → DebugDrawPass::Prepare 只取 Tested → UpdateBuffer → Draw(LineList)
     Depth test ON, depth write OFF
```

| 缺口 | 说明 |
|------|------|
| 无索引命令 | 只能高阶形状或逐段 `Line` |
| AlwaysVisible | 分桶有、绘制无 |
| Snapshot | Boxes/Spheres/Capsules；无 Lines / 索引 |

## Proposed Architecture

### 命令

```cpp
struct DebugIndexedLinesCommand
{
    std::vector<DebugVertex> Vertices;   // or shared_ptr to immutable blob — slice picks
    std::vector<uint32_t> Indices;       // pairs → lines
    EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
    EDebugLifetime Lifetime = EDebugLifetime::Transient;
};
```

入队时 **拷贝**进 service（与现有 command 一致）；大网格由调用方自控频率。

### 公开 API（示意）

```cpp
namespace DebugDraw
{
    void IndexedLines(
        const DebugVertex* vertices,
        uint32_t vertexCount,
        const uint32_t* indices,
        uint32_t indexCount,   // must be even
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    // optional convenience: uniform color + Vector3 positions
}
```

### 展开

```text
for each pair (i0, i1) in Indices:
  if i0/i1 >= vertexCount: skip (+ warn)
  else AppendLine(bucket, V[i0].Position, V[i1].Position, color policy)
```

颜色策略（锁定）：**每端点用该顶点 Color**（两端色可不同 → 与现 AppendLine 单色比，若 AppendLine 只支持单色，则用 i0 色或平均——实现选「起点色」并写测）。更简单：**命令级统一 Color 覆盖** + 位置数组重载。推荐首版：

- 重载 A：`IndexedLines(positions, indexCount, color, depth)`
- 重载 B：完整 `DebugVertex` 数组（色随顶点；展开时线段用起点色）

### Pass

```text
Prepare:
  BuildFrameGeometry()
  upload Tested verts → m_DrawVertexCountTested
  upload AlwaysVisible verts → m_DrawVertexCountAlways
  （可用同一 VB 分区 offset，或两次 Ensure+Update；切片选更简单的「同一缓冲前后半」或「两缓冲」）

BuildRenderPass:
  BeginRenderPass(color, depth, no clear)
  if testedCount: SetPSO(depthTest), Draw(...)
  if alwaysCount: SetPSO(noDepthTest), Draw(...)
  EndRenderPass
```

PSO：共享 shader / layout；仅 DepthStencil 位不同。

### 与 UE 的映射（学习锚点，非抄 API）

| UE | minEngine |
|----|-----------|
| `DrawDebugLine` + `DepthPriority` | `Line` / `IndexedLines` + `EDebugDepthMode` |
| `DrawDebugBox` | 已有 `Box`；描边可继续用 |
| `DrawDebugMesh(Verts, Indices)` | **不**做三角填充；只做 **线索引** 子集 |

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `DebugDraw` API | 入队形状 | 何时描边、选哪块 |
| `DebugDrawService` | 队列、展开、分桶 | RHI / Pass |
| `DebugDrawPass` | 上传、双 PSO、Draw | 生成几何 |
| 游戏（minCraft） | 选块结果 → IndexedLines 或 Box | Pass 内部 |

## Boundaries

- **Allowed：** PhysicsDebugDraw / 游戏 / Editor → `DebugDraw::`
- **Forbidden：** 游戏直接改 `DebugDrawPass` PSO；Renderer 解析体素

## Dependencies

- Depends on: RND-F11 代码面、SceneDepth 附件
- Soft follow: F47 snapshot 路径 — 索引命令进 `DebugDrawSnapshot` 与 `ApplyFrameSnapshot`
- Depended on by: minCraft 描边（树外）

## Ownership & Lifetime

- 命令数据：Service 队列拥有拷贝；`ClearFrameQueues` 每帧消费后清空（现契约）。
- VB：Pass 拥有，容量倍增策略不变。
- AlwaysVisible PSO：Pass 成员，与 Tested PSO 同生命周期。

## Data Flow

```text
Caller IndexedLines(...)
  → EnqueueIndexedLines
  → BuildFrameGeometry (expand to LineList buckets)
  → Pass upload + draw (Tested then AlwaysVisible)
```

## Control Flow

与 F11 相同：Prepare 建几何并清队列；BuildRenderPass 提交 Draw。新增第二 PSO 绑定。

## State & Invariants

- `indexCount % 2 == 0`；否则丢弃命令并 Warn。
- Tested 与 AlwaysVisible **互斥分桶**（一条命令只进一个桶）。
- Depth write 恒 false。
- 软顶点上限：展开后截断，行为与现 Box 展开一致。

## API / Interface Semantics

| API | 所有权 | 错误 |
|-----|--------|------|
| `IndexedLines(...)` | 入参只读；内部拷贝 | 空/奇数索引 → no-op + Warn |
| `GetVertices(mode)` | 只读展开结果 | — |

线程：保持 F11 / 现 snapshot 约定（谁入队谁负责线程）；本 Feature 不新开多线程收集。

## Failure Model

| 失败 | 行为 |
|------|------|
| 奇数索引 / 越界 | 跳过；不崩 |
| VB 扩容失败 | 与现 Pass 相同（无绘制） |
| AlwaysVisible PSO 创建失败 | Error；可降级只画 Tested（切片写明） |

## Alternatives & Trade-offs

### Option A（推荐）：CPU 展开 + 双 PSO + IndexedLines API

- Pros：改动面小；API 对游戏友好；补全 AlwaysVisible。
- Cons：大网格有 CPU 拷贝；非真 DrawIndexed。

### Option B：Pass 真 DrawIndexed

- Pros：少展开内存。
- Cons：IB 管理、双桶偏移更复杂；体量不值。**延期。**

### Option C：仅文档教人循环 `DebugDraw::Line`

- Pros：零实现。
- Cons：无共享顶点、易错、AlwaysVisible 仍坏。**不选。**

## Integration Impact

- `DebugDrawTypes` / Service / Pass / 可选 Snapshot + `PhysicsDebugDraw` 无强制改。
- 导出：若新公开 `DebugDraw::IndexedLines` 需 `MINENGINE_API` + snapshot 流程。
- 着色器可不变。

## Migration / Compatibility

- 默认仍 Tested；旧调用方零改动。
- AlwaysVisible 从「静默丢弃」变为「真能看见」——行为修复，视为 bugfix 并入本 Feature。

## Verification Strategy

- 单测：4 顶点正方形边索引 → 展开 8 顶点（4 段 × 2）进入 Tested 桶。
- 单测：同一几何 AlwaysVisible → Always 桶非空、Tested 空。
- Pass：初始化后存在 depth-off PSO（可测成员或 probe）；Prepare 在双桶都有数据时 `m_ShouldRender`。
- 回归：`scene-debug-draw-snapshot` / smoke。
- 手验：方块后的线被挡住；AlwaysVisible 线穿墙仍见。

## Open Questions

- Snapshot 是否 S01 必含 IndexedLines？— **偏好含**；若挡进度可 S02。**non-blocking。**
- 统一色重载 vs 仅 DebugVertex？— 建议 **两个重载都做**，实现量小。**non-blocking。**

## Design Review

- **Verdict:** `Pending`（L1；主人确认深度语义后可 Planned）
- **Reviewer / date:**
- **Link or summary:**

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime N/A or resolved
- [x] State/invariants resolved
- [x] API semantics sufficiently defined
- [x] Failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Open questions deferred explicitly

**Ready for implementation?** `yes`（主人确认后）

## Acceptance Checklist

- [ ] 确认描边默认 Tested（已按主人指示写入 R2/R4）
- [ ] Registry / ACTIVE_WORK 同步
- [ ] 实现后 Progress + 测例名

## Slices（建议）

| Slice | 内容 | DoD |
|-------|------|-----|
| **S01** | `IndexedLines` 命令 + 展开 + 测例 | 测例绿 |
| **S02** | Pass 双 PSO + AlwaysVisible 真绘制 | 手验或测例；Tested 回归 |
| **S03** |（可选）Snapshot 字段 + ApplyFrameSnapshot | snapshot 测例 |
| **S04** |（可选）统一色便利重载、导出登记 | verify 导出 |

## minCraft 消费示意（非本仓库范围）

```cpp
// Aim block AABB outline — depth tested so occluders hide the line
DebugDraw::Box(worldFromBlock, halfExtent, color, EDebugDepthMode::Tested);
// or IndexedLines with 8 corners + 24 indices (12 edges)
```
