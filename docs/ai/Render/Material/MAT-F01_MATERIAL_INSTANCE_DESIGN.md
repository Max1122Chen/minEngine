# MAT-F01 — MaterialInstance（Parent + parameter overrides）

## Meta
- **ID:** MAT-F01
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:** [Material.h](../../../../minEngine/minEngine/src/Runtime/Function/Render/Material.h) · [MaterialInstance.h](../../../../minEngine/minEngine/src/Runtime/Function/Render/Material/MaterialInstance.h)（空壳） · Demo / 0.1.0 · [RND-F17](../RND-F17_ISOLATED_PREVIEW_DRAW_SCHEDULING_DESIGN.md)（预览调度正交）

## TL;DR

`Material` 已能编译图并 `SetTextureParameter` / `SetScalarParameter`，但改的是 **共享资产**。Demo 需要多物体变体（换色/换贴图）而不复制整图。`MaterialInstance` 目前为空类。本 Feature 提供 Parent + Override + Bind 路径；资产格式可二期。

## Scope
- **In:** Runtime `MaterialInstance`（持 Parent、标量/纹理覆盖）；Draw 时用覆盖绑定；可反射/序列化最小集（或先 transient）
- **Out:** 完整 UE MIC 层级 / Static Switch / 材质层叠；强制同期完成 `.memi` 管线（可选二期）；RND-F17 调度

## Reader quick start
1. 本文件：MVP 边界
2. 代码：`Material` 参数表与 `BindForDraw`；空 `MaterialInstance.h`
3. 旧参考（Tier B）：`MATERIAL_RUNTIME_BRIDGE_CHECKLIST` B 节（部分已由 `Material` 本体消化）

---

## 1) 背景与目标

**Pain：** Demo / Prefab 变体只能改 Parent 或复制 `.memat`。

**成功：** 多个 Mesh 共享同一 Parent Material，各自 Instance 覆盖颜色/贴图且互不影响。

## 2) 现状

- `MaterialInstance` 空壳，无 `.cpp`
- 参数与 GPU bind 在 `Material` 上
- 材质图 Phase 0–5 / Editor E0–E4 已可用

## 3) 方案

> **Planned** — MVP 倾向：Runtime Instance 先于完整资产类型；Design 细节在开码前补全并 Pre-flight。

### 3.1 数据流 / 模块边界
（待定：Instance 是否 `Asset`、与 `MeshRenderer`/组件持有关系）

### 3.2 API 或行为契约
（待定：`Set*` / `GetParent` / `BindForDraw` 归属）

## 4) 备选方案（可选）

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Runtime-only Instance | 快，够 Demo | 无独立资产 | **MVP 倾向** |
| B. 同期 `.memi` 资产 | 作者友好 | 拖调度轨 | 二期 |
| C. 继续只改 Material | 零工作 | 破坏共享 | 否 |

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 与 Parent 编译缓存生命周期纠缠 | 泄漏/错绑 | 明确 Parent 编译、Instance 只覆盖值 |
| 预览路径未隔离时手验噪音 | 误判 | 依赖或避开 RND-F17 |

## 6) 验收标准

- [ ] Design 拍板；Registry → In Progress
- [ ] 至少两物体同 Parent、不同覆盖，画面正确
- [ ] 改 Instance 不写回 Parent 资产（除非显式 Apply）
- [ ] 单测或 smoke 记录于 PROGRESS_LOG

## 7) Status note

Planned — 本波 Primary 为 RND-F17 讨论；本 Feature 登记占位。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 登记 Planned |
