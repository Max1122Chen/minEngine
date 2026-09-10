# RND-F16 — 2D Rendering Foundation — Implementation Plan

## Meta
- **ID:** `RND-F16`
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:** [Design Spec](./RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)

## TL;DR
Path A（Sprite）**Done**。下一刀 **Path B（ScreenUI）**：Queue@`SceneRenderContext` → Pass → `WidgetComponent`（无 Canvas）。详见 Design §10。

## Scope
- **In:** Path A 全套；Path B：`UIDrawCommand` / `ScreenUIQueue` / `ScreenUIPass` / `WidgetComponent`+Proxy；复用 `SpriteQuadMesh`
- **Out:** Canvas；Layout/Hit-test；UVRect GPU remap；Billboard；ImGui 耦合

## Reader quick start
1. Design §10（Path B）
2. 下表 S03
3. `PROGRESS_LOG.md`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `RND-F16-S00` | `SpriteQuadMesh` 共享 unit quad | Done | 编译 |
| `RND-F16-S01` | `SpriteComponent` + `SpriteSceneProxy` + 入队 | Done | 编译 |
| `RND-F16-S02` | 透明分流 + 默认 Material + 单测 | Done | `sprite-translucency` |
| `RND-F16-S03a` | `UIDrawCommand` + `ScreenUIQueue` + `ScreenUIPass` 骨架 | Done | 编译 |
| `RND-F16-S03b` | `WidgetComponent` + Proxy + `BuildScreenUIQueue` + 可画色块 | Done | Editor 目视 + `screen-ui-coords` |

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
- **Verify:** `minEngineTests.exe test sprite-translucency`

### RND-F16-S03a — ScreenUI Queue + Pass 骨架
- **Goal:** 帧内有 ScreenUI 槽位；空队列安全；挂在正确图序
- **Touch:**
  - `SceneRenderContext`（`ScreenUIQueue` + Reset）
  - `UIDrawCommand.h`
  - `ScreenUIPass.*`
  - `ForwardRenderer::BuildFrameRenderGraph` / Execute 挂钩
- **DoD:**
  - [x] `ScreenUIQueue` 与 Opaque/Translucent 并列
  - [x] Graph：Translucent → (Debug) → **ScreenUI** → Post → Present
  - [x] Depth 关；空队列 no-op
  - [x] **不**从 `BuildRenderQueue` 写入 ScreenUI
- **Verify:** 编译；可选日志/断点确认 Pass 被调度

### RND-F16-S03b — WidgetComponent 最小可画
- **Goal:** Comp→Proxy→`BuildScreenUIQueue`→色块/贴图；像素左上
- **Touch:**
  - `WidgetComponent.*`、`WidgetSceneProxy`
  - `RenderScene` Widget 登记（非 Primitive 列表）
  - `BuildScreenUIQueue` + 屏幕矩阵（§10.3）
  - ScreenUI 材质工厂（可复用/仿 Sprite Unlit）
  - `minEngine.h` + 反射 codegen
- **DoD:**
  - [x] 反射：Texture(可选) / Color / Size / StableOrder；Location.xy = 左上像素
  - [x] 复用 `SpriteQuadMesh`
  - [x] 绝不入 Opaque/Translucent
  - [x] 两 Widget 重叠时 StableOrder 保序可见
- **Verify:** Editor 目视；`minEngineTests.exe test screen-ui-coords`

## 3) 依赖顺序

```text
S00 → S01 → S02 → S03a → S03b → (UI-F01 Canvas…)
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| Canvas / Layout | UI-F01 | Path B MVP 可画 | Path B Done 后 |
| UVRect GPU remap | Sprite+Widget 共用债 | 需要 atlas 时 | 维护者 |
| Path C World UI | 后置 | Screen UI 稳 | — |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 初稿；先 Path A S00–S02 |
| 2026-09-04 | Path A Done；S03 拆 S03a/S03b；对齐 Design §10 拍板 |
| 2026-09-04 | S03a/S03b 代码落地；`screen-ui-coords` 单测 |
