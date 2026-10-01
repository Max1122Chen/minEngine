# ED-F24 — AssetEditorServices GUI 收敛 — Design Spec

## Meta
- **ID:** `ED-F24`
- **Type:** Refactor
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-10-01（S01–S06 Done）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ED-F18](./ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md)（Services = 中性原语；Surface/GUI = Client）
  - [ASSET-F04](../Asset/ASSET-F04_CONTENT_WORKFLOW_DESIGN.md)（CB Workflow / Automation 对等）
  - [ASSET-F06](../Asset/ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md)（AM Facade **Done**；§ Client Surfaces）
  - [MCP-F04](./MCP/MCP-F04_AUTOMATION_DOMAIN_EXPANSION_DESIGN.md)（`asset.*` ops）
- **Depends on:** ASSET-F06 **Done**；现有 `AssetEditorServices` + Automation Surface
- **Blocks:** 无硬挡；改善 Agent/GUI 行为同源，降低 F05 等新 Product 双入口漂移

## TL;DR

**问题：** F06 之后，MCP/`asset.*` 已正确走 `Surface → AssetEditorServices (AES) → AssetManager`，但 Content Browser / AssetWorkflow 的 Import、Create、Move、Rename、CreateFolder、DeleteFolder 等仍常**直连 AM**。底层语义今天大体一致，但校验、Create 白名单、错误形态会逐渐漂移。

**方案：** 让 AES 成为 Editor 侧**唯一资产库门面**（Automation 与 GUI 共用）。UI 继续拥有对话框/ImGui；Workflow/CB 只编排 UX，再调 AES。对齐 Create 类型集合；GUI 已有的 Reimport 补一层薄 `AES::Reimport`。

**状态：** Design **Done**（S01–S06）；GUI 库操作经 AES；grep 门禁通过。

## Problem

1. **双入口：** MCP 走 AES；GUI Import（`AssetWorkflowModule::ImportAssetDialog`）与多数 CB Mutation **直调 AM**。
2. **Create 不对齐：** AES 白名单 = Scene / Material / AnimationGraph / Sprite / SpriteAtlas。CB 还能建 **MaterialInstance**；AnimGraph 窗口用 AM 糖衣建 AnimationGraph，不经 AES。
3. **校验漂移风险：** AES Import 做路径解析、product 兼容、options 校验、Skeletal→Clip 排序。GUI Import 自实现 product 选择 + 自动/待选 UX，再调 `AM::Import`——日后 option 规则容易两边不一致。
4. **F06 Client Surfaces** 已写明 AES 是 Editor **推荐入口**；GUI 直连只是 F06 范围可接受，不是终态。

## Requirements

- 今日会改资产库的 GUI 路径——**Create / Import / Move / Rename / RenameFolder / CreateFolder / DeleteFolder / BatchRename / Delete（资产）**——必须走 AES（与 Automation 同一组函数），除非列入 Non-Goals。
- 对人与 Agent 暴露的 **Create 类型集合** 只有一份矩阵（CB 菜单 ∪ MCP 白名单 ∪ AnimGraph「新建」）；**AES 为强制执行点**。
- GUI 仍拥有：系统文件对话框、ImGui 模态框、product 选择 UI、树/列表刷新、选中态 UX。
- Automation op 名与 JSON 字段**不变**（沿用 F06 / MCP-F04 冻结）。
- 不新增 MCP 粗 Tool；不另造「仅 GUI 的 ImportService」。
- 收敛后：新增可创建类型或 Import option，应是 **AES（+ Bootstrap）一处改**，而不是 AES + Workflow + CB + Surface 各改一遍。

## Constraints

- ED-F18：AES 保持 **Editor 中性原语**（字符串/类型化资产操作 + `AssetEditorResult`）。Surface 管 JSON；ImGui 留在窗口/模块。
- 禁止把文件对话框或 ImGui 沉进 AES。
- 禁止把 Mutation/Import 算法搬进 Editor——AES 继续调 AM。
- 保留 AM 公开 API（F06）；Editor 客户端优先 AES；Engine/测试仍可直调 AM。
- 验证：编过 Editor + 既有 asset automation / CB 相关测或 smoke。

## Preferences

- 优先改调用点，不新造并行 `IAssetLibrary` 接口。
- 扩大 AES `Create` 白名单，而不是让 Workflow 为 MIC 长期保留 `CreateAsset<T>` 特例。
- 本文以中文为主；标识符/类型名保持英文。

## Assumptions

- BatchRename 已走 AES——可作为模板。
- SpriteEditorServices / AnimGraph「新建文档」可改为 AES 客户端，UX 不变。
- 只读注册表查询（Inspector 用的 `FindAssetMeta*` / `LoadAsset`）MVP 可仍直连 AM（见 Non-Goals）；AES 查询包装可后置。

## Non-Goals

- 重写 MCP 协议或 CapabilityRegistry op 集。
- 强迫所有 SubEditor Save 走 AES（Material/Sprite/AnimGraph 仍用领域 `SaveAsset` / `*EditorServices`）。
- 把 ContentMutation 实现搬进 Editor。
- 统一 Prefab Create（仍走 PrefabUtility / Scene 制作——除非日后把 Prefab CreateHandler 产品化进 CB）。
- 大改 CB UX（选中 chrome、图标等）。
- Texture F05 cook（独立 Feature）。

## Current Architecture

```text
Automation:  asset.* → EditorAutomationSurface → AES → AM
GUI Import:  CB action → AssetWorkflowModule::ImportAssetDialog → AM::Import
GUI Create:  CB action → Workflow::TryCreate* → AM::CreateAsset<T>
GUI Mutate:  ContentBrowserWindow → AM::Move/Rename/CreateFolder（直连）
GUI Batch:   Workflow::TryBatchRename → AES::BatchRename → AM   ✓ 已收敛
GUI Delete:  Workflow 待删确认 → AM::DeleteAsset / DeleteFolder
AnimGraph:   窗口 → AM::CreateAsset<AnimationGraph>
Sprite svc:  SpriteEditorServices → AM::CreateAsset<Sprite|Atlas>
```

证据锚点（示意）：
- AES Import：`AssetEditorServices.cpp` → `AM::Import`
- GUI Import：`AssetWorkflowModule.cpp` `ImportAssetDialog` → `AM::Import`
- CB Move/Rename/Folder：`ContentBrowserWindow.cpp`
- AES Create 白名单：`AssetEditorServices::Create`（无 MaterialInstance）

## Proposed Architecture

```text
                    ┌─────────────────────────────┐
                    │  Clients                    │
                    │  Surface | CB | Workflow | AnimGraph | SpriteServices
                    └─────────────┬───────────────┘
                                  │ 库级变更 / create / import
                                  ▼
                    ┌─────────────────────────────┐
                    │  AssetEditorServices (AES)  │
                    │  Create / Import / Move / … │
                    └─────────────┬───────────────┘
                                  ▼
                    ┌─────────────────────────────┐
                    │  AssetManager（F06 门面）    │
                    └─────────────────────────────┘

仅 UI（不进 AES）：
  FileDialog、product 选择 ImGui、重命名缓冲、树重建、选中态
```

### 目标调用矩阵

| 操作 | GUI 拥有者（UX） | 必须调用 |
|------|------------------|----------|
| Import（多文件） | Workflow 对话框 + product 选择 | UI **先**填好 `products[]`（可用 `AES::ListImportProducts`），再逐文件调 `AES::Import`；**禁止**对多候选源传空 products（AES 会 Fail）；不做 ImportMany |
| Create Scene/Mat/MIC/Sprite/Atlas | Workflow TryCreate* | `AES::Create` |
| Create AnimationGraph | AnimGraph 窗口 /（可选）CB | `AES::Create` |
| CreateFolder / Move / Rename | CB 模态 / 拖拽 | `AES::CreateFolder` / `Move` / `Rename` |
| RenameFolder / DeleteFolder | CB / Workflow | AES |
| Delete 资产 | Workflow | `AES::Delete(context, path)`（须带 `IEditorContext`，与 MCP 一致） |
| BatchRename | Workflow | `AES::BatchRename`（已有） |
| Reimport | Workflow | **新增** `AES::Reimport` → `AM::Reimport` |
| List / ListImportProducts | GUI 可选使用 | AES（MCP 已用） |

### Create 白名单（单一矩阵）

| assetType | CB 菜单 | MCP `asset.create` | 其他 GUI | AES Create |
|-----------|---------|--------------------|----------|------------|
| Scene | 有 | 有 | — | 有 |
| Material | 有 | 有 | — | 有 |
| MaterialInstance | 有 | **补** | — | 有 |
| Sprite | 有 | 有 | SpriteServices | 有 |
| SpriteAtlas | 有 | 有 | SpriteServices | 有 |
| AnimationGraph | 无（可后置） | 有 | AnimGraph 窗口 | 有 |
| Prefab | 无 | 无 | PrefabUtility | **无**（Out） |
| Texture2D | F05 前无 | F05 前无 | — | 随 F05 加入 |

### AES API 增量（尽量小）

| API | 变更 |
|-----|------|
| `Create` | 改为 allowlist + `CreateAssetByTypeId`（推荐）；并纳入 MaterialInstance |
| `Reimport` | **新增**薄包装：path → `AM::Reimport` + `AssetEditorResult` |
| `Import` | 保持单文件语义；GUI 自行循环。不引入 `ImportMany`（见 Q2） |

**推荐 Create 实现：**

```text
AES::Create(type, dir, name):
  if type 不在 kCreatableAssetTypeIds → Fail（白名单）
  asset = AM.CreateAssetByTypeId(type, name, dir, err)
  从 meta 填 outPath / outGuid
```

与 F06 无类型 Create 对齐；F05 给 Texture2D 开 Create 时只需往 allowlist 加一行。

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| **AES** | 资产库 Editor 原语；白名单；既有 Import 校验；统一 `AssetEditorResult` | ImGui；文件对话框；MCP JSON；Mutation 算法 |
| **EditorAutomationSurface** | 协议 ↔ AES | 再抄一份 Import 校验 |
| **AssetWorkflowModule** | 面向 CB 的工作流：对话框、product 选择 UX、删除确认、Inspector 宿主 | 库级 mutate/create/import 直连 AM |
| **ContentBrowserWindow** | 树/列表、拖拽 UX、重命名模态、刷新 | 超出「展示 AES 错误」的 move/rename 业务规则 |
| **SpriteEditorServices / AnimGraph UI** | 领域编辑；创建走 AES | 平行的创建策略 |
| **AM** | F06 门面（契约不变） | Editor 客户端策略 |

## Boundaries

- **允许：** GUI → AES → AM；Surface → AES → AM；测试 → AM 或 AES。
- **Done 后禁止：** Workflow/CB 库级 mutate/create/import 直调 AM（grep 门禁）。
- **MVP 允许例外：** Inspector/子编辑器用的 `LoadAsset` / `FindAssetMeta*` / 领域 `SaveAsset`。
- **禁止：** AES 依赖 Automation Json 类型；AES 包含 `<imgui.h>`。

## Dependencies

- 依赖：现状 AES；AM `CreateAssetByTypeId`；`IEditorContext::GetAssetEditorServices`。
- 被依赖：CB、Workflow、Surface（不变）、后续 F05 Editor 挂线。
- 新方向：GUI 资产库操作**必须**依赖 AES（Automation 已如此）。

## Ownership & Lifetime

不适用新增长期状态——主要是改调用点。AES 仍为 Editor 壳持有的进程级服务（现状）。

## Data Flow

```text
用户选文件（FileDialog）
  → Workflow 分类 product（UI）
  → 对每个：AES.Import(source, dest, products, overwrite, …)
  → AES 校验 + AM.Import
  → Workflow 打日志 / 刷新 CB 模型
```

```text
用户创建 MaterialInstance
  → Workflow.TryCreate…
  → AES.Create("MaterialInstance", dir, name, path, guid)
  → AM.CreateAssetByTypeId → MaterialInstanceLoader::CreateHandler
```

## Control Flow

- 右键菜单仍在 `ContentBrowserBuiltInActions`，目标 Workflow/CB 方法只调 AES。
- 拖拽移动：CB 算 `newPath`，调 `AES.Move`，失败展示 `AES.Message`。
- Automation 注册表不变。

## State & Invariants

- AES 库级变更成功后，Registry/文件系统不变量仍由 AM/Mutation 维护。
- **Editor 不变量：** Agent 可创建类型 ⊆ AES allowlist；CB Create 菜单 ⊆ AES allowlist。
- 多文件 Import 部分失败：逐文件 AES 结果；Workflow 汇总成功/失败数（与现行为一致）。

## API / Interface Semantics

- 同步、仅 Editor 主线程（不变）。
- 错误：始终把 `AssetEditorResult.Message` 打到日志/UI；禁止丢掉 AES 文案。
- Import `sourcePath`：AES 已支持相对/绝对解析——GUI 应直接传 FileDialog 返回的绝对路径，勿再抄一份 resolve。
- Create 默认名：Workflow 可继续用 “NewMaterial” 等；唯一性仍由 AM `AllocateUnique` 负责。

## Failure Model

| 失败 | 行为 |
|------|------|
| AES Fail | GUI 展示/记录 Message；禁止静默改调 AM 重试 |
| 多文件 Import 中途失败 | 继续后续文件（现 GUI 行为）；除非日后要求 fail-fast |
| 白名单未命中 | 失败并带上类型名（MCP 已如此） |
| Reimport 不支持该类型 | AES 透传 AM 错误串 |

## Alternatives & Trade-offs

### A（推荐）— GUI 收敛到现有 AES

- **优点：** 单一策略面；符合 ED-F18 / F06；API 增量小；F05 友好。
- **缺点：** 要改 CB/Workflow/Sprite/AnimGraph 调用点；Move 后刷新/选中需小心。

### B — 在 AES+AM 之上再抽 `AssetLibraryFacade`

- **优点：** 新名字。
- **缺点：** 多层；AES 已是 Editor 门面——**否决**。

### C — 只把校验下沉到 AM::Import，保留 GUI 双入口

- **优点：** Editor 改动少。
- **缺点：** Create 白名单与 Editor option UX 仍分裂；不解决 Agent/GUI Create 对等——**否决**为唯一方案。

**选定 A。**

## Integration Impact

| 区域 | 影响 |
|------|------|
| `AssetEditorServices` | allowlist；CreateByTypeId；可选 Reimport |
| `AssetWorkflowModule` | ImportAssetDialog / TryCreate* / Delete* → AES |
| `ContentBrowserWindow` | Move/Rename/CreateFolder → AES |
| `SpriteEditorServices` | Create → AES |
| `AnimGraphWindow` | Create → AES |
| Automation / MCP | 行为尽量不变；MIC create 变为可用（加法） |
| 测试 | accept 可测 `asset.create` MIC；Editor 资产路径 grep 禁直连 |

## Migration / Compatibility

- 仅代码改向；无资产格式变更。
- MCP：补 MaterialInstance create 为**加法**兼容。
- Create 矩阵写入本文；ED-F24 Done 时更新 F06 Client Surfaces「GUI 必须经 AES」。

## Verification Strategy

- Grep 门禁：`AssetWorkflowModule` / `ContentBrowserWindow` / `AnimGraphWindow` / `SpriteEditorServices` 库级 mutate/create/import 不得直接调用 `AssetManager::Get().(Import|CreateAsset|MoveAsset|RenameAsset|CreateFolder|DeleteFolder|DeleteAsset|Reimport)`（允许 Find/Load/Save）。
- 人手/Automation：`asset.create` MaterialInstance；`asset.import` 不变；CB 建 MIC；CB Import mesh/png；拖拽移动；重命名；建文件夹。
- 构建 `MaximumEditor` + `minEngineTests` asset-manager / prefab / smoke。

## Open Questions

| # | 问题 | 挡实现？ | 默认 |
|---|------|----------|------|
| Q1 | CB Create 菜单是否加 AnimationGraph？ | 否 | **否**（已采纳） |
| Q2 | AES 是否提供 `ImportMany`？ | 否 | **否**（已采纳）——GUI 循环 `Import` |
| Q3 | 本 Feature 是否包装 Find/Load？ | 否 | **Out**（已采纳） |
| Q4 | Sprite 面板创建也必须走 AES？ | 否 | **是**（已采纳） |
| Q5 | 做成 ASSET-F04 切片还是独立 ED-F24？ | 否 | **ED-F24**（已采纳） |

## 与 ASSET-F05 时序

- 允许与 F05-S01（Product/格式）**并行**。
- F05 在 ED-F24 S03 完成前，GUI Import 可短暂仍直连 `AM::Import`（Product 已挂则两边都能导）；**不**把这当作终态。
- F05 自动导入（S06）应经 `AES::Import`；若早于 ED-F24 S03，须在 F05 标明「过渡直连 AM」例外。

## Slice Plan

| Slice | 目标 | 验收 |
|-------|------|------|
| **S01** | AES Create → allowlist + `CreateAssetByTypeId`；加入 MaterialInstance | **Done** |
| **S02** | Workflow TryCreate* + Sprite/AnimGraph create → AES | **Done** |
| **S03** | ImportAssetDialog → AES::Import（保留对话框/选择 UX） | **Done** |
| **S04** | CB Move/Rename/CreateFolder（及 RenameFolder 若有）→ AES | **Done** |
| **S05** | Delete 资产/文件夹 + Reimport → AES | **Done** |
| **S06** | Grep 门禁 + F06/F04 短注 + Progress | **Done** |

## Design Review

- **Verdict:** Ready with deferred items
- **Reviewer / date:** agent design-review · 2026-10-01（Mode A；对话报告，未另存文件）
- **Link or summary:**
  - 采纳：Import 前必须填 products；grep 含 AnimGraph/Sprite；Delete 带 context；Q1–Q5 默认。
  - 延后：AnimGraph 进 CB、Find/Load 包装、ImportMany。
  - 与 F05：可并行 S01；GUI Import 终态等本 Feature S03。

## Implementation Readiness

- [x] Requirements 已理解
- [x] Constraints 已识别
- [x] 现有 AES / GUI 路径已核对
- [x] 职责与边界已定义
- [x] 备选方案已比较
- [x] 验证策略存在
- [x] 开放问题默认倾向已确认（Q1–Q5）
- [x] L2 design-review

**Ready for implementation?** `yes` — **S01** 可开工。

## Acceptance Checklist

- [x] Design review（L2）
- [ ] Registry / ACTIVE_WORK / Progress 同步
- [ ] GUI 库操作经 AES（Create/Import/Mutation/Delete/Reimport）
- [ ] Create 矩阵：MIC 进 MCP；AES allowlist 为单一真源
- [ ] Automation op 契约不变
- [ ] Grep 门禁文档化并通过
