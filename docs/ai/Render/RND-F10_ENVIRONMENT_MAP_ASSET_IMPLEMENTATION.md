# EnvironmentMap Asset — Implementation Plan (Draft)

## Meta
- **ID:** `RND-F10`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-03
- **Related:** [Design Spec](./RND-F10_ENVIRONMENT_MAP_ASSET_DESIGN.md)

## TL;DR

先接线、后 Bake。EnvMap **仅项目 Content**；EngineDefault 只作复制种子。Bake 是唯一必须深碰现代 RHI 的硬核切片。

## Scope
- **In:** 与 Design 一致
- **Out:** 每帧 RDG Bake；Atmosphere

## 1) 切片总览（建议）

| Slice | 内容 | 难度 | 状态 | 依赖 |
|-------|------|------|------|------|
| S01 | `EnvironmentMap` Asset + 注册/序列化；项目种子 IBL | 中低 | **Done** | — |
| S02 | `SkyBoxComponent` ref → Proxy → SkyPass 用 Asset 环境图 | 中低 | **Done** | S01 |
| S03 | Set1 IBL 从场景 Sky/Asset 解析；共享 BRDF LUT | 中 | **Done** | S02 |
| S04 | 删除/停用全局 `EngineIBLEnvironment` 运行时入口；文档 | 中低 | **Done** | S03 |
| S05 | **现代 RHI Baker**（重写 EnvMapCapture）；付清 TD-015 | **高** | **Done** | S01 |
| S06 | Editor/CLI 触发 Bake + 可选写回磁盘 | 中 | **Deferred → TD-021** | S05 |

S01–S05 竖切完成：项目 EnvMap + HDR bake + 现代 RHI Capture；死代码已删。S06 挂 TD-021。

## 2) 验证习惯

| 阶段 | 命令 |
|------|------|
| 回归 | `.\scripts\verify.ps1` |
| 图 | `minEngineTests.exe test render-graph` |
| 目视 | 默认 EnvironmentMap：天空 + PBR 金属边缘环境色 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-03 | Draft 切片表 |
| 2026-08-03 | S05 Done：运行时 HDR bake；TD-015 glad mip 残留 |
| 2026-08-03 | TD-015 Done（GenerateMips）；S04 删死代码；S06 → TD-021 |
