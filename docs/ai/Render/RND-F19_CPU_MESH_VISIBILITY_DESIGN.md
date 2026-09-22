# RND-F19 — CPU Mesh Visibility（主视图视锥剔除）

## Meta
- **ID:** RND-F19
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22（S02–S03 Done；Editor 手验后收口）
- **Related:**
  - 排序：[RND-F20](./RND-F20_MESH_DRAW_SORTING_DESIGN.md)（实现建议在本 Feature 后）
  - **后续占位：** [RND-F21](./RND-F21_SHADOW_FRUSTUM_CASTER_CULLING_DESIGN.md) — 光视锥 / CSM cascade caster 剔除（**本期不做**；已登记防遗忘）
  - 前置：[RND-F18](./RND-F18_MESH_BATCH_PASS_MDC_REFACTOR_DESIGN.md) Done
  - 测量：[CORE-F22](../Platform/Core/CORE-F22_PROFILER_HARNESS_DESIGN.md) Done
  - 数学：`Runtime/Core/Math/Geometry/AABB.h`（已有 `Transform`）· `Frustum.h`
  - 渲染：`ForwardRenderer::BuildRenderQueue` · `MeshBatch` / Collector · Pass Processor · `RenderCamera` · `SceneRenderContext`

## TL;DR

**问题：** F18 入队对几乎所有 mesh Proxy 做 Collect；主相机看不见的图元仍进 Opaque / Translucent。

**方案：** 在 **数学域** 落地可复用的 `minEngine::Math::Geometry::Frustum`（`Intersect(AABB)`）；渲染入队用主相机 `ViewProj` 建锥，按「主视图可见 / 仅阴影候选」分流 Collect 与 Dispatch。ShadowDepth **不**套用主相机锥。光视锥与 CSM 体积剔除 → **RND-F21**。

**状态：** Done — S02/S03 已合入路径；Editor 手验后收口。

## Scope
- **In:**
  - `Math::Geometry::Frustum` + **同文件内** `Plane`（必做，不作「可选」）：`FromViewProjection`、`Intersect(const AABB&)`
  - `FromViewProjection` 必须按当前 RHI clip（`GetClipSpaceCapabilities` / `GetFrustumNdcZNear|Far`）提平面
  - 主视图剔除接入共享 `BuildRenderQueue`；Opaque / Translucent 只吃主视图可见 Batch
  - 主视图不可见但 `CastShadow`：仍 Collect，只 Dispatch 给 ShadowDepth；**若本帧 `EnableShadows==false`，锥外 caster 整段 skip**（不必 shadowOnly Collect）
  - **契约：`GetMeshBatches` 写入的 `MeshBatch.m_BoundingBox` 必须是 world AABB**（与剔除同一套 `ResolvePrimitiveWorldBounds`）
  - 开关（默认 On）+ F22 Scope + 可选计数
  - 单测：Frustum∩AABB；身后 caster 仍进 Shadow 队列（阴影开启时）
- **Out:**
  - **RND-F21：** 光视锥 / 每 cascade 体积剔 caster（已占号，见 Registry）
  - 距离剔除、LOD、遮挡、BVH、多线程 Collect
  - 渲染排序（→ F20）
  - 体素 / Chunk；改 minCraft；Editor Profiler UI

## Reader quick start
1. §3.1 数学类型 `Frustum`  
2. §3.2–3.3 渲染侧结构与接口  
3. §3.4 数据流（主视图 vs 阴影）  
4. §6 验收 · §8 切片 · Related **RND-F21**

---

## Pre-flight（摘要）

| 项 | 判断 |
|----|------|
| Prerequisites | F18、AABB、RenderCamera ViewProj、F22 → **sound**；无 `Frustum` / `Plane` → 本期补在 Math::Geometry |
| Tech-debt risk | **medium**（丢影 / Bounds 空间） |
| Recommendation | **Go** — 数学类型与入队分流分开验收 |

---

## 1) 背景与目标

### 1.1 Pain

F18 解决扩展性；未解决「值不值得 Collect」。全量入队在规模上升时浪费 CPU 与后续 Pass 工作。

### 1.2 成功标准

> 主锥外 mesh 不进 Opaque / Translucent；锥外 caster 仍可进 Shadow。`Frustum` 是与渲染无关的数学类型，可供日后 CSM / 光锥复用。开关 Off ≈ 今日全收集。

---

## 2) 现状

| 项 | 现状 |
|----|------|
| 入队 | Collect → Opaque / Translucent / ShadowDepth Processor |
| `AABB` | `Math::Geometry::AABB`；`Intersect(AABB)` / `Transform(AABB, Matrix4)` 已有 |
| `Plane` / `Frustum` | **不存在** |
| Component Bounds | `StaticMesh` / `Skeletal` 的 `GetBoundingBox()` 当前返回 **mesh 资产盒（偏 local）**；剔除前必须变到 world（本 Feature 契约） |
| `MeshBatch.m_BoundingBox` | 今日多直接拷 Component 盒 — 实现时一并改为 **world**（或 Collect 前解析） |
| 相机 | `RenderCamera::GetViewProjMatrix()` |

---

## 3) 方案

### 3.0 设计原则

1. **Frustum 属数学域** — 不依赖 `RenderCamera` / Proxy；渲染只负责「何时用哪份矩阵建锥」。  
2. **Intersect 保守** — 骑缝算相交；无效 AABB fail-open（见下）。  
3. **主锥 ≠ 阴影可见集** — 硬契约 §3.4。  
4. **Pass 策略仍在 Processor** — 本 Feature 增加的是 Collect 前分流，不把主锥逻辑写进 ShadowDepthProcessor。  
5. **可关闭、可测。**

---

### 3.1 数学域：`Frustum`

**路径（建议）：**  
`Runtime/Core/Math/Geometry/Frustum.h` · `Frustum.cpp`  
命名空间：`minEngine::Math::Geometry`（与 `AABB` / `Ray` 一致）。

#### 3.1.1 平面表示

**必做：** `Plane` 与 `Frustum` 同放在 `Frustum.h`（可不单独拆 `Plane.h`）。

```cpp
namespace minEngine::Math::Geometry
{
    /** ax + by + cz + d = 0; Normal points toward the frustum interior. */
    struct Plane
    {
        Vector3 Normal{ 0.0f, 0.0f, 0.0f };
        float Distance = 0.0f;

        void Normalize();
        float SignedDistance(const Vector3& point) const;
    };
}
```

`Frustum` 持有 **6** 个平面。枚举顺序 **锁定**：

```cpp
enum class FrustumPlane : uint8_t
{
    Left = 0,
    Right,
    Bottom,
    Top,
    Near,
    Far,
    Count
};
```

#### 3.1.2 `Frustum` 类型

```cpp
namespace minEngine::Math::Geometry
{
    class Frustum
    {
    public:
        Frustum() = default;

        /**
         * Extract 6 planes from view-projection.
         * Near/Far extraction must honor active clip depth via
         * GetClipSpaceCapabilities / GetFrustumNdcZNear|Far.
         */
        static Frustum FromViewProjection(const Matrix4& viewProjection);

        /**
         * Conservative intersection. Precondition: bounds.IsValid().
         * Do not call on invalid bounds — caller fail-opens.
         */
        bool Intersect(const AABB& bounds) const;

        const Plane& GetPlane(FrustumPlane plane) const;
        const Plane* GetPlanes() const { return m_Planes; }

    private:
        Plane m_Planes[static_cast<size_t>(FrustumPlane::Count)]{};
    };
}
```

**契约（锁定）：**

| 项 | 约定 |
|----|------|
| `Intersect` | 仅 `bounds.IsValid()`；与建锥矩阵同一空间（主视图 = world） |
| 无效 AABB | **调用方** fail-open 为可见；不调用 `Intersect` |
| `ctx.Camera == nullptr` | 视同关闭剔除：全部 mainCollector 三路 |
| 无堆分配 | 固定 6 平面 |
| 复用 | 任意 VP → **RND-F21** |

**单测（数学）：** 单位盒内/外/骑缝；锥后应为 false；OpenGL vs Vulkan NDC 各至少一条近/远相关用例（或参数化）。

---

### 3.2 渲染侧数据结构

#### 3.2.1 World Bounds 解析

```cpp
Math::Geometry::AABB ResolvePrimitiveWorldBounds(const PrimitiveSceneProxy& proxy);
```

| 步骤 | 说明 |
|------|------|
| 1 | `localBox = component->GetBoundingBox()`（资产/local） |
| 2 | Static/Skeletal：`Transform(localBox, proxy.m_Transform.ToMatrix())`；Sprite：`Transform(localBox, proxy.m_ModelMatrix)` |
| 3 | **F19 契约变更：** `GetMeshBatches` 必须把 **同一 world 盒** 写入 `MeshBatch.m_BoundingBox`（改 Static/Skeletal/Sprite 的 Collect 实现，或抽取共用 FillWorldBounds） |

禁止：剔除用 world、Batch 仍留 local（会害 F20 半透明深度）。

#### 3.2.2 每 Proxy 可见性决策

```cpp
struct PrimitiveVisibilityDecision
{
    bool bMainViewVisible = true;
    bool bWantShadowCollect = false;
    Math::Geometry::AABB WorldBounds{};
};
```

```text
cullEnabled = m_bFrustumCullMainView && ctx.Camera != nullptr
shadowsEnabled = /* SceneDrawFlags::EnableShadows for this Execute */

if !WorldBounds.IsValid():
  bMainViewVisible = true   // fail-open
else if !cullEnabled:
  bMainViewVisible = true
else:
  bMainViewVisible = mainFrustum.Intersect(WorldBounds)

bWantShadowCollect = shadowsEnabled && proxy.m_CastShadow
skipEntirely = !bMainViewVisible && !bWantShadowCollect
```

#### 3.2.3 Collector 分流（锁定双 Collector）

| Collector | 内容 | 下游 |
|-----------|------|------|
| `mainCollector` | 主视图可见 | Opaque + Translucent + ShadowDepth |
| `shadowOnlyCollector` | 主视图不可见且要阴影 | **仅** ShadowDepth |

锥内 caster 只进 `mainCollector` 一次。

#### 3.2.4 调试计数（可选）

`MeshVisibilityStats`：ProxiesConsidered / MainViewVisible / ShadowOnlyCollect / SkippedEntirely。非存盘格式。

#### 3.2.5 开关

`ForwardRenderer::m_bFrustumCullMainView = true`；可选 Debug Command（名记 Progress）。

---

### 3.3 渲染侧接口一览

| API | 位置 | 职责 |
|-----|------|------|
| `Frustum::FromViewProjection` | Math::Geometry | 建锥 |
| `Frustum::Intersect(AABB)` | Math::Geometry | 相交（valid only） |
| `ResolvePrimitiveWorldBounds` | Render helper | local→world |
| `GetMeshBatches` | Proxy | **输出 world Bounds**（F19 起） |
| `BuildRenderQueue` | Forward（Manual 共用） | §3.4 |
| Pass Processor | 不变 | 不读主锥 |

---

### 3.4 数据流（帧）

```text
if Camera && cullEnabled:
  mainFrustum = Frustum::FromViewProjection(Camera.ViewProj)
else:
  cull disabled path

for proxy in PrimitiveSceneProxies:
  WorldBounds = ResolvePrimitiveWorldBounds(proxy)
  decision = ...
  if skipEntirely: continue
  if bMainViewVisible: GetMeshBatches(mainCollector)
  else if bWantShadowCollect: GetMeshBatches(shadowOnlyCollector)

Dispatch:
  mainCollector → Opaque, Translucent, ShadowDepth
  shadowOnlyCollector → ShadowDepth only
```

**开关 Off / 无 Camera：** 仅 mainCollector 三路（≈ F18）。

**F21 预留：** 对阴影候选再 `Frustum::FromViewProjection(lightOrCascadeVP).Intersect(worldBounds)`。

---

### 3.5 模块边界

| 模块 | 拥有 | 不拥有 |
|------|------|--------|
| `Math::Geometry::Frustum` | 平面、Intersect、FromViewProjection | 相机、场景、Pass |
| 入队 / ForwardRenderer | 主锥、Bounds 解析、双 Collector、开关 | 光锥、cascade |
| Pass Processor | 材质/半透明/CastShadow | 主相机 Frustum |
| Proxy `GetMeshBatches` | world `m_BoundingBox` | 视锥测试 |
| **RND-F21** | 光/cascade Frustum | 改主视图分流 |

---

### 3.6 后续（已挂号）

| ID | 内容 |
|----|------|
| **RND-F21** | 光视锥 / CSM caster（复用 `Frustum`） |
| RND-F20 | 排序（依赖 world Bounds） |
| （未号） | 距离剔除、遮挡、BVH |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 数学域 `Frustum` + `Intersect(AABB)` | **选用** |
| 渲染专用 `ViewFrustum` 名 | **否** |
| 不可见则完全跳过 Collect | **否** |
| 单 Collector + Batch 标志 | **否**（双 Collector 锁定） |
| 本期做光锥 | **否** → F21 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Bounds 仍 local | Resolve + Collect 契约；F20 依赖 |
| Clip / NDC 提平面错误 | `GetFrustumNdcZ*`；双后端单测 |
| 主锥误用于 Shadow | §3.4；身后 caster 单测 |
| 无 Camera | fail-open 全收集 |

---

## 6) 验收标准

- [x] `Frustum` + `Plane`；`Intersect` 单测（内/外/骑缝）  
- [x] 主锥外不进 Opaque / Translucent；Off / 无 Camera 回归  
- [x] 主锥外 + CastShadow + 阴影开启 → 仍可进 ShadowCasterQueue  
- [x] 阴影关闭时锥外 caster 不 Collect  
- [x] `MeshBatch.m_BoundingBox` 为 world  
- [x] Profiler Scope 挂接入队  
- [x] F21 占位仍在 Registry  

---

## 7) Status note

Done — `frustum` suite PASS；Editor 手验通过后收口。下一刀建议 `RND-F20`。

---

## 8) 建议切片

| Slice | Goal | Verify |
|-------|------|--------|
| **S01** | Design → Planned | 文档审批 |
| **S02** | `Frustum`/`Plane` + 数学单测（含 clip 差异） | tests |
| **S03** | World bounds + 双 Collector + 开关 + Scope + 身后阴影 | Editor + tests |
| **S04** | 收口；链 F20/F21 | Progress |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft：主视图视锥；Shadow 不用主锥；F20 分离 |
| 2026-09-22 | 细化：`Math::Geometry::Frustum`；双 Collector；登记 F21 |
| 2026-09-22 | **Review 修订：** Plane 必做；Intersect 前置 valid；无 Camera/关阴影行为；GetMeshBatches world Bounds 硬契约；双 Collector 锁定 |
| 2026-09-22 | **In Progress：** 实现 Frustum/Plane、world Bounds、BuildRenderQueue 双 Collector、`frustum` 单测 |
| 2026-09-22 | **Done：** Editor 手验收口；Registry/Active/Progress 更新 |
