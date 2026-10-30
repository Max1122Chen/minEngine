# RND-F29 — Orthographic Camera（透视之外的投影模式）

## Meta
- **ID:** `RND-F29`
- **Type:** Feature
- **Complexity:** `L1`
- **Status:** `In Progress`（S01–S02 已落地；S03 可选后置）
- **Owner:** agent + maintainer
- **Last updated:** 2026-10-29
- **Branch:** `feat/render`（worktree `minEngine-render`）
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · `RenderCamera` / `CameraComponent` · `RHIClipSpace::MakeOrthographic` · [RND-F19](./RND-F19_CPU_MESH_VISIBILITY_DESIGN.md)（Frustum 自 ViewProj）

## TL;DR

`RenderCamera` / `CameraComponent` 今日只走透视投影。本 Feature 增加 **投影模式 + 正交镜头参数**，复用已有 `RHIClipSpace::MakeOrthographic`，并修正正交下的 `ScreenPointToRay`。不重写相机系统、不做正交专用 Pass。

## Pre-flight

| 项 | 结论 |
|----|------|
| 复杂度 | **L1** — 局部扩展既有类型 |
| 前置 | `RHIClipSpace::MakeOrthographic`（阴影 / ScreenUI 已用）**健全**；`Frustum::FromViewProjection` **健全** |
| 技术债风险 | **低** — 若 FOV 字段在正交下语义不清，须文档化「正交忽略 FOV」 |
| WIP | 主树 `CORE-F47` 并行；本工作在 `feat/render` |
| 哲学 | 投影是机制；不引入玩法相机框架 |
| 建议 | **Go** |

## Problem

1. `RenderCamera::UpdateProjectionMatrix` 写死 `MakePerspective(FOV, aspect, near, far)`。
2. `CameraComponent` 只暴露 FOV / near / far；无法在编辑器或游戏里切到正交。
3. `ScreenPointToRay` 假设近远平面 NDC 反投影成会聚射线；正交下方向应平行于相机 forward。
4. 游戏侧（含俯视 / 2.5D / 工具视口）需要正交，却只能 hack 投影矩阵。

## Requirements

- **R1：** `RenderCamera` 支持 `Perspective | Orthographic` 投影模式；默认保持 **Perspective**（现行为不变）。
- **R2：** 正交以 **垂直世界高度（OrthoHeight）+ Aspect** 推导 left/right/bottom/top；near/far 与透视共用字段。
- **R3：** `UpdateProjectionMatrix` 按模式调用 `MakePerspective` 或 `MakeOrthographic`（经 `RHIClipSpace`，保留 ZO/NO clip 适配）。
- **R4：** `ScreenPointToRay` 在正交下产生 **平行射线**（方向 = 相机 forward，原点由近平面反投影）。
- **R5：** `CameraComponent` 暴露模式与 OrthoHeight（反射 / 编辑属性可跟进）；`ApplyLensToRenderCamera` 同步全部镜头字段。
- **R6：** Aspect 仍由呈现侧（`SceneViewport` / Offscreen）写入 `m_AspectRatio`；组件不抢 aspect 所有权（与今日一致）。
- **R7：** 至少一条自动化测例：正交 `ViewProj` 可断言；正交 `ScreenPointToRay` 平行性。

## Constraints

- **C1：** 不 fork 第二套 `RenderCamera`；单类型双模式。
- **C2：** 不改阴影级联里的光空间正交逻辑（已独立使用 `MakeOrthographic`）。
- **C3：** 源码注释不含 Feature/Slice ID。
- **C4：** 向前演进；无长期「旧 API + 新 API」双轨。

## Preferences

- **P1：** 正交主参数用 **OrthoHeight**（世界单位，视锥垂直全高），宽度 = `OrthoHeight * aspect`。对齐常见引擎「ortho size」语义，避免同时暴露 left/right/top/bottom 四参。
- **P2：** 枚举名倾向 `ECameraProjectionMode`（或项目既有命名风格下的等价物）。
- **P3：** 正交模式下 FOV getter 仍可读历史值，但不参与矩阵；文档写清。

## Assumptions

- **A1：** 主视图剔除已用 `Frustum::FromViewProjection(viewProj, …)`，正交 ViewProj 自动正确，无需单独「FOV 锥」路径。若发现硬编码透视锥处，本 Feature 修或记债。
- **A2：** ImGuizmo `SetOrthographic` 与镜头模式联动可后置；本 Feature 不强制改 Editor gizmo。
- **A3：** 默认 OrthoHeight（如 `10.0f`）仅在首次切到正交时有意义；透视默认不受影响。

## Non-Goals

- 多相机管线 / 分屏正交策略。
- 正交专用渲染 Pass 或材质变体。
- 像素完美 2D 相机（像素吸附、整数缩放）——可后续 Feature。
- 改 `ScreenUICoords::MakePixelOrthoProjection`（屏幕 UI 正交与世界相机正交无关）。
- 完整 UE `UCameraComponent` OrthoWidth 双轨 API；若以后要 OrthoWidth，可加糖衣 `SetOrthoWidth = height * aspect`，不在本切片。

## Current Architecture

| 组件 | 今日行为 |
|------|----------|
| `RenderCamera` | 位置/旋转 → View；FOV/aspect/near/far → Perspective Proj；`ViewProj = P*V` |
| `CameraComponent` | 持有 `shared_ptr<RenderCamera>`；EOF 推 transform；`ApplyLens` 推 FOV/near/far |
| `RHIClipSpace` | 已有 `MakePerspective` / `MakeOrthographic`（RH + ZO/NO） |
| Aspect | Offscreen / Viewport 按缓冲尺寸写 `m_AspectRatio` 再 `UpdateProjectionMatrix` |
| Frustum | `Frustum::FromViewProjection` — 与投影类型无关 |

## Proposed Architecture

### 类型与字段

```cpp
enum class ECameraProjectionMode : uint8_t
{
    Perspective = 0,
    Orthographic,
};

// RenderCamera（示意）
ECameraProjectionMode m_ProjectionMode = ECameraProjectionMode::Perspective;
float m_FOV = 45.0f;           // Perspective only
float m_OrthoHeight = 10.0f;   // Orthographic: full vertical size in world units
float m_zNear = ...;
float m_zFar = ...;
float m_AspectRatio = ...;
```

### 投影

```text
Perspective:
  Proj = MakePerspective(radians(FOV), aspect, near, far)

Orthographic:
  halfH = OrthoHeight * 0.5
  halfW = halfH * aspect
  Proj = MakeOrthographic(-halfW, +halfW, -halfH, +halfH, near, far)
```

### 拾取射线

```text
Perspective: 保持现有 near/far NDC 反投影 → 原点+方向

Orthographic:
  反投影近平面得到 origin
  direction = normalize(camera forward)   // 与 far 点无关；仍可用 far 点做健壮性校验
```

### 组件面

- `Get/SetProjectionMode`、`Get/SetOrthoHeight`
- `SetFieldOfView` 仅在透视语义下影响矩阵；切模式时 `ApplyLens` 一次刷新
- 反射：`ME_PROPERTY` 暴露模式与 OrthoHeight（实现切片定具体标签）

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `RenderCamera` | 模式、镜头参数、View/Proj/ViewProj、ScreenPointToRay | 谁是 Main Camera、viewport 尺寸 |
| `CameraComponent` | 把组件字段推到 `RenderCamera`；Main 绑定 | 写 aspect、RHI |
| `SceneViewport` / Offscreen | 写 aspect、触发投影更新 | 正交参数权威 |
| `RHIClipSpace` | 矩阵构造（已有） | 相机状态 |

## Boundaries

- **Allowed：** Component → RenderCamera；Viewport → aspect on RenderCamera；Renderer 只读 ViewProj。
- **Forbidden：** 游戏代码直接拼 glm::ortho 绕过 `RHIClipSpace`（新路径）；DebugDraw / Physics 依赖投影模式特殊分支。

## Dependencies

- Depends on: `RHIClipSpace`、现有 `RenderCamera`/`CameraComponent`
- Depended on by: 游戏俯视相机、未来 Editor 正交预览（可选）
- 无新第三方依赖

## Ownership & Lifetime

Not applicable beyond existing `CameraComponent` 拥有 `shared_ptr<RenderCamera>`。无新 GPU 资源。

## Data Flow

```text
Authoring (mode, OrthoHeight, FOV, near, far)
  → CameraComponent::ApplyLensToRenderCamera
  → RenderCamera fields
  → UpdateProjectionMatrix (+ View from transform)
  → ViewProj → PerFrame UBO / Frustum / picking
```

## Control Flow

与今日相同：EOF / setter 调 `ApplyLens`；viewport resize 写 aspect 后 `UpdateProjectionMatrix`。

## State & Invariants

- 默认模式 = Perspective；未改调用方行为逐字节兼容（同 FOV/aspect/near/far）。
- `OrthoHeight > 0`、`aspect > 0`、`near < far`；非法值实现期 clamp 或 assert（切片选定一种并测）。
- 正交下矩阵 **不**读取 FOV。

## API / Interface Semantics

| API | 语义 |
|-----|------|
| `SetProjectionMode` | 切换模式并刷新 Proj |
| `SetOrthoHeight` | 世界单位垂直全高；`<=0` 拒绝或 clamp |
| `ScreenPointToRay` | 模式感知；bufferSize 非法时保持现有兜底 |

线程：与现有一致（游戏线程写镜头；渲染读矩阵）——本 Feature 不改线程契约。

## Failure Model

- 非法 OrthoHeight / aspect：不产生 NaN 矩阵；测例覆盖。
- 正交 + 极端 near/far：与透视同样由调用方负责；不新增特殊错误通道。

## Alternatives & Trade-offs

### Option A（推荐）：OrthoHeight + aspect

- Pros：参数少；与 aspect 所有权模型一致；实现小。
- Cons：不能独立设非对称 left/right（本阶段不需要）。

### Option B：显式 left/right/bottom/top

- Pros：最大灵活。
- Cons：与 viewport aspect 双源冲突；API 面大。**不选。**

### Option C：仅文档「调用方 SetProjectionMatrix」

- Pros：零代码。
- Cons：绕过组件权威；射线/序列化无模式。**不选。**

## Integration Impact

- `CameraComponent` 反射生成物更新。
- Offscreen 内部相机默认仍透视。
- Editor ImGuizmo 正交标志：**可选**后置，不挡本 Feature Done。
- Frustum 测试可增一例正交盒可见性。

## Migration / Compatibility

- 默认 Perspective → 旧内容 / 场景无需迁移。
- 无废弃 API。

## Verification Strategy

- 单元：正交 Proj 元素（或对已知点的 clip 映射）断言；两屏幕点射线方向点积 ≈ 1。
- 回归：现有 `frustum` / 依赖主相机的 smoke 仍绿。
- 手验（可选）：场景里切 Orthographic，视口不再透视变形。

## Open Questions

- Inspector 是否本切片就加枚举下拉？— **non-blocking**；可 S01 数据面、S02 编辑器属性。
- 默认 OrthoHeight 数值？— **non-blocking**；建议 `10.0f`，实现时写进 Design 定稿。

## Design Review

- **Verdict:** `Pending`（L1：主人过目即可开工；不强制完整 design-review 仪式）
- **Reviewer / date:**
- **Link or summary:**

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

**Ready for implementation?** `yes`（L1；主人确认 Draft→Planned 后即可编码）

## Acceptance Checklist

- [ ] 主人确认 OrthoHeight 语义与默认值
- [ ] Feature registry 状态同步
- [ ] 实现后 Progress 条目 + 测例名

## Slices（建议）

| Slice | 内容 | DoD |
|-------|------|-----|
| **S01** | `RenderCamera` 模式 + OrthoHeight + Proj + Ray | 测例绿 |
| **S02** | `CameraComponent` 字段 / setter / 反射 | 构建绿；可选 Inspector |
| **S03** |（可选）Frustum 正交测例；Editor gizmo 联动 | 按需 |
