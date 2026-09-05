# ASSET-F02 — 正式 Import / Load 注册式管线 + 可复用 ImportDialog — Design Spec

## Meta
- **ID:** `ASSET-F02`
- **Type:** Feature（含小型 true refactor：Load/Import 分发）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05（S00–S05 Done；自动化 + 手动验 PASS）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F01 Design](./ASSET-F01_IMPORT_PIPELINE_DESIGN.md)（MVP Done；S04 / `.memesh` Deferred）
  - [Implementation Plan](./ASSET-F02_IMPORT_SERVICE_IMPLEMENTATION.md)
- **Branch:** `feat/animation`（或后续 `feat/asset-import`）
- **Depends on:** `ASSET-F01` MVP；现有 `AssetManager` / `AssetTypeRegistry` / Loaders / `AssetWorkflowModule`

## TL;DR
**问题：** Import 是脚手架（分叉 API + Editor 硬编码）；Load 是 `LoadAssetByMeta_Internal` 字符串 if 链 + 每类型 `LoadAsset_Impl` 特化。二者都是 **中心开关耦合**——新资产要改 AssetManager / Dialog 多处，违背开闭。  
**方案：** 各资产 **Loader / 模块在初始化时主动 Register**：
- **LoadHandler**（按 `AssetTypeId`）
- **ImportProduct**（按开放的 `ProductId` 字符串 + 回调，**不是**闭集枚举）
- AssetManager 只做：缓存 / Meta / 查表调用；Editor Dialog **枚举已登记的 ImportProduct**（过渡态）。  
**不做：** 大虚基类插件框架；完整 Import Settings 面板（后续 Feature）。  
**当前：** **Done**（S00–S05）。

## Scope
- **In：**
  - **Load：** 以注册表替换 `LoadAssetByMeta_Internal` if 链；新增类型不再改 AM 中心文件（typed `LoadAsset<T>` 可过渡保留）
  - **Import：** 统一 `Import(ImportRequest)`；按 `ProductId` 查已登记 Import 回调；删除中心 `ImportProductKind` 枚举分发
  - 各 Loader（或 `Register*Handlers`）**主动登记** Load / Import；bootstrap 集中调用
  - AnimationClip Import：显式 Skeleton（Picker）；`{stem}_Skeleton` 仅 Runtime 可选 fallback
  - Editor：**共用 ImportDialog** 从 Import 注册表拉产物列表（过渡态）
  - `AssetWorkflowModule` 变薄
  - （可选）吸收 F01-S04：SourcePath UI + 最小 Reimport
- **Out：**
  - `.memesh` / Load 去 Assimp
  - 完整 Import Settings 框架（另开 Feature）
  - Retarget / ANIM Graph / Watcher 自动重导
  - 反射 codegen 自动生成 Register（可记后续债）
  - 强制删除 typed `LoadAsset<T>`（可分两期；本期至少 untyped 路径走注册表）

## Reader quick start
1. 本文件：注册式契约与边界
2. 现状痛点：`AssetManager::LoadAssetByMeta_Internal` · `LoadAsset_Impl<>` · `ImportExternalMesh` / `ImportAnimationClip` · `EditorMeshImportProductDialog`
3. 审阅后：Impl Plan 切片

---

## 1) 背景与目标

### Pain
1. **Import：** API 分叉；Editor 写死产物；Clip Skeleton 业务在 Editor。
2. **Load：** 新类型需改 `RegisterBuiltinTypes` + `LoadAsset_Impl` 声明/定义 + **`LoadAssetByMeta_Internal` 再加一支 if** —— 中心文件持续膨胀。
3. **开闭：** 扩展靠改 AssetManager / Dialog，而不是「加一个模块并 Register」。
4. **F01-S04** 等过渡债仍在。

### 成功标准（Feature Done）
- 新增可加载资产类型：主要在 **该类型 Loader + Register 调用**；**不**再改 `LoadAssetByMeta_Internal`。
- 新增简单 Import 产物：主要在 **该产物 RegisterImportProduct + cook 实现**；Dialog 只遍历注册表。
- 不存在中心 `switch(ImportProductKind)` / 闭集枚举作为扩展主路径。
- Clip 可显式选 Skeleton；AM 无 stem 拼路径业务（fallback 若保留在 Import 回调内）。
- 文档写明：共用 Dialog = 过渡；Import Settings = 后续。

### 表述澄清：「Asset 主动 Register」

| 期望说法 | 推荐落地 |
|----------|----------|
| 「Asset 类实例自己 Register」 | **不推荐**（过晚、难测、生命周期糊） |
| 「资产类型 / Loader 模块主动 Register」 | **推荐**：`StaticMeshLoader::RegisterPipeline()` 等，在 `AssetManager::Initialize`（或模块启动）集中调用 |

即：**扩展点在资产侧模块**，中心只持有表；不是运行时每个 `MEObject` 自注册。

---

## 2) 现状

| 层 | 现状 | 缺口 |
|----|------|------|
| `AssetTypeRegistry` | 仅元数据（Id / 扩展名 / 对话框） | 无 Load/Import 回调 |
| Load（untyped） | `LoadAssetByMeta_Internal` 长 if 链 | 新类型改中心 |
| Load（typed） | `LoadAsset_Impl<T>` 特化散落 Loaders | 与 untyped 双轨，但至少编译期 |
| Import | `ImportExternalMesh` / `ImportAnimationClip` / `ImportAsset` | 无统一入口；无注册表 |
| Editor | mesh 专用三按钮 Dialog | 与 Runtime 产物枚举双写 |

---

## 3) 方案

### 3.1 目标分层

```text
[各资产 Loader / 模块]
  RegisterAssetType(元数据)           // 已有，可保持
  RegisterLoadHandler(AssetTypeId)    // 新增
  RegisterImportProduct(ProductDesc)  // 新增（仅需要从 Source cook 的类型）
       │
       ▼
[AssetManager]  持有两张表；对外：
  LoadByMeta / Import(request)  → 查表调用
  （不再：中心 if 链 / ProductKind switch）
       ▲
[EditorImportDialog]  枚举 GetImportProducts()；组装 ImportRequest
```

**原则：**
- Source≠Asset（F01）不变；Import Product **产出**引擎资产，键是 `ProductId`，不必等于源扩展。
- Cook / Load 实现留在 Loader；AM 不写 Assimp。
- Dialog 不调用 Loader。

### 3.2 Load：注册式分发

```text
using AssetLoadFn = std::shared_ptr<Asset>(*)(const AssetMeta& meta, std::string& outError);

void AssetManager::RegisterLoadHandler(std::string_view assetTypeId, AssetLoadFn fn);

std::shared_ptr<Asset> AssetManager::LoadAssetByMeta_Internal(...) {
  auto* handler = FindLoadHandler(meta.AssetType);
  if (!handler) { outError = "no load handler"; return nullptr; }
  return handler(meta, outError);  // 内部仍可走原 Loader + 填 cache 的既有逻辑
}
```

**与 typed `LoadAsset<T>` 的关系（务实两期）：**

| 期 | 做法 |
|----|------|
| F02 本期 | Untyped 路径必须走注册表；`LoadAsset<T>` 可暂留，内部改为调同一 Loader，或特化仅转调注册函数 |
| 后续可选 | 收敛/删除重复特化；或保留 `LoadAsset<T>` 作为类型安全糖，**唯一实现**仍是注册的 handler |

**登记示例（语义）：**

```text
// SkeletalMeshLoader.cpp（或 RegisterSkeletalMeshPipeline）
void RegisterSkeletalMeshPipeline(AssetManager& am) {
  AssetTypeRegistry::Get().RegisterType({ ... }); // 若尚未登记
  am.RegisterLoadHandler("SkeletalMesh", &LoadSkeletalMeshFromMeta);
  am.RegisterImportProduct({ ... }); // 见下
}
```

`AssetManager::Initialize`：调用各 `Register*Pipeline()`（显式列表一次；**新增类型 = 加一行调用 + 新 cpp**，不再改 if 链）。

### 3.3 Import：注册式产物（取代 ImportProductKind）

**弃用：** 中心闭集 `enum class ImportProductKind` 作为扩展主路径。  
**改用：** 开放字符串 `ProductId` + 描述符 + 回调。

```text
struct ImportRequest {
  path SourcePath;
  path DestDirectory;
  string ProductId;              // 开放键，如 "StaticMesh" / "SkeletalMesh" / "AnimationClip" / "NativeCopy"
  string SkeletonAssetPath;      // 过渡 bag；日后可迁到 typed options
  int AnimationIndex = 0;
  // 预留：options bag → 未来 Import Settings
};

struct ImportCreatedAsset { string AssetPath; string AssetTypeId; GUID Guid; };

struct ImportResult {
  bool bSuccess;
  string ErrorMessage;
  vector<ImportCreatedAsset> Created;
  vector<string> Warnings;
};

struct ImportProductDescriptor {
  string ProductId;
  string DisplayName;            // UI
  // 源扩展是否可选用该产物（或 CompatibleSourcePredicate）
  bool (*AcceptsSourceExtension)(string_view ext);
  bool bNeedsSkeletonPicker;     // Dialog 条件控件（过渡）
  ImportResult (*Import)(const ImportRequest& request);
};

void AssetManager::RegisterImportProduct(const ImportProductDescriptor& desc);
const vector<ImportProductDescriptor>& AssetManager::GetImportProducts() const;

ImportResult AssetManager::Import(const ImportRequest& request) {
  auto* product = FindImportProduct(request.ProductId);
  if (!product) return Fail("unknown product");
  if (!product->AcceptsSourceExtension(ext)) return Fail("incompatible source");
  return product->Import(request);
}
```

**NativeCopy：** 也登记为一个 ImportProduct（或保留薄 `ImportAsset` 转调同一 handler），避免「native 走旁路、external 走注册表」的二次分裂。

**Clip + Skeleton 规则**（实现落在 AnimationClip 的 Import 回调内）：
1. 优先 `Request.SkeletonAssetPath`
2. 可选 fallback：`{Dest}/{stem}_Skeleton.meskeleton`
3. 皆无 → 失败，错误可读

**一对多：** 一个 Source 扩展可被多个 Product `Accepts`（FBX → Static / Skeletal / Clip）；Dialog 列出所有 Accepts==true 的已登记产物。这是 Import≠AssetType 1:1 的关键，**不能**只把 Import 回调塞进 `AssetTypeDescriptor` 完事。

### 3.4 与 `AssetTypeRegistry` 的边界

| 表 | 职责 |
|----|------|
| `AssetTypeRegistry` | 身份与扫描/对话框扩展名（AssetPath 侧） |
| LoadHandler 表 | 按 **AssetTypeId** 加载 |
| ImportProduct 表 | 按 **ProductId** 从 Source cook |

**不推荐**把 Load/Import 函数指针塞进现有 `AssetTypeDescriptor` 同一结构长期混放——Import Product 与 AssetType 基数不同。可共置在同一「管线注册」API 文件中，但 **逻辑上两张表**。

若希望调用体验统一，可提供：

```text
RegisterAssetPipeline({
  TypeDescriptor,
  LoadFn,
  optional ImportProductDescriptor
});
```

内部仍拆写入两张表。

### 3.5 Editor：共用 ImportDialog（过渡）

1. FileDialog 选 Source。
2. 对每个 external source：Dialog 展示 `GetImportProducts()` 中 `AcceptsSourceExtension` 为真的项。
3. 选中项若 `bNeedsSkeletonPicker` → 显示 Skeleton 选择。
4. 组装 `ImportRequest{ ProductId=... }` → `AssetManager::Import`。
5. Native 源：走 `ProductId=NativeCopy`（或自动推断唯一 Accepts 的产物）。

删除：`EditorMeshImportProductDialog` / `MeshImportProductChoice` / Editor 内 stem 拼 Skeleton。

**与 Import Settings：** F02 仅描述符 + 少量条件控件；完整 Settings 面板另开 Feature，届时仍消费同一 `Import` / ProductId。

### 3.6 删除列表

| 删除 / 收敛 | 时机 |
|-------------|------|
| `LoadAssetByMeta_Internal` 类型 if 链 | Load 注册表就绪后 |
| 中心 `ImportProductKind` 枚举分发 | Import 注册表就绪后（本 Draft **不再采用**该枚举作扩展模型） |
| `EditorMeshImportProductDialog` | Dialog 改读注册表后 |
| 长期双轨：旧 ImportExternal* public API | 调用方迁到 `Import` 后删除 |

### 3.7 可选：F01-S04

同前：SourcePath 展示建议纳入；Reimport 调已登记 Product（需 meta 记录 ProductId 或可从 AssetType 反推）；Watcher 自动重导 Out。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 保留 `ImportProductKind` + 中心 switch | 实现快 | 仍闭集；新产物改枚举/AM | **否**（已否决） |
| B. Loader/模块 Register Load + ImportProduct | OCP；与 Load 一并治本 | 需梳理 bootstrap；迁移量中等 | **选用** |
| C. 大虚基类 `IAssetImporter` 插件体系 | 「很 OO」 | 过重、空抽象 | **否**（表 + 函数指针/std::function 足够） |
| D. 只改 Import、Load if 链不动 | 范围小 | 同类债留下；下次还要动 AM | **否**（本次集中调整） |
| E. Import 回调塞进 `AssetTypeDescriptor` | 一处登记 | Product 与 Type 基数不一致（一源多产物） | **否**；用并列 ImportProduct 表 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Register 遗漏 → 运行时才发现 | Load/Import 失败 | Initialize 后断言：每个 Builtin AssetTypeId 均有 LoadHandler；测试覆盖 |
| `std::function` 擦除过重 | 调试难 | 优先原始函数指针；需要捕获再用 function |
| typed `LoadAsset<T>` 与注册表双轨 | 行为不一致 | 约定唯一实现在 Loader；特化只转调 |
| 范围变大（Load+Import+Dialog） | 难一次合入 | 切片：先 Load 注册表 → Import 注册表 → Dialog → 删旧 API |
| 过早 Settings 框架 | 空转 | 仍 Out |

---

## 6) 验收标准

- [x] `LoadAssetByMeta_Internal`（或后继）**无**按类型 if/else 链；经 `RegisterLoadHandler` 查表
- [x] 新增 Load 类型的 DoD：**不**要求修改 AM 分发函数体（只加 Register 调用 + Loader）
- [x] `AssetManager::Import(ImportRequest)` 经 `RegisterImportProduct` 查表；**无**中心 ProductKind switch
- [x] Dialog 产物列表来自 `GetImportProducts()`（过滤 Accepts）
- [x] Clip 可显式 Skeleton；无 stem 拼路径在 Editor
- [x] 旧 mesh Dialog / 旧 Import 分叉 API 删除或无引用（按切片）
- [x] `test smoke` + `asset-manager` / `animation-clip` PASS；手动 Import 三类产物
- [x] Design / Registry / ACTIVE_WORK / Progress 对齐

> 手动 Import（Static / Skeletal / AnimationClip + Reimport）：维护者确认 PASS（2026-09-05）。

---

## 7) 建议切片预览

| Slice | 内容 | 优先级 |
|-------|------|--------|
| S00 | LoadHandler 注册表；迁移现有类型；删除 if 链 | 高（地基）— **Done** |
| S01 | ImportProduct 注册表 + `Import()`；迁移 Mesh/Clip/Native | 高 — **Done** |
| S02 | 各 Loader `Register*Pipeline`；Initialize 显式调用 | 高 — **Done**（`AssetPipelineBootstrap`） |
| S03 | EditorImportDialog 读注册表；删 mesh Dialog；Skeleton Picker | 高 — **Done** |
| S04 | 删除旧 public Import* 双轨；收敛 typed Load 转调 | 中 — **Done** |
| S05 | （可选）SourcePath UI + Reimport | 中 — **Done** |

```text
S00 → S01 → S02 → S03 → S04
                     ↘ S05（可选）
```

---

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done** |
| What's not | Import Settings 框架（另 Feature）；按 Loader 再拆 Register*Pipeline（可选） |
| Unblock | — |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | Draft：正式 Import + 共用 Dialog；曾用 `ImportProductKind` 枚举模型 |
| 2026-09-05 | 全文中文 |
| 2026-09-05 | **修订：** 弃用中心 `ImportProductKind`；改为 Loader/模块 `RegisterLoadHandler` + `RegisterImportProduct`；Load if 链一并纳入；澄清「资产模块主动登记」非实例自注册 |
| 2026-09-05 | S00–S05 Done；自动化 + 手动验 PASS |
