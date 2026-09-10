# ANIM-F04 — Blend Tree (1D) — Implementation Plan

## Meta
- **ID:** ANIM-F04
- **Status:** Planned（Design Review；审批前不编码）
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Related:** [Design Spec](./ANIM-F04_BLEND_TREE_DESIGN.md)

## TL;DR
S00 数据/序列化 → S01 Runtime 求值+单测 → S02 Inspector → S03 画布标记（可选）→ S04 文档。

## 1) 切片总览

| Slice | 内容 | 状态 | 验证 |
|-------|------|------|------|
| S00 | AnimState Kind + BlendTree1D 结构；Validate；JSON | Planned | 旧图加载 |
| S01 | Instance 评价 BlendTree1D；	est animation-graph 扩展 | Planned | 单测 |
| S02 | Inspector 编辑阈值阈/Param/Clip | Planned | 手测 |
| S03 | 画布副标题/图标（非嵌套编辑器） | Planned | 目视 |
| S04 | Progress / 验收勾选 | Planned | DoD |

## 2) Engineering notes
- **Build:** Editor + minEngineTests（animation-graph）
- **错峰：** 避免与 ED-F06 同时大改 AnimGraphSmBridge / Window

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-10 | 初版切片 |
