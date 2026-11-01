# ASSET-F10-S03 — Derived DataAsset Create (Editor / Automation)

## Meta
- **ID:** `ASSET-F10` Slice `S03`
- **Type:** `Feature`（切片设计；承接 F10 Deferred）
- **Complexity:** `L1`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-11-01`
- **Related:**
  - Parent [ASSET-F10](./ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md)（S01/S02 Done：多态 Load + `CreateDataAsset`）
  - [ASSET-F07](./ASSET-F07_DATA_ASSET_DESIGN.md)（AES/CB 基类 Create Done；派生类选择 Deferred）
  - [ASSET-F04](./ASSET-F04_CONTENT_WORKFLOW_DESIGN.md)（`asset.create` 自动化）
  - minCraft 反馈：`ENGINE_FEEDBACK_PROMPT_DATAASSET_DERIVED_LOAD.md`（Load 已 Addressed；**创建面仍缺**）
- **Depends on:** F10 S01/S02；`ReflectionSystem::GetDerivedClasses`；`AssetManager::CreateDataAsset`
- **Blocks:** minCraft 表型 DataAsset 用 MCP/CB **创建**派生实例（Load 已通）
- **Branch:** `feat/asset`（或现有 asset 区分支；勿用 ID 命名）

## TL;DR

多态 **Load / 运行时 CreateDataAsset(MEClass*)** 已落地；缺口在 **编辑器与自动化入口仍只建基类**。  
S03 把「按类创建」接到 AES / Content Browser / `asset.create`：payload 带 `runtimeClassName`，菜单用反射扫描 `IsA(DataAsset)`（排除 Abstract），**不**恢复 per-type handler / 扩展名注册。

## Intent playback（维护者可纠）

minCraft 要建 `BlockContentTable` 一类 `T : DataAsset`。引擎已能 `CreateDataAsset(T::StaticClass())` 并正确 Load，但 Content Browser「Create DataAsset」、AES `Create("DataAsset")`、MCP `asset.create { assetType: "DataAsset" }` 全部走 `CreateHandler` → `NewObject<DataAsset>`。游戏侧感觉「无法创建派生类」，实际是 **UI/自动化没接线**，不是管线再缺一套 Loader。成功标准：已知类名即可创建派生 `.medata`；人机可选类；Inspector 编辑派生字段并 Save。不做玩法表、不做每类型扩展名。

## Problem

| 层 | 现状 | 结果 |
|----|------|------|
| Runtime | `CreateDataAsset(classInfo)` + Loader 按类 `NewObject` + meta.`RuntimeClassName` | **可用**（suite / GameModuleStub 已证） |
| AES | `Create(assetType)` → `CreateAssetByTypeId` → DataAsset **CreateHandler 固定基类** | 只能建基类 |
| CB | `TryCreateDataAssetInDirectory` → AES `Create("DataAsset")` | 同上 |
| MCP | `asset.create` 仅 `assetType` / `directory` / `name` | 无法传派生类名 |
| Inspector | `AssetType == "DataAsset"` → Load → `GetClass()` 反射编辑 | 对**已存在**派生文件大致可用；创建入口仍缺 |

消费反馈文案仍写「Load 写死基类」——那一段已被 F10 S01 修掉；**今日阻塞是 Create 表面**。

## Requirements

### Must

1. **按类创建可达：** 给定已反射注册且 `IsA(DataAsset)` 的类（含基类），AES / MCP 能创建 `.medata`，且 meta.`RuntimeClassName` 与 `GetClass()->GetName()` 一致。
2. **兼容旧调用：** `assetType=DataAsset` 且**不**带类名 → 仍创建基类（今日行为）。
3. **禁止静默降级：** 类名找不到、非 DataAsset、或 Abstract → **失败**，不得建成基类当成功。
4. **零管线注册：** 不要求游戏 `RegisterType` / `Register*Handler`；不新增派生扩展名。
5. **类目录（人机）：** Content Browser Create 可列出可创建的 DataAsset 类（至少含基类 + 当前进程已加载模块中的派生类）。
6. **验证：** GameModuleStub `SampleTableDataAsset`（或等价）经 AES/`asset.create` 创建 → Save 已含 → Evict → Load → `Cast` 成功。

### Should

7. Inspector 标题/只读行显示 `RuntimeClassName`（或 `GetClass()->GetName()`），避免看起来永远是「DataAsset」。
8. MCP 响应带回 `runtimeClassName` / `path` / `guid`。
9. 能力表 / 简短契约说明：游戏模块须在创建前已加载（类已进 `ReflectionSystem`）。

### Must not

- Core 内置 Block/Item 表类型。
- `RegisterDerivedDataAsset<T>` 一类「一键注册 TypeRegistry+handlers」（F10 已否决 Option C）。
- 为每个派生类拆 `AssetTypeId` / 扩展名。
- 恢复 `CreateAsset<T>` 糖衣（F08）。
- 本期强制 ClassMetadata `Creatable` 标注（见 Alternatives；噪声大时再加）。

## Constraints

- Capabilities, not Opinions：机制是「按反射类创建 DataAsset」；可创建集合来自反射，不是引擎白名单写死游戏类名。
- `.medata` ↔ `AssetType == "DataAsset"` 不变；实例类只在 `RuntimeClassName`。
- Runtime 不依赖 Editor；类目录扫描 API 可放 Runtime（供测试），UI 在 Editor。
- 源码注释无 Feature/Slice/Bug/ADR ID。

## Preferences

- **Agent 优先接线：** 先打通 `runtimeClassName` 参数（无 UI 也能 unblock minCraft MCP）。
- 类目录默认：**反射派生扫描**；仅排除 `Abstract` 与无法 `NewObject` 的类。
- 基类保留在目录中（Notes / smoke）。

## Assumptions

- 产品 Editor 加载游戏模块后，游戏 `ME_CLASS` 已注册（与 Load 派生资产同一前提）。
- `GetDerivedClasses(DataAsset)` 含跨模块已注册类。
- 今日无大量「不可实例化却非 Abstract」的 DataAsset 派生；若出现再补 Creatable 标注。

## Non-Goals

- 异步创建；Import/Cook；表行 = 领域 Def 序列化策略定稿（仍见 F10 Open Q）。
- DataAsset 独立文档 Tab（仍 CB Inspector）。
- 修改多态 Load 语义（S01 不变）。

## Current Architecture

```text
CreateDataAsset(classInfo) → DataAssetLoader::Create
  NewObject(classInfo) → AssetType=DataAsset → RuntimeClassName=class → .medata

CreateAssetByTypeId("DataAsset") → CreateHandler → Create(..., DataAsset::StaticClass())

AES::Create(type, dir, name)          // 无类名参数
CB TryCreateDataAssetInDirectory      // 写死 "DataAsset"
MCP asset.create                      // payload 无 runtimeClassName

ReflectionSystem::GetDerivedClasses(base)  // 已有
MEClass::Abstract specifier               // 已有
```

## Proposed Architecture

两个名字继续分责（继承 F10）：

| 字段 | Create 时 |
|------|-----------|
| `AssetType` | 恒 `"DataAsset"`（管线） |
| `RuntimeClassName` | 用户/调用方指定的实例类 |

```text
[MCP / AES]
  assetType == "DataAsset"
  runtimeClassName 空 → CreateDataAsset(DataAsset::StaticClass())
  runtimeClassName 非空 → FindClass → 校验 IsA(DataAsset) ∧ ¬Abstract → CreateDataAsset(class)

[CB]
  Create → DataAsset
    → 子菜单：CollectCreatableDataAssetClasses()
    → 选中类 → CreateDataAsset(class) → 选中新资产

[Catalog]
  GetDerivedClasses(DataAsset)
    过滤：!Abstract；可 NewObject（CreateDefaultInstance 可用）
    排序：基类优先，其余按显示名
```

游戏侧契约（官方，仍零 handler）：

```cpp
// 模块已加载 → 类在反射表中
AssetManager::Get().CreateDataAsset(
    BlockContentTable::StaticClass(), "Blocks", "Assets/Content", err);

// 或 MCP
// asset.create { assetType:"DataAsset", runtimeClassName:"Game::BlockContentTable",
//                directory:"Assets/Content", name:"Blocks" }
```

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `AssetManager::CreateDataAsset` | 按类创建（已有） | UI 列表 |
| `AssetEditorServices::Create` | 解析可选 `runtimeClassName`；白名单仍是 typeId `DataAsset` | 玩法类名枚举写死在引擎 |
| `CollectCreatableDataAssetClasses` | 反射扫描 + Abstract 过滤 | 游戏模块加载 |
| CB / Automation | 调用上述 API | 自建 Loader |
| 游戏模块 | `T : DataAsset` + 模块加载使反射可见 | Register*Handler |

## Boundaries

- Allowed：Editor/MCP → AES → `CreateDataAsset`；扫描反射。
- Forbidden：引擎 Core 引用 minCraft 类型；把派生类加入 `IsCreatableAssetTypeId` 字符串表当独立 assetType。

## Dependencies

- Depends on：F10 S01/S02；Reflection `GetDerivedClasses` / `Abstract`。
- Depended on by：minCraft 内容表创建；可选后续「任意多态资产 Create」复用同一 payload 形状。

## Ownership & Lifetime

与 F07/F10 相同：创建后进 AM cache；不改 Guid/path 契约。

## Data / Control Flow

```text
Caller (MCP|CB|code)
  → resolve MEClass* (name or menu pick)
  → CreateDataAsset
  → .medata + .meta(RuntimeClassName)
  → CB select / return path+guid
```

## API / Interface Semantics

### `AssetEditorServices::Create`（扩展）

- **新增可选参数：** `runtimeClassName`（`string_view`，默认可空）。
- **当 `assetType != "DataAsset"`：** 若传入非空类名 → **InvalidArgument**（本期不推广到其它 typeId）。
- **当 `assetType == "DataAsset"`：**
  - 空类名 → 现行为（基类）。
  - 非空 → `FindClass`；失败 / 非 DataAsset / Abstract → Fail，文案区分三种原因。
  - 成功 → `CreateDataAsset(classInfo, ...)`，**不**走 `CreateAssetByTypeId`。
- **Whitelist：** 仍只允许 typeId `DataAsset`（及其他现有 creatable）；**不**把游戏类名加进 type 白名单。

### MCP `asset.create`

- Payload 增加可选 `runtimeClassName`（别名可接受 `className`）。
- Ok 结果：`path`, `guid`, `runtimeClassName`（实际写入值）。

### `CollectCreatableDataAssetClasses()`（Runtime 或 Editor 工具函数）

- **Returns：** `vector<const MEClass*>`，稳定排序。
- **Includes：** `DataAsset` 自身 + 所有非 Abstract 派生。
- **Thread：** 游戏角色 / 主线程；与反射表一致。

### CB

- 「Create DataAsset」改为：若仅一类（或产品偏好）可直接建基类；**推荐**弹出类列表（ImGui combo / 子菜单）。
- 创建失败 Toast/Log 用 AES Message。

### Inspector

- 选中 `AssetType=="DataAsset"` 保持；Load 已按 RuntimeClassName 建对类 → 反射字段自然是派生 schema。
- Should：显示类名只读标签。

## Failure Model

| Failure | Behavior |
|---------|----------|
| `runtimeClassName` 找不到 | Fail；不建文件 |
| 找到但非 `IsA(DataAsset)` | Fail |
| Abstract | Fail |
| 游戏模块未加载导致类不存在 | Fail（与 Load 缺类同形）；文档说明须先加载模块 |
| 磁盘 / Register 失败 | 与今日 CreateDataAsset 相同 |

## Alternatives & Trade-offs

### Option A（推荐）— payload 类名 + 反射扫描目录

- **Pros：** 复用已有 `CreateDataAsset`；Agent 零 UI；与 F10「零 handler」一致；`GetDerivedClasses` 已存在。
- **Cons：** 目录可能含测试/stub 类；依赖模块已加载。

### Option B — 仅 MCP/AES 类名，CB 仍只建基类

- **Why not as sole scope：** 人机仍痛；增量很小，应同切片做 CB 子菜单。

### Option C — 游戏 `RegisterCreatableDataAsset(class)` 显式目录

- **Summary：** 薄注册表只存 `MEClass*`，不注册 Loader。
- **Why not MVP：** 多一步样板；反射扫描已够。若目录噪声大，升为 Should 后续。

### Option D — ClassMetadata `Creatable=true` 才入菜单

- **Why defer：** 要改宏/生成与存量类标注；Agent 路径不需要。噪声出现后再做 opt-out/opt-in。

### Option E — 复活 per-type AssetTypeId

- **Why not：** F10 已否决；与共享 `.medata` 管线冲突。

## Integration Impact

| Area | Change |
|------|--------|
| `AssetEditorServices` | Create 签名 + 分支调 `CreateDataAsset` |
| Automation | `asset.create` payload；Capability 说明 |
| `AssetWorkflowModule` / CB action | 类选择 UI |
| 小工具函数 | `CollectCreatableDataAssetClasses` |
| Tests | AES 或 AM 级：带类名创建 SampleTable；错误类名失败；空类名仍基类 |
| F10 Design | S03 状态改为指向本文；不再「空 Deferred」 |

## Migration / Compatibility

- 旧 MCP / CB：不传类名 → 基类，行为兼容。
- 已有派生 `.medata`：仅创建面变化；Load 不变。
- minCraft：去掉「只能 create_from 基类再手改」类绕过；改用 `runtimeClassName`。

## Implementation Slices

| Slice | Deliverable | Est. |
|-------|-------------|------|
| **S03a** | AES `runtimeClassName` + MCP `asset.create` + 失败语义；单测 Create 派生 roundtrip | 小 |
| **S03b** | `CollectCreatableDataAssetClasses` + CB Create 子菜单/组合框 | 小 |
| **S03c** | Inspector 显示运行时类名；文档/Capability 文案；F10 Meta/Registry 收口 | 极小 |

建议落地顺序：**S03a → S03b → S03c**（先 unblock Agent）。

## Verification Strategy

- **Automated：**
  - `Create(..., "DataAsset", ..., runtimeClassName=SampleTableDataAsset)` → meta 类名正确 → Evict → Load → `Cast` + 行可写。
  - 错误类名 / 非 DataAsset 名 → Fail，磁盘无新文件（或失败时清理与今日 Create 一致）。
  - 省略类名 → 基类 `RuntimeClassName` 为 `DataAsset`（或引擎基类全名，与 Save 写入一致）。
- **Manual：** 产品 Editor 加载 minCraft 模块 → CB 见派生类 → 创建 → Inspector 见表字段 → Save。
- **Consumer：** minCraft MCP 建表资产；更新反馈稿状态（消费仓事务）。

## Open Questions

1. **类名字符串形态：** — **已拍板：** 与 `MEClass::GetName()` / meta.`RuntimeClassName` 一致（例 `GameModuleStub::SampleTableDataAsset`）。
2. **CB 交互：** — 已用顶层「Data Asset」子菜单列类。
3. **是否隐藏基类：** — 保留基类项（工具/笔记）。

## Design Review

- **Verdict:** `Accepted`（维护者确认后实现）
- **Ready for implementation?** Done

## Acceptance Checklist

- [x] 维护者认可 Option A（类名参数 + 反射目录）
- [x] S03a/b/c 实现 + `test data-asset` PASSED
- [x] F10 Design / Registry：S03 Done；ACTIVE_WORK 更新
- [ ] minCraft 反馈稿改为创建面 Resolved（消费仓事务）

## Changelog

| Date | Note |
|------|------|
| 2026-11-01 | Done：AES/`asset.create` `runtimeClassName`；`ResolveCreatableClass` / `CollectCreatableClasses`；CB Data Asset 子菜单；Inspector 显示类名；suite `data-asset` |
| 2026-11-01 | Draft：从 minCraft「无法创建派生 DataAsset」重读缺口——Load 已通，补齐 Create 表面；否决 handler 注册回潮 |
