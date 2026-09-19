# RND-F17 — Isolated preview / multi-view draw scheduling

## Meta
- **ID:** RND-F17
- **Type:** Refactor
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:**
  - [TD-027](../TECH_DEBT.md) **Done**
  - [ENGINE_0_1_0_ROADMAP](../ENGINE_0_1_0_ROADMAP.md) §3.4 IsoRT
  - [RND-F06](./RND-F06_FORWARD_RENDERER_DESIGN.md) Done · [RND-F07](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md)（图内资源纪律）
  - `SceneDrawDesc` · `SceneViewport` · `SceneRenderTarget` · `RenderGraph::SetupAttachments`
  - Prefab B / `AssetThumbnailService` · `PreviewScene` · `MAT-F01`（正交）

## TL;DR

串图主因：长寿 `ForwardRenderer::m_FrameRenderGraph` 在 `SetupAttachments` 中 **仅按尺寸/格式** 复用物理纹理，多 `SceneDrawDesc` 同尺寸互相覆盖，再经 `PublishGraphColorTexture` 被多 Viewport 长期采样。

**方案（已落地）：** 保持单 `ForwardRenderer` + Granite 式图内分配；物理复用键为 **`ViewId` + `SceneRenderTarget*`（双键 AND）** + dims/format。已重开 Scene3D 缩略图；TD-027 Done。

## Scope
- **In:**
  - `SceneDrawDesc` / `SceneViewport` 携带稳定 `ViewId`
  - `RenderGraph` 物理槽按 View 双键分缓存
  - 双键不一致时 fail-closed（新缓存）+ 诊断日志
  - 重开 CB/Inspector Scene3D 缩略图；关闭 TD-027 绕路
- **Out:**
  - 每预览独立长寿 `ForwardRenderer` / 完整第二套 Manual 缩略图管线
  - 恢复 Scene/ShadowManager 外置创建主路径帧 RT（违反 F07）
  - RND-F12 全 bake 语义；Sort/Batch；完整 Prefab B 产品
  - `MAT-F01` MaterialInstance

## Reader quick start
1. §3 方案与复用契约
2. 代码：`RenderGraph::SetActiveViewKey` · `SceneViewport::AllocateViewId` · `AssetThumbnailService::ResolveBackendKind`

---

## 1) 背景与目标

**Pain：** 主视口 + Preview + Scene3D 缩略图同帧串图；TD-027 曾禁用 Scene3D。

**成功（已达成工程门禁）：**
- RDG 物理纹理按 View 隔离（单测）
- Scene3D 缩略图路径重新启用
- 仍遵守 F07：中间 RT 由 RDG 分配

**手验（维护者）：** Level + Material Preview 同帧；CB Material/Mesh 缩略图不串主视口。

## 2) 现状（实现后）

| 组件 | 行为 |
|------|------|
| `SceneViewport` | `Initialize` 分配单调 `ViewId`；`BuildDrawDesc` 写入 desc |
| `SceneDrawDesc` | `ViewId` 字段；`Execute` 要求非 0 |
| `RenderGraph` | `m_ViewPhysicalCaches` 按 `{ViewId, SRT*}`；`SetActiveViewKey` 后 Bake/SetupAttachments |
| `AssetThumbnailService` | Material/StaticMesh → Scene3D |
| TD-027 | **Done** |

---

## 3) 方案（已实现）

### 3.0 设计原则

1. **调度：** 单 Renderer，按 View 重绑执行。  
2. **分配：** F07 — 中间 RT 仍由 RDG `SetupAttachments` 创建/复用。  
3. **隔离：** 复用键 = View 双键 + dims；禁止仅同 Size 跨 View 共享。  
4. **显示：** `PublishGraph*` 仍指向本 View 缓存中的物理纹理。

### 3.1 View 双键

| 键 | 来源 |
|----|------|
| `ViewId` (`uint64_t`) | `SceneViewport::AllocateViewId`（静态单调） |
| `SceneRenderTarget*` | `SceneDrawDesc::RenderTarget` |

复用：`ViewId` ∧ `SRT*` ∧ dims/format。部分匹配 → Warn + 新缓存条目。

### 3.2 切片

| Slice | 状态 |
|-------|------|
| S01 ViewId + desc | **Done** |
| S02 SetupAttachments 双键缓存 | **Done** |
| S03 重开 Scene3D / TD-027 | **Done** |
| S04 文档收口 | **Done** |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| A. 每预览长寿独立 Renderer | **否** |
| B. View 双键扩展物理复用 | **选用（已实现）** |
| C. Manual 缩略图旁路 | **否** |
| D. 整帧外置 RT 双轨 | **否** |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| `ViewId == 0` | Execute 早退 + Error 日志 |
| Viewport 销毁后缓存残留 | 可接受；图 Reset 不清缓存；后续可加 Release |
| 阴影同池 | 同 ViewKey；预览可关阴影 |

---

## 6) 验收标准

- [x] 维护者批准 Design
- [x] `test render-graph` PASS（含双 ViewKey 隔离用例）
- [x] `Editor` + `minEngineTests` 构建通过
- [x] 启用 Scene3D（去掉 TD-027 禁用绕路）
- [x] TD-027 → Done
- [x] `ViewId == 0` 不可静默当全局池
- [x] 未恢复外置帧 SceneColor 双轨
- [ ] 手验：Level + Material Preview；CB Scene3D 缩略图（维护者）

---

## 7) Status note

**Done** — 工程落地；手验项留给维护者确认后可再记一笔 PROGRESS。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 登记 Planned；吸收 IsoRT / TD-027 |
| 2026-09-18 | 定稿 Review：ViewId + SRT* |
| 2026-09-18 | 实现 S01–S04；Status → **Done**；`test render-graph` 7/7 |
