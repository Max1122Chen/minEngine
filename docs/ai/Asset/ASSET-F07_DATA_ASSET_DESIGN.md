# ASSET-F07 — Thin DataAsset（可编辑定义资产）— Design Spec

## Meta
- **ID:** `ASSET-F07`
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-10-08
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F03](./ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md) · [ASSET-F06](./ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md) · [ASSET-F08](./ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN.md)（**Done**，已解锁）
  - [ED-F24](../Editor/ED-F24_ASSET_EDITOR_SERVICES_GUI_CONVERGENCE_DESIGN.md)（Create / 服务层 AES）
  - [ASSET-F07 Design Review](./ASSET-F07_DATA_ASSET_DESIGN_REVIEW.md)
- **Depends on:** ASSET-F08 Done；ASSET-F06 Done；AES Create allowlist；`AssetSerializeHelper`；`PropertyValueWidget`
- **Blocks:** 无硬阻塞；游戏模块可据此把硬编码定义迁到可编辑资产（另开 Feature）
- **Branch:** `feat/asset-load-api`（与 F08 同轨合入）

## TL;DR

**问题：** 游戏侧大量结构化定义缺少与 Material/Sprite 同质的「可 Create / 可反射编辑 / 可 Load·Unload」资产类型。

**方案：** 引擎薄基类 `DataAsset : Asset`，扩展名 `.medata`，走 F06 Create/Load/Save + `AssetSerializeHelper`；AES/CB 可创建；CB Inspector 对 `DataAsset` 做反射字段编辑 + 显式 Save。运行时 `LoadAssetByPath`/`ByGUID`/`ByMeta` + `minEngine::Cast<T>`（见 F08 / `ObjectCast.h`）。

**状态：** **Done**（S01–S03 + suite `data-asset` 通过）。Import/Cook/Primary / Undo / 游戏 AES 动态 creatable **Out/Deferred**。

## Problem

1. minCraft 等项目定义常硬编码在 C++ Registry / Factory。
2. 已有 Asset 生命周期（Guid、`.meta`、cache、Mutation），缺「纯数据定义」资产槽。
3. CB Inspector 对多数资产仅 meta 文本；需 DataAsset 反射编辑路径。

## Requirements

### Must

1. **类型：** `minEngine::DataAsset`（`ME_CLASS`），继承 `Asset`；可选 `m_Notes` 供 Inspector smoke（非玩法语义）。
2. **管线：** typeId `"DataAsset"`，扩展名 **`.medata`**；RegisterLoad/Save/CreateHandler。
3. **序列化：** `AssetSerializeHelper`（与 Prefab/Sprite 同质 JSON）；无 cook。
4. **Editor Create：** AES allowlist + `TryCreateDataAssetInDirectory` + CB Create + `asset.create` 同源。
5. **Editor Edit：** 选中 typeId `DataAsset` → Load → `PropertyValueWidget` → 显式 Save 按钮。
6. **Runtime：** ByPath/ByGUID/ByMeta；窄化用 `Cast<T>`（禁止恢复 AM `LoadAsset<T>`）。
7. **扩展契约：** 游戏模块派生 `T : DataAsset`，自注册 typeId + 扩展名 + handlers（AES creatable 动态注册 Deferred）。
8. **验证：** suite Create → 改字段 → Save → 丢缓存 → Load → 断言。

### Should

9. 派生类型 Inspector：基类按 typeId；派生以后续 cast/`IsA` 扩展（本期仅基类 typeId）。

### Must not / Non-Goals

- JSON/CSV/MD Import、Incremental Cook、PrimaryDataAsset、pak、Async 大改、Core 内玩法类型、恢复 `LoadAsset<T>`。

## Constraints

- Capabilities not Opinions：Core 只提供薄机制。
- 同质 Identity / Lifecycle（F03/F06）。
- Editor Create 经 AES；一扩展名一 AssetTypeId。
- Runtime 不依赖 Editor。

## Proposed Architecture

```text
[Editor / Agent]
  AES.Create("DataAsset") / asset.create
        ↓
  DataAssetLoader::CreateHandler → .medata + .meta
        ↓
  CB select → Inspector Load → PropertyValueWidget → SaveHandler

[Runtime / Game]
  LoadAssetByPath / ByGUID / ByMeta → shared_ptr<Asset>
        ↓
  Cast<DataAsset> / Cast<Derived>
```

## Type layout

```cpp
ME_CLASS()
class DataAsset : public Asset {
  ME_GENERATED_BODY()
  ME_PROPERTY(EditAnywhere, meta = (Setter = "SetNotes", Getter = "GetNotes"))
  std::string m_Notes;
};
```

## Implementation Slices

| Slice | Deliverable | Status |
|-------|-------------|--------|
| **S01** | `DataAsset` + TypeRegistry `.medata` + Load/Save/Create + Bootstrap | **Done** |
| **S02** | AES allowlist + TryCreate + CB Create | **Done** |
| **S03** | CB Inspector 反射编辑 + Save | **Done** |
| **S04** | 扩展契约 + Tests（Notes roundtrip；独立 SampleDataAsset 类 Deferred） | **Done**（Notes） |
| **S05** | Design/Registry/Progress 收口 | **Done** |

## Verification

- suite `data-asset`：Create → SetNotes → Save → drop refs → Load → notes/Guid 一致。
- 回归：`asset-manager`、`smoke`。
- Editor：编译通过（CB Create / Inspector 手验可选）。

## Open Questions（已拍板）

1. Inspector 匹配：基类 **typeId == "DataAsset"**；派生 cast 扩展 Deferred。
2. Save UX：显式 Save 按钮。
3. Undo：Deferred。
4. `m_Notes`：保留。

## Acceptance Checklist

- [x] Design review complete（L2）
- [x] S01–S04 验证通过（Notes roundtrip）
- [x] Progress / Registry 同步
- [x] F08 Done 解锁已兑现

## Changelog

| Date | Note |
|------|------|
| 2026-10-08 | Draft：薄 DataAsset + 同质 AM；Import/Cook Out |
| 2026-10-08 | Self-review Ready with deferred；依赖 F08 |
| 2026-10-08 | **Done**：S01–S03 实现 + suite `data-asset`；文档重建（误清空后按 Review+实现恢复） |
