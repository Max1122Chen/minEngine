# Render Binding / RHI Hygiene Sweep — Implementation Plan

## Meta
- **ID:** `RND-F09`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-03
- **Related:** [Design Spec](./RND-F09_RHI_HYGIENE_SWEEP_DESIGN.md)

## TL;DR

六切片对应六债（无 EnvMap）。顺序：缓存 → 契约 → 清扫。S01–S06 已完成。

## Scope
- **In:** TD-013/014/016/017/018/019
- **Out:** TD-015；Vulkan

## Reader quick start
1. Design
2. 下表
3. `PROGRESS_LOG.md`

---

## 1) 切片总览

| Slice ID | 内容 | TD | 状态 | 验证 |
|----------|------|-----|------|------|
| RND-F09-S01 | Set0 脏标记 + 缓存指针 | 013 | **Done** | smoke |
| RND-F09-S02 | Material SRV → TextureViewCache | 014 | **Done** | smoke |
| RND-F09-S03 | `RHI` Clear 中性 API；去 OpenGL cast | 017 | **Done** | smoke |
| RND-F09-S04 | PSO Apply 补 cull/depthFunc/blend 因子 | 016 | **Done** | smoke；半透待目视 |
| RND-F09-S05 | Shader 反射/CB 残留清扫 | 018 | **Done** | 编译；无死类型 |
| RND-F09-S06 | ShadowTypes unit → EngineShaderBindings | 019 | **Done** | smoke |

## 2) 切片详情

### RND-F09-S01 — Set0 dirty cache
- **Goal:** 指针未变不重建 Set0。
- **Touch:** `EngineSceneBindingSets.h/.cpp`
- **DoD:**
  - [x] 缓存三指针；脏才 Create
  - [x] Shutdown 清空
- **Verify:** `test smoke`；黄金场景

### RND-F09-S02 — Material ViewCache
- **Goal:** Material 纹理 SRV 走 cache。
- **Touch:** `Material.h/.cpp`
- **DoD:**
  - [x] Rebuild 无裸 CreateSRV（纹理路径）
  - [x] 改纹理参数仍正确刷新（仍走 Rebuild）
- **Verify:** smoke + 材质预览/视口

### RND-F09-S03 — Backbuffer clear via RHI
- **Goal:** `RenderSystem` 不认识 OpenGL。
- **Touch:** `RHI.h`、`OpenGLRHI.*`、`RenderSystem.cpp`
- **DoD:**
  - [x] `RHISetBackbufferClearColor` / `RHIClearBackbuffer`
  - [x] 零 `static_cast<OpenGLRHI*>` in RenderSystem；去 `friend RenderSystem`
- **Verify:** Editor 背景 clear；smoke

### RND-F09-S04 — PSO Apply completeness
- **Goal:** desc 已有字段落到 GL。
- **Touch:** `OpenGLRHI.cpp`（Apply）、`RHIBlendStateDesc` 注释
- **DoD:**
  - [x] cull + depth func（已有）+ blend factors（SrcAlpha/OneMinusSrcAlpha）
  - [x] 因子尚未 desc 驱动 — 注释标明
- **Verify:** 半透物体；smoke

### RND-F09-S05 — Shader remnant cleanup
- **Goal:** 无假 Shader Asset 表面。
- **Touch:** 删除 `ShaderResource.h` + gen；ContentBrowser 过滤
- **DoD:**
  - [x] 无死类型注册/入口
- **Verify:** 编译；CB 抽检

### RND-F09-S06 — Unit constant consolidation
- **Goal:** `ShadowTypes` 不再承载 GL unit。
- **Touch:** `ShadowTypes.h`、`EngineShaderBindings.h`、`EngineSceneBindingSets.cpp`
- **DoD:**
  - [x] `kGL_SpotShadowBaseUnit` / `kGL_PointShadowBaseUnit`
  - [x] SlotIndex 路径不变
- **Verify:** 阴影目视；smoke

## 3) 依赖顺序

```text
S01 → S02 → S03 → S04 → S05 → S06
```

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-03 | Planned 初稿 |
| 2026-08-03 | Done：S01–S06；`verify.ps1` smoke PASS |
