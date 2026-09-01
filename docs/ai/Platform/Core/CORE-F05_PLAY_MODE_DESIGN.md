# Play Mode — Placeholder

## Meta
- **ID:** `CORE-F05`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../../ACTIVE_WORK.md)
- **Branch:** `master`
- **Depends on:** `CORE-F06`（建议先完成 Component Enable）

## TL;DR
Editor **Edit / Play** 模式切换：Play 时场景以运行时方式 Tick（Physics、Script、Audio 等）；**Stop** 回到 Edit 状态（不丢未保存 Scene 编辑上下文）。ANIM-F01 前置能力。

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | 正式 Design、Implementation Plan、代码 |
| Scope sketch | `Engine`/`SceneManager` 运行时域；Editor 工具栏 Play/Stop；与 `bEnabled`、Physics 步进对齐 |
| Branch | `master`（内核） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Registry 占位登记（双轨 backlog 审批稿） |
