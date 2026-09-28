# ASSET-F04 — Content Browser 工作流 + 资产自动化对齐 — Design Spec

## Meta
- **ID:** `ASSET-F04`
- **Type:** Feature
- **Complexity:** L2
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-28
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F02](./ASSET-F02_IMPORT_SERVICE_DESIGN.md) · [ASSET-F03](./ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)
  - Content Browser / 管线（参考，非 backlog）：[ASSET_PIPELINE_DESIGN](../Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md)
  - MCP：[MCP-F01](../Editor/MCP/MCP-F01_ARCHITECTURE_DESIGN.md) · [MCP-F04](../Editor/MCP/MCP-F04_AUTOMATION_DOMAIN_EXPANSION_DESIGN.md)
  - Sprite 域服务（已有，本 Feature **挂线**）：[SPR-F04](../Sprite/SPR-F04_SPRITE_AUTHORING_SERVICES_DESIGN.md) · [SPR-F06](../Sprite/SPR-F06_SPRITE_ATLAS_WORKFLOW_DESIGN.md)
  - 外部驱动（非权威）：minCraft `ASSET-F01_ENGINE_GAP_MCP_SPRITEATLAS_BAKE.md`
- **Depends on:** `AssetManager::{Import,DeleteAsset,MoveAsset,RenameAsset}`；`SpriteEditorServices`；`AssetEditorServices` / Automation Surface；CB 菜单 action 体系
- **Blocks:** minCraft MCP bake 全链；人手多选创建 Atlas

## TL;DR

**问题：** 资产数据 API（Import / Move / Rename / Delete / SpriteServices）大多已存在，但 Content Browser 仍是单选壳、不能建空文件夹、没有 Move UI。Automation 只暴露 `asset.list` / `create` / `delete` / `open`，没有 import / 库级 move·rename·folder，也没有 `sprite.*` Surface。minCraft 与人手 Atlas 工作流都被卡住。

**方案：** 用有序的 **SelectionSet + Primary** 统一单选/多选；补齐文件夹原语与 CB 操作面；Delete 做确认 + **尽力警告**（不做全项目引用图）；Import 用统一门面（发现元 op + `products[]` + 按需 options）；在既有粗 Tool 上扩展细 op，不新增 MCP Tool。

**当前：** **In Progress**（实现已落地，待人手/MCP 验收）— Selection·空文件夹 CRUD·拖拽 Move·批改名·Delete 确认警告·Import/sprite Automation 已接线；非空 MoveFolder Out。

## Scope

### In

| 轨 | 内容 |
|----|------|
| **L1 CB / 库原语** | SelectionSet；Create / Rename / Delete **文件夹**；Move（菜单和/或拖拽）；批量 Rename（`Foo` / `Foo_1` / …）；Delete 确认 + 尽力警告；接线 CreateAtlasFromSelection |
| **L2 Automation** | 统一 Import 门面（`list_import_products` + `import`/`products[]`）；`asset.move`/`rename`/folder；`sprite.*` Surface 挂线 |
| **验收竖切** | Agent/脚本：目录 PNG → Texture → Sprite → Atlas → Rebuild → Save |

### Out

| 项 | 说明 |
|----|------|
| **全项目资产引用图 / 硬拦截删除** | 独立大 Feature；本波 **只警告**（见 §3.5） |
| 资产级 Undo/Redo 栈 | 继续「Not a Scene undo unit」；Sprite 文档内 Undo 仍走既有 Session 栈 |
| Duplicate / 剪贴板 / Show in OS Explorer | 可记后续 slice；**不**挡本 Feature Done |
| 新 MCP 粗 Tool（如 `editor_sprite_*`） | **禁止**；只扩细 op |
| Assimp / Import product 注册模型大改 | 复用 ASSET-F02 |
| Async load / 流式资产 | Capability Roadmap 另项 |
| 复合一键 `bake_from_directory` | **可选**后置 convenience；原语优先 |

## Reader quick start

1. §0 Pre-flight · §3 选型（Selection / Rename / Delete / MCP）
2. §4 架构与职责 · §5 API / op 表
3. §7 切片 · §10 验收
4. 代码锚点：`ContentBrowserWindow`、`AssetWorkflowModule`、`AssetManager::MoveAsset`、`AssetEditorServices`、`SpriteEditorServices`、`CapabilityRegistry`

---

## 0) Pre-flight

| 项 | 判断 |
|----|------|
| **Complexity** | **L2** — 跨 CB 选中模型、文件系统文件夹、Automation 扩域；不做引用图则不必升 L3 |
| **Prerequisites** | Import / Move / Rename / Delete / CreateAsset / SpriteEditorServices：**sound**；CB 多选·文件夹·Surface sprite：**missing** |
| **Tech-debt if now** | **Medium** — 若只加 minCraft 专用 composite 绕过 Services，违反 Agent-Friendly；本设计强制同源 |
| **WIP** | ACTIVE Primary 已指向本 Feature；勿与 ED-F11 余量在同一 CB 文件无切片冲突 |
| **Philosophy** | Capabilities not opinions；文件夹与选中是机制；bake 走共享 Services + 薄 Surface |
| **Recommendation** | **Go Design** → Review → Implementation。默认切片：Selection+Folder → Move/BatchRename → Automation import+sprite → 拖拽 polish |

---

## 1) 问题

### 1.1 Pain

1. **CB 单选：** `m_SelectedAssetIndex` + `SetSelectedAsset(单指针)`；`SelectedAssets` vector 永远 ≤1 →「Create Atlas from Sprites」名存实亡。
2. **无空文件夹：** 空白 / hierarchy RMB 仅 Import / Refresh / Create *资产*。
3. **Move API 闲置：** `AssetManager::MoveAsset` 已处理 sidecar meta + Guid 不变；CB 无拖拽/菜单。
4. **Delete 门禁不对称：** Prefab 有 Instance 检查；其它类型走 `LogReferenceWarningsForDelete` stub（「scan not implemented」）后硬删。
5. **Automation 缺口：** 无 `asset.import` / move / rename / folder；`SpriteEditorServices` 未挂 Surface → Agent 无法 bake Atlas。
6. **产品定位：** 这是引擎 **Asset Workflow 能力** Feature，不是 minCraft 专用后门；minCraft 是第一条外部验收竖切。

### 1.2 成功标准

- 人手：多选 Sprite → Create Atlas；建空文件夹；Move；批量 Rename；Delete 有确认与尽力警告。
- Agent：不新增粗 Tool，仅用 `editor_asset` / `editor_edit` / `editor_query` 完成 import→sprite→atlas→rebuild→save。
- meta：Move/Rename/Delete 继续保持「文件 + `.meta` + Registry」一致；**Guid 在 Move/Rename 下不变**（已有契约，本 Feature 不得破坏）。

---

## 2) 现状架构（须尊重）

```text
ContentBrowserWindow  → 菜单 Context（SelectedAssets vector，现≤1）
                      → AssetWorkflowModule（Import dialog / Create* / Delete / Open / Reveal）
AssetManager          → Import / DeleteAsset / MoveAsset / RenameAsset / RegisterAsset
                      → sidecar path.meta；Move 同步 rename meta + WriteMetaFile；Guid 不变
AssetEditorServices   → List / Create / Delete（Automation）
SpriteEditorServices  → CreateSpriteFromTexture / CreateAtlas / AddAtlasMembers / Rebuild / Save*
CapabilityRegistry    → asset.list|create|delete|open only
```

**已拍板可复用：** MCP 粗 Tool 稳定、细 op 增长（MCP-F01/F04）；Sprite 可变编辑走 Services + Command 栈（SPR-F04）。

---

## 3) 决策（本轮讨论锁定）

### 3.1 SelectionSet（单选 = 集合 size==1）

**采用统一 SelectionSet，废除并行「单选专用」状态机。**

| 字段 | 语义 |
|------|------|
| `Entries` | **有序**列表；元素为 `Asset`（工程相对路径）或 `Directory`（相对路径） |
| `PrimaryIndex` | last-clicked；Inspector / 默认单目标 op 看 Primary |
| 存储 | 存 **path 字符串**，不长期握 `AssetMeta*`（Move/Delete 后会悬空） |

**混选策略：** 允许资产与文件夹同时出现在集合中；**危险/语义不清的 op 在混选时禁用**（Batch Rename、CreateAtlasFromSelection 仅接受「全是 Sprite 资产」）。

**菜单上下文：** 由 SelectionSet 物化。右键命中未选中项时：先将该项设为唯一选中（或加入并设 Primary），再开菜单——与常见 CB 一致。

`AssetWorkflowModule::SetSelectedAsset` → 演进为 `SetSelection` / `GetSelection`；Inspector 绑定 Primary 资产（若 Primary 是目录则清空资产 Inspector 或显示目录摘要）。

### 3.2 批量 Rename

| 规则 | 值 |
|------|-----|
| UI | **一个**输入框，输入 base **stem**（不含扩展名） |
| size==1 | 目标名 = `Foo` + 原扩展名 |
| size>1 | 按 **Selection 顺序**：`Foo`, `Foo_1`, `Foo_2`, … |
| 扩展名 | 各自保留；禁止改扩展名 |
| 作用域 | 仅 **资产**；文件夹 Rename 单独入口（单目标） |
| 冲突 | 任一目标 path 已存在 / 已注册 → **整批失败**（优先 dry-run 全检再执行） |
| 底层 | 循环 `RenameAsset` / 同父 `MoveAsset` |

### 3.3 文件夹

| API（概念） | 行为 |
|-------------|------|
| `CreateFolder(parentRel, name)` | `create_directories`；**无** `.meta`（空目录不是资产） |
| `RenameFolder` / `MoveFolder` | 文件系统 rename；刷新 CB 树；**不**改资产 Guid；移动子树内资产须更新 Registry path（见 §5.1） |
| `DeleteFolder` | 默认：**拒绝非空**；递归删属显式危险 op，本波 **Out**（除非后续 slice 明确要做） |

CB：空白 RMB + hierarchy 目录 RMB 增加 **New Folder**。

### 3.4 Move（资产）

- UI：拖拽到目录（优先）+ 可选「Move To…」；多选 Move 保持叶子文件名；目标目录须存在。
- 底层：既有 `MoveAsset`（已处理 meta sidecar + Guid + cache key）。
- 父目录不存在 → 失败（故 Create Folder 是前置）。

### 3.5 Delete 门禁（本波）

| 层级 | 本 Feature |
|------|------------|
| 确认对话框 | **In** — 显示数量与类型摘要 |
| Prefab Instance 门禁 | **保持**现有硬规则 |
| 打开中文档 | 若目标为已打开资产：提示关闭/Save；实施时对齐现有 dirty 流 |
| **尽力警告** | **In** — 替换今日 stub：对**已加载**资产 / 打开 Session 做 best-effort 列表，写入确认 UI 与日志；**不阻断**删除（Prefab 规则除外） |
| 全项目引用图 + 硬拦截 | **Out** — 独立后续 Feature |

### 3.6 MCP / Automation 克制

```text
粗 Tools（不变）: editor_query / editor_edit / editor_verify / editor_control / editor_asset
细 ops（本 Feature 增）: asset.import | asset.list_import_products | move | rename | folder.*; sprite.*
真源: AssetManager / AssetWorkflowModule / SpriteEditorServices
```

- **不**为 Sprite/Atlas 新增粗 Tool。
- 库级变更 → kind `asset`。
- 打开文档后的字段编辑 → kind `edit` + session（对齐 Material）。
- 无 UI 的 Rebuild/Save bake：可为 `asset` 或 `edit`（Impl 定一种并写清 capability）。
- Padding / PaddingMode：优先薄封装 `sprite.set_atlas_padding`（调 `SpriteAtlas::SetPadding` / `SetPaddingMode`），避免 Agent 依赖不稳定的通用 reflect set。

### 3.7 统一 Import 门面（一对多 / 一次多产物）

**目标：** 日后所有外部源导入共用**同一套第一层契约**；内部 wizard / cook 可因 Product 不同而异，但 GUI Dialog、Automation、MCP **不**各自发明第二套参数面。

**与 ASSET-F02 关系：** 继续以 `ImportProduct` 注册表 + `AssetManager::Import` 为真源。本 Feature 补齐：**发现（meta）**、**显式 Product 选择**、**单次请求多 Product**，以及 Automation 挂线。不重做 Assimp cook。

**第一层契约（概念）：**

```text
# 发现（query）— Agent / UI 共用
asset.list_import_products
  in:  sourcePath | extension
  out: products[] {
         productId, displayName,
         requiredOptions[] { key, type, meaning },
         optionalOptions[] { key, type, meaning },
         notes?   # e.g. SkeletalMesh creates buddy Skeleton
       }

# 执行（asset）— 唯一导入入口
asset.import
  sourcePath, destDirectory
  products: [ { productId, options?{...} }, ... ]   # >=1
  overwrite?: bool
```

**选择语义：**

| 调用方意图 | 填法 |
|------------|------|
| 只导 StaticMesh | `products: [{ productId: "StaticMesh" }]` |
| 只导 SkeletalMesh | `products: [{ productId: "SkeletalMesh" }]`（现状会附带 buddy Skeleton，发现结果应注明） |
| 只导 AnimationClip | `products: [{ productId: "AnimationClip", options: { skeletonAssetPath } }]` |
| 一口气 SkeletalMesh + Clip | `products: [ {SkeletalMesh}, {AnimationClip, options:{...}} ]`；引擎按**依赖序**执行（先 mesh/skeleton，后 clip） |
| PNG→Texture | `products: [{ productId: "NativeCopy" }]`；若兼容产物唯一，允许省略 `products` 并由 Host 自动填（对齐人手自动导入） |

**Options 不进粗 Tool 固定 schema：**
MCP 粗 Tool 仍只带 `domain`/`action`/通用 payload。具体 keys 由 **`list_import_products` 描述**，调用方按需填入 `options` 字典；Host 按该 Product 的 required/optional **校验**，未知 key **拒绝**。禁止把 Skeleton/AnimationIndex/未来所有旋钮一股脑写进 Tool JSON Schema。

**内部 wizard：** Dialog / 未来 Settings 面板 = 同一门面的 UI 适配器（多选 Product、为需 Skeleton 的项填 options）。Automation 无 Dialog。

**后置（不挡本波）：** 命名 Bundle/Preset（如 `SkeletalWithClips` 展开为 products[]）——仅糖衣，第一层仍是 `products[]`。

---

## 4) 方案架构

```text
                    ┌─────────────────────────────┐
                    │  ContentBrowser SelectionSet │
                    │  (ordered + Primary)         │
                    └─────────────┬───────────────┘
                                  │
          ┌───────────────────────┼───────────────────────┐
          ▼                       ▼                       ▼
   AssetWorkflowModule    ContentBrowser UI         Automation Surface
   (确认 / 对话框)         (多选 / 拖拽 / 菜单)       (JSON ↔ 路径)
          │                       │                       │
          └───────────────────────┴───────────┬───────────┘
                                              ▼
                              AssetManager / SpriteEditorServices
                              (库与域原语；meta/Guid 契约)
```

### 4.1 职责

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `ContentBrowserSelection`（名实施可调） | 有序集合、Primary、Ctrl/Shift、菜单物化 | 执行 Import/Delete |
| `ContentBrowserWindow` | 绘制、拖拽、inline rename UI、手势 | 直接改 Registry |
| `AssetWorkflowModule` | 确认框、批改名规则、CreateFolder 编排、CreateAtlasFromSelection | Pack 算法；MCP JSON |
| `AssetManager` | 文件+meta+Registry；Move/Rename/Delete；Import | ImGui；引用图索引 |
| `AssetEditorServices` | Automation 库级 op | Sprite pack |
| `SpriteEditorServices` | Sprite/Atlas 域原语（已有） | 新 Tool；解析 MCP payload |
| Automation Surface | kind/op、JSON↔路径、capability | 领域规则二次实现 |
| MCP Server | 粗 Tool 转发 | 业务逻辑 |

### 4.2 边界

- **允许：** GUI / Automation → Workflow 或 *EditorServices → AssetManager / SpriteServices。
- **禁止：** MCP/minCraft 直接改磁盘绕过 Registry；Surface 内复制 Pack；为 Agent 单独造第二套 CreateAtlas。
- **文件夹：** 非资产；不进 `AssetRegistry` 为「Folder 类型」；仅文件系统 + CB 树模型。

### 4.3 所有权与生命周期

- SelectionSet：Editor / ContentBrowserModule 拥有；项目关闭时清空。
- 资产 Guid / meta：仍由 AssetManager 拥有（ASSET-F03 契约不变）。
- 批操作：同步、主线程；失败模型见 §6。

---

## 5) API / 接口语义

### 5.1 Runtime / Editor（概念契约）

```text
# Selection（Editor）
SetSelection(entries[], primaryIndex)
GetSelection() → ordered entries + primary
ClearSelection()

# Folders（AssetManager 或薄 FolderOps；落点唯一，避免两套）
CreateFolder(parentRel, name) → ok | error
RenameFolder(oldRel, newName) → ok | error   # 同父
MoveFolder(oldRel, newParentRel) → ok | error  # 若纳入 In
DeleteFolder(rel, { requireEmpty: true }) → ok | error

# 资产（已有，CB/Automation 接线）
MoveAsset / RenameAsset / DeleteAsset / Import
```

**MoveFolder 注意：** 子树内每个已注册资产必须更新 Registry path（等价于对每个资产 `MoveAsset`，或一次「目录前缀重键」事务）。须保证 **Guid 不变** 与 meta sidecar 跟随。若第一波成本高，可 **S01 只做空目录 Create + 空目录 Rename/Delete**，非空 MoveFolder 放到后续 slice（见 Q1）。

### 5.2 批量 Rename（Workflow）

```text
BatchRenameAssets(orderedPaths[], baseStem) →
  dry-run 冲突检查 → 顺序 RenameAsset
```

### 5.3 Delete 警告（Workflow）

```text
CollectDeleteWarnings(paths[]) → Warning[]
  - Prefab：沿用硬失败路径（可不进「仅警告」列表）
  - 其它：扫描已加载 ObjectPtr / 打开 Session 等 best-effort
ShowDeleteConfirm(paths, warnings) → user bool
```

### 5.4 Automation ops（草案表）

| op | kind | 行为 |
|----|------|------|
| `asset.list` | query | 已有 |
| `asset.create` | asset | 已有；白名单可含 Sprite/SpriteAtlas |
| `asset.delete` | asset | 已有；确认由 Agent 负责，Host 仍执行门禁 |
| `asset.open` | control | 已有 |
| `asset.import` | asset | 统一门面：`sourcePath` + `destDirectory` + `products[]`（每项 `productId` + 按需 `options`）；见 §3.7 |
| `asset.move` | asset | oldPath → newPath |
| `asset.rename` | asset | path + newFileName |
| `asset.create_folder` | asset | parent + name |
| `asset.rename_folder` / `asset.delete_folder` | asset | 见 §3.3 |
| `sprite.create_from_texture` | asset 或 edit | 封装 `CreateSpriteFromTexture` |
| `sprite_atlas.create` | asset | 空 Atlas（若与 `asset.create` 重叠则避免重复） |
| `sprite_atlas.add_members` | edit/asset | `AddAtlasMembers` |
| `sprite_atlas.set_padding` | edit/asset | padding + mode |
| `sprite_atlas.rebuild` | edit/asset | 需 RHI（Editor 进程） |
| `sprite_atlas.save` / `sprite.save` | edit/asset | `SaveAtlas` / `SaveSprite` |

粗 Tool：**不增名**。更新 `editor_asset` / `editor_query` 描述以涵盖 import 发现与执行。`sprite_*` 挂 `editor_edit` 或 `editor_asset`（Impl 定一种，capability 可查询）。
**禁止**为 FBX/Skeleton 等另开粗 Tool 或把全部 import 旋钮写进固定 Tool schema。

---

## 6) 失败模型

| 失败 | 行为 |
|------|------|
| Move/Rename 目标已存在 | 失败，不覆盖；批操作在 dry-run 后整批取消 |
| Move 改扩展名 | 保持现有拒绝 |
| Delete Prefab 仍有 Instance 且未 unpack | 硬失败 + 消息 |
| Delete 其它有警告 | 列出警告；用户/Agent 仍可删 |
| Import：无兼容 Product | 错误返回；可附扩展名提示 |
| Import：多兼容 Product 且未提供 `products` | 错误返回，并列出候选 `productId`（禁止静默默认 StaticMesh） |
| Import：`productId` 非法 / 与源不兼容 | 错误返回 |
| Import：缺 required option（如 Clip 缺 Skeleton） | 错误返回，指出缺失 key |
| Import：多 Product 批中一项失败 | 优先 dry-run 全检后再执行；执行中失败见 Q5 |
| Rebuild 无 RHI / Pack 失败 | 返回错误；资产保持重建前（SPR-F02） |
| 混选触发 Batch Rename | CanExecute false |
| 非空目录 Delete | 默认拒绝 |

---

## 7) 备选与权衡

### Selection

| 选项 | 说明 | 结论 |
|------|------|------|
| **A. SelectionSet + Primary** | 单/多统一 | **采用** |
| B. 单选与多选两套 API | 菜单与 Automation 分叉 | 拒绝 |

### 批量改名编号

| 选项 | 说明 | 结论 |
|------|------|------|
| **A. Foo, Foo_1, Foo_2** | 单选无后缀 | **采用** |
| B. 全员 Foo_0… | 更机械 | 不选 |
| C. 模板 `Foo_{n}` | 灵活 | 后置 |

### Delete

| 选项 | 说明 | 结论 |
|------|------|------|
| **A. 确认 + 尽力警告** | 不建引用图 | **采用** |
| B. 硬拦截未解析引用 | 需索引子系统 | **Defer** 独立 Feature |
| C. 仅日志无 UI | 人手不可见 | 拒绝 |

### MCP

| 选项 | 说明 | 结论 |
|------|------|------|
| **A. 扩细 op** | 粗 Tool 稳定 | **采用** |
| B. 每域新 Tool | Tool 爆炸 | 拒绝 |
| C. 仅 composite bake | 快但不可组合 | 可后置，不替代原语 |

---

## 8) 集成影响

| 模块 | 影响 |
|------|------|
| `ContentBrowserWindow` / Module | 选中模型重做；拖拽；文件夹菜单 |
| `AssetWorkflowModule` | Selection API；BatchRename；DeleteConfirm；Folder 编排 |
| `AssetManager` | Folder helpers；强化 delete 警告为可查询列表 |
| `AssetEditorServices` + Surface + CapabilityRegistry | 新 op |
| `SpriteEditorServices` | 可能加 SetPadding 薄封装 |
| MCP Server `register.ts` | 更新 Tool 描述字符串；默认 **不加** 新 Tool 名 |
| minCraft | 可去掉 solid-strip 临时方案；改走引擎 bake op |

**兼容性：** forward-only；无义务保留旧 Selection API。  
**Undo：** 库操作仍非 Scene undo unit。

---

## 9) 开放问题

| # | 问题 | 是否挡实现？ | 默认倾向 |
|---|------|--------------|----------|
| Q1 | 非空目录 MoveFolder 是否本 Feature In？ | 否 | S01 只做空文件夹；非空 MoveFolder 随后 slice |
| Q2 | `sprite_*` 挂 `editor_edit` 还是 `editor_asset`？ | 否（Impl 前定） | 需 Session 的 mutate → edit；无 Session 的 bake 竖切 → asset |
| Q3 | Delete 警告最小扫描范围？ | 否 | 已加载资产 + 打开文档 Session |
| Q4 | 是否做可选 composite `sprite_atlas.bake_from_directory`？ | 否 | **后置**；先验收原语 |
| Q5 | Import 多 Product 批失败时是否回滚已成功项？ | 否（Impl 前定） | 优先 dry-run 全检；失败则不开始写盘；执行中失败则报已创建路径（完整事务后置） |
| Q6 | 命名 Bundle（如 SkeletalWithClips）本波是否 In？ | 否 | **Out**；本波只用显式 `products[]` |

---

## 10) 验证策略

| 项 | 方式 |
|----|------|
| Selection 单/多 / Primary | 编辑器手验 + 可选 automation |
| 建空文件夹且树可见 | 手验 |
| MoveAsset + meta Guid 不变 | 扩展 `minEngineTests` asset 测 |
| 批改名冲突 → 整批失败 | 单测 |
| Delete 确认含警告文案 | 手验；Prefab 硬门禁回归 |
| `list_import_products` + `import`（PNG；FBX 歧义失败；显式 Product 成功）→ sprite → atlas bake | MCP/Automation 竖切 |
| 粗 Tool 数量 | 审查：无新增 `editor_sprite*` Tool |

---

## 11) 切片计划（草案；Impl 文档细化）

| Slice | 目标 | 验收 |
|-------|------|------|
| **S01** | SelectionSet + Ctrl/Shift 多选 + 菜单物化 | 多选高亮；CreateAtlasFromSelection ≥2 Sprite |
| **S02** | Create / Rename / Delete **空**文件夹 | 空白与 hierarchy RMB |
| **S03** | Move UI（拖拽或菜单）+ 多选 Move | 文件+meta+Guid |
| **S04** | Batch Rename UI + 规则 | Foo / Foo_1…；冲突整批失败 |
| **S05** | Delete 确认 + 尽力警告 API | 非 Prefab 可删且见警告 |
| **S06** | Automation：统一 Import 门面（发现 + `products[]`）/ move / rename / folder | MCP 或 pipe；含 FBX 歧义失败用例 |
| **S07** | Automation：`sprite*` / atlas rebuild+save 挂线 | minCraft bake DoD |
| **S08**（可选） | 非空 MoveFolder / composite bake / OS Reveal | 不挡 Feature 主 Done |

建议主线顺序：**S01→S02→S06→S07**（先解锁 Agent + 多选 Atlas），**S03–S05** 并行或紧随。

---

## Design Review

- **Verdict:** Pending
- **Reviewer / date:**
- **Link or summary:**

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined（op 表草案；MoveFolder 深度见 Q1）
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [ ] Unresolved questions resolved or explicitly deferred/accepted（Q1–Q4 默认倾向已写，待 Review 确认）

**Ready for implementation?** `yes` (accepted defaults Q1–Q6; implementation landed).

## Acceptance Checklist

- [ ] Design review complete（L2）
- [ ] Implementation plan traces to design decisions
- [x] Progress log updated when status changes
- [x] Feature registry status synced
- [x] CB：多选、空文件夹、Move、批改名、Delete 警告
- [x] Automation：import + sprite/atlas bake 竖切（ops 已挂；MCP 粗 Tool 未增）
- [x] 无新增 MCP 粗 Tool；无破坏 Move Guid/meta 契约
