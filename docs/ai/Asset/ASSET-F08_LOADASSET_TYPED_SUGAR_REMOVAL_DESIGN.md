# ASSET-F08 — 移除 LoadAsset<T> 类型糖衣 — Design Spec

## Meta
- **ID:** `ASSET-F08`
- **Type:** Feature / Refactor
- **Complexity:** L1（调用点迁移多，但架构决策已清；非新管线）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-10-08
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F06](./ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md)（Handler 真源；曾保留「可选糖衣」）
  - [ASSET-F07](./ASSET-F07_DATA_ASSET_DESIGN.md)（**Blocked by** 本 Feature；薄 DataAsset）
- **Depends on:** ASSET-F06 Done
- **Blocks:** ASSET-F07 实现开工
- **Branch intent:** 从当前 `master` 开 `feat/asset-load-api`（名称可微调）；**先于** `feat/asset-dataasset`

## TL;DR

**问题：** F06 已废除 `LoadAsset_Impl` 特化脊柱，但保留了 `LoadAsset<T>` 糖衣。它**不参与** typeId→Handler 分派，只做 `dynamic_pointer_cast`，易造成「T 决定如何 Load」的假象；类型不符时静默 `nullptr` 且丢掉 `outError`。与后续 DataAsset「薄基类 + 派生才是真资产」叙事也不符。

**方案：** 删除 `AssetManager::LoadAsset<T>`；全仓改为 `LoadAssetByPath` / `ByGUID` / `ByMeta`；需要静态类型时用 `minEngine::Cast<T>`（`ObjectCast.h`：反射 `IsA` + `static_cast`），或已核对 `meta.AssetType` 时用 `static_pointer_cast`。可选：同步删除同族 `CreateAsset<T>`（见 Open Questions；默认 **一并删除**）。

**状态：** **Done**（`feat/asset-load-api`）。解锁 ASSET-F07。

## Problem

1. 权威 Load 已是 `LoadAssetByMeta_Internal` → `FindLoadHandler(meta.AssetType)`；`LoadAsset<T>` 是假分派入口。
2. 糖衣隐藏错误语义（cast 失败无 error string）。
3. 全仓 Editor/Runtime/Tests 仍大量调用糖衣，巩固错误心智模型。
4. ASSET-F07 若以 `LoadAsset<DataAsset>` 为「轻松用法」，会与「派生类才是有意义资产」冲突。

## Requirements

### Must

1. 从 `AssetManager.h` **删除** `template LoadAsset<T>(path)`。
2. 迁移所有引擎仓调用点到：
   - `LoadAssetByPath(path, outError)` / `LoadAssetByGUID` / `LoadAssetByMeta`，返回 `shared_ptr<Asset>`；
   - 需要具体类型时：优先 `minEngine::Cast<T>(asset)`；已核对 typeId/Handler 契约时可用 `static_pointer_cast`。失败时打日志 / 传播 `outError`（至少不比糖衣更差）。
3. 测试与文档不再把 `LoadAsset<T>` 写成推荐 API；F06 Design 中「可选糖衣」行改为 **Removed by F08**。
4. `minEngine` + `minEngineTests` 相关 suite 通过（至少 `asset-manager`、smoke，以及改动触及的 Editor 编译）。

### Should

5. 提供极薄的本地习惯（**非** AM 成员）：例如调用点旁 `as<T>(shared_ptr<Asset>)` 私有 helper——**禁止**再回到 AM 上的全局 typed Load。
6. 类型不符时优先保留 / 填写 `outError`（若调用方有该参数）。

### CreateAsset<T>（默认 In）

7. **默认一并删除** `CreateAsset<T>` 糖衣；Create 真源仅 `CreateAssetByTypeId`。理由：与 Load 同病（假分派 / 巩固错误 API）。若审批改成 Out，则 F08 仅删 Load，Create 记 Deferred。

## Constraints

- 不改 Handler 注册模型、不改 Guid/F03、不改 cache 语义。
- 不引入新的「按 C++ 类型查 Loader」机制。
- Runtime 不依赖 Editor。
- forward-only：无长期 `#define LoadAsset` 兼容层。

## Preferences

- 中文主文；标识符英文。
- 迁移时优先保留 `outError` 路径，少用「忽略 error 的糖衣替身」。

## Assumptions

- 调用点数量可控（约数十处，以 Editor 为主）；可单 PR / 单 Feature 切片完成。
- `dynamic_pointer_cast` 在 Asset 继承树上仍可接受；本 Feature 目标是去掉**误导性 API**，不是消灭一切 RTTI。

## Non-Goals

- DataAsset 类型本身（→ F07）。
- Import/Cook、PrimaryDataAsset、引用图。
- （已部分收回）薄 `Cast<T>` 已作为 Object 机制提供；**全仓**把所有 `dynamic_*_cast` 清完可后置，不挡 F08 Done。
- 改变 Automation/MCP 对外 JSON 契约（除非内部实现被迫改，行为应对调用方透明）。

## Current Architecture

```text
LoadAssetByPath / ByGUID / ByMeta
        ↓
LoadAssetByMeta_Internal
        ↓
FindLoadHandler(meta.AssetType) → Handler → shared_ptr<Asset> → cache

LoadAsset<T>(path)   // 糖衣：Internal + dynamic_pointer_cast<T>，无 outError
CreateAsset<T>(...) // 糖衣：TypeRegistry → CreateAssetByTypeId
```

## Proposed Architecture

```text
LoadAssetByPath / ByGUID / ByMeta   // 唯一公开 Load 入口
        ↓
（调用方）需要静态类型时：
  - 优先 minEngine::Cast<T>(asset)（反射 IsA + static_cast；见 ObjectCast.h）
  - 若已核对 meta.AssetType / Handler 契约，可用 static_pointer_cast
  - 避免散落 dynamic_pointer_cast（C++ RTTI；与 MEObject 图不一致）

CreateAssetByTypeId                 // 唯一公开 typed-Create 入口（按 typeId）
```

**不恢复** `AssetManager::LoadAsset<T>`：类型窄化是 Object 机制，不是 Load 分派。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `AssetManager` | 无类型 Load/Create 门面；Handler 调度 | C++ 模板类型分派 |
| 调用方 | 需要时显式 downcast；处理 nullptr / error | 再包一层全局 LoadAsset<T> |
| Tests | 覆盖 ByPath + cast 失败/成功 | 保留糖衣测例 |

## Boundaries

- **Allowed：** 文件内 / 匿名命名空间小 helper（若项目 cpp-style 允许；否则用 static 成员或直接 cast）。
- **Forbidden：** 在 AM 上恢复任何 `LoadAsset` 模板；为每个 Asset 子类加特化 Load。

## Ownership & Lifetime

Not applicable beyond existing `shared_ptr` / weak cache — 生命周期不变。

## Data Flow / Control Flow

与 F06 后相同；仅去掉糖衣这一跳。

## API / Interface Semantics

### 删除

```cpp
// REMOVE
template<typename T>
std::shared_ptr<T> LoadAsset(const std::string& path);

// DEFAULT REMOVE (Open Question)
template<typename T>
std::shared_ptr<T> CreateAsset(const std::string& assetName, const std::string& directoryRel);
```

### 保留 / 推荐

```cpp
std::shared_ptr<Asset> LoadAssetByPath(const std::string& path, std::string& outErrorMessage);
std::shared_ptr<Asset> LoadAssetByGUID(const GUID& guid, std::string& outErrorMessage);
std::shared_ptr<Asset> LoadAssetByMeta(const AssetMeta& meta, std::string& outErrorMessage);
std::shared_ptr<Asset> CreateAssetByTypeId(std::string_view assetTypeId, ...);
```

迁移示例：

```cpp
// before
auto mat = AssetManager::Get().LoadAsset<Material>(path);

// after
std::string err;
std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByPath(path, err);
std::shared_ptr<Material> mat = std::dynamic_pointer_cast<Material>(asset);
if (!mat) { /* log err or "type mismatch" */ }
```

## Failure Model

| 情况 | 行为 |
|------|------|
| path 无 meta | ByPath 失败 + outError（既有） |
| Handler 失败 | 既有 |
| Load 成功但 cast 失败 | 调用方检测；建议日志含 path + 期望类型 + `meta.AssetType` |

## Alternatives & Trade-offs

### Option A（推荐）— 删除糖衣 + 迁移调用点

- Pros：合同清晰；为 F07 铺路；去掉假分派。
- Cons：一次改较多文件。

### Option B — 仅 `[[deprecated]]` 不删除

- Pros：改动小。
- Cons：心智债务仍在；F07 仍易写错。**不选。**

### Option C — 改成「T 参与分派」的真 typed Load

- Pros：无。
- Cons：逆 F06；需要 type→handler 第二套表。**不选。**

## Integration Impact

| Area | Impact |
|------|--------|
| `AssetManager.h` | 删模板 |
| Editor / Runtime / Tests / Loaders | 调用点迁移 |
| 外仓（minCraft 等） | 若链接并调用了 `LoadAsset<T>`，需同步改（ED-F21 契约）；实现期 grep 外仓 |
| Docs | F06 / F07 / handbook 措辞 |

## Migration / Compatibility

- 引擎仓：本 Feature 内迁完。
- 外仓：开工前 `rg 'LoadAsset\\s*<'`；若有则同 PR 或联动说明。
- 无 ABI 稳定承诺（个人引擎 forward-only）。

## Implementation Slices

| Slice | Deliverable | Verify |
|-------|-------------|--------|
| **S01** | 清单：全部 `LoadAsset<` /（若 In）`CreateAsset<` 调用点 | 表格进 Impl 或 Progress |
| **S02** | 删 AM 模板；迁移 Runtime + Loaders + Tests | `asset-manager` + 相关 suite |
| **S03** | 迁移 Editor（Workflow / Thumbnail / SubEditors / Automation） | Editor 编译；触及面 smoke/手验 |
| **S04** | 外仓 grep + 必要修复；F06/F07 文档交叉更新；Registry Done | Doc DoD |

## Verification Strategy

- 编译：`minEngine`、`Editor`、`minEngineTests`。
- 测试：`asset-manager`；既有依赖 Load 的 suite（prefab / material / sprite 等相关）。
- Grep 门禁：仓内无 `LoadAsset\\s*<`（除历史文档说明「已删除」）。
- 若 Create 一并删：同样门禁 `CreateAsset\\s*<`。

## Open Questions

1. **`CreateAsset<T>` 是否同期删除？**  
   - **默认：是（In）。**  
2. **cast 失败是否强制统一 helper？**  
   - **修订：提供** `minEngine::Cast<T>`（`ObjectCast.h`，IsA + static_cast）；调用点优先用它，不恢复 AM `LoadAsset<T>`。全仓清 `dynamic_*_cast` 可后置。  
3. **外仓无调用时是否仍开独立分支？**  
   - **默认：是**；保持与 F07 分支分离，先合 master。

## Design Review

- **Verdict:** Ready with deferred items（见 Review 文档；待维护者批默认）
- **Link:** [ASSET-F08 Design Review](./ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN_REVIEW.md)

## Implementation Readiness

- [x] Requirements / Constraints / Architecture
- [x] Alternatives
- [x] Verification
- [ ] Open Questions 维护者确认

**Ready for implementation?** `yes`（条件：确认 Create 默认一并删）— 未批准前不写代码。

## Acceptance Checklist

- [x] `LoadAsset<T>` 从 AM 移除且无残留调用（仅注释/历史日志文案）
- [x] `CreateAsset<T>` 同样移除
- [x] 相关测试通过（asset-manager / smoke；实现期 Editor 编译）
- [x] F07 交叉引用已更新；`ObjectCast.h` 提供 `Cast<T>`
- [x] Registry：F08 Done；F07 可开工

## Changelog

| Date | Note |
|------|------|
| 2026-10-08 | Planned：删除 Load 类型糖衣；默认含 Create&lt;T&gt;；阻塞 F07 |
| 2026-10-08 | 补充：`ObjectCast.h` 的 `Cast&lt;T&gt;`（反射 IsA）；调用点勿散落 `dynamic_pointer_cast`；仍不恢复 AM typed Load |
| 2026-10-08 | **Done**：模板删除 + 调用点迁移 + Cast 机制；解锁 F07 |
