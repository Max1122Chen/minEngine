# ASSET-F10 — DataAsset Polymorphic Load / Create Design Spec

## Meta
- **ID:** `ASSET-F10`
- **Type:** `Feature`
- **Complexity:** `L2`
- **Status:** `In Progress`
- **Owner:** project maintainer
- **Last updated:** `2026-10-17`
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F07](./ASSET-F07_DATA_ASSET_DESIGN.md)（薄基类 Done；派生扩展契约需修正）
  - [ASSET-F03](./ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md) · [ASSET-F06](./ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md) · [ASSET-F08](./ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN.md)
  - minCraft 反馈：`ENGINE_FEEDBACK_PROMPT_DATAASSET_DERIVED_LOAD.md`（消费仓缺口发现；非引擎任务清单）
- **Depends on:** ASSET-F07 Done；`NewObject(MEClass*)`；`AssetSerializeHelper`；反射 `CreateDefaultInstance`
- **Blocks:** minCraft 表型内容权威（消费侧 DATA-F05）去掉 per-type Bootstrap
- **Branch:** `feat/asset/dataasset-polymorphic`（建议；勿用 ID 命名）

## TL;DR

`RuntimeClassName` 是 **`AssetMeta` 的通用身份字段**（所有资产 Save 时写入实际 `GetClass()`），不是为 DataAsset 开的私有洞。它对应 UE `FAssetData::AssetClassPath`：索引里永远知道「这个资产是哪个类」。

**DataAsset 是第一个必须用它来建对象的消费者**：一条管线 typeId `"DataAsset"` 覆盖多个派生类，Loader 不能写死 `NewObject<DataAsset>`。其它 Loader（Texture2D、Mesh…）仍按 typeId 建自己的类；字段只做**冗余校验**（对得上才算成功）。游戏派生表不注册 handler。

## Problem

1. Load 路径：`DataAssetLoader::Load` → `NewObject<DataAsset>` → `ReadObjectFromFile`。运行时 `GetClass()` 为基类，派生 `ME_PROPERTY`（含 `std::vector` 行）不按派生 schema 填充。
2. Create 同源：`CreateHandler` 固定基类 + `AssetType "DataAsset"`，无法表达派生实例类。
3. ASSET-F07 Design §扩展曾写「游戏自注册 typeId + 扩展名 + handlers」——消费侧照做后样板爆炸（minCraft `ContentDataAssetBootstrap`），且与「Capabilities, not Opinions」冲突：把管线碎片推给每个游戏类型。
4. Save 对基类指针尚可写盘，与 Load 不对称 → 往返不可靠。

## Requirements

### Must

1. **`AssetMeta::RuntimeClassName` 是通用字段。** 任何成功 Save 的资产都写入 `asset.GetClass()->GetName()`。空字符串只表示「旧 meta，尚未重写」，不是「基类 DataAsset」的编码。
2. **写入点集中在 AssetManager**（Save / Create 落 meta 的同一处），不在每个 Loader 里各写一遍。
3. **缺字段分两档（见下节）。** 依据是「`AssetType` 能否唯一推出类」，不是资产重不重要。
4. **CreateDataAsset：** 按 `MEClass*` 建派生实例；`AssetType` 仍 `"DataAsset"`；扩展名仍 `.medata`；meta 写入该类名。
5. **禁止静默降级（DataAsset）：** 类名缺失、找不到，或不是 `DataAsset` 派生 → Load 失败。不得建成基类当成功。
6. **可恢复资产（校验消费者）：** Loader 仍按 typeId 建注册类。类名缺失 → Warning + 加载成功后尝试把 `GetClass()->GetName()` 写回 `.meta`。类名存在但不符 → Error 失败。本期**不**把 Texture/Mesh Loader 改成按类名 `NewObject`。
7. **存量：** 1:1 资产（Texture 等）缺字段仍能 Load。本 Feature 之前的基类 `.medata` 缺字段会在严格档下失败——S01 用一次性迁移把它们补成基类名（见缺字段策略），不在正式 Load 里猜。
8. **游戏零管线注册：** 派生 DataAsset 不要求 `RegisterType` / `Register*Handler`。
9. **验证：** 派生样例（含 `std::vector` of `ME_STRUCT`）Create → Save → drop cache → Load → `Cast<T>` + 行一致；另测一种非 DataAsset Save 后 meta 带上类名，且类名被篡改时 Load 失败。

### Should

10. `CreateAssetByTypeId("DataAsset")` 仍创建基类，但 Save 后 `RuntimeClassName` 为 `DataAsset` 的类名（不是空）。
11. `FindAssetMetasByRuntimeClass` 改为扫描 meta 字段（今日经 TypeRegistry 反查 typeId，派生无效）。
12. 游戏契约：模块加载后反射可见该类；消费 `LoadAssetByPath` + `Cast<T>`。

### Must not / Non-Goals

- Core 内置玩法 Content 表（Block/Item/Recipe）。
- 为每个派生类提供独立扩展名 / 独立 AssetTypeId / 独立 Loader（反对把 Bootstrap 升格为引擎 API）。
- `RegisterDerivedDataAsset<T>` 一类「一键注册 TypeRegistry+handlers」作为主路径。
- AES creatable 类选择器、Inspector 对 `IsA(DataAsset)` 的派生匹配（原 F07 Deferred → **本 Feature S03 / 可另开**）。
- 恢复 `LoadAsset<T>` / `CreateAsset<T>` 糖衣（F08 已否决）。
- Import / Cook / PrimaryDataAsset / Async（Async 见 `ASSET-F09`，且依赖本 Feature 的同步多态 Load）。
- 强制「表行 = 领域 Def」序列化策略定稿（见 Open Questions；不挡 S01）。

## Constraints

- Capabilities not Opinions：机制是「按运行时类建 DataAsset 实例」；玩法 schema 留在游戏模块。
- 一扩展名一 AssetTypeId：`.medata` ↔ `"DataAsset"` 不变；**实例类**用 `RuntimeClassName` 区分，不拆扩展名。
- 同质 Identity（F03）：Guid / path / cache；Load 不得换身。
- Runtime 不依赖 Editor。
- 源码注释不含 Feature/Slice/Bug/ADR ID。

## Preferences

- 类身份放 **sidecar `.meta`**（我们的资产索引），不优先 peek 资产 JSON 的 typeName。
- 字段语义对所有资产相同：实际运行时类名。DataAsset 与其它类型的差别只在 **Load 怎么用这个字段**（构造 vs 校验）。
- Create 入口：`AssetManager::CreateDataAsset` + Loader 实现。
- 失败信息分开：缺类 / 非 DataAsset / 类名与实例不符 / 反序列化失败。

## Assumptions

- 游戏模块在首次 Load 派生资产前，目标类已进入 `ReflectionSystem`（与今日其他跨模块 `ME_CLASS` 相同）。
- `MEClass::CreateDefaultInstance` 对游戏 DLL 内派生类可用（与 `NewObject(classInfo)` 现路径一致）。
- 存量仅基类 `.medata`；无生产环境依赖「错误建成基类但仍当成功」的行为。

## Non-Goals

见 Requirements Must not。另：不把 minCraft Bootstrap 迁进引擎仓。

## Current Architecture

```text
AssetMeta: AssetName, AssetPath, AssetType, SourcePath, Guid
  ※ 无 RuntimeClassName

AssetTypeRegistry: "DataAsset" → RuntimeClassName=DataAsset, ext=.medata
AssetPipelineBootstrap: Load/Save/Create handlers → DataAssetLoader（均写死基类）

Load:  meta.AssetType → LoadHandler_DataAsset → NewObject<DataAsset> → ReadObjectFromFile
Create: CreateAssetByTypeId("DataAsset") → NewObject<DataAsset> → Save + RegisterAsset(..., "DataAsset")

已有但未用于 DataAsset Load：
  NewObject(MEClass*) / CreateDefaultInstance
  FindAssetMetasByRuntimeClass（今日经 TypeRegistry 反查 typeId；对派生无效）
```

证据：`DataAssetLoader.cpp` Load/Create；`AssetMeta.h`；minCraft `ContentDataAssetBootstrap.cpp`（workaround）。

## External references（源码锚点）

### Unreal — 类是资产索引的一等字段，不是 DataAsset 私有

`FAssetData`（`Engine/Source/Runtime/CoreUObject/Public/AssetRegistry/AssetData.h`）：

- `AssetClassPath`：每个资产都有，例如 `/Script/Engine.StaticMesh`。UE5.1 起废弃短名 `AssetClass`，改为路径。
- 用途：Content Browser、`GetAssetsByClass`、未加载时知道类型。**加载本身不靠这个字段 new 对象。**

真正建对象的是包内 Export：Linker 按序列化进去的 `UClass` 构造。`StaticMesh` 与 `UMyTable : UDataAsset` 走同一条路。

`UDataAsset`（`Engine/Classes/Engine/DataAsset.h` + `Private/DataAsset.cpp`）：

- `NativeClass` 在构造函数里设为 `GetClass()`，标了 `AssetRegistrySearchable`。
- 那是可搜索标签（区分 native 类），**不是** Load 时用来 `NewObject` 的开关。DataAsset 没有单独 Loader。

对照到我们：没有 Package Export Map。sidecar `.meta` 就是我们的 `FAssetData`。所以类名放在 `AssetMeta`，对所有资产成立。DataAsset 额外用它做构造，是因为我们用 **一个 typeId + 一个 Loader** 覆盖一整棵派生树，而 UE 用 Export 里的真实类。

### Unity — `.meta` 不存运行时类

本机未挂 Unity 源码树，不引用具体文件。公开格式是：`.meta` = guid + importer；YAML/二进制正文里才有类（native 的 class id，或 MonoBehaviour 的 script guid）。类不在 `.meta`。我们若把类放进 `AssetMeta`，对齐的是 **UE 资产索引**，不是 Unity `.meta`。

### Godot — 类在资源文件头

`ResourceLoaderBinary::recognize` / `recognize_script_class`（`core/io/resource_format_binary.cpp`）：同一 `.res` 格式，头里有类型字符串和 script class。一个 Loader 认所有 `Resource` 子类。这支持「多态资源不必每种注册一个格式」，但类写在资源体内，不在旁路 meta。我们 MVP 仍把类放 meta（Load 已持有 meta，不必先 peek 正文）。

## Proposed Architecture

两个名字，职责分开：

| 字段 | 谁都有 | 回答的问题 |
|------|--------|------------|
| `AssetType` | 是 | 走哪条管线 / 哪个 handler / 哪个扩展名（`"Texture2D"`、`"DataAsset"`） |
| `RuntimeClassName` | 是 | 这个实例的反射类（`"Texture2D"`、`"BlockContentTable"`） |

多数内置资产两者字符串相同或一一对应。DataAsset 是例外：`AssetType` 恒为 `"DataAsset"`，`RuntimeClassName` 可以是派生类。

```text
[Save — 所有资产，一处]
  写完资产体
  meta.RuntimeClassName = asset.GetClass()->GetName()
  WriteOrUpdateMetaFile

[Load — 可恢复：AssetType 在 TypeRegistry 上只有一个合法类]
  仍按 AssetType 建对象
  类名缺失 → Warning，Load 成功，best-effort 重写 meta
  类名不符 → Error，失败
  重写失败（只读目录等）→ 再 Warning，Load 仍成功

[Load — 严格：一个 AssetType 对应多类（今天只有 DataAsset）]
  类名缺失 → Error，失败（不猜基类，避免把派生焊成基类并写回磁盘）
  否则 FindClass → 必须 IsA(DataAsset) → NewObject(class)
  ReadObjectFromFile

[CreateDataAsset(classInfo)]
  NewObject(classInfo)
  AssetType = "DataAsset"
  RuntimeClassName = classInfo->GetName()
  ext = .medata
```

游戏侧（官方）：

```cpp
// 模块启动：确保反射可见（无需 RegisterLoadHandler）
(void)BlockContentTable::StaticClass();

auto asset = AssetManager::Get().LoadAssetByPath("Assets/.../Blocks.medata", err);
auto* table = Cast<BlockContentTable>(asset.get());
```

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `AssetMeta` | 持久化通用 `RuntimeClassName` | 决定用它来 new 还是只校验 |
| `AssetManager` | Save/Create 时写入该类名；`CreateDataAsset`；Load 仍按 `AssetType` 分发 | 玩法类型；每个 Loader 的构造策略 |
| `DataAssetLoader` | **用**类名构造实例（本 Feature 的构造消费者） | 定义字段本身；其它资产的校验 |
| 其它 Loader | 建自己的注册类；若 meta 类名非空则核对 | 按任意类名 `NewObject`（本期不做） |
| `AssetTypeRegistry` | 管线 typeId ↔ 扩展名；基类 `"DataAsset"` | 每个游戏派生 typeId |
| 游戏模块 | `T : DataAsset` + 反射可见 + `Cast<T>` | 抄 Loader / 自定义扩展名 |

## Boundaries

- Allowed：游戏挂接反射；调用 AM Load/Create/Save；`Cast` 窄化。
- Forbidden：引擎 Core 引用 minCraft 类型；官方文档鼓励 per-derived `Register*Handler`；用自定义扩展名作为派生 Load 的必要条件。

## Dependencies

- Depends on：F07 DataAsset 管线；ObjectManager 多态 NewObject；Json `AssetSerializeHelper`。
- Depended on by：minCraft 内容表资产；未来 `ASSET-F09` 异步 DataAsset（同步多态正确是前提）。
- New direction：meta 多一个字段；不新增 ThirdParty。

## Ownership & Lifetime

与 F07 相同：AM cache 持有 `shared_ptr<Asset>`；Guid 身份 F03。本 Feature 不改生命周期，只改正实例动态类型。

## Data Flow

```text
任意资产 Save → 资产体 + .meta.RuntimeClassName = GetClass()
DataAsset Load → 类名决定 NewObject → 正文按该类 schema → Cast<T>
其它资产 Load → typeId 决定 NewObject → 类名只核对
```

## Control Flow

- Load：同步；仍走现有 `LoadAssetBy*` → handler 表（键仍为 `"DataAsset"`）。
- Create：同步；新 API 可直接调 `DataAssetLoader`，不必经 `CreateHandler` 函数指针（基类 CreateHandler 保留）。
- 无异步（F09 另做）。

## State & Invariants

1. `AssetType == "DataAsset"` ↔ 扩展名 `.medata`。`RuntimeClassName` 不参与扩展名映射。
2. 任意资产 **Save 成功之后**：`meta.RuntimeClassName == asset.GetClass()->GetName()`。
3. 任意资产 **Load 成功且 meta 类名非空**：加载后 `GetClass()->GetName()` 与 meta 一致。
4. DataAsset：非空类名必须 `IsA(DataAsset)`，否则 Load 失败。
5. 可恢复类型允许暂时缺字段；成功 Load 后应尝试补写。严格类型（DataAsset）缺字段不是合法稳态。

## API / Interface Semantics

### `AssetMeta::RuntimeClassName`

- **Responsibility：** 所有资产实例的反射类名（UE `AssetClassPath` 的短名形态；我们尚无 `/Script/Module.Class` 路径，MVP 用 `MEClass::GetName()`）。
- **Empty：** 尚未补写的旧 meta。可恢复类型：Warning + 尝试重写。严格类型：Error。**不是**「我是 DataAsset」的记号。
- **After Save：** 必为非空，等于当时 `GetClass()->GetName()`。
- **Persistence：** `.meta`，与 Guid 同文件。

### `DataAssetLoader::Load`

- Resolve class from meta；失败返回 nullptr + outError。
- 不改变 `AssetType` 分发。

### `AssetManager::CreateDataAsset`（名称可微调）

- **Inputs：** `const MEClass* classInfo`（或 className）、assetName、directoryRel、outError。
- **Precondition：** `classInfo` 非空且 `IsA(DataAsset)`（含基类自身）。
- **Output：** `shared_ptr<Asset>`；失败 nullptr。
- **Ownership：** 进入 AM cache；调用方持 shared_ptr 不影响身份。
- **Sync：** 是。
- **Does not：** 注册新 AssetType。

### `CreateAssetByTypeId("DataAsset")`

- 仍创建基类。Save 后 `RuntimeClassName` 为基类名。

### Save 路径

- **唯一写入点**在 AssetManager（`SaveRegisteredAsset` 成功后，或 `WriteOrUpdateMetaFile` 之前统一赋值）。Loader 不各自维护该字段。
- 对象的 `GetClass()` 必须已设置（`NewObject` 路径已 `SetClass`）。

## 缺字段策略

分档依据：**这个 `AssetType` 在 `AssetTypeRegistry` 里是不是唯一对应一个运行时类。**

| 档 | 谁 | 缺 `RuntimeClassName` | 字段在但类不对 |
|----|-----|------------------------|----------------|
| **可恢复** | Texture2D、StaticMesh、Material 等 1:1 管线 | **Warning**，按 typeId 加载，成功后 **尝试重写** meta 为 `GetClass()->GetName()`。写失败只再 Warning，Load 仍成功 | **Error + 失败** |
| **严格** | 一个 typeId 多类。今天只有 `DataAsset` | **Error + 失败**。不假定基类 | **Error + 失败**（找不到类、或不是 `DataAsset` 派生，同样失败） |

重写放在 **Load 成功之后**，用对象的真实 `GetClass()`，不要在读 meta 时先猜。只读工程写失败不能把可恢复资产变成打不开。

**为什么 DataAsset 缺字段不能 Warning 后当基类：** 一旦猜错，重写会把错误类名焊进 `.meta`，下次就变成「合法的基类文件」，派生字段回不来。1:1 资产没有这个风险，因为 typeId 已经唯一决定类。

**本 Feature 之前的基类 `.medata`：** 旧 Loader 只能写出 `DataAsset`，所以缺字段在历史上是安全的。不把这条例外写进正式 Load。S01 用一次性迁移：把这些 meta 补上基类名（测试夹具、仓库内文件、以及给用户项目的显式修复入口）。迁完再走严格档。

## Failure Model

| Failure | Behavior |
|---------|----------|
| 严格档：类名缺失 | Load 失败 |
| DataAsset：`FindClass` 失败，或类不是 `DataAsset` 派生 | Load/Create 失败 |
| 任意资产：类名非空且与加载后 `GetClass()` 不符 | Load 失败 |
| 可恢复档：类名缺失 | Warning；Load 成功；best-effort 重写 |
| 可恢复档：重写 meta 失败 | Warning；Load 仍成功 |
| `CreateDefaultInstance` 失败 | Load/Create 失败 |
| 反序列化失败 | 与今日相同；不进成功 cache |

## Alternatives & Trade-offs

### Option A（推荐）— 通用 meta 类名；DataAsset 用它构造，其它资产用它校验

- **Summary：** 字段属于资产身份，不属于 DataAsset。构造策略按管线选。
- **Pros：** 与 UE `FAssetData::AssetClassPath` 同构；以后查询/重定向有一处权威；DataAsset 多态不污染字段语义；旧 Loader 只加一行核对。
- **Cons：** 所有 `.meta` schema 变一次；Save 路径要有唯一写入点。

### Option B — 资产 JSON 写 root typeName，Load 先 peek

- **Summary：** `writeObjectTypeName` + 半解析再建对象。
- **Why not selected（MVP）：** `ReadObjectFromFile` 当前假设对象已存在；peek API 额外成本；类身份与 Guid 身份分居两文件更绕。可作为 meta 损坏时的恢复手段（Deferred）。

### Option C — `RegisterDerivedDataAsset<T>` 注册 TypeRegistry + handlers

- **Summary：** 把 minCraft Bootstrap 收成引擎糖衣。
- **Why not selected：** 固化错误扩展模型；每类型仍占 typeId/扩展名；与「多态创建即可」目标相反。

### Option D — 仅改 Load 用 TypeRegistry.`RuntimeClassName`

- **Summary：** 不改 meta，靠 typeId→类。
- **Why not：** 派生若共用 `"DataAsset"` typeId，注册表只有基类名；若拆 typeId，又回到 Option C。

### Option E — 字段只加在 DataAsset 路径，其它资产不写

- **Summary：** 最小 diff，只修 minCraft。
- **Why not：** 态度错了。类名是资产索引事实，不是 DataAsset 功能开关。以后 Mesh 派生或查询会再打一个洞。UE 没有「只有 DataAsset 才有 AssetClass」。

## Integration Impact

| Area | Impact |
|------|--------|
| `AssetMeta` | 通用新字段；生成反射 |
| `AssetManager` Save | **所有**成功保存写入 `RuntimeClassName` |
| 非 DataAsset Loader | Load 成功前做类名核对；构造逻辑不动 |
| `DataAssetLoader` | 按类名 `NewObject`；CreateDataAsset |
| AES / CB | S01 仍建基类；S03 类选择 |
| `FindAssetMetasByRuntimeClass` | S02 改扫 meta 字段 |
| minCraft | 删除 Bootstrap；`.medata` + CreateDataAsset |

## Migration / Compatibility

- **Forward：** 一次 Save 后所有资产 meta 带实际类名。
- **Backward：** 1:1 资产缺字段仍可 Load，并尝试补写。DataAsset 缺字段在迁移补上类名之前会 Load 失败；迁移只填基类名，因为旧管线写不出派生实例。
- **消费仓：** minCraft 自定义扩展名资产需迁移为 `.medata` + meta 类名（消费侧事务；引擎提供机制即可）。
- **F07 文档：** §扩展契约改为指向本 Feature；废弃「自注册 handlers」表述。

## Implementation Slices

| Slice | Deliverable | Status |
|-------|-------------|--------|
| **S01** | 通用字段；AM 统一写入；可恢复档缺字段 Warning+重写、不符则失败；DataAsset 缺字段/错类则失败；CreateDataAsset；派生 `vector<ME_STRUCT>` roundtrip | **Done**（仓库内无旧 `.medata`，未做批量迁移工具） |
| **S02** | `FindAssetMetasByRuntimeClass` 扫 meta 字段；F07 扩展契约勘误 | **Done** |
| **S03** | AES / Inspector 派生（类选择 Create；`IsA(DataAsset)` 编辑）— 可另 Feature 若膨胀 | Deferred 候选 |

## Verification Strategy

- **Core：** suite `data-asset` 扩展（或紧邻 case）：
  - 基类 Notes Create → Save → Load。
  - 测试内 `SampleTableDataAsset : DataAsset` + `std::vector<SampleRow>`（`ME_STRUCT`）→ CreateDataAsset → 填行 → Save → 丢 cache → Load → `Cast` + 行字段。
  - DataAsset：类名不存在，或指向非 DataAsset → Load 失败。
  - 非 DataAsset（现成一种即可，如走现有 create 的类型）：Save 后 meta 类名等于 `GetClass()`；把 meta 类名改成别的再 Load → 失败；删掉字段再 Load → 仍成功。
- **Regression：** 现有 `data-asset` smoke；触及 meta 的 asset-manager。
- **消费侧（非引擎 DoD）：** minCraft 去掉 Bootstrap 后表加载；可记 Progress 备注，不挡引擎 Done。

## Open Questions

1. **Create API 挂点：** `AssetManager::CreateDataAsset` vs 仅 `DataAssetLoader::Create` + AM thin wrap？ — **倾向 AM 公开 + Loader 实现**；blocking? no（实现时可定）。
2. **空类名：** 已拍板。空不是基类编码。可恢复档 Warning 并补写；严格档（DataAsset）Error。Save 后一律写 `GetClass()->GetName()`。存量基类 `.medata` 走一次性迁移，不走 Load 内猜测。blocking? no。
3. **表行 = 领域 Def / 非反射袋：** 官方推荐形态？ — **本期文档建议：行内仅 ME_PROPERTY / ME_STRUCT**；自定义袋另议；blocking? **no**。
4. **S03 是否留在 F10：** 若编辑器工作量大，拆 `ED-*` / 下一 ASSET。 — blocking? no。

## Design Review

- **Verdict:** `Pending`
- **Reviewer / date:**
- **Link or summary:** 维护者确认方向后 → Ready / Impl Plan → S01。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [ ] Unresolved questions resolved or explicitly deferred/accepted — Open Q 已标 non-blocking；待维护者审阅

**Ready for implementation?** `yes`（S01/S02 已落地；S03 Deferred）

## Acceptance Checklist

- [x] Design review waived by maintainer for urgent path（方向已确认后直接实现）
- [x] S01/S02 实现 + suite `data-asset` PASSED
- [x] Progress log updated
- [x] Feature registry status synced
- [x] F07 扩展契约勘误（S02）

## Changelog

| Date | Note |
|------|------|
| 2026-10-17 | S01/S02 落地：`AssetMeta::RuntimeClassName`、Save 统一写入、Load 两档、`CreateDataAsset`、`FindAssetMetasByRuntimeClass`。suite `data-asset` PASSED。存量基类 `.medata` 无仓库内文件，未做批量迁移工具。 |
| 2026-10-17 | 缺字段两档：AssetType 能唯一推出类 → Warning + best-effort 重写 meta；不能（DataAsset）→ Error + 失败。存量基类 `.medata` 一次性迁移，不在 Load 里猜成基类。 |
| 2026-10-17 | 修订：`RuntimeClassName` 为通用 AssetMeta 字段（对齐 UE `FAssetData::AssetClassPath`）。DataAsset 用它构造；其它资产 Save 写入并在 Load 时冗余校验。否掉「只为 DataAsset 改 meta」。 |
| 2026-10-16 | Draft：多态 Load/Create；否定 per-type 注册主路径；源起 minCraft 反馈 |
