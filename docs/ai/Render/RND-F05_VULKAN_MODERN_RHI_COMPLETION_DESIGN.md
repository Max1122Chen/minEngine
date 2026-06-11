# RND-F05 — Vulkan backend & modern RHI completion

## Meta

| Field | Value |
|-------|--------|
| **Feature ID** | `RND-F05` |
| **Status** | Planned |
| **Last updated** | 2026-06-11 |
| **Depends on** | `RND-F03` **Done**（含 M3 后端内绞杀，见 F03 §12.2）· `RND-F04` **Done**（现代 RHI 语义终态，见 [F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md)） |
| **Related** | [RND-F02](./RND-F02_MODERN_RHI_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · [RND-F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) |

> **Legacy mapping：** 2026-06-01 初稿登记为 `RND-F04`；2026-06-11 维护者将 Vulkan 顺延为 **F05**，F04 专指现代 RHI 进一步演进。

## 1) Goal

Introduce **Vulkan** as a second backend implementing the same `RHICreate*` / `RHICmd*` contract, and **complete** modern RHI capabilities that F02 describes but F02/F03/F04 left simplified on OpenGL (transitions, stricter resource views, descriptor pools, sync, true PSO compile, etc.).

## 2) Non-goals (draft)

- RenderGraph（`RND-F01`）
- Feature parity with every UE RHI extension in one milestone

## 3) Done definition (draft)

TBD — milestone table (Present → Shadow → Base → Material) with GL/VK parity checks.

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：登记为 RND-F04 |
| 2026-06-11 | 顺延为 RND-F05；依赖 F03 + F04 |
