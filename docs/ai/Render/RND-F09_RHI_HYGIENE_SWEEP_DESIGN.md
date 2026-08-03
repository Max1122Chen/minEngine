# Render Binding / RHI Hygiene Sweep — Design Spec

## Meta
- **ID:** `RND-F09`
- **Type:** Refactor
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-03
- **Related:** [Implementation](./RND-F09_RHI_HYGIENE_SWEEP_IMPLEMENTATION.md), [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [ACTIVE_WORK](../ACTIVE_WORK.md), [TECH_DEBT](../TECH_DEBT.md)（TD-013/014/016/017/018/019）, [RND-F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md), [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md)

## TL;DR

F07/F08 之后主路径资源所有权已清。本 Feature **打包付清** 与绑定缓存、RHI 契约、残留清扫相关的 Open 债（**不含 TD-015 EnvMap**）。目标：Set0/Material 少分配、Apply/Clear 可后端中立、阴影 unit 语义收口——为后续 EnvMap 专题与 F05 Vulkan 减摩擦。

## Scope
- **In:** TD-013、TD-014、TD-016、TD-017、TD-018、TD-019。
- **Out:** **TD-015 EnvMap**（用户明确后议）；RND-F05 Vulkan 实现；RND-F06-S03 目录改名；阴影算法/质量。

## Reader quick start
1. 本文件 §3 逐债方案
2. [Implementation](./RND-F09_RHI_HYGIENE_SWEEP_IMPLEMENTATION.md) 切片顺序
3. 入口：`EngineSceneBindingSets.*`、`Material.cpp`、`OpenGLRHI::ApplyGraphicsPipelineState`、`RenderSystem.cpp`、`ShadowTypes.h` / `EngineShaderBindings.h`

---

## 1) 背景与目标

**Pain：** 绑定每帧重建、Material SRV 散造、PSO Apply 不全、`RenderSystem` 绑死 OpenGL、反射/单位常量残留——与「现代 RHI + 第二后端」方向不一致。

**Done：** 上表六条 TD Status→Done；`test smoke` PASS；黄金场景无回归。

## 2) 现状（按债）

| TD | 现状一句话 |
|----|------------|
| 013 | `BuildSceneSet0` 每帧 `CreateShaderBindingSet`；Set1 已有脏标记 |
| 014 | `Material::RebuildMaterialBindingSet` 裸 `CreateShaderResourceView` |
| 016 | Apply 主要覆盖 program/VAO/blend on-off/depth test+mask；cull/blend 因子等缺 |
| 017 | `RenderSystem` `static_cast<OpenGLRHI*>` 调 Window Clear |
| 018 | Shader Asset 已删，反射/浏览器可能仍挂类型名 |
| 019 | `SPOT_SHADOW_*_UNIT` 仍在 `ShadowTypes` + Set1 layout；选图已用 SlotIndex |

## 3) 方案

### 3.1 TD-013 — Set0 脏标记

- 在 `EngineSceneBindingSets` 缓存上次 `perFrame/lights/perObject` 指针（及 layout 有效性）。
- 仅指针变化或 `m_SceneSet0` 空时 `CreateShaderBindingSet`。
- UBO 内容仍每帧 `UpdateSubresource`，与 BindingSet 寿命解耦。

### 3.2 TD-014 — Material SRV → ViewCache

- `Material` 持有 `RHITextureViewCache`（或 Renderer 注入共享 cache，优先 **Material 自持** 简单寿命）。
- `RebuildMaterialBindingSet`：`GetOrCreate(cmdList, texture)` 替代裸 Create。
- 纹理参数变更：失效对应 key 或整表 Clear 后重建 Set2（与现 rebuild 同频即可）。

### 3.3 TD-016 — PSO Apply 补全（GL 子集）

- 扩展 `OpenGLRHI::ApplyGraphicsPipelineState`：至少 **cull mode**、**depth compare**、**blend 因子/equation**（按现 `RHIGraphicsPSODesc` 已有字段）。
- 不发明新 desc；缺字段则先文档化「仍 deferred」而非空开 API。
- 手工：半透/双面网格场景抽检。

### 3.4 TD-017 — 后端中立 Clear

- `RHI` 增加中性入口，例如 `ClearBackbuffer(const float rgba[4])` / `SetBackbufferClearColor`（命名以现有 RHI 风格为准）。
- `OpenGLRHI` 实现内调 WindowSystem；`RenderSystem` 只调 `RHI*`。
- **删除** `RenderSystem` 内 `static_cast<OpenGLRHI*>`。

### 3.5 TD-018 — Shader 残留清扫

- 搜反射注册 / ContentBrowser 过滤 / 文档中的 `Shader` Asset 类型。
- 删除死注册与 UI 入口；不改渲染主路径。

### 3.6 TD-019 — Unit 常量收口

- 采样下标：继续只认 `SlotIndex`（已落地）。
- GL layout unit：**仅** `EngineShaderBindings`（或 layout 初始化旁）。
- 从 `ShadowTypes.h` 移除 `SPOT_SHADOW_MAP_BASE_UNIT` / `POINT_SHADOW_MAP_BASE_UNIT`（或改为 deprecated 转发到 Bindings 一次后删）。
- 更新 `static_assert` 与 Set1 layout 引用。

### 3.7 与 F03 / F05 关系

- 本 Feature **不**关闭整个 `RND-F03`；只付清所列 TD。
- TD-015 仍挂 F03 / 独立后续专题。
- F05 受益于 016/017，但不阻塞本 Feature 收口。

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A 单 Feature 打包六债 | 一次排期、顺序清晰 | 跨文件略散 | **选用** |
| B 每债单独 Feature | ID 细 | 过碎 | 拒绝 |
| C 并入 F03 Done 清单 | 少新 ID | F03 已过大、EnvMap 纠缠 | 拒绝 |

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Set0 脏标记漏判 | 错绑 UBO | 指针三元组全比；Initialize/Shutdown 清缓存 |
| Material cache 与多线程 | 暂无 | 保持单线程 Renderer 假设 |
| Apply 补全改变默认 GL 状态 | 画面差 | 缺省对齐当前「未设时驱动默认」；目视黄金场景 |
| Clear API 命名争议 | 小 | 跟现有 RHI 动词风格 |

## 6) 验收标准

- [x] TD-013/014/016/017/018/019 → **Done**（Notes 记日期与 F09）
- [x] TD-015 仍 Open / Deferred（本 Feature 不碰）
- [x] `minEngineTests.exe test smoke` PASS（`.\scripts\verify.ps1`）
- [ ] Editor 黄金场景目视 OK（待用户）
- [x] `RenderSystem` 无 `OpenGLRHI*` cast（清窗路径）

## 7) Status note

S01–S06 已落地：Set0 脏缓存、Material `RHITextureViewCache`、`RHISetBackbufferClearColor`/`RHIClearBackbuffer`、PSO Apply 补 blend 因子（cull/depthFunc 已有）、删除 `ShaderResource` 反射/CB 入口、阴影 GL unit 迁入 `EngineShaderBindings`。Blend 因子尚未 desc 驱动（注释标明）。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-03 | Planned：登记 F09；打包六债；明确排除 TD-015 |
| 2026-08-03 | Done：S01–S06 实现；verify smoke PASS；TD-015 仍 Open |
