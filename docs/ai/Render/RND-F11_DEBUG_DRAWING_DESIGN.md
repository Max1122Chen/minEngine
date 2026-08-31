# DebugDrawing — Design Spec (placeholder)

## Meta
- **ID:** `RND-F11`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Branch:** `feat/debug-drawing`
- **Related:** [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md) · [PHYS-F03 placeholder](../Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md)

## TL;DR
即时调试图元（线、盒、球/胶囊 wireframe 等）供 **Editor 主视口**消费，优先支撑 **Physics** 测试与可视化（collider、contact、trace）。正式方案待下一 Agent 补全。

## Scope
- **In (MVP intent):**
  - 帧缓冲 `DrawLine` / `DrawBox` / 基础 wireframe 原语
  - Editor 视口渲染（GL + VK）
  - Physics collider / contact / trace 可视化消费方
- **Out (initial):**
  - 完整 UE `DrawDebugHelpers` 对等 API
  - Standalone 游戏运行时 HUD
  - 文本标签、ImGui 混合排版

## Reader quick start
1. 本文件：占位 + 边界（**待扩写**）
2. 下一 Agent：补 §方案、切片、验收；参考 `ACTIVE_WORK.md` DebugDrawing 轨
3. 代码入口：待建（预期 `Render/DebugDrawing/` 或同级模块）

---

## 1) 背景与目标
Physics F01/F02 已提供 Contact、LineTrace、形状查询，但缺少视口内可视化，测试与调试依赖日志/断言。本 Feature 补齐调试绘制基础设施，并为 PHYS-F03 等后续玩法工作铺路。

## 2) 现状
- Runtime 无 `DebugDraw` / `DrawDebug` 模块
- Editor 主视口 GL/VK 路径已可用（ED-F01）
- `FEATURE_REGISTRY` 已登记 `RND-F11`；本文件为占位 Draft

## 3) 方案
**待下一 Agent 填写。** 建议 Pre-flight 时比较：
- **A（推荐倾向）：** 独立 `DebugDrawPass` + 动态 VB，接入 Forward 管线末尾
- **B：** ImGui `ImDrawList` 3D 投影（快，VK/深度一致性弱）

## 4) 验收（草案）
- [ ] Editor 视口可见 collider wireframe（至少 Box）
- [ ] `LineTrace` 命中点/线段可画
- [ ] VK + GL 同场景目视一致
- [ ] 不影响 shadow 质量轨在 `feat/render` 的并行修复

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 占位 Draft；分支 `feat/debug-drawing` 开轨 |
