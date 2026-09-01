# Editor Workflow — Placeholder

## Meta
- **ID:** `ED-F02`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../ACTIVE_WORK.md)
- **Branch:** `feat/editor`

## TL;DR
编辑器日常 **资产与场景工作流**：打开/切换 Scene、创建 Scene/Material 等资产、Material Editor SkyBox、Viewport 鼠标捕获、Component 下拉图标与 Abstract 过滤。与 `master` 内核并行；**一次 merge 检查点** 合回 `master`。

## Status note（Planned）

| Slice | 内容 | 优先级 |
|-------|------|--------|
| S01 | 打开 Scene（File/Open、切换、dirty 提示） | 高 |
| S02 | 创建资产（Scene、Material、…） | 高 |
| S03 | Material Editor SkyBox 修复 | 中 |
| S04 | Viewport 鼠标约束 | 中 |
| S05 | Abstract Component 过滤 + Component 下拉图标 | 低 |

**开干前：** `git checkout feat/editor && git merge master`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Registry 占位登记（双轨 backlog 审批稿） |
