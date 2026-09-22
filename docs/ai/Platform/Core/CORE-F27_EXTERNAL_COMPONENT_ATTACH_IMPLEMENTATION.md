# CORE-F27 — Implementation Plan

## Meta
- **ID:** `CORE-F27`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:** [Design Spec](./CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md) · [System Review](../Reflection/REFLECTION_MULTI_MODULE_CODEGEN_SYSTEM_REVIEW.md)

## TL;DR

S01–S04 **Done**。多模块 reflection codegen 落地；S05 ScriptBinding / S06 逃生口仍 Deferred。

## Scope
- **In:** S01–S04
- **Out:** 重写 parser；LoadLibrary 增量 Finalize；改 minCraft；S05/S06

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `CORE-F27-S01` | Design / Review 纠偏定稿 | **Done** | 维护者确认后实现 |
| `CORE-F27-S02` | `minengine_add_reflection_codegen`；引擎改用 | **Done** | 默认 smoke |
| `CORE-F27-S03` | GameModuleStub：`ME_CLASS` Prim + codegen + `AddComponent<T>` | **Done** | `test game-module-stub` |
| `CORE-F27-S04` | BOOTSTRAP：多模块步骤 + 世界搭建归游戏仓 | **Done** | 文档 |
| `CORE-F27-S05` | ScriptBinding 分模块 | Deferred | — |
| `CORE-F27-S06` | `AddComponentInstance` 逃生口 | Deferred | — |

## 2) 落地摘要

- `minEngine/cmake/MinEngineReflection.cmake`
- 引擎 `CMakeLists.txt`：`Generated/` 不再进主 GLOB；由封装 `target_sources`
- Stub：`PRIVATE minEngine` + 自有 `Generated/Reflection` + `StubGamePrimitiveComponent`
- Suite：`game-module-stub`（开 `MINENGINE_GAME_*` 时链 stub）

## 3) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft → 实现 Done |
