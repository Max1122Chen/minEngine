# RND-F04 — Vulkan backend & modern RHI completion

## Meta

| Field | Value |
|-------|--------|
| **Feature ID** | `RND-F04` |
| **Status** | Planned |
| **Last updated** | 2026-06-01 |
| **Depends on** | `RND-F03` **Done**（含 M3 后端内绞杀，见 F03 §12.2） |
| **Related** | [RND-F02](./RND-F02_MODERN_RHI_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) |

## 1) Goal

Introduce **Vulkan** as a second backend implementing the same `RHICreate*` / `RHICmd*` contract, and **complete** modern RHI capabilities that F02 normative design describes but F02/F03 left simplified on OpenGL (transitions, stricter resource views, descriptor pressure, sync, etc.).

## 2) Non-goals (draft)

- RenderGraph
- Feature parity with every UE RHI extension in one milestone

## 3) Done definition (draft)

TBD — milestone table (Present → Shadow → Base → Material) with GL/VK parity checks.

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：登记 F04；VK + 现代 RHI 补全，依赖 F03 |
