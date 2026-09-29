# ASSET-F06 — AssetManager Facade Refactor — Design Spec

## Meta
- **ID:** `ASSET-F06`
- **Type:** Refactor
- **Complexity:** L3
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-29（补 Client Surfaces：Editor/Automation/MCP）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F02](./ASSET-F02_IMPORT_SERVICE_DESIGN.md) · [ASSET-F03](./ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)
  - [ASSET-F04](./ASSET-F04_CONTENT_WORKFLOW_DESIGN.md) · [ASSET-F05](./ASSET-F05_TEXTURE_COOKED_ASSET_DESIGN.md)
  - [ASSET-F01](./ASSET-F01_IMPORT_PIPELINE_DESIGN.md)（`.memesh` Deferred，非本 Feature 硬目标）
  - [ASSET_REGISTRY_REFACTOR_PLAN](../Platform/ContentBrowser/ASSET_REGISTRY_REFACTOR_PLAN.md)（R0/R1 Done）
- **Depends on:** F02 注册表；AssetRegistry R0；Serialization / Reflection（**仅有 Saver 的类型**）
- **Blocks:** ASSET-F05（结构落地后再做 Texture 引擎侧资产文件）

## TL;DR

`AssetManager` 为统一门面（缓存、meta/Guid、查表、委托 ContentMutation）。类型侧为可选的 **Importer / Loader / Saver**；**Create 归 Saver**（有引擎序列化格式才能 Save，Create = 默认对象 + 首次 Save + Register）。无 `AssetSaveKind` 枚举：有 `Save` 即走引擎原生序列化 helper。Load 可用 Assimp/stb。Builtin 集中 `Register*`；注册 API 对外可用。废除模板特化脊柱。

## Problem

1. AM（~2900 行）混杂类型 IO、文件夹 CRUD、模板特化与 F02 注册双轨。
2. `*Loader` 与 Import 符号/职责纠缠；Import 大块仍在 AM。
3. Save 多份复制，且曾被误建成「SaveKind / Codec」分叉。
4. Create 与 Save 分离注册易重复「默认真源 + 落盘」逻辑。
5. ContentMutation 需求已存在（F04），但缺独立合同与数据模型。

## Requirements

- AM 唯一主门面；类型算法不在 AM.cpp。
- 职责：**Importer**（可选）、**Loader**（通常有）、**Saver**（可选，含 Create）、**ContentMutation**（盘面）。
- Capability Matrix 可 enforce；无能力 API 失败。
- 废除 `LoadAsset_Impl` / `SaveAsset_Impl` / AM 上 `CreateAsset<>` 特化。
- Builtin bootstrap `Register*`；公开 Register API。
- Texture（F05）：扩展 `Texture2D` + 引擎侧资产文件；本 Feature 只留挂接结构。
- 行为可验证；故意变更文档化。
- **客户端兼容：** Automation/MCP 契约冻结；AM Mutation 签名保留；Registry 事件对 TreeModel 等价；见 § Client Surfaces。

## Constraints

- ASSET-F03 Guid / 单对象 / Load 不换身。
- 无强制 `IAssetIO`；无完整 UE Factory 框架。
- 不强制 `.memesh` / Texture 像素重编码。
- Runtime 不依赖 Editor；forward-only。

## Preferences

- `fn*` + descriptor；Loader/Importer 分目录。
- Save 统一经 `AssetSerializeHelper`（引擎序列化）；类型 Saver 只做 NewDefault / 调用 helper / 类型特有前后步骤。

## Assumptions

- 「可 Save」⇔ 存在引擎原生可序列化表示（`.memtl` / `.mescene` / Texture 引擎侧文件等）；**不是**把 Mesh 顶点或 png 像素当反射 dump 目标。
- Mesh 无 Saver；几何留在 obj/glb。
- 游戏可用公开 Register API 扩展；本期无插件热加载。

## Non-Goals

- 异步加载、引用图、Registry R2/R3、F05 全文、清理 Texture Wrapping/Filtering。

---

## Current Architecture（摘要）

```text
AM 胖门面：ImportProduct + LoadHandler + Load/Save/Create 特化 + Folder/Move/Delete
Registry R0 已拆索引；Loaders 部分 Load→Assimp ImportFromFile
```

---

## Target Architecture

### 逻辑结构

```text
Builtin Register*  (+ 游戏可再 Register)
        |
        v
 AssetManager 门面 ──┬── AssetRegistry
                     ├── AssetContentMutation
                     ├── Pipeline[TypeId] → Load / Saver(Save+Create?)
                     └── ImportProduct[ProductId] → Importer
```

### 语义合同

| 操作 | 含义 | 谁实现 | 主键 |
|------|------|--------|------|
| **Import** | 外部 raw → Content 可解释形态 + Register | \*Importer | ProductId |
| **Load** | Content 文件 → 内存 Asset（可 Assimp/stb） | \*Loader | TypeId |
| **Save** | 内存对象 → 引擎原生序列化文件 | \*Saver（经 SerializeHelper） | TypeId |
| **Create** | 无外部源：NewDefault → Save → Register（F03） | **\*Saver** | TypeId |
| **Mutation** | 路径/文件夹变更 | ContentMutation | paths |

**推论：** 无 Saver ⇒ 无 Save、无 Create（Mesh）。有 Saver ⇒ Save 使用引擎序列化；Create 为 Saver 的可选能力（Texture 可有 Save 无 Create）。

**Reimport：** SourcePath → Import（覆盖、保 Guid）。

### 职责模型

| 单元 | 负责 | 不负责 |
|------|------|--------|
| **AssetManager** | 统一 API；缓存；注册表；meta/Guid；调度 Create=Saver.Create+Register | 类型算法 |
| **\*Importer** | 外部源入场 | Load；Mutation |
| **\*Loader** | Content → 对象 | 首次外部导入政策 |
| **\*Saver** | Save；可选 Create（默认对象+首次序列化） | Import；Mutation；无序列化格式的类型 |
| **AssetSerializeHelper** | 统一 ToFile/FromFile 选项与错误 | 类型默认值政策 |
| **ContentMutation** | 盘+meta+Registry+缓存失效 | 解析/导入 |
| **AssetRegistry / TypeRegistry** | 索引 / 类型元数据 | IO 算法 |

### Capability Matrix

| TypeId | Import | Load | Save | Create（Saver） | 说明 |
|--------|--------|------|------|-----------------|------|
| Material / MI | — | Y | Y | Y | |
| Scene / Prefab | — | Y | Y | Y | |
| Sprite / Atlas / AnimGraph | — | Y | Y | Y | |
| AnimationClip | Y | Y | Y | — | |
| Skeleton | 随 Mesh Product | Y | Y | — | |
| StaticMesh / SkeletalMesh | Y | Y | **—** | **—** | 无引擎序列化网格正文 |
| Texture2D | Y | Y | Y | — | 引擎侧文件；扩 Texture2D |
| 其他 | 迁入时如实登记 | Y | 按需 | 按需 | |

### 注册描述符（无 SaveKind）

```cpp
struct AssetMutationHooks {
    bool (*BeforeDelete)(AssetManager&, const AssetMeta&, std::string& err) = nullptr;
};

struct AssetPipelineDescriptor {
    AssetTypeDescriptor Type;
    AssetLoadHandlerFn Load = nullptr;

    // Saver：空 = 无 Save/Create
    bool (*Save)(AssetManager&, const AssetMeta&, const MEObject&, std::string& err) = nullptr;
    // Create：仅当 Save != nullptr 时有意义；空 = 不可 Create
    std::shared_ptr<Asset> (*Create)(
        AssetManager&,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& err) = nullptr;

    AssetMutationHooks Mutation{};
};

// Import 仍用 F02 ImportProductDescriptor（ProductId）
```

**约定：** `Save != nullptr` 时，实现须通过 `AssetSerializeHelper` 写出引擎格式（允许类型包装一层做校验，但持久化载体是引擎序列化，不另建 SaveKind）。

**Create 流程（AM 编排，Saver 提供 Create）：**

```text
Create(typeId, name, dir):
  pipe = pipelines[typeId]
  if !pipe.Create || !pipe.Save → fail
  asset = pipe.Create(am, name, dir, err)   // NewDefault + 写文件（内部调 helper）+ 准备 meta
  RegisterAsset / 写 meta Guid（F03）
  cache
  return asset
```

类型模块可将 Create 写成：`NewObject` → `helper.ToFile` → 返回对象；AM 保证 Guid/Registry 合同。

### 接口形态与 Bootstrap

- 函数表注册；不强制虚接口。
- Builtin：`AssetPipelineBootstrap` 集中 `Register*`。
- 外部：公开 `RegisterPipeline` / `RegisterImportProduct`。

### Texture（结构；细节 F05）

扩展 `Texture2D`；引擎侧资产文件 + 源图路径；Saver 序列化引擎侧字段；Loader 读文件再 stb。Wrapping/Filtering 后续清。

### ContentMutation（详细）

#### 定位

资产**工作区**变更服务：在项目 Content 根下改文件/文件夹布局，并保持 `AssetMeta` 路径、`AssetRegistry`、加载缓存一致。  
**不是**通用文件系统 API；不解析资产正文。

#### 依赖与协作

```text
Caller (AM 门面 / 日后直连)
    → AssetContentMutation
         → std::filesystem（绝对路径经 PathRegistry / AM.Resolve）
         → 读写旁路 .meta（路径字段）
         → AssetRegistry（Register/Unregister/Move/Batch）
         → AssetManager 缓存失效钩子（或 Mutation 回调 AM）
         → 可选 Pipeline.Mutation.BeforeDelete
```

Mutation **不**调用 Loader/Importer/Saver。

#### 数据模型

```cpp
// 路径约定：与今日 AM 一致 — 项目相对 Content 路径（normalize 后）
// 资产文件 + 旁路 meta：AssetPath + AssetPath + ".meta"

struct ContentFolderCreateRequest {
    std::string ParentDirectoryRel;  // 空 = Content 根
    std::string FolderName;
};

struct ContentAssetMoveRequest {
    std::string OldAssetPathRel;     // 不含 .meta
    std::string NewAssetPathRel;
};

struct ContentAssetRenameRequest {
    std::string OldAssetPathRel;
    std::string NewFileName;         // 含扩展名
};

struct ContentDeleteAssetRequest {
    std::string AssetPathRel;
    bool bUnpackOpenPrefabInstanceRefs = false; // Prefab 政策；经 BeforeDelete 或 AM 预步进
};
```

内部状态：Mutation **无长期状态**；Registry/缓存为协作真源。可用 `AssetRegistryBroadcastBatchScope` 包裹批量 Move/Delete。

#### 目标 API（可由 AM 转发同名）

```cpp
class AssetContentMutation {
public:
    explicit AssetContentMutation(AssetManager& manager); // 取 Registry、Resolve、缓存失效

    bool CreateFolder(
        const ContentFolderCreateRequest& request,
        std::string& outError,
        std::string* outCreatedDirectoryRel = nullptr);

    bool RenameFolder(
        const std::string& directoryRel,
        const std::string& newFolderName,
        std::string& outError);

    bool DeleteFolder(
        const std::string& directoryRel,
        std::string& outError,
        bool requireEmpty = true);

    bool MoveAsset(
        const ContentAssetMoveRequest& request,
        std::string& outError);

    bool RenameAsset(
        const ContentAssetRenameRequest& request,
        std::string& outError);

    bool DeleteAsset(
        const ContentDeleteAssetRequest& request,
        std::string& outError);
};
```

行为口径（与现 F04/AM 对齐，抽离时保持）：

| API | 行为要点 |
|-----|----------|
| **CreateFolder** | 建空目录；无 `.meta`；不发资产 Registered |
| **RenameFolder** | 重命名目录；更新其下所有资产 path + meta + Registry Moved（Batch） |
| **DeleteFolder** | 默认 `requireEmpty`；非空失败 |
| **MoveAsset** | 移资产文件+`.meta`；更新 meta.AssetPath；Registry Moved；缓存按旧 path 失效 |
| **RenameAsset** | 同目录改文件名；同 Move 的 Registry/meta 规则 |
| **DeleteAsset** | 可选 BeforeDelete；删文件+meta；Unregister；缓存失效 |

失败：盘操作失败则**不**改 Registry；返回 `outError`。

#### 与 AM 门面关系

- 过渡期：`AssetManager::MoveAsset` 等一行转发 Mutation，保证 CB/Automation 源码少改。
- 长期：调用方可依赖 Mutation；AM 保留转发以维持「单门面」。

### AssetManager 门面 API（目标）

| API | 行为 |
|-----|------|
| `RegisterPipeline` / `RegisterImportProduct` | 登记 |
| `LoadAssetByMeta/Path/Guid` | 查 Load |
| `SaveAsset(path, MEObject&)` | 查 Save；经类型 Saver → helper |
| `CreateAsset(typeId, name, dir)` | 查 Saver.Create；F03 Register |
| `Import` / `Reimport` | Product 表 |
| `CreateFolder` / `RenameFolder` / `DeleteFolder` / `MoveAsset` / `RenameAsset` / `DeleteAsset` | → Mutation |
| `LoadAsset<T>` | 可选糖衣 |

### 目录建议

```text
Runtime/Resource/
  AssetManager.*
  AssetContentMutation.*
  AssetSerializeHelper.*
  AssetPipelineBootstrap.cpp
  Loaders/
  Importers/
  Savers/          // 或与 Loaders 同文件 MaterialLoader+Save；Mesh 无 Saver
```

---


## Client Surfaces（Editor / Automation / MCP）

> F06 若只改 Runtime 门面而忽略调用链，会打断 CB、Automation 与 MCP。本节把接触面与**兼容策略**写成验收合同。

### 调用链（现状）

`	ext
MCP Tools (minEngineMcpServer)
  → Automation pipe ops (asset.*)
    → EditorAutomationSurface
      → AssetEditorServices  （主资产编排门面）
        → AssetManager

Content Browser / AssetWorkflowModule / Sprite* / AnimGraph UI
  → AssetManager（部分直连；部分经 AssetEditorServices / Workflow）

AssetTreeModel / Thumbnails / Preview / Appearance
  → AssetManager（查询、Subscribe、Load）
`

MCP **不**直接链 Runtime；稳定 Automation op 名与 payload 即可，MCP 粗 Tool 映射可不动。

### 接触面清单

| 层 | 代表位置 | 今日依赖 | F06 风险 | 策略 |
|----|----------|----------|----------|------|
| **AssetEditorServices** | Editor/.../AssetEditorServices.* | Create(type字符串) 内仍 CreateAsset<T>；Import/Move/Rename/CreateFolder/Delete*/List/ListImportProducts | Create 特化删除；Mutation 搬家 | Services 改为 CreateAsset(typeId,…) 或保留薄 CreateAsset<T> 糖衣；Mutation **AM 同名转发**，Services 源码可几乎不动 |
| **EditorAutomationSurface** | sset.list/create/import/delete/create_folder/move/rename/rename_folder/delete_folder + batch_rename 等 | 经 AssetEditorServices | op 名/字段变更会破 MCP | **冻结** Automation op 名与 JSON 字段语义；实现只换底层调用 |
| **MCP** | minEngineMcpServer tools → composeOp | 依赖上述 asset.* | 无 Runtime 直连 | 默认 **零改 MCP**；仅当 Automation 契约变才改 |
| **ContentBrowserWindow** | 直连 CreateFolder/MoveAsset/RenameAsset/RenameFolder | AM 成员 | Mutation 抽离 | AM 保留同签名转发；CB 可不改或随后改调 Mutation |
| **AssetWorkflowModule** | CreateAsset<Scene/Material/Sprite…>、Import Dialog、Delete 确认 | 模板 Create + Import | 特化删除 | 改 CreateAsset(typeId) 或糖衣；Import 仍 Import(ImportRequest) |
| **SpriteEditorServices / AnimGraphWindow** | CreateAsset<Sprite/Atlas/AnimGraph> | 模板 Create | 同上 | 同 Workflow |
| **AssetTreeModel** | Subscribe / FindAssetMetasUnderDirectory / Moved 等 | Registry 事件 | 事件 Kind/时序变化 | **冻结** AssetRegistryChangeKind 与 Batch 语义；Mutation 必须发与今日等价事件 |
| **Thumbnail / Preview / Appearance** | LoadAsset<T> / ByGUID | Load 特化双轨 | Load 改 Handler | 保留 LoadAsset<T> 糖衣；行为不变 |
| **Tests** | AssetManagerTest 等 | Create/Load/Save 特化 | 断言失败 | Slice 内改测；Create 改 typeId 或糖衣 |
| **ProjectAssetWatcher** | Scan / 外部同步 | AM Scan/Register | Mutation 通关文牒（Registry 计划 R2） | F06 不强制 R2；不破坏今日「AM CRUD 已更新 Registry」闭环 |

### 兼容合同（硬）

1. **Automation sset.* op 名与既有字段语义不变**（创造/导入/删除/文件夹/move/rename）。失败时仍返回可读 Message；新增字段只能可选附加。
2. **MCP：** 默认本 Feature **不改**；验收包含既有 asset 相关 smoke/accept（若有）或手动 sset.list / sset.create 一条。
3. **AM 对外 Mutation 签名保留**（CreateFolder/MoveAsset/RenameAsset/DeleteAsset/RenameFolder/DeleteFolder），实现委托 AssetContentMutation。允许新增重载，禁止无替换地删改参数含义。
4. **Import(ImportRequest) / GetImportProducts / Reimport 形状保留**；实现迁出不影响 Editor Import Dialog / AssetEditorServices::Import。
5. **CreateAsset<T>：** 允许保留为薄糖衣（CreateAsset(GetTypeId<T>(),…) + cast）；或删除后改所有调用方为 typeId — **默认保留糖衣**以降低 Editor 改动面，特化实现删除。
6. **Registry 订阅：** Subscribe/Unsubscribe、Moved/Registered/Unregistered 对 TreeModel 行为等价；批量操作继续可用 Batch scope。
7. **AssetEditorServices** 保持 Editor/Automation 的**推荐入口**；F06 不要求 MCP/Automation 改调 Mutation 或 Saver。

### 按 Slice 的客户端改动预期

| Slice | Runtime | Editor / Auto / MCP |
|-------|---------|---------------------|
| S02 Mutation | 新类 + AM 转发 | **期望 0** CB/Services 改动 |
| S03–S04 Load/Save | Helper + Handler | Thumbnail/Preview 无感（糖衣）；Save 调用方无感 |
| S05 Importers | 迁出 AM | Import Dialog / Services / sset.import **无感** |
| S06 Create→Saver | 注册 Create | AssetEditorServices / Workflow / Sprite / AnimGraph / Tests：糖衣下 **小改或 0**；去掉糖衣则显式改 typeId |
| 全程 | — | Automation/MCP 契约回归 |

### 非目标（客户端）

- 不新增 MCP 粗 Tool。
- 不把 Automation 改成直连 Mutation（可后置）。
- 不重做 Content Browser UI。

---

## Boundaries

**允许：** AM 调度；Saver 调 SerializeHelper；Mutation 改盘+Registry；外部 Register。

**禁止：** AM 堆类型算法；无 Saver 却 Create 成功；引入 SaveKind/IAssetIO；Mutation 内 Load/Import；Mesh Save 顶点；Texture 常态改写 png。

## Dependencies

F02/F03、Registry、Serialization（有 Saver 的类型）。CB/Automation/F05 依赖本结构。

## Ownership & Lifetime

缓存属 AM；Registry 由 Import/Create/Register/Mutation 更新；Mutation/Loader/Saver 无全局表所有权。

## Data Flow

```text
Import → Importer → 文件 → Register
Load   → Loader → cache
Save   → Saver → SerializeHelper → 盘
Create → Saver.Create → Register → cache
Mutation → 盘+meta → Registry → cache invalidate
```

## State & Invariants

- F03；无能力失败；Mutation 失败不改 Registry；Save 失败保留旧文件。

## Failure Model

未知 TypeId、无 Load/Save/Create/Import、盘失败、序列化失败：明确错误消息。

## Alternatives（已选定）

| 主题 | 选定 |
|------|------|
| Create 归属 | **并入 Saver** |
| SaveKind | **取消**；有 Save ⇒ 引擎序列化 |
| 接口 | 函数表 |
| Mesh Save | 无 |
| Mutation | 独立类型 + AM 转发 |
| CreateAsset<T> | **保留薄糖衣**（降 Editor 改动） |
| Automation/MCP | **冻结 op 契约** |

## Complexity Assessment

L3；Planned + Readiness 后大段编码。

## Open Questions

1. F04 未验收是否开 Mutation 抽离 — 建议验收后或显式允许。
2. Savers/ 独立目录 vs Loader 同文件 Save — 默认同模块文件即可，不强制目录。
3. CreateAsset<T> 糖衣保留多久 — **默认长期保留薄糖衣**，避免 Editor 大面积改动。

## Implementation Slices

| Slice | 目标 |
|-------|------|
| S01 | 矩阵 + Inventory；禁新双轨 |
| S02 | ContentMutation（按上节 API）+ AM 转发；**验证 CB/Services 零改或仅重链** |
| S03 | SerializeHelper；废除 Save 特化；Saver.Save 挂接 |
| S04 | Load 仅 Handler；废 Load 特化 |
| S05 | Importers 迁出；符号分离 |
| S06 | Bootstrap；Create 经 Saver；**清点并改 Editor CreateAsset 调用方**；Automation create 回归 |
| → F05 | Texture Saver/Loader/Importer |

## Risks

- 旧 F05 像素 cook 文案 — 以本文+修订 F05 为准。
- RenameFolder 大批 Moved 事件 — 沿用 Batch。
- 漏改 CreateAsset<T> 调用方（Workflow/Sprite/AnimGraph/Tests）— S06 清单验收。
- 破坏 Automation/MCP 契约 — Slice 回归 sset.create/sset.import/sset.move。
- TreeModel 丢事件 — Mutation 必须走 Registry 既有 Kind。

## Acceptance Criteria

- [ ] 无 SaveKind；Save 注册为 fn 有无。
- [ ] Create 仅通过 Saver.Create；无 Saver 则 Create 失败。
- [ ] Mesh 无 Save/Create；Material 无 Import。
- [ ] ContentMutation 具备上节 API 行为；AM 可转发。
- [ ] 无 Load/Save 特化表；Import 不在 AM.cpp。
- [ ] 无 IAssetIO；Register API 公开。
- [ ] F05 可零改 AM 算法文件挂接。
- [ ] L3 Readiness 后再大段编码。

## Test Plan

- 现有 asset-manager / smoke / Editor。
- Create Material round-trip；Mesh Save/Create 应失败；Mutation move/rename/folder；Import/Load Mesh。
- Automation：sset.create / sset.import / sset.move / sset.create_folder（经 Editor 或现有 accept）行为不回归。
- CB：CreateFolder + 拖拽 Move + Rename（人手或与 F04 验收合并）。
- AssetTreeModel：Move/Rename 后列表/树与 Registry 一致。

## Migration

Forward-only。冲突文档更新 F05。


## Pre-Implementation Baseline（重构前规模，2026-09-29）

| 度量 | 数值 |
|------|------|
| AssetManager.h | ~483 行 |
| AssetManager.cpp | ~2906 行 / ~103 KB |
| LoadAsset_Impl 声明（头文件） | ~19 |
| SaveAsset_Impl 特化（cpp） | ~10 |
| CreateAsset<> 特化（头文件） | ~7 |
| Loaders/* | 34 文件 / ~3425 行（Loader 与部分 Import 混杂） |
| AssetTypeRegistry.cpp | ~414 行（含 Builtin 上帝表） |
| AssetPipelineBootstrap.cpp | ~52 行 |
| 门面能力 | Load/Save/Create 特化 + ImportProduct + CRUD/Folder + Registry 委托 |

**实现后对比：** 在 Progress / 本附录更新同表（行数、特化数、是否仍有 Assimp 在 Loader 公共 Import 符号、Mutation 是否独立文件）。

### 实现注意：多重 Load / Save

历史路径中存在「Create/Save/Load 链路里再次 Save 或 Load 换身」等重复 IO（部分已由 F03 约束）。实现 F06 时：

- **禁止**照搬「能跑就算」的旧特化正文而不审调用图。
- 每个类型迁入 Pipeline 时检查：Create → 是否多余 Load；Save → 是否重复 ToFile；Load → 是否二次反序列化或误走 Import。
- 发现即在本 Slice **顺手修掉**（真修复，非再包一层），并在 Progress 记一笔。


## Appendix A — Inventory

| 单元 | 现状 | 目标 |
|------|------|------|
| Static/SkeletalMeshLoader | Load→Assimp ImportFromFile | Loader vs Importer 分符号 |
| AnimationClipLoader | 双职责 | 拆分 |
| AM ImportExternalMesh* | 在门面 | → Importers |
| Texture2DLoader | 身份≈png | F05 引擎侧文件 |
| AM Create/Save 特化 | 双轨 | → Saver + Helper |

## References

- Godot Loader/Saver/Importer 三分
- UE Registry / Factory / UTexture 设置
- 仓内 `AssetManager.*`、`AssetMeta.h`、F04 文件夹 API