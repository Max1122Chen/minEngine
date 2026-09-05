# ASSET-F02 — Formal Import/Load 注册式管线 — Implementation Plan

## Meta
- **ID:** ASSET-F02
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Related:** [Design Spec](./ASSET-F02_IMPORT_SERVICE_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Branch:** feat/animation

## TL;DR
Register 式 Load/Import + 共用 ImportDialog。S00–S05 **Done**（自动化 + 手动验 PASS）。

## Scope
- **In:** Design In 项
- **Out:** Import Settings 框架；.memesh；Retarget

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| ASSET-F02-S00 | LoadHandler 注册表；删除 LoadAssetByMeta_Internal if 链 | Done | smoke / asset-manager / animation-clip |
| ASSET-F02-S01 | ImportProduct 注册表 + Import()；builtin 产物转调旧 API | Done | smoke / asset-manager / animation-clip |
| ASSET-F02-S02 | Pipeline 登记迁出 AssetPipelineBootstrap；Initialize 只调 RegisterAll | Done | smoke / asset-manager / animation-clip |
| ASSET-F02-S03 | EditorImportDialog 读注册表；Skeleton Picker；删 mesh Dialog | Done | build + 手动 |
| ASSET-F02-S04 | 旧 Import* 改为 private；Editor 只调 Import() | Done | smoke / asset-manager / animation-clip |
| ASSET-F02-S05 | SourcePath Inspector + Reimport | Done | build + 手动 |

## 2) 切片详情

### ASSET-F02-S00 — LoadHandler
- **Goal:** untyped Load 查表；新类型不改分发函数体
- **Touch:** AssetManager.h/.cpp
- **DoD:**
  - [x] RegisterLoadHandler / FindLoadHandler
  - [x] 登记于 Initialize（经 Bootstrap）
  - [x] if 链删除
- **Verify:** minEngineTests.exe test smoke / asset-manager / animation-clip

### ASSET-F02-S01 — ImportProduct + Import()
- **Goal:** 开放 ProductId；Import(ImportRequest) 查表
- **Touch:** AssetManager.h/.cpp
- **DoD:**
  - [x] RegisterImportProduct / GetImportProducts / Import
  - [x] NativeCopy / StaticMesh / SkeletalMesh / AnimationClip 登记
  - [x] AnimationClip：空 Skeleton 时 Runtime fallback {stem}_Skeleton
- **Verify:** build + smoke

### ASSET-F02-S02 — Pipeline 迁出
- **Goal:** 登记调用不堆在 AM Initialize 内联
- **DoD:**
  - [x] AssetPipelineBootstrap + RegisterAllAssetPipelines
  - [x] AM Initialize：RegisterBuiltinTypes + RegisterAllAssetPipelines
  - [x] 删除 AM 内死代码 RegisterBuiltinLoadHandlers / RegisterBuiltinImportProducts
- **Note:** 完整按 Loader 文件拆 Register*Pipeline 可后续；本刀达到「迁出 AM」目标

### ASSET-F02-S03 — Editor Dialog
- **Goal:** Dialog 枚举 GetImportProducts()；显式 Skeleton Picker
- **DoD:**
  - [x] EditorImportDialog（Product combo + Skeleton combo）
  - [x] 唯一 Accepts 且无需 Skeleton → 自动 Import
  - [x] 删除 EditorMeshImportProductDialog / MeshImportProductChoice
  - [x] Editor 无 stem 拼 Skeleton

### ASSET-F02-S04 — 删双轨
- **Goal:** Editor/测试只调 Import；旧 public Import* 删除或 private
- **DoD:**
  - [x] ImportAsset / ImportExternalMesh / ImportAnimationClip private
  - [x] Editor 经 AssetManager::Import
  - [x] Product handlers 传递 OverwriteExisting

### ASSET-F02-S05 — SourcePath + Reimport
- **Goal:** Inspector 展示 SourcePath；最小 Reimport
- **DoD:**
  - [x] Inspector Source + Reimport 按钮
  - [x] AssetManager::Reimport（AssetType→ProductId；Clip 从已载 Clip 取 Skeleton）

## 3) 依赖顺序

`	ext
S00 → S01 → S02 → S03 → S04
                     ↘ S05
`

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | 建 Impl；S00–S01 Done |
| 2026-09-05 | S02–S05 Done；自动化 + 手动验 PASS |
