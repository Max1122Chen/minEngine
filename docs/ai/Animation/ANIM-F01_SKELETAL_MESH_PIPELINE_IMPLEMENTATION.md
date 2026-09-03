# ANIM-F01 — Skeletal Mesh Pipeline — Implementation Plan

## Meta
- **ID:** `ANIM-F01`
- **Type:** Implementation Plan
- **Status:** Review
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [Design Spec](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Branch:** `feat/animation`

## TL;DR
按 Design §2.7 落地平行 Skeletal 栈：S00 数学核 → S00b Static Loader 改名 → S01 导入 → S02 GPU/材质变体 → S03 Component/Proxy → S04 Asset 接线与可视验收。  
**当前：** S01–S04 **Done**；竖切目视（stick）**通过**；Shadow skinned **Deferred**；Feature Status → **Review**。下一焦点 **ASSET-F01**。

## Scope
- **In:** Design Scope（Skeleton / SkeletalMesh / Pose→palette→GPU；Loader 命名；`MeshDeformationMode`）
- **Out:** Clip/Player（F02）、Graph（F03）、Event/IK/Root Motion/Retarget、cooked 二进制、per-section BoneMap

## Reader quick start
1. [Design](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md) — 契约与 §2.7 接口
2. 本文件切片表
3. `PROGRESS_LOG.md` — 已落地记录

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `ANIM-F01-S00` | `Pose` / `SkeletonBone` / `Skeleton`：local→global、palette、Bind Pose + 单测 | **Done** | `minEngineTests.exe test skeleton-pose` — 4 cases / 16 assert PASS |
| `ANIM-F01-S00b` | `MeshLoader`→Static 命名；抽出 `AssimpMeshImportUtil` | **Done** | 编译 OK；`skeleton-pose` PASS；无残留 `MeshLoader` 符号 |
| `ANIM-F01-S01` | `SkeletalMeshImportData` + `SkeletalMeshLoader`（骨/权重/几何） | **Done** | 编译 OK；Loader 日志路径就绪（目视资产待维护者） |
| `ANIM-F01-S02` | `SkeletalMesh` GPU layout + Material skinned VS + palette UBO | **Done** | skinned includes + set0 binding3；Shadow skinned Deferred |
| `ANIM-F01-S03` | `SkeletalMeshComponent` + Proxy + Forward 入队 | **Done** | Forward 路径接线；Shadow Deferred（`CastShadow=false`） |
| `ANIM-F01-S04` | AssetTypeRegistry + Load 接线 | **Done**（目视验收待勾） | `skeleton-pose` + smoke PASS；Editor 目视 pending |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

---

## 2) 切片详情

### ANIM-F01-S00 — Pose / Skeleton 数学核
- **Goal:** 无 Assimp、无 RHI 的 Pose 管道：FillBindPose、LocalToGlobal、BuildSkinningPalette；常量与不变量按 Design §2.7。
- **Touch:**
  - `Runtime/Function/Animation/Pose.h` / `Pose.cpp`
  - `Runtime/Function/Animation/Skeleton.h` / `Skeleton.cpp`
  - `Runtime/Function/Animation/AnimationConstants.h`
  - `Tests/Suites/SkeletonPoseTest.*` + `TestSuiteRegistration.cpp`
- **DoD:**
  - [x] `Pose` / `SkeletonBone` / `Skeleton` API 与 Design §2.7.2 对齐（可微调命名，语义不变）
  - [x] `SetBones`（或等价）供测试/Import 注入；校验 ParentIndex、骨数上限
  - [x] 单测覆盖：双骨链 global、identity inv-bind 下 palette==global、FillBindPose
  - [x] 无 Assimp / RHI 依赖进入 Animation 核
  - [x] `ME_CLASS` 延后至 S04（注释已标明） → **S04 已补 `ME_CLASS` + gen**
- **Verify:** `minEngineTests.exe test skeleton-pose` — **PASS**（4 cases, 16 assertions）

### ANIM-F01-S00b — Static Loader 命名澄清
- **Goal:** 消除泛化 `MeshLoader`；Static 导入 API 对称，为 Skeletal Loader 让路。
- **Touch:** `AssimpMeshImportUtil.*`；`StaticMeshLoader`（`StaticMeshImport*` + `ImportFromFile`）；删除 `MeshLoader.*`
- **DoD:**
  - [x] 无残留公共符号 `MeshLoader` / `MeshImportData`
  - [x] 现有 StaticMesh 导入路径行为不变（API 对称收束）
- **Verify:** `minEngineTests` 编译；`skeleton-pose` PASS；`asset-manager` 需在 `minEngine/bin` 下跑（缺 EngineConfig 时从 repo root 会失败，与本切片无关）
- **Note:** 可与 S01 同 PR，但逻辑上先改名再加骨骼导入更清晰。

### ANIM-F01-S01 — Skeletal 导入（CPU）
- **Goal:** Assimp → `SkeletalMeshImportData` + `Skeleton` 骨表；≤4 influences 归一化。
- **Touch:** `SkeletalMeshLoader.*`、`AssimpMeshImportUtil`、Import 校验日志
- **DoD:**
  - [x] 骨层级、InverseBind、权重写入；超限截断+warning；超 `kMaxBonesPerSkeleton` 失败
  - [x] 坐标系与 Static 导入一致（共享 Assimp flags / ConvertMatrix）
- **Verify:** 编译通过；真实 FBX 目视/日志待维护者资产

### ANIM-F01-S02 — GPU Skinning + 材质变体
- **Goal:** `SkeletalMesh` 上传 VB/IB/layout；Material 壳 `MeshDeformationMode::Skinned`；palette 绑定约定（UBO/SSBO 选型在本切片钉死）。
- **Touch:** `SkeletalMesh.*`、`GLSLMaterialShellAssemblerImpl`、VS template、RHI bind 路径
- **DoD:**
  - [x] Layout loc 0–5 按 Design
  - [x] PSO/编译 key 含 deformation 模式（独立 skinned shader + layout）
  - [x] Palette = set0 binding 3 UBO ring（256 mat4 / draw slot）
  - [ ] Shadow 与主 Pass 使用同一 skinned VS → **Deferred**（Component 默认不投阴影）
- **Verify:** `minEngineTests` 编译；skinned includes 接入 MaterialCompiler

### ANIM-F01-S03 — Component / Proxy / Queue
- **Goal:** 镜像 Static：`SkeletalMeshComponent` + `SkeletalMeshSceneProxy`；`BuildRenderQueue` 并列分支；`SetLocalPose` / `ResetToBindPose` / 单骨调试。
- **Touch:** Component、Proxy、`ForwardRenderer` / `SceneMeshDrawUtils`、Shadow 入队
- **DoD:**
  - [x] CreateSceneProxy 带 palette；`u_Model` 仍为整体变换
  - [x] Base Pass 绘制 skinned（Forward + ManualRenderer）
  - [ ] Shadow 绘制 skinned → **Deferred**
- **Verify:** 代码路径就绪；Editor 目视 pending

### ANIM-F01-S04 — Asset 接线与 Feature 验收
- **Goal:** `AssetTypeRegistry` 登记 `Skeleton` / `SkeletalMesh`；LoadFromAssetMeta；端到端验收与文档收口。
- **Touch:** `AssetTypeRegistry`、Import UX（能导入即可）、docs Status
- **DoD:**
  - [ ] Design §5 验收项勾选（目视）
  - [x] Registry 类型 + Load 接线；Progress 条目
  - [x] Static 回归：smoke / asset-manager PASS
- **Verify:** `test skeleton-pose` + `test smoke` PASS；Editor 目视清单 pending

---

## 3) 依赖顺序

```text
S00 → S00b → S01 → S02 → S03 → S04
         ↘---- 可与 S01 同 PR（若改名 diff 可控）
```

**硬依赖：** S02/S03 依赖 S00 的 palette API；S01 依赖 S00 的 `Skeleton` 数据模型；S04 依赖 S01–S03。

---

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| （无） cooked mesh | Design Out / TD | 竖切完成后再开 TD | F01 Done 后 |
| MeshComponent 薄基类 | 避免空抽象 | Static/Skeletal Component 出现真实重复 | F02/F03 后评估 |
| Vulkan 蒙皮 | 若 GL 先通 | GL 验收后 | S02/S03 注明是否双后端 |

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 初稿；S00 开工 |
| 2026-09-03 | S00 Done（`skeleton-pose` PASS）；S00b 开工 |
| 2026-09-03 | S00b Done：`StaticMeshImport*` + `AssimpMeshImportUtil`；删除 `MeshLoader` |
