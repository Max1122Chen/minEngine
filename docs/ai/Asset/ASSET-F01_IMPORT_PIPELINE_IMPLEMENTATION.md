# ASSET-F01 — External Import Pipeline — Implementation Plan

## Meta
- **ID:** `ASSET-F01`
- **Type:** Implementation Plan
- **Status:** Done（MVP）
- **Owner:** project maintainer
- **Last updated:** 2026-09-04（MVP 收口；续作 Deferred）
- **Related:** [Design Spec](./ASSET-F01_IMPORT_PIPELINE_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Branch:** `feat/animation`

## TL;DR
按 Design：Source≠Asset；Registry 去 FBX Infer → Import 写出引擎几何 → Skeleton 独立资产 → Static 对齐。  
**当前：** S00–S03 **Done**；S04 / `.memesh` **Deferred**；Feature → **Done（MVP）**，暂停不挡 Anim。

## Scope
- **In:** Design MVP（Registry、Import API、Editor 产物选择、Skeleton 序列化、Static/Skeletal 导入）
- **Out:** `.memesh` 二期；Clip 导入；自动 reimport；完整多产物 Material 集

## Reader quick start
1. [Design](./ASSET-F01_IMPORT_PIPELINE_DESIGN.md)
2. 本文件切片表
3. `PROGRESS_LOG.md`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `ASSET-F01-S00` | Registry 去 `.fbx`/`.gltf` Infer；Sources 约定；扫描忽略；迁移说明 | Done | 扫描不再登记 FBX；`.obj`/`.glb` 仍可用 |
| `ASSET-F01-S01` | `ImportExternalMesh`：Source→写出引擎几何 + 显式类型 Register；Editor 选 Static/Skeletal | Done | FBX→Skeletal 产物路径非 FBX |
| `ASSET-F01-S02` | Skeleton Save/Load；`.meskmesh` ObjectPtr；Import 成对 Cook | Done | Load 走 ResolvePendingAssetRef；不临时 GUID |
| `ASSET-F01-S03` | Static FBX/OBJ Import 对齐 + 回归 | Done | `.obj` + MinSkinnedStick + smoke；手动 Static Import OK |
| `ASSET-F01-S04` | Meta `SourcePath` +（可选）最小 Reimport | Deferred | — |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

**Meta：** `AssetPath` = 几何 `.glb`；`SourcePath` = Import 源。**无** Skeleton 专用字段。

**Skeletal buddy（S02）：** `{glbStem}.meskmesh` — Serializer 写出 `SkeletalMesh::m_Skeleton`（`shared_ptr` ObjectPtr / GUID ref）。Loader：`Deserialize` → `ResolvePendingAssetRef` → Assimp 读 `.glb`。

**MVP 产物：** Skeletal → `.glb` + `.meskmesh` + `.meskeleton`；Static → `.obj`；源 → `Assets/Sources/`。

---

## 2) 切片详情

### ASSET-F01-S00 — Registry / 扫描政策
- **Goal:** `.fbx`/`.gltf` 不再自动成为 Static/Skeletal AssetType；约定 `Assets/Sources/`。
- **Touch:** `AssetTypeRegistry.cpp`；`AssetManager::ScanAssets`（可选跳过 `Sources/`）；docs
- **DoD:**
  - [ ] StaticMesh 扩展仅 `.obj`（或另列引擎产物扩展）
  - [ ] SkeletalMesh 扩展仅引擎产物（`.glb` MVP）
  - [ ] 增加 Import-Source 对话框用过滤器（`.fbx;.gltf;.glb`）API，不绑定 AssetType
  - [ ] Design/ACTIVE_WORK/Progress 记迁移：旧 FBX meta 需删或重导
- **Verify:** 启动扫描日志中无新登记 `Animations/*.fbx` 为 StaticMesh；`MinSkinnedStick.glb` 仍为 SkeletalMesh

### ASSET-F01-S01 — Import API + Editor 选择
- **Goal:** 显式产物类型；Assimp 仅 Import；写出引擎几何 + Register。
- **Touch:** `AssetManager`；`AssetMeta.SourcePath`；`*MeshLoader` cook 写出；`AssetWorkflowModule::ImportAssetDialog`
- **DoD:**
  - [ ] `ImportExternalMesh(source, destDir, productType)`（或等价）
  - [ ] 源复制到 `Sources/`；产物在 `Meshes/`（或 dest）
  - [ ] Editor：导入前选 StaticMesh / SkeletalMesh
  - [ ] 旧 `ImportAsset` 复制路径：对 mesh 互换扩展拒绝或转发到新 API
- **Verify:** 导入 UEFN/测试 FBX 为 Skeletal → Content Browser 类型正确；AssetPath 为 `.glb`

### ASSET-F01-S02 — Skeleton + ObjectPtr 引用
- **Goal:** Cook 成对产出；Load 经 `.meskmesh` Deserialize + `ResolvePendingAssetRef`；不临时 GUID。
- **Touch:** `SkeletonLoader`；`LoadAsset_Impl<Skeleton>`；`.meskmesh` buddy；`SkeletalMesh::m_Skeleton` `ME_PROPERTY`；`ImportExternalMesh`（Skeletal）；`Matrix3/4` primitive codec
- **DoD:**
  - [x] `.meskeleton`：直接 Serialize `Skeleton`（`SkeletonBone` + `m_Bones`；`Matrix4` primitive）
  - [x] Import：`.meskeleton` + `.glb` + `.meskmesh`（`m_Skeleton` ObjectPtr GUID）
  - [x] Load：`Deserialize .meskmesh` → Resolve Skeleton → Assimp `.glb`
  - [x] **无** `AssetMeta::SkeletonPath`；**无** wire `SkeletonFileData`
- **Verify:** `smoke` / `skeleton-pose` / `asset-manager` PASS；手动 Import FBX→Skeletal 待确认

**Legacy：** 无 `.meskmesh` 的旧 `.glb` → WARN + 从 glb 抽骨；新 Import 写全 triplet。

### ASSET-F01-S03 — Static 对齐与回归
- **Goal:** Static FBX Import 与 OBJ 并存；回归。
- **DoD:**
  - [x] Static FBX → cook `.obj` + Register；`SourcePath` 写入
  - [x] 现有 `.obj` / stick / smoke 回归
  - [x] 维护者手动验 Static + Skeletal Import 通过（2026-09-04）
- **Verify:** `minEngineTests` smoke；手动 OBJ + stick + FBX Static/Skeletal

### ASSET-F01-S04 — SourcePath UI / Reimport
- **Status:** Deferred  
- **Unblock:** S01–S03 Done 且维护者要闭环

---

## 3) 依赖顺序

```text
S00 → S01 → S02 → S03
              ↘ S04 (optional)
```

## 4) 延后 / 取消

| Slice | Reason | Unblock |
|-------|--------|---------|
| S04 | 非竖切必需 | S01–S03 + 排期 |
| `.memesh` | Design 二期 | MVP 稳定后 |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 初稿；与 Design Draft 对齐；开工 S00 |
| 2026-09-03 | S00/S01 Done；S02 对齐 Design §3.8（`.meskmesh` + ObjectPtr）；Blocked 待审批 |
| 2026-09-03 | §3.8 修订：Reject `.mesk`；Skeleton = ObjectPtr ref |
| 2026-09-04 | S02 Done：Matrix3/4 primitive；Skeleton 直序列化；`.meskmesh` buddy；Import cook 成对 |
| 2026-09-04 | S03 Done：Static Import 手动通过；MVP 竖切收口 |
| 2026-09-04 | Status → **Done（MVP）**；暂停续作；焦点 → ANIM-F02 |
