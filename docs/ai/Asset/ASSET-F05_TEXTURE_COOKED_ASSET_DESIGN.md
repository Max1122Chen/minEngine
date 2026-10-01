# ASSET-F05 — Texture2D 引擎侧 Cook 资产 — Design Spec

## Meta
- **ID:** `ASSET-F05`
- **Type:** Feature
- **Complexity:** L2
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-10-01（design-review 修订）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F02](./ASSET-F02_IMPORT_SERVICE_DESIGN.md) · [ASSET-F03](./ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)
  - [ASSET-F04](./ASSET-F04_CONTENT_WORKFLOW_DESIGN.md) · [ASSET-F06](./ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md)（**Done**）
  - [ED-F24](../Editor/ED-F24_ASSET_EDITOR_SERVICES_GUI_CONVERGENCE_DESIGN.md)（GUI/MCP 经 AES；Create allowlist 可挂 Texture2D）
  - [RND-F22](../Render/RND-F22_TEXTURE_SAMPLER_SEPARATION_DESIGN.md)（Sampler 已分离；本 Feature 不重做）
  - [SPR-F02](../Sprite/SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)（曾提 `.metex` Deferred）
  - 外部驱动（非权威）：minCraft 灰度通道反馈
- **Depends on:** **ASSET-F06 Done**（RegisterLoad/Save/Create/ImportProduct）；ImageLoader；Texture2D / RHI；F03 Guid
- **Blocks:** 稳定灰度 Color 导入；SpriteAtlas 与 Texture 共用像素合同；后续压缩/mip cook

## TL;DR

**问题：** Texture2D 身份几乎等于 Content 里的 PNG/JPG（NativeCopy）；无导入设置、无 cook 产物、无 Reimport。1 通道灰度按 `RED` 上传后被当 RGB 采样 → 发红。

**方案：** 将 Texture2D 提升为引擎 cook 资产 **`.metex`**（+ `.meta` Guid）。源图可追溯（`SourcePath`）；Content 身份是 `.metex`（设置 + 已 cook 像素）。Load **只读** `.metex`。通过 F06 钩子注册：`Texture2DImporter` Product + Loader 的 Save/Create/Load。Inspector / `asset.import` options 驱动 Recook。

**状态：** Design **Planned**；design-review **Ready with deferred**。S01 前须拍板 `.metex` 容器与 NativeCopy 互斥验收；自动导入入口见下。

## Problem

1. `AssetTypeRegistry` 将 Texture2D 绑在 `.png/.jpg/.jpeg`；导入 = **NativeCopy**。
2. `Texture2DLoader::LoadFromAssetMeta` → `ImageLoader` → `CreateFromPixels`（`channels==1` → `TextureFormat::RED`）。
3. 无 Texture 的 `SourcePath` / Reimport；无 SaveHandler / CreateHandler / ImportProduct `Texture2D`。
4. Sprite / Material 已是 `.me*` 引擎文件；Texture 是例外。
5. SpriteAtlas Rebuild 仍假定成员磁盘路径是图片文件。

## Requirements

- Texture2D 的 **Content 身份** = `.metex`（+ sidecar `.meta` Guid；F03 不变）。
- 导入 PNG/JPG：源可追溯 + cook 出 `.metex`（设置 + 像素）；**不再**把 NativeCopy 当资产身份。
- 通道语义（名称可微调）：
  - **Color**（默认）：1ch → 扩为 **RGB8** 且 R=G=B；颜色采样不发红。
  - **SingleChannel / Mask**：保留 R8；着色器读 `.r`。
- 设置落在 `.metex`；Inspector 改 → Recook；Automation 经 `asset.import` options / 既有 edit 面（不新增 MCP 粗 Tool）。
- Load：仅从 `.metex` 建 `Texture2D` + GPU；运行时不要求源 PNG 存在。
- Authoring 像素 API 供 Atlas / 预览 / Recook；**禁止**假定 `meta.AssetPath` 扩展名是图片。
- 3/4 通道既有视觉默认不变；Mask 路径可测。
- 存量 Content PNG+meta 有迁移策略（见 Migration）。
- 可选：监视 Content 树新增 raw 图 → 同一 Importer（默认开，可关）。

## Constraints

- F03：单对象、Guid 稳定；Load 不换身。
- 不引入完整 UE DerivedData / 多平台 cook 矩阵（L2 Out）。
- Core 不出现 minCraft / biome 类型。
- 不新增 MCP 粗 Tool；用 `asset.import` / 既有 edit。
- ED-F24 落地后 Editor create/import 优先走 AES；F05 **不得**再造第三条 Editor Import 入口。
- 验证：单测或 smoke（WORKFLOW_PROFILE `tests-required`）。

## Preferences

- 扩展名：`.metex`（SPR-F02 意向）。
- ProductId：`Texture2D`。
- 图片扩展名退出 NativeCopy Accepts（或 NativeCopy 拒绝图片并提示改用 Texture2D）。
- 本文中文主文；标识符英文。

## Assumptions

- 正常 Editor Load 需要 RHI；无头测可只验 cook 字节。
- MVP cook = 未压缩 LDR；ASTC/BC Deferred。
- HDR / 等距柱状图等特殊路径 Deferred。

## Non-Goals

- 全项目引用图 / 依赖分析。
- 自动识别「草遮罩 vs 反照率」。
- Biome / 顶点色 tint 玩法。
- 完整压缩格式表、流式 mips、VT。
- 一次强制删光所有 Content PNG（允许过渡）。
- 本期必须把 SpriteAtlas 生成页落成 `.metex`（API 预留即可；页 cook Out）。

## Current Architecture（F06 之后）

```text
PNG/JPG --NativeCopyImporter--> Content/*.png + .meta
                                      |
                                 LoadHandler_Texture2D
                                      |
                                 Texture2DLoader::LoadFromAssetMeta
                                      |
                                 ImageLoader --> CreateFromPixels --> Texture2D(RHI)

Bootstrap 现状：
  RegisterLoadHandler("Texture2D", ...)
  RegisterImportProduct(NativeCopy)  # 含 png/jpg 等
  # 无 SaveHandler / CreateHandler / Texture2D Product
```

F06 已提供扩展点，F05 **无需**改 AM 算法大文件即可挂接（Reimport 类型分派等薄改除外）：

| 钩子 | Texture 今天 | F05 目标 |
|------|--------------|----------|
| `RegisterLoadHandler` | 有 | Load **cooked** `.metex` |
| `RegisterSaveHandler` | 无 | 有——写 `.metex` |
| `RegisterCreateHandler` | 无 | 可选空/默认 Texture Create |
| `RegisterImportProduct` | 仅 NativeCopy | ProductId `Texture2D` → `Texture2DImporter` |
| `Reimport` | 不支持 | 有 SourcePath 的 Texture2D 分支 |

## Proposed Architecture

```text
源 PNG/JPG（Sources 或 Content 下 raw，未注册）
        │
        ▼ Import / Recook / AutoImport
 Texture2DImporter  （解码 → 应用设置 → 写 .metex）
        │
        ├─ Content/Foo.metex          （设置 + cooked 像素）
        ├─ Content/Foo.metex.meta     （Guid，AssetType=Texture2D，SourcePath）
        └─ AssetRegistry
        │
        ▼ Load
 Texture2DLoader::LoadFromAssetMeta → Texture2D + RHI
        │
        ▼ Authoring
 TryGetCookedPixels → SpriteAtlas / 预览 / Recook 校验
```

### Bootstrap 注册（规范）

```text
RegisterLoadHandler("Texture2D", LoadHandler_Texture2D)      # 已有；实现改读 .metex
RegisterSaveHandler("Texture2D", Texture2DLoader::SaveHandler)
RegisterCreateHandler("Texture2D", Texture2DLoader::CreateHandler)  # 若本期做空创建
RegisterImportProduct({
  ProductId = "Texture2D",
  AcceptsSourceExtension = Texture2DImporter::Accepts...,
  Import = Texture2DImporter::Import,
  ...
})
# NativeCopy：停止接受 .png/.jpg/.jpeg
```

### 磁盘形态

| 文件 | 内容 |
|------|------|
| `Foo.metex` | Cook 文档：导入设置 + 像素布局 + **已 cook 像素** |
| `Foo.metex.meta` | AssetMeta：类型 Texture2D、Guid、SourcePath → 源图 |
| 源图 | 可编辑；**不是** Content 资产身份 |

**`.metex` 逻辑布局**（Impl 二选一；验收 = Load 不读 PNG）：

```text
Header/JSON:
  version, width, height
  cookedFormat          # RGB8 / RGBA8 / R8
  channelSemantics      # Color | SingleChannel
  sRGB                  # Color 默认 true；Mask 默认 false
  wrapU/V, filter       # 与 RND-F22 Sampler 字段对齐的最小集
  sourceHash?           # 可选脏检测

Payload:
  cookedPixels[]        # row-major，与 cookedFormat 一致
```

实现默认倾向：单文件（JSON 头 + raw/base64）**或** JSON + `Foo.metex.bin`——Impl Plan 选定。

### 通道语义合同

| Semantics | 1ch 源 | 3/4ch 源 | 采样 |
|-----------|--------|----------|------|
| **Color**（默认） | 扩为 **RGB8**，R=G=B（MVP 固定；不默认 RGBA8） | 保持（必要时补 A） | `tex.rgb` 为颜色 |
| **SingleChannel** | cook 为 R8 | 取 R（或选项）→ R8 | 只读 `.r` |

### Import Product

- ProductId `Texture2D`；Accepts `.png/.jpg/.jpeg`。
- 流程：保留/拷贝源 → cook → `DestDirectory/Name.metex` → Register + SourcePath（对齐 mesh）。
- `asset.list_import_products` / `asset.import`：暴露 `channelSemantics`，可选 `sRGB` / wrap / filter。
- Editor 入口经 AES（ED-F24）；GUI **不得**直调 Importer。
- **NativeCopy 互斥（S01 可测合同）：** `NativeCopyImporter::AcceptsSourceExtension` 对 `.png/.jpg/.jpeg` 返回 false；`ListImportProducts("x.png")` **仅**含 `Texture2D`。

### Save / Create / Recook

| 操作 | 行为 |
|------|------|
| **Save** | 经 SaveHandler 把当前设置 + cooked 缓冲写入 `.metex` |
| **Create** | MVP **支持**：极小默认像素（如 1×1 Color），便于日后 AES allowlist；也可先 Import-only——**默认：支持 Create** |
| **Recook / Reimport** | **同一路径**：`AM::Reimport`（及 Importer 内部 helper）读 SourcePath + 当前 `.metex` 设置 → cook → 替换 payload → 刷新同 Guid `Texture2D`。Inspector「Apply」= 持久化设置后走该路径。 |
| **Reimport（对外）** | Editor/AES 只暴露 Reimport；不另造平行 Recook API |

### Authoring 像素 API

```text
TryGetCookedPixels(texture|assetPath) → width, height, format, bytes 视图
```

- Atlas Rebuild / 预览 / Recook 校验走此 API。
- **禁止：** 仅靠 `ImageLoader(meta.AssetPath)` 作为 Atlas 唯一路径。

## Naming

F06 之后 Mesh/Clip 已用 `Importers/*` + `RegisterImportProduct`。Texture 同模式：

| 名称 | 职责 |
|------|------|
| `Texture2DImporter` | Product Import + cook/recook 辅助 |
| `Texture2DLoader` | Load / SaveHandler / CreateHandler |
| ProductId `Texture2D` | Bootstrap 注册键 |

不引入全局 ImportService 类型。`ImageLoader` 只是解码后端，不是资产身份。

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `Texture2DImporter` | 解码源、应用语义、写/读 cook、Recook | MCP JSON；ImGui；AES 编排 |
| `Texture2DLoader` | Load `.metex`→Texture2D+RHI；Save/Create；迁移期 legacy PNG Load | 猜测用途 |
| `AssetManager` | Register* 分派；Reimport 入口；Mutation | 像素算法 |
| `AssetEditorServices` | Editor Import/Create 入口（ED-F24 后） | cook 格式细节 |
| 自动导入（Editor） | 监视 Content 新 raw 图 → debounce → 同一 Importer | `RegisterAsset(png)` |
| `AssetMeta` | Guid、路径、类型、SourcePath | 通道语义正文（在 `.metex`） |
| `Texture2D` | 运行时 RHI + 可选缓存 cook 描述 | 把源 PNG 路径当身份 |
| SpriteAtlasPacker | 经像素 API 取源 | 以 ImageLoader(PNG path) 为唯一路径 |

## Boundaries

- **允许：** Import/Recook/Load/Atlas → Importer/Loader → AM；Editor 客户端 → AES → AM。
- **禁止：** 迁移窗口结束后游戏/MCP 仍把 Content PNG 当 Texture 身份；Surface 内复制 cook；Core 出现 minCraft 类型；对 raw png 做 `RegisterAsset` 当 Texture2D。
- **设置真源：** `.metex` body；`.meta` 仅身份 + SourcePath。
- **自动导入：** 同一 Importer；默认开；永不把 raw 注册为 Texture。

## Dependencies

- 依赖：F06 Register*；ImageLoader；RHI；RND-F22 sampler 字段；F03 Guid。
- 软依赖：ED-F24（GUI/MCP 单入口）。F05 可先落 Product/Loader；CB Import 在 ED-F24 S03 前可能仍双入口。
- 被依赖：Sprite、SpriteAtlas、Material 贴图、UI Image、minCraft atlas bake。

## Ownership & Lifetime

- Guid / Registry：AM（F03）。
- 磁盘 cooked：Importer/Loader 写入；Load 后 GPU 由 `Texture2D` 持有。
- 源文件：工程树；删 `.metex` 不自动删源（可警告）。
- Recook：同路径 + 同 Guid 对象更新。

## Data Flow

```text
源 PNG → 解码 → 应用 ChannelSemantics/sRGB → cooked 缓冲 → 写 .metex → Register
Load: 读 .metex → Texture2D 字段 → RHICreateTexture2D
改设置 → Recook(SourcePath) → 替换 payload → 刷新 RHI（同 Guid）
```

## Control Flow

- GUI Import / `asset.import` → Product `Texture2D`（ED-F24 Done 后经 AES）。
- Inspector Apply → Recook → SaveHandler / 文件替换。
- `AM::Reimport` → Texture2D 分支。
- Atlas Rebuild → `TryGetCookedPixels`（仅迁移期允许 legacy fallback）。

## State & Invariants

- 新 Texture2D 规范扩展名：`.metex`。
- Recook / 迁移过程 Guid 不变。
- Color 1ch cook：通道数 ≥ 3 且 R=G=B（可测）。
- SingleChannel cook：单通道；文档标明读 `.r`。
- 无 SourcePath 时 Recook 失败并给出明确错误。

## API / Interface Semantics

### Import

- `productId: "Texture2D"`
- options：`channelSemantics`（默认 Color），可选 `sRGB`、`wrap`、`filter`
- 输出：`.metex` 路径 + Guid

### Recook / Reimport

- 输入：已注册 Texture2D 路径或 Guid
- 读 SourcePath + 当前设置（options 可覆盖）
- 失败：不破坏 Guid；写临时再替换 **或** 明确回滚（Impl 选一种）

### Load

- 新资产仅 cooked；legacy PNG Load 仅迁移期

### 像素 API

- 只读视图；所有权在 API 上写明

## Failure Model

| 失败 | 行为 |
|------|------|
| 源缺失/损坏 | Import/Recook 失败；保留旧 `.metex` |
| 未知 semantics | 拒绝 |
| 硬切后仍 Load legacy PNG | 明确错误 + 迁移提示 |
| 无 RHI 的 Load | 测试可只解析 cook；Editor 正常路径要 RHI |

## Alternatives & Trade-offs

| 方案 | 结论 |
|------|------|
| **A** `.metex` = 设置 + cooked 像素 | **选定**——运行时不依赖 PNG |
| **B** 仅 meta 设置，Load 仍解 PNG | 不满足 Done 标准——否决 |
| **C** 仅 GPU swizzle（无 cook 文件） | 不可作唯一方案——否决；可后置优化 |

## Integration Impact

- `AssetTypeRegistry`：Texture2D 扩展 → `.metex`；对话框更新。
- NativeCopy：去掉图片 Accepts（或互斥拒绝）。
- CB 缩略图：经 cook/Load 预览，不假定 png 扩展名。
- GuidRef：迁移 **保留 Guid**。
- ED-F24：CreateHandler 就绪后把 `Texture2D` 加入 AES allowlist。
- minCraft：重导/迁移块贴图以修复灰度 Color。

## Migration / Compatibility

| 阶段 | 行为 |
|------|------|
| **S01–S02** | 新导入只产 `.metex`；Load **仍接受** legacy Content PNG（**警告**） |
| **S05** | 迁移命令/工具：PNG+meta → `.metex`，设 SourcePath，**保留 Guid**，更新 Registry 路径 |
| **S06+** | 文档宣布 legacy Load Deprecated；硬删留给后续若需要 |

兼容承诺：迁移工具保留 Guid；不承诺永久双路径。

## 自动导入（监视 Content）

**目标：** 用户可不先学 Sources，直接把 PNG 丢进 Content。监视**项目 Content 根**；**新增** PNG/JPG 时 debounce，经 **`AES::Import`（ProductId=`Texture2D`）** 导入——与手动 Import / MCP **同源**。**禁止** Watcher 直调 `Texture2DImporter`。若本切片早于 ED-F24 S03，允许过渡直连 `AM::Import`，须在 Progress 标明例外并在 ED-F24 S03 后改回 AES。raw 可留在磁盘但**不注册**。

| 规则 | 约定 |
|------|------|
| 根 | `PathRegistry::GetProjectContentRoot()` |
| 事件 | MVP：**创建**图片；源变更自动 Recook 默认 **关** |
| 忽略 | `.metex` / `.meta` / 非图片；已是某 Texture 的 SourcePath；Editor 自身写盘回声 |
| Debounce | 解压/批量拷贝合并 |
| 幂等 | 已有 `.metex` 且 SourcePath→该文件 → 跳过 |
| 开关 | `AutoImportTextures` 默认 **开** |

自动导入默认语义：**Color**（含 1ch 扩展）。永不自动选 SingleChannel。

默认输出：与 `Foo.png` 同目录生成 `Foo.metex`（+ `.meta`）；SourcePath 指向留下的 raw。

```text
手动 Import / asset.import / 自动导入  ─┐
  （均经 AES::Import，过渡可 AM::Import）─┼→ Texture2D Product → .metex
                                         ┘
```

## Content Browser 合同

| 项 | 约定 |
|----|------|
| Texture 瓦片 | 只显示已注册 `.metex`（AssetType=Texture2D） |
| Raw PNG/JPG | 不是 Texture 资产 → **不**作为 Texture 瓦片列出 |
| 目录树 | 真实文件夹；自动导入完成前可能短暂只有 raw |
| 缩略图 | cook/Load 预览；**禁止**假定扩展名是 png |
| 过渡 | legacy 已注册 PNG 可标 Deprecated，迁移后消失 |

## Open Questions

| # | 问题 | 挡实现？ | 默认（Planned 已采纳） |
|---|------|----------|------------------------|
| Q1 | `.metex` 单文件 vs JSON+`.bin` | S01 须拍板 | **S01 默认：单文件**（JSON 头 + 二进制/chunk）；若实现受阻再改 JSON+bin |
| Q2 | 是否静默自动迁移全库 legacy PNG？ | 否 | 工具/命令；**禁止**静默改全盘 |
| Q3 | HDR | 否 | **Deferred** |
| Q4 | Color 用 R8+swizzle 省空间？ | 否 | MVP **扩 RGB** |
| Q5 | 与 ED-F24 并行？ | 否 | Product/Loader 可先落；GUI Import 随 ED-F24 收敛 |
| Q6 | 本期 Atlas 页落 `.metex`？ | 否 | **Out** |
| Q7 | 自动导入默认开？ | 否 | **开** |
| Q8 | 源更新自动 Recook？ | 否 | MVP **关**（仅监视新增） |
| Q9 | MVP 是否做空 CreateAsset Texture2D？ | 否 | **是**（极小默认），便于 AES allowlist |

## Slice Plan

| Slice | 目标 | 验收 |
|-------|------|------|
| **S01** | 单文件 `.metex` + Importer + Product `Texture2D` + Registry 扩展名 + NativeCopy 拒 png/jpg/jpeg | 显式导入 → `.metex`；Load 不打开 PNG；`ListImportProducts(png)` 仅 Texture2D |
| **S02** | Color / SingleChannel cook + 测试 | 灰度 Color 不发红；Mask 为 R8 |
| **S03** | SaveHandler + CreateHandler + Reimport + Bootstrap | Save 往返；有 SourcePath 可 Reimport |
| **S04** | Inspector 设置 + Recook | 改语义可见生效 |
| **S05** | 像素 API + Atlas/预览；legacy Load 警告 | Atlas 成员 `.metex` OK |
| **S06** | 自动导入 + 迁移命令 + 文档；AES allowlist Texture2D（若 ED-F24 就绪） | 丢 PNG → `.metex`；Guid 保留迁移样例 |

## Design Review

- **Verdict:** Ready with deferred items
- **Reviewer / date:** agent design-review · 2026-10-01（Mode A）
- **Link or summary:**
  - 已补：自动导入经 AES；NativeCopy↔Texture2D 互斥验收；Recook=Reimport 同路径；1ch Color→RGB8；S01 默认单文件 `.metex`。
  - 延后：HDR、Atlas 页 cook、静默全库迁移。
  - 实现顺序：可与 ED-F24 并行；**建议先 ED-F24 S01**，再 F05 S01（或紧随）。

## Implementation Readiness

- [x] Requirements 已理解
- [x] Constraints 已识别
- [x] F06 后架构已核对
- [x] 职责/边界已定义
- [x] Register* 集成已定义
- [x] 失败/迁移/验证已定义
- [x] 备选已比较
- [x] 开放问题有默认（Q1–Q9）
- [x] Design-review 通过
- [ ] Implementation Plan 对齐切片（S01 开工前补短节即可）

**Ready for implementation?** `yes` — 在 ED-F24 S01 之后（或并行）开 **F05-S01**；S01 检查清单含容器形态 + NativeCopy 互斥测。

## Acceptance Checklist

- [x] Design review（L2）
- [ ] Implementation plan 可追溯到设计
- [ ] Progress / Registry / ACTIVE_WORK 同步
- [ ] 新 Texture 导入 → `.metex`；Load 不依赖 raw
- [ ] Color / Mask 合同 + Recook
- [ ] Atlas 像素 API 路径；Guid 迁移策略
- [ ] NativeCopy 不再把 png 当 Texture 身份
- [ ] 无新增 MCP 粗 Tool；F03 Guid 保持
