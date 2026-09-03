# ASSET-F01 — External Import Pipeline — Placeholder

## Meta
- **ID:** `ASSET-F01`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · [ANIM-F01](../Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md)
- **Branch:** TBD（合适窗口再开；**不阻塞** `ANIM-F01` 目视）
- **Depends on:** 现有 `AssetTypeRegistry` / `AssetManager`；产出侧对齐已有 `StaticMesh` / 将来原生 `SkeletalMesh` 等

## TL;DR
把 **FBX / glTF 等互换格式** 定位为 **Import Source（外部源）**，而不是引擎 `AssetType`。  
显式 Import 动作 → 产出引擎可直接加载的原生资产（`StaticMesh` / `SkeletalMesh`(+`Skeleton`) / Material 等，或它们的集合）。  
Assimp（或等价）**只出现在 Import/Cook**，不把 `.fbx` 路径登记成 `StaticMesh`/`SkeletalMesh` 本体。

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | 正式 Design 正文、Implementation、Editor 导入向导 UI |
| In（预期） | Source vs Asset 分层；Registry **不再**用 `.fbx` 自动 Infer→`StaticMesh`；Import 产出原生资产 + meta；多产物（mesh/skel/mat）分期 |
| Out（预期） | 完整 DCC 往返、增量 reimport 策略定稿可后置、运行时直接读 FBX 当权威资产 |
| ANIM 过渡 | `ANIM-F01` 目视用 **外部脚本** 拆最小验证资源；不依赖本 Feature |
| Unblock | 维护者排期；建议在 ANIM 竖切稳定后或与 ED 工作流窗口合并 |

## 动机（已共识）

- `.fbx` 可含网格 / 骨骼 / 动画 / 材质等，**不能**等同于单一 `StaticMesh` 或 `SkeletalMesh`。
- 当前扫描：扩展名 `.fbx` → 默认 `StaticMesh`，与上述定位冲突（临时捷径债务）。
- 引擎「可直接接受」的资产形态：简单几何（如 `obj`）、引擎自有 `.me*`；互换格式走管线。

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 占位；与 ANIM 轨约定：先外部脚本验证，本 Feature 后排 |
