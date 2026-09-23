# RND-F23 — Scene Mesh Pass 默认背面剔除

## Meta
- **ID:** RND-F23
- **Type:** Feature
- **Complexity:** L1
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - PSO：`EnginePipelineLayouts::GetOrCreateSceneMeshGraphicsPipelineState`
  - 对照：ShadowPass 显式开 cull；SkyBoxPass / DebugDrawPass 显式关
  - Desc 默认：`RHIRasterizerStateDesc::bCullEnabled = false`

## TL;DR

**问题：** Scene Mesh Opaque/Translucent PSO 未开光栅剔除；内部可见背面。

**方案：** 在 `GetOrCreateSceneMeshGraphicsPipelineState` 对 Opaque/Translucent 设 `bCullEnabled=true`、`CullMode=Back`；ScreenUI 显式关 cull。不改全局 Desc 默认，不改 Shadow/Sky。

**状态：** Review — design-review **Ready**。

## Problem

- 根因：`bCullEnabled` 默认 false；Scene Mesh PSO 构建未写 RasterizerState。
- RHI：`!bCullEnabled` → 无剔除。

## Requirements

- R1：Opaque → Back cull。
- R2：Translucent → **同 Back cull**（锁定；双面后置）。
- R3：ScreenUI → cull off（`bCullEnabled=false`，`CullMode=None`）。
- R4：Sky / DebugDraw / Shadow 不变。
- R5：GL/VK 同走 PSO desc。

## Constraints

- 不引入材质 TwoSided（记后续 / TECH_DEBT）。
- 不改 `RHIRasterizerStateDesc` 全局默认（防误伤 Present/Post）。
- PSO key 已含 `passKind`；本期 cull 恒定，**不必**扩 key。同进程热改代码后旧 PSO 缓存极少见；实现可不强制清缓存。

## Preferences

- 单点改 `GetOrCreateSceneMeshGraphicsPipelineState`。
- 不碰 FrontFace / winding（与 Shadow TD-025 独立）。

## Assumptions

- A1：CCW = front（引擎/GL 惯例）；错误 winding 修资产。
- A2：主因是无 cull，非负缩放。

## Non-Goals

- TwoSided / 每 MDC cull；Shadow cull 策略；CPU 背面剔；修全仓 winding。

## Current Architecture

```
GetOrCreateSceneMeshGraphicsPipelineState(passKind)
  sets Depth/Blend only
  RasterizerState left default (cull off)
```

## Proposed Architecture

```
if ScreenUI:
  bCullEnabled = false; CullMode = None
else: // Opaque, Translucent
  bCullEnabled = true; CullMode = Back
```

## Responsibilities

| Unit | For | Not |
|------|-----|-----|
| EnginePipelineLayouts | Scene Mesh raster 默认 | 材质双面 UI |
| Pass | 选 passKind | 复制 cull |
| RHI | 应用 desc | 猜意图 |

## Boundaries

- Allowed：Scene Mesh 单点。
- Forbidden：改全局 Rasterizer 默认。

## Verification Strategy

- 手验：封闭 mesh 外部正常；穿入内部不应大面积见背面。
- 回归：ScreenUI 可见；Sky；Shadow。
- 可选：对 PSO desc 字段断言（若可测）。

## Open Questions（非阻塞）

1. TwoSided 何时开 Feature — Defer。
2. 负缩放翻 cull — Out。

## Design Review

- **Mode:** A
- **Verdict:** Ready
- **Date:** 2026-09-22
- **Findings absorbed：** API 名更正为 GraphicsPipelineState；Translucent=Back 锁定；不扩 key。
- **Ready for implementation?** yes

## Suggested Slices

| Slice | 内容 |
|-------|------|
| S01 | 设 cull + 手验 Opaque/Translucent/ScreenUI/Sky |
| S02 | Progress；可选断言 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
| 2026-09-22 | design-review Ready；API 名与 Translucent 锁定 |