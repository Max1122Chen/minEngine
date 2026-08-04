# RND-F05 — Vulkan backend & modern RHI completion

## Meta

| Field | Value |
|-------|--------|
| **Feature ID** | `RND-F05` |
| **Status** | Planned |
| **Last updated** | 2026-08-04 |
| **Depends on** | `RND-F03` **Done** · `RND-F04` **Done**（现代 RHI 语义终态） |
| **Related** | [RND-F02](./RND-F02_MODERN_RHI_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · [RND-F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) |

> **Legacy mapping：** 2026-06-01 初稿登记为 `RND-F04`；2026-06-11 维护者将 Vulkan 顺延为 **F05**，F04 专指现代 RHI 进一步演进。

## 1) Goal

Introduce **Vulkan** as a second backend implementing the same `RHICreate*` / `RHICmd*` contract, and **complete** modern RHI capabilities that F02 describes but left simplified on OpenGL (transitions, stricter resource views, descriptor pools, sync, true PSO compile, etc.).

**Product end-state (maintainer 2026-08-04):** 上层渲染管线（Pass / RDG / 材质 / 阴影 / IBL 等）应对 **OpenGL 与 Vulkan 同等支持**；仅光追等 **API/硬件限制** 的能力可单后端。达成方式是 **多刀竖切、逐步扩大 VK 覆盖面**，不是单一切片「一次做完」。

## 2) Non-goals (near-term)

- 单一切片内 GL/VK 全管线 parity
- RenderGraph 重写（`RND-F01` 已有；F05 对接而非另起）
- DebugDrawing（`RND-F11`，F05 竖切可演示后再设计）
- Feature parity with every UE RHI extension in one milestone

## 3) Done definition (Feature 级，草案)

分里程碑扩展（每刀可独立演示/回归），例如：

| 阶段（示意） | 内容 |
|--------------|------|
| S0 | Design/Impl 切片表 + 设备/实例/表面选型 |
| S1 | Vulkan 设备 + 交换链 + Clear/Present |
| S2 | 最简图形管线（全屏或单 mesh） |
| S3+ | 逐步接入 Forward / Shadow / Material / IBL… 与 GL 对照 |

具体切片表在开干前写入 Implementation Plan；本文件随切片更新。

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：登记为 RND-F04 |
| 2026-06-11 | 顺延为 RND-F05；依赖 F03 + F04 |
| 2026-08-04 | F03 关账；明确多刀竖切 + 最终 GL/VK 双后端同上层；DebugDrawing 后置 |
