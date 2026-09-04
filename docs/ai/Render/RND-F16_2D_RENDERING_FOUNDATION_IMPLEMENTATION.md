# RND-F16 — 2D Rendering Foundation — Implementation Plan

## Meta
- **ID:** `RND-F16`
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:** [Design Spec](./RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)

## TL;DR
优先 **Path A（Sprite）** 三刀：共享 quad → Component/Proxy/入队 → 透明规则验证。Path B Deferred。

## Scope
- **In:** `SpriteComponent` / `SpriteSceneProxy` / `SpriteQuadMesh` / 默认 Unlit Material 工厂 / `BuildRenderQueue` + `RenderScene` 更新
- **Out:** Billboard；ScreenUI Pass；`WidgetComponent` 实现；完整 UV atlas 编辑器

## Reader quick start
1. Design §9
2. 下表切片
3. `PROGRESS_LOG.md`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `RND-F16-S00` | `SpriteQuadMesh` 共享 unit quad | Done | 编译 |
| `RND-F16-S01` | `SpriteComponent` + `SpriteSceneProxy` + 入队 | Done | 编译 |
| `RND-F16-S02` | 透明分流 + 默认 Material + 单测 | Done | `sprite-translucency` |
| `RND-F16-S03` | Path B ScreenUI | Deferred | — |

## 2) 切片详情

### RND-F16-S00 — Shared SpriteQuadMesh
- **Goal:** 进程内共享 unit quad VB/IB/layout（`a_Position/a_TexCoord/a_Normal/a_Tangent`），中心原点、边长 1、法线 +Z
- **Touch:** `Runtime/Function/Render/Sprite/SpriteQuadMesh.*`
- **DoD:**
  - [x] `EnsureInitialized(RHI&)` 可重复调用
  - [x] 非拥有指针可供 Proxy 使用
- **Verify:** 目标 `minEngine` / `Editor` 编译

### RND-F16-S01 — SpriteComponent + Proxy + 入队
- **Goal:** Comp→Proxy→Opaque/Translucent 同构 StaticMesh；默认不投射阴影；Size×WorldMatrix
- **Touch:** `SpriteComponent.*`、`SpriteSceneProxy.h`、`ForwardRenderer::BuildRenderQueue`、`RenderScene::UpdatePrimitive`、`minEngine.h`
- **DoD:**
  - [x] 反射属性：Texture / Color / Size / UVRect
  - [x] 无 Texture 或未编译 Material → 不入队
  - [x] `CastShadow == false`
- **Verify:** 编译；反射 codegen 通过

### RND-F16-S02 — Translucency + default material
- **Goal:** `ComputeSpriteNeedsTranslucentPass`；默认 Unlit textured Material（Opaque/Translucent）；Tint/Opacity 参数同步 Color
- **Touch:** `SpriteTranslucency.*`、`SpriteMaterialFactory.*`、可选 test
- **DoD:**
  - [x] Color.a 或 Channels≥4 → Translucent
  - [x] 单测覆盖谓词（若易加）
- **Verify:** `minEngineTests.exe test …` 或文档记录手动

### RND-F16-S03 — Path B（Deferred）
- **Goal:** ScreenUI Queue/Pass + `WidgetComponent` 契约落地
- **DoD:** 见 Design §10
- **Verify:** —

## 3) 依赖顺序

```text
S00 → S01 → S02 → (S03 Deferred)
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| S03 Path B | 先扎实 Sprite | Path A MVP Done | 维护者排期 |
| UVRect≠0..1 GPU remap | 可用缓存 mesh 或后置 | S02 后 | 需要时 |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 初稿；先 Path A S00–S02 |
