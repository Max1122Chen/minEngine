# RND-F17 — Isolated preview / multi-view draw scheduling

## Meta
- **ID:** RND-F17
- **Type:** Refactor
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:** [TD-027](../TECH_DEBT.md) · [ENGINE_0_1_0_ROADMAP](../ENGINE_0_1_0_ROADMAP.md) §3.4 IsoRT · Prefab B · `AssetThumbnailService` · `PreviewScene` · [RND-F06](./RND-F06_FORWARD_RENDERER_DESIGN.md) Done

## TL;DR

主 Level Viewport、Material/Inspector `PreviewScene`、Content Browser Scene3D 缩略图若共用 **单 ForwardRenderer + 单帧 RDG/同尺寸 transient RT**，会串图。当前用 **禁用 Scene3D 缩略图** 绕过（TD-027）。本 Feature 重建 **多 view 提交与 RT 身份**，使预览可安全重开，并解锁 Prefab 并排/隔离预览。

## Scope
- **In:** 多 `SceneDrawDesc` 调度契约；RT / graph 身份（禁错误别名）；CB tile / Inspector / Material Preview /（可选）Prefab Stage 与主视口互不污染；重新启用 Scene3D 缩略图并关 TD-027 绕路
- **Out:** Sort/Batch 主航道；RND-F12 全语义；完整 Prefab B 子编辑器产品（可依赖本 Feature，但不在本 DoD）；MaterialInstance（`MAT-F01`）

## Reader quick start
1. 本文件：问题边界与候选方案（讨论中）
2. 对照：UE `FSceneRenderer` / PreviewScene；Godot Viewport；Unity Camera/stack
3. 代码入口：`RenderSystem::SubmitSceneDraw` · `ForwardRenderer::Execute` · `Editor/.../AssetThumbnailService` · `Preview/PreviewScene`

---

## 1) 背景与目标

**Pain：** 帧内多次 `SubmitSceneDraw` 共享错误资源身份 → 主视口与预览串图；CB 被迫 icon fallback。

**成功：** 任意组合打开主视口 + Material Preview + N 个 Scene3D thumb，画面各自正确；TD-027 → Done 或 Closed。

## 2) 现状

- `RenderSystem` 队列 `SceneDrawDesc`，由单一 `SceneRenderer`（`ForwardRenderer`）顺序 Execute
- CORE-F25 已让组件挂 owning Scene 的 `RenderScene`（Prefab Stage 串世界部分缓解）
- TD-027：`ResolveBackendKind` 对非 Texture2D 返回 None，禁用 Scene3D；实现路径仍在

## 3) 方案

> **讨论中（2026-09-18）** — 对照 UE / Godot / Unity 后再拍板。候选方向见会话结论，写入本节后 Status → Review / In Progress。

### 3.1 数据流 / 模块边界
（待定）

### 3.2 API 或行为契约
（待定）

## 4) 备选方案（可选）

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 每预览上下文独立 Renderer 或独立 RDG 实例 | 隔离强 | 成本高 | 讨论中 |
| B. 显式 RT 身份 / 禁同尺寸错误别名 | 改动面可控 | 需理清 RDG 池语义 | 讨论中 |
| C. 预览走简化 Manual/离屏路径 | 实现快 | 双路径行为漂移 | 讨论中 |

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 范围膨胀成完整 Prefab B | 拖 0.1.0 | Prefab B Out；本 Feature 只解串图 |
| 与 MAT-F01 预览耦合 | 扯皮 | Instance 可先 Runtime；预览重开依赖本 Feature |

## 6) 验收标准

- [ ] Design 拍板并 Status → In Progress
- [ ] Scene3D CB/Inspector 缩略图可开且不串主视口
- [ ] Material Preview 与 Level Viewport 同帧共存正确
- [ ] TD-027 关闭或改为 Done
- [ ] 记录验证命令 / 手验步骤于 PROGRESS_LOG

## 7) Status note

Planned — 方向讨论后再开实现切片。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 登记 Planned；吸收 0.1.0 IsoRT / TD-027 |
