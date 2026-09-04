# ASSET-F01 — External Import Pipeline — Design Spec

## Meta
- **ID:** `ASSET-F01`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-03（§3.8 资产模型与引用；待审批）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ANIM-F01](../Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md)（消费方：SkeletalMesh / Skeleton）
  - [Implementation Plan](./ASSET-F01_IMPORT_PIPELINE_IMPLEMENTATION.md)
- **Branch:** `feat/animation`（可继续本轨；或另开 `feat/asset-import`）
- **Depends on:** `AssetManager` / `AssetTypeRegistry` / `AssetWorkflowModule`；产出对齐 `StaticMesh` / `SkeletalMesh` / `Skeleton`

## TL;DR
**问题：** `.fbx`/`.gltf` 被当成引擎 `AssetType`（扫描默认 StaticMesh），Assimp 在 **Load** 时反复解析；互换格式与原生资产混为一谈。  
**方案：** Source（DCC 文件）≠ Asset（引擎可加载实体）。显式 **Import** → Assimp 只在 Import/Cook → 登记 **原生类型** 资产（mesh / skeleton / 后续 material、clip）。  
**当前：** In Progress — S00–S02 Done；下一 S03 Static 对齐 / 回归。

## Scope
- **In（MVP）：**
  - Source vs Asset 术语与目录约定
  - Registry：**不再**用 `.fbx`/`.gltf` 自动 Infer 为 `StaticMesh`/`SkeletalMesh`
  - `ImportAsset` 从「复制+猜类型」升级为「可选产物类型 + Cook/写出 + Register」
  - Editor：导入时选择产物（至少 StaticMesh / SkeletalMesh）
  - FBX/glTF → **SkeletalMesh**（+ 可独立引用的 **Skeleton**）与 → **StaticMesh**
  - Meta 记录 **`SourcePath`**（Import 源回溯；见 §3.8.3）— **不含** Skeleton 等业务引用
- **Out（MVP）：**
  - 完整多产物一次导入（Material/AnimationClip 集合）— **分期**
  - 真·二进制 cooked mesh（`.memesh`）定稿 — 可第二期；MVP 可用「引擎拥有的中间几何文件」过渡
  - 增量 reimport / 依赖图 / 源文件监听自动重导
  - DCC 往返（导出回 FBX）
  - Clip 导入（属 `ANIM-F02` 消费；本 Feature 可预留扩展点）

## Reader quick start
1. 本文件：分层与导入契约
2. 现状代码：`AssetManager::{ImportAsset,ScanAssets,RegisterAsset}` · `AssetTypeRegistry` · `AssetWorkflowModule::ImportAssetDialog` · `*MeshLoader::LoadFromAssetMeta`
3. [Implementation Plan](./ASSET-F01_IMPORT_PIPELINE_IMPLEMENTATION.md)：切片表

---

## 1) 背景与目标

### Pain
1. `.fbx` 语义过载：可含静态网格、蒙皮、骨骼、动画、材质。
2. `FindByExtension` 先匹配 StaticMesh → FBX 人型被登记成 StaticMesh。
3. `ImportAsset` = 复制进 `Assets/` + 写 `.meta`；**无转换**。
4. `LoadFromAssetMeta` 对 mesh **再次 Assimp** — 运行时身份绑在 DCC 文件上。

### 成功标准（MVP）
- 用户选 FBX → 选 **SkeletalMesh** → 得到可挂 `SkeletalMeshComponent` 的资产；**Registry 里 AssetType 不是「FBX」**。
- 项目扫描 **不会** 再把裸 `.fbx` 登记成 Static/Skeletal mesh。
- Assimp **不**出现在「每帧 / 普通 Load 路径的身份定义」中（Load 只读引擎拥有的几何资产文件；若 MVP 过渡期几何仍是引擎写出的 `.glb`，Assimp 仅用于读该引擎产物亦可接受，但 **源 FBX 不再是 AssetPath**）。

---

## 2) 现状

| 能力 | 现状 |
|------|------|
| `ImportAsset` | 复制到 Content + `InferAssetTypeFromExtension` + `RegisterAsset` |
| 扫描 | 同 Infer；未知扩展跳过 |
| StaticMesh 扩展 | `.obj` `.fbx` `.gltf` |
| SkeletalMesh 扩展 | `.fbx` `.gltf` `.glb`（共享扩展时 Static **优先**） |
| Skeleton | Registry 有 `.meskeleton`；**无** `LoadAsset_Impl` / Save |
| Editor | Content Browser / 菜单「Import Asset...」多选；**无** Static/Skeletal 选择 |
| 原生可写资产 | `.memtl` `.mescene` `.meenv`；**无**原生 mesh 容器 |
| ANIM-F01 | Loader/组件/蒙皮绘制已通；验证用脚本生成的 `.glb` |

---

## 3) 方案

### 3.1 核心分层

```text
[Source]  Assets/Sources/.../Hero.fbx     （可选保留；不进 AssetType 网格表）
    │  Import (显式，用户选产物)
    ▼
[Cook]    Assimp 仅此处
    │
    ├─► Skeleton     → *.meskeleton (+ .meta)     AssetType=Skeleton
    ├─► SkeletalMesh → 引擎拥有几何文件 (+ .meta) AssetType=SkeletalMesh
    └─► StaticMesh   → 引擎拥有几何文件 (+ .meta) AssetType=StaticMesh
```

**原则：**
- **AssetPath** 永远指向引擎资产文件（或引擎写出的几何产物），不指向用户丢进来的原始 FBX（除非该文件被明确登记为 Source 记录类型 — MVP 可不做 Source AssetType）。
- **SourcePath**（meta 新字段或 ImportSettings）：记录原始 FBX 的项目相对路径，供 reimport。

### 3.2 目录约定（建议）

| 路径 | 用途 |
|------|------|
| `Assets/Sources/`（或 `Import/`） | 放置/复制进来的 FBX/glTF；**扫描忽略网格 Infer** |
| `Assets/Meshes/` / `Assets/Skeletons/` | Import 产出的可引用资产 |
| 现有 `Assets/**` | `.memtl` / `.mescene` 等不变 |

MVP 可先：Import 时把源复制到 `Sources/`，产物写到 `Meshes/`；若用户把 FBX 直接丢进 `Assets/`，扫描忽略 `.fbx`（只警告或列为 unmanaged）。

### 3.3 Registry 调整

1. 从 `StaticMesh` / `SkeletalMesh` 的 `Extensions` 中 **移除** `.fbx` / `.gltf`（保留 `.obj` 给 Static；Skeletal 过渡期可保留 `.glb` 作为**引擎写出的**几何扩展，或同样改为仅 Import 产物扩展）。
2. `ScanAssets` / `ProjectAssetWatcher`：对移除后的扩展不再自动 Register 为 mesh。
3. 导入对话框：`BuildFileDialogFilters` 增加「Import Sources (*.fbx;*.gltf;*.glb)」；产物类型由 UI/参数决定，不靠扩展名猜 Static vs Skeletal。

### 3.4 Import API 契约（目标形态）

```text
ImportRequest
  SourceAbsoluteOrProjectPath
  DestDirectoryRel          // e.g. Meshes/
  Product: StaticMesh | SkeletalMesh | (later: SkeletonOnly, …)
  Options: (scale, axis, merge meshes, …)  // MVP 可极少

ImportResult
  CreatedAssets[]           // path + type + guid
  Warnings[] / Errors
```

实现落点（建议）：
- `AssetManager::ImportExternalMesh(...)`（或泛化 `ImportFromSource`）
- Editor：`AssetWorkflowModule` 在文件对话框后弹出产物选择（最少两个 radio）再调用上述 API
- Cook：复用 `StaticMeshLoader::ImportFromFile` / `SkeletalMeshLoader::ImportFromFile`，但写出**新文件**并 `RegisterAsset(path, explicitType)`，而不是 Register 源 FBX

### 3.5 MVP 几何产物格式（务实选择）

| 选项 | 说明 | MVP 建议 |
|------|------|----------|
| A. 真·`.memesh` 二进制 | 正确长期形态；工作量大 | **二期** |
| B. Import 写出引擎拥有的 `.glb`/`.obj` | 现有 Loader 可 Load；Assimp 仍读产物文件 | **选用（过渡）** |
| C. 仅内存 Cook、无落盘 | 无法做稳定 GUID/引用 | 否 |

**选用 B：**  
- Skeletal Import → 写出 `*.glb`（或引擎约定扩展）+ `SkeletalMesh` meta；可选同时写 `*_Skeleton.meskeleton`。  
- Static Import → 写出 `*.obj` 或 `*.glb` + `StaticMesh` meta。  
- **原始 FBX** 留在 `Sources/`，不登记为 Static/Skeletal。  
- 文档明确：B 是过渡；二期用 `.memesh` 替换写出与 Load，Assimp 彻底退出 Load。

### 3.6 Skeleton 资产（概要）

- Import Skeletal 时：**始终**在 **Import/Cook** 阶段产出可引用的 `Skeleton`（写 `.meskeleton` + `.meta`）。
- 补齐 `LoadAsset_Impl<Skeleton>` / Save（JSON，对齐 `.memtl` 风格）。
- **Load 阶段** `SkeletalMesh` 通过 **cooked 资产内的引用**（§3.8.4）解析 Skeleton；停止「每次 Load mesh 时 `GenerateGUID` 临时 Skeleton」。
- **不在**通用 `AssetMeta` 上增加 `SkeletonPath` 等类型特化字段（见 §3.8.4）。

### 3.7 与 ANIM / 现有 Loader 关系

- **Import 阶段**调用现有 Assimp 解析逻辑（可抽 `*Loader::ImportFromFile` 为共享 Cook）。
- **Load 阶段**读引擎产物；SkeletalMeshLoader 对「已是引擎写出的 glb」可继续 Assimp，直到 `.memesh`。
- `scripts/animation/generate_min_skinned_glb.py` 仍可作为无 FBX 的回归输入。

### 3.8 资产模型：StaticMesh vs SkeletalMesh（待审批）

#### 3.8.1 问题陈述

| 维度 | StaticMesh | SkeletalMesh |
|------|------------|--------------|
| 磁盘产物 | 单一几何文件（MVP：`.obj`） | **几何 + Skeleton** 两份引擎资产 |
| 外部依赖 | 无 | 必须 Resolve 已登记的 `.meskeleton` |
| Import/Cook | 写出几何 + Register | 写出几何 + **写出 Skeleton** + Register **两者** |
| Load | 读几何 → GPU（Assimp 读产物） | 读几何 + **按引用 Load Skeleton** → GPU |
| 序列化方向（MVP） | 仅 **Load**（Import 已写出 `.obj`） | **Cook 写 + Load 读**（Skeleton 必须可 Save/Load） |

**现状债务：** `SkeletalMeshLoader::LoadFromAssetMeta` 每次从 `.glb` 再 Assimp 抽骨并在内存里临时 `GenerateGUID()` Skeleton——能画，但无持久引用、无 Content Browser 上的 Skeleton 资产、重启后 GUID 不稳定。

#### 3.8.2 Import/Cook vs Load（边界钉死）

| 阶段 | 职责 | 禁止 |
|------|------|------|
| **Import/Cook** | 解析 DCC Source（Assimp）；写出几何 + Skeleton + `.meskmesh`；Register | 不应依赖「用户第一次 Open 才写盘」 |
| **Load** | 只读已登记产物；Resolve Skeleton 引用；建 GPU 资源 | 不应静默再 Cook、不应临时造 Skeleton GUID |

Cook **只在 Import**（或显式 Reimport，S04）；Load 找不到 cooked 产物 / Skeleton 引用时 **报错或明确 fallback**，不自动补写。

#### 3.8.3 Path 字段分工（几何 vs Import 源）

| 字段 | 所在层 | 语义 | 示例 |
|------|--------|------|------|
| **`AssetPath`** | `AssetMeta`（通用） | 本资产的**主文件**；Skeletal MVP = cooked **几何** `.glb` | `Meshes/Hero.glb` |
| **`SourcePath`** | `AssetMeta`（通用） | Import **DCC 源**（reimport 回溯） | `Sources/Hero.fbx` |

- **几何路径**由 `AssetMeta.AssetPath` 承担 → **不必**在 `SkeletalMesh` 上再维护一份到 `.glb` 的路径（与维护者共识）。
- **Skeleton 引用**不走 path 字符串 → 见 §3.8.4（`ObjectPtr` / GUID ref）。

**Reject：** `AssetMeta::SkeletonPath`、`.mesk` 里手写 `SkeletonAssetPath` 字符串——前者污染通用 meta，后者绕开已有序列化设施。

#### 3.8.4 Skeleton 引用：`SkeletalMesh` 上的 `ObjectPtr`（MVP 选用）

**原则：** 与 `SkeletalMeshComponent::m_Mesh` / Scene 里组件引用资产相同——用反射 **`ME_PROPERTY std::shared_ptr<Skeleton> m_Skeleton`**，序列化为 **GUID ref**；反序列化走 `Serializer::ResolvePendingAssetRef` → `AssetManager::LoadAssetByGUID`（已加载则命中 cache）。

**磁盘布局（MVP）：**

```text
Hero.glb                    ← AssetMeta.AssetPath（几何；Assimp → GPU 过渡）
Hero.glb.meta               ← SourcePath 等通用字段；无 Skeleton 专用字段
Hero.meskmesh               ← 引擎原生描述（Serialize SkeletalMesh；含 m_Skeleton ObjectPtr ref）
Hero_Skeleton.meskeleton    ← 独立 Skeleton 资产（+ .meta）
```

`.meskmesh` 是 **buddy 描述文件**（同目录、`{glbStem}.meskmesh`），**不**单独 Register 为第二个 AssetType；由 `SkeletalMeshLoader` 在 Load 时与 `.glb` 成对读取。

**Import/Cook 写出：**

1. Assimp 解析（一次）  
2. 写 + Register `Hero_Skeleton.meskeleton`  
3. 写 + Register `Hero.glb`（`meta.AssetPath` = 此路径，`meta.SourcePath` = Sources/…）  
4. 内存中 `SkeletalMesh::SetSkeleton(loadedSkeleton)` → **`SaveAsset` / Serializer** 写出 `Hero.meskmesh`（`allowObjectPtrSerialization = true`）

**Load 顺序：**

```text
LoadAsset<SkeletalMesh>(meta.AssetPath)   // meta → Hero.glb
  → 若存在 Hero.meskmesh：
       Deserialize(SkeletalMesh 描述字段)
       → PendingObjectRef(m_Skeleton)
       → ResolvePendingAssetRef（Skeleton 已在内存则 cache 命中，否则 LoadAsset<Skeleton>）
  → Assimp 读 Hero.glb → 建 GPU 缓冲
  → SetSkeleton + 返回
```

与 Scene / Material 一致：**引用在反序列化阶段自然恢复**，Loader 不负责拼路径字符串。

**`SkeletalMesh` 类型侧（实现钉死）：**

- `ME_PROPERTY std::shared_ptr<Skeleton> m_Skeleton`（参与 `.meskmesh` 序列化）
- **不**增加 `GeometryPath`（几何由 `meta.AssetPath` 指向的 `.glb` 提供）
- GPU 缓冲（`m_VertexBuffer` 等）**不**写入 `.meskmesh`（Load 时从 `.glb` 重建，同 Static 过渡策略）

**备选（未选用）：**

| 方案 | 说明 |
|------|------|
| `AssetMeta::SkeletonPath` | 污染通用 meta → **Reject** |
| `.mesk` + 路径字符串 | 重复发明 ObjectPtr → **Reject** |
| 纯伴生 `{stem}_Skeleton` 无序列化 | 无法表达 GUID ref / 共享骨架 → 仅作 legacy fallback |
| 二期 `.memesh` 单文件 | 几何 + ObjectPtr 合一；`.glb` + `.meskmesh` 废弃 |

#### 3.8.5 Import Skeletal 产出清单（Cook 一次完成）

```text
ImportExternalMesh(..., SkeletalMesh):
  1. 复制 Source → Assets/Sources/<name>.fbx
  2. Assimp 解析（一次）
  3. 写 + Register Skeleton → <dest>/<name>_Skeleton.meskeleton
  4. 写 + Register 几何 → <dest>/<name>.glb  （meta.AssetPath；meta.SourcePath）
  5. Serialize SkeletalMesh（m_Skeleton ObjectPtr）→ <dest>/<name>.meskmesh
```

Load  thereafter 读 `.meskmesh` + `.glb` + 已登记的 `.meskeleton`；**不再**从 glb 内临时 `GenerateGUID()` Skeleton。

**Legacy：** 无 `.meskmesh` 的旧 `.glb`（如 `MinSkinnedStick`）可 WARN + 从 glb 抽骨 fallback；新 Import 必须写全 triplet。

#### 3.8.6 与 StaticMesh 对称性

| | StaticMesh | SkeletalMesh |
|---|------------|--------------|
| `meta.AssetPath` | `.obj`（几何） | `.glb`（几何） |
| 额外磁盘 | 无 | `.meskmesh`（ObjectPtr 描述）+ `.meskeleton`（骨表资产） |
| Load | Assimp → GPU | Deserialize refs → Assimp → GPU |
| Save（MVP） | 不需要 | **需要**（`.meskeleton` + `.meskmesh` 在 Cook 写出） |

Static 保持薄；Skeletal 多的是 **独立 Skeleton 资产 + 原生描述序列化**，不是给 `AssetMeta` 打补丁。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 继续 FBX=Asset + 导入时弹类型 | 改动小 | Source/Asset 仍混淆；Load 绑 FBX | 否 |
| B. Source/Asset 分层 + Import 写出引擎几何（glb/obj） | 分层正确；复用 Loader；可验人型 FBX | Load 仍可能 Assimp 产物文件 | **MVP 选用** |
| C. 一步到位 `.memesh` | 最干净 | 阻塞导入可用性 | 二期 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 已登记的 `.fbx` meta（如 Mannequin→StaticMesh） | 扫描/加载行为分裂 | 迁移说明：删除错误 meta 或提供一次性「转为 Source + 重新 Import」；不自动猜 |
| 过渡期仍 Assimp 读 glb | 「Assimp 已退出 Load」话术不严谨 | 文档写清：退出的是 **源 FBX 身份**；真 cooked 二期 |
| 导入 UI 复杂 | 拖期 | MVP 仅两产物 + 默认目录 |
| 大 FBX / 多 mesh | 时间与产物爆炸 | MVP：合并或只导首个 skinned mesh；warning 列出跳过项 |
| Skeleton 双份 | mesh 内嵌 vs 独立资产 | 强制独立 Skeleton 资产 + sidecar 引用；Load 不临时 GUID |
| sidecar 与 glb 不同步 | 改 mesh 未重导 | `.meskmesh` 与 `.glb` 同 stem 成对；S04 Reimport |

---

## 6) 验收标准

- [ ] `.fbx`/`.gltf` **不会**被 `ScanAssets` 登记为 `StaticMesh`/`SkeletalMesh`
- [ ] Editor Import：可选 StaticMesh / SkeletalMesh；FBX 人型可导为 SkeletalMesh 并挂组件显示（OpenGL）
- [ ] 产物 `AssetPath` 不是原始 FBX；meta 含 Source 回溯信息（字段名在实现计划钉死）
- [ ] Skeletal 导入产出可加载的 `Skeleton` 资产；mesh 经 **`.meskmesh` + ObjectPtr** 引用（`ResolvePendingAssetRef`）
- [ ] 现有 `.obj` StaticMesh 与 `MinSkinnedStick.glb` 回归不坏
- [ ] Assimp 不进入动画/帧循环模块（保持 ANIM-F01 约束）
- [ ] Design / Registry / ACTIVE_WORK / Progress 对齐；Impl Plan 切片可执行

---

## 7) 建议切片（预览；以 Impl Plan 为准）

| Slice | 目标 |
|-------|------|
| S00 | 术语 + Registry 去 FBX Infer + Sources 忽略规则 + 文档/迁移说明 |
| S01 | Import API：Source→写出引擎几何 + Register(显式类型)；Editor 产物选择 |
| S02 | Skeleton Save/Load；Import 成对 Cook；`.meskmesh` Serialize `m_Skeleton` ObjectPtr |
| S03 | Static FBX/OBJ Import 路径对齐；回归 |
| S04 |（可选）Reimport 最小闭环 / meta SourcePath UI |
| 二期 | `.memesh` cooked + Load 去 Assimp |

---

## Pre-flight（轻量）

| 项 | 结论 |
|----|------|
| 依赖 | ANIM-F01 Review（Skeletal 消费侧可用） |
| 债务 | 现有 FBX meta；无原生 mesh 格式 → MVP 用写出 glb/obj |
| WIP | 本轨可继续；不与 F02 Clip 抢 |
| Go/Defer | **Go Draft**；真 `.memesh` Defer |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 占位 |
| 2026-09-03 | 升 **Draft**：Source/Asset 分层、Registry/API/MVP 产物格式 B、切片预览；下一焦点开工文档 |
| 2026-09-03 | Status→**In Progress**；Impl Plan 就绪；S00/S01 编码 |
| 2026-09-03 | **§3.8** Static/Skeletal 资产模型、Import/Load 边界、双 Path、`SkeletonAssetPath` sidecar（**待审批**）；Reject `AssetMeta::SkeletonPath` |
| 2026-09-03 | **§3.8 修订**：几何路径 = `meta.AssetPath`（`.glb`）；Skeleton = `SkeletalMesh::m_Skeleton` **ObjectPtr** + `.meskmesh` buddy；Reject `.mesk` 路径字符串 |
| 2026-09-04 | §3.8 **审批通过**；S02 落地：Matrix3/4 primitive；Skeleton 直序列化；Import cook triplet |
