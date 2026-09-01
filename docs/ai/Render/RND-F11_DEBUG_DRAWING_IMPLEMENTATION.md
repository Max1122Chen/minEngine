# DebugDrawing — Implementation Plan

## Meta
- **ID:** `RND-F11`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Branch:** `feat/debug-drawing`
- **Design:** [RND-F11_DEBUG_DRAWING_DESIGN.md](./RND-F11_DEBUG_DRAWING_DESIGN.md)

## TL;DR

**RND-F11 MVP（S01–S02）已完成。** 交付引擎级 Debug 通道 + Editor 视口 collider wireframe 示范消费。

**本 Feature 不包含：** Physics contact/trace 可视化、Editor toggle、Persistent lifetime。后者单独立项（见 Design §13 / ACTIVE_WORK）。

**架构约束（不变）：**
- **提交在 Renderer 外**：调用方在 `SubmitSceneDraw` **之前** 入队 `DebugDraw::` / 子系统自有适配层
- **ForwardRenderer 只消费**：`Scene.Debug` pass；**不** include Physics、**不**调用 `PhysicsDebugDraw`
- **`DebugDrawService`** 是底层服务；**不**规定其他子系统何时/如何调用（由消费方自行决定）
- **`PhysicsDebugDraw`** 位于 `Physics/` — 仅为 Physics 域示范适配，非 Debug 核心

## Scope
- **In (MVP):** Design §3 Runtime/Debug、Render Pass、Shader、`PhysicsDebugDraw` collider wireframe、Editor 提交点
- **Out (deferred):** contact/trace debug、toggle、Persistent；`ManualRenderer` DebugPass；ForwardRenderer→Physics 耦合

---

## 1) 切片总览

| Slice ID | 标题 | 状态 | 验证 |
|----------|------|------|------|
| RND-F11-S01 | Debug 核心 + Pass + RDG 竖切 | **Done** | GL+VK 轴线 → 后移除 smoke |
| RND-F11-S02 | Collider wireframe | **Done** | Editor collider 目视 |
| ~~RND-F11-S03~~ | ~~Contact + LineTrace~~ | **Deferred** | 归 Physics / 后续 Debug 消费 Feature |
| ~~RND-F11-S04~~ | ~~Debug draw toggle~~ | **Deferred** | 归后续 Editor/Debug Feature |

---

## 2) 切片详情（已完成）

### RND-F11-S01 — Debug 核心 + Pass + RDG 竖切

**Goal：** `DebugDraw` API + `DebugDrawPass` + RDG；Editor 可见世界空间线段；**零 Physics 依赖**。

**DoD：** 见 Design §15.2；`verify.ps1`；GL+VK Editor 目视。

### RND-F11-S02 — Collider wireframe

**Goal：** `PhysicsDebugDraw` 遍历 gameplay `Scene` 提交 Box/Sphere/Capsule wireframe；移除 S01 轴线 smoke。

**Editor 提交点：**

```cpp
if (scene && HasSceneDrawFlag(flags, SceneDrawFlags::EnableDebugDraw))
{
    PhysicsDebugDraw::SubmitScene(*scene, PhysicsDebugDraw::GetOptions());
}
RenderSystem::Get().SubmitSceneDraw(desc);
```

**收尾补丁（同分支）：** `DebugDraw.vert` 视图方向 depth bias，缓解 wireframe z-fighting。

**DoD：**

- [x] Box / Sphere / Capsule wireframe 目视正确
- [x] Channel 颜色可区分
- [x] S01 轴线 smoke 已移除
- [x] Thumbnail / Preview 视口无 collider 线（未开 `EnableDebugDraw`）
- [x] `physics-smoke` / `physics-shapes` 通过
- [ ] [BUG-PHYS-003](../bugs/BUG-PHYS-003.md) — intermittent Add `BoxColliderComponent` crash（Physics/Editor，非本 Feature 阻塞）

---

## 3) 延后工作（不在 RND-F11）

| 主题 | 建议归属 | 说明 |
|------|----------|------|
| Contact / LineTrace 可视化 | Physics 或独立消费 Feature | 需 Physics 提供数据与调用策略；Debug 只提供 `Line`/`Point` API |
| Persistent lifetime（秒级 TTL） | 后续 `RND-F11` Phase 2 或新 Feature ID | `DebugDrawService` 扩展 `EDebugLifetime` |
| Editor toggle / category | 后续 Editor/Debug Feature | 不纳入首个 Debug 通道 Feature |
| `AlwaysVisible` depth | Phase 2 | 与 Persistent 同批 |

---

## 4) Feature Done

- [x] S01–S02 Done
- [x] Design §15.1 MVP（collider + depth test）满足
- [x] §4.3 grep 验收（ForwardRenderer 无 Physics 耦合）
- [x] `FEATURE_REGISTRY` / `PROGRESS_LOG` / `ACTIVE_WORK` 更新
- [x] 范围修订：contact / toggle 移出本 Feature

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 初稿：S01–S04 |
| 2026-09-01 | 解耦修订：提交在 Editor；PhysicsDebugDraw → Physics/ |
| 2026-09-01 | **收尾：** MVP 收窄为 S01–S02；S03/S04 延后；Status → Done |
