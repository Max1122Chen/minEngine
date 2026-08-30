# Audio System — Placeholder

## Meta
- **ID:** `AUD-F01`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../../ACTIVE_WORK.md)
- **Branch / worktree:** `feat/audio` · `D:/Dev/GitRepo/minEngine-audio`

## TL;DR
引擎 **音效子系统**：音频资产、播放组件、与 Scene/Engine tick 集成。与渲染弱相关；从 `master` 竖切。

## Scope
- **In:** 音频资产类型与加载、基础播放 API、3D/2D 音源组件（MVP 范围待 Design）
- **Out:** 中间件选型定稿前的实现细节；与动画 lipsync 的深度集成（后置）

## 依赖
- **Hard:** AssetManager、Engine 生命周期
- **Soft:** 无

## 验收（草案）
- [ ] 加载并播放测试 wav/ogg（格式 TBD）
- [ ] `minEngineTests` smoke 子集或专用 suite
- [ ] 不依赖 VK / RenderGraph

## Status note（Planned）
| 字段 | 内容 |
|------|------|
| Why now | 可与 Launcher、Physics 并行 |
| What's not | 后端选型、Design 正文、Implementation Plan |
| Resume when | S01：后端调研 + 最小播放竖切 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry + ACTIVE_WORK 登记；`feat/audio` + worktree 初始化 |
