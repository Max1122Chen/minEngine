# ASSET-F05 — Texture2D Cooked Asset（引擎侧纹理资产）— Design Spec

## Meta
- **ID:** `ASSET-F05`
- **Type:** Feature
- **Complexity:** L2
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-29
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F02](./ASSET-F02_IMPORT_SERVICE_DESIGN.md) · [ASSET-F03](./ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md) · [ASSET-F04](./ASSET-F04_CONTENT_WORKFLOW_DESIGN.md)
  - [ASSET-F06](./ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md)（**先于本 Feature**）
  - [RND-F22](../Render/RND-F22_TEXTURE_SAMPLER_SEPARATION_DESIGN.md)（Sampler 已分离；本 Feature 不重做）
  - [SPR-F02](../Sprite/SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)（曾提 `.metex` Deferred）
  - 外部驱动（非权威）：minCraft `ENGINE_FEEDBACK_PROMPT_TEXTURE_IMPORT_CHANNELS.md`
- **Depends on:** **ASSET-F06**（AM Facade / Import≠Load / 模块自注册）落地后实现；ImageLoader；Texture2D / RHI；AssetMeta Guid 契约（F03）
- **Blocks:** 稳定灰度 Color 导入；SpriteAtlas 与 Texture 共用像素合同；后续压缩/mip cook

## TL;DR

**问题：** Texture2D 身份几乎等于 Content 里的 PNG/JPG（NativeCopy）；无引擎导入设置、无 cook 产物。1 通道灰度按 `RED` 上传后被 RGB 采样 → 发红。用途语义无处持久化，改意图也无法 Reimport。

**方案（L1）：** 将 Texture2D 提升为引擎 cook 资产 **`.metex`**（+ `.meta` Guid）。源图进入 `Assets/Sources/…`（或等价源树），Content 身份是 `.metex`。Cook 写入：**导入设置**（含通道语义）+ **已应用设置后的像素载荷**。运行时 Load **只读 `.metex`**，不再把 raw PNG 当作资产本身。Inspector / Automation 可改设置并触发 Recook。

**当前：** Design **Draft** — **实现排队在 ASSET-F06 之后**；口径待与 F06 对齐（扩展 Texture2D + 引擎侧资产文件，像素可仍 png） — 命名 `Texture2DImporter`；Sources 自动导入为 In（默认可关）。

## Problem

1. `AssetTypeRegistry` 将 Texture2D 扩展绑为 `.png/.jpg/.jpeg`；导入 = NativeCopy。
2. `Texture2DLoader::LoadFromAssetMeta` 直接 `ImageLoader` 解源文件 → `CreateFromPixels`（`channels==1` → `TextureFormat::RED`）。
3. `AssetMeta.SourcePath` / `Reimport` 未覆盖 Texture2D；无 `SaveAsset_Impl<Texture2D>` / `CreateAsset<Texture2D>`。
4. Sprite / Material 已是引擎 `.me*` 数据模型；Texture 是例外。
5. SpriteAtlas Rebuild 通过成员 Texture 的 **磁盘路径** 再 `ImageLoader`，与「资产=PNG」耦合。

## Requirements

- Texture2D **Content 身份** 为引擎文件（推荐扩展名 `.metex`），Guid 仍在 sidecar `.meta`（F03 不变）。
- 导入 PNG/JPG：**源保留可追溯** + **cook 出 `.metex`**（设置 + 像素），不再仅 NativeCopy 进 Content 当资产。
- 至少两种通道语义（名称可微调）：
  - **Color**（默认颜色路径）：1ch → 扩 RGB8/RGBA8 且 R=G=B（A=255 若 RGBA），或文档化的等价 cook；颜色采样不发红。
  - **SingleChannel / Mask**：保留单通道 cook（R8）；着色器约定读 `.r`。
- 设置持久化在资产侧；**Inspector 可改 → Recook**；Automation 同一字段。
- Load：从 `.metex` 建 `Texture2D` + 上传 GPU；**不要求**运行时存在源 PNG（源仅服务 Reimport/Recook）。
- SpriteAtlas / Sprite 取源像素：优先引擎资产像素 API（读 cook 缓冲或等价），不依赖「资产路径必须是 PNG」。
- 3/4 通道既有视觉行为默认不变；显式 Mask 路径可测。
- 迁移：存量 Content 内 PNG+meta 有过渡策略（见 Migration）。
- **Sources 自动导入：** 监视 `Assets/Sources/**` 新增 PNG/JPG，按格式特征默认设置后导入为 `.metex`（可关）；Content 裸图不自动 Register。

## Constraints

- 遵守 ASSET-F03：单对象、Guid 稳定；Load 不换身。
- 不引入完整 UE DerivedData / 多平台 cook 矩阵（L2 Out）。
- 不把 minCraft / biome tint 塞进 Core。
- 不新增 MCP 粗 Tool；挂既有 `asset.import` / edit 面。
- 验证：相关单测或 smoke（WORKFLOW_PROFILE `tests-required`）。

## Preferences

- 扩展名对齐仓内惯例：`.metex`（SPR-F02 已占名意向）。
- 导入 Product 名：`Texture2D`（或 `Texture2DCook`）；图片不再默认走 `NativeCopy`。
- 文档中英可混；Progress 可用中文摘要。

## Assumptions

- 编辑器进程具备 RHI 时才上传；无 RHI 的 headless 测可只验证 cook 文件与像素布局。
- 近期不强制 ASTC/BC 压缩；cook 像素以 LDR 未压缩（或引擎已有格式）为主。
- HDR 等距柱状图等特殊路径可本 Feature **Deferred**（单独注明）。

## Non-Goals

- 全项目引用图 / 依赖分析。
- 自动识别「草遮罩 vs 石头色图」。
- Biome / 顶点色 tint（玩法侧）。
- 完整压缩格式表、流式 mips、VT。
- 强制一次删光所有 Content PNG（允许过渡期双路径）。
- SpriteAtlas generated page 落盘（可随后用 `.metex`，不绑死本期验收）。

## Current Architecture

```text
PNG/JPG --NativeCopy--> Content/*.png + .meta
                              |
                         Load: ImageLoader --> CreateFromPixels --> Texture2D(RHI)
```

- Material/Sprite：`.memtl` / `.mesprite` JSON + GuidRef。
- Mesh cook：源进 `Assets/Sources`，产品进 Content，`SourcePath` 可 Reimport。
- Texture：**无** SourcePath 管线、无 Save、无导入设置。

## Proposed Architecture

```text
                    ┌─────────────────────────────┐
                    │  Source image (PNG/JPG)     │
                    │  Assets/Sources/...         │
                    └─────────────┬───────────────┘
                                  │ Import / Recook
                                  ▼
                    ┌─────────────────────────────┐
                    │  Texture2DImporter       │
                    │  (decode → apply settings   │
                    │   → write .metex payload)   │
                    └─────────────┬───────────────┘
                                  │
          ┌───────────────────────┼───────────────────────┐
          ▼                       ▼                       ▼
   Content/*.metex         *.metex.meta              AssetRegistry
   (settings+pixels)       (Guid, SourcePath)        AssetType=Texture2D
          │
          ▼ Load
   Texture2DLoader::LoadCooked → Texture2D + RHI
          │
          ▼ Authoring pixels API
   SpriteAtlas / Editor preview / Recook
```

### 资产磁盘形态（推荐）

| 文件 | 内容 |
|------|------|
| `Foo.metex` | 引擎 cook 文档：导入设置 + 像素描述 + **已 cook 像素**（见格式） |
| `Foo.metex.meta` | 标准 `AssetMeta`：`AssetType=Texture2D`，`SourcePath`→源图，Guid |
| `Assets/Sources/.../Foo.png` | 可编辑源；**不是** Content 身份 |

**`.metex` 逻辑布局（实施可选 JSON+sidecar 或单文件分节；须一种真源）：**

```text
Header / JSON:
  version
  width, height
  cookedFormat          # e.g. RGB8 / RGBA8 / R8
  channelSemantics      # Color | SingleChannel
  sRGB                  # bool（Color 默认倾向 true；Mask 默认 false）
  wrapU/V, filter       # 与现 Sampler/Wrapping 对齐的最小集
  sourceHash?           # 可选，便于脏检测

Payload:
  cookedPixels[]        # row-major，与 cookedFormat 一致
```

实施默认倾向：**单文件** = JSON header + base64/raw chunk，或 **JSON + `Foo.metex.bin`**。择一写入 Impl；验收只要求「Load 不读 PNG」。

### 通道语义（cook 合同）

| Semantics | 1ch 源 | 3/4ch 源 | 采样约定 |
|-----------|--------|----------|----------|
| **Color**（默认） | 扩 RGB(A)，R=G=B | 保持（必要时补 A） | `tex.rgb` 为颜色 |
| **SingleChannel** | cook 为 R8 | 取 R（或导入选项指定通道）→ R8 | 只读 `.r` |

默认：未标注且走 Texture 导入 → **Color**。

### 导入 Product

- 新 Product：`Texture2D`（Accepts `.png/.jpg/.jpeg`）。
- **图片扩展名不再 Accepts 进 NativeCopy**（或 NativeCopy 对图片返回引导改用 Texture2D Product）。
- 流程对齐 mesh：`copy → Sources` → cook → `DestDirectory/Name.metex` → Register + SourcePath。
- `asset.list_import_products` / `asset.import`：暴露 `channelSemantics` 等 options。

### 编辑器改意图

1. Inspector 编辑 `.metex` 设置（至少 ChannelSemantics、sRGB、Filter/Wrap）。
2. Apply → **Recook**（读 `SourcePath`；失败则报错且不写坏 Guid）。
3. Save `.metex`；刷新已加载 `Texture2D`（F03：同 Guid 对象更新像素/RHI，禁止换身）。

### Authoring 像素 API

```text
Texture2DCookedPixelsView / TryGetCookedPixels(texture|path)
  → width, height, format, bytes
```

- Atlas Rebuild、预览、Recook 校验走此 API。
- **禁止**假定 `meta.AssetPath` 扩展名是图片。


## Naming（与 Mesh/Anim 现状对齐）

### 骨骼网格 / 动画今天怎么导？（没有引擎侧 *Importer 类型）

现状是 **F02 注册式 Product + AssetManager 成员/静态 handler + Loader/Util**，不是独立 SkeletalMeshImporter 类：

`	ext
EditorImportDialog / asset.import
  → AssetManager::Import(request) 按 ProductId 查表
  → ImportProduct_SkeletalMesh / StaticMesh / AnimationClip
  → ImportExternalMesh / ImportAnimationClip
  → AssimpMeshImportUtil::Cook… / AnimationClipLoader（写 Content 产品 + Sources 拷贝 + SourcePath）
  → RegisterAsset(.glb / .meaclip + .meta)

Load 时另走 LoadHandler → SkeletalMeshLoader 等（内部会用到 Assimp::Importer，那是第三方库类名）
`

登记点：AssetPipelineBootstrap::RegisterImportProduct({ ProductId, Import = &ImportProduct_… })。

因此：**「有没有叫 Importer 的东西」≠ 有没有导入管线。** Mesh 的 cook 逻辑活在 AssimpMeshImportUtil + AssetManager::ImportExternalMesh*。

### Texture 命名决策

**不采用**全局 ImportService（撞 F02 整仓 Import 语义）。

| 方案 | 决策 |
|------|------|
| A. 完全镜像 Mesh | ImportProduct_Texture2D + Texture2DCookUtil（无 Importer 后缀） |
| B. 类型名 Texture2DImporter | 可读；**本质仍是 Product handler / cook util**，由 Bootstrap 注册 |

**默认采用 B 作文档名**；实现可落成 Texture2DImporter 类 **或** ImportProduct_Texture2D + util——验收不绑类名，绑 Product Texture2D + .metex 合同。

| 动词 | 含义 |
|------|------|
| **import** | Product 入口（Register、写 meta、可选挪/留源） |
| **cook** | 源像素→.metex 载荷 |
| **load** | Texture2DLoader：.metex→Texture2D+RHI |

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `Texture2DImporter` | decode 源、应用语义、写/读 `.metex`、Recook；供 Product / Sources 自动扫描调用 | 全局 Import 编排；ImGui；MCP JSON |
| `Texture2DLoader` | Load cooked → `Texture2D`+RHI；过渡期 legacy PNG | 猜用途 |
| `AssetManager` / Import Product `Texture2D` | 登记 Product、Register、Reimport 入口 | 像素算法细节 |
| **Sources 自动导入（Editor）** | 监视 `Assets/Sources` 新增图片 → debounce → 同一 Importer | Content 裸 PNG Register；无开关的强制导入 |
| `AssetMeta` | Guid、Path、Type、SourcePath | 通道语义正文（在 `.metex`） |
| `Texture2D` | 运行时 RHI + 可选缓存的 cooked 描述 | 持有源 PNG 路径当身份 |
| Editor Inspector / Automation | 改设置、触发 Recook | 自写磁盘格式 |
| SpriteAtlasPacker | 经像素 API 取成员源 | 直接 ImageLoader(PNG path) 作为唯一路径 |

## Boundaries

- **允许：** Import/Recook/Load/Atlas → Texture2DImporter / Loader → AssetManager。
- **禁止：** 游戏仓/MCP 把 Content PNG 再当 Texture 身份（过渡期除外）；Surface 内复制 cook；Core 出现 minCraft 类型。
- **设置真源：** `.metex` body；`.meta` 只身份与 SourcePath。
- **源文件：** `Assets/Sources/**` raw **不**注册为 Texture、**不**单独出 Texture `.meta`。
- **自动导入：** 仅 Sources；走 Importer；默认开、可关；禁止 `RegisterAsset(png)`。

## Dependencies

- Depends on: ImageLoader、AssetManager、RHI、现有 Sampler 字段（RND-F22）。
- Depended on by: Sprite、SpriteAtlas、Material 默认贴图、UI Image、minCraft 块图集 bake。
- 新方向：Content 扩展名 Texture2D → `.metex`；Sources 承担 raw。

## Ownership & Lifetime

- Guid / Registry：AssetManager（F03）。
- Cooked 像素磁盘：`.metex`（+可选 bin）由 Texture2DImporter 写；Load 后 GPU 由 `Texture2D` 持有。
- 源图：Sources 树；删除 `.metex` 不自动删源（可警告）。
- Recook：更新同路径文件与同 Guid 运行时对象。

## Data Flow

`Source PNG → decode → apply ChannelSemantics/sRGB rules → cooked buffer → write .metex → Register`  
`Load: read .metex → Texture2D fields → RHICreateTexture2D`  
`Edit settings → Recook from SourcePath → replace payload → refresh RHI`

## Control Flow

- GUI Import Dialog / `asset.import` → Import Product `Texture2D`。
- Inspector Apply → Texture2DImporter::Recook。
- `AssetManager::Reimport` 扩展支持 Texture2D（有 SourcePath）。
- Atlas Rebuild → `TryGetCookedPixels`（失败再 fallback 过渡策略）。

## State & Invariants

- Content 中 Texture2D 的 **canonical** 扩展为 `.metex`（新资产）。
- Guid 在 Recook / 迁移中不变。
- Color cook 的 1ch 结果：通道数 ≥3 且 R=G=B（验收可测）。
- SingleChannel cook：单通道；文档标明采样 `.r`。
- 无 SourcePath 时 Recook 失败（可提示重新指定源）。

## API / Interface Semantics

### Import

- `productId: "Texture2D"`
- options: `channelSemantics`（默认 Color）、可选 `sRGB`、`wrap`、`filter`
- out: `.metex` path + Guid

### Recook / Reimport

- 输入：已注册 Texture2D 路径或 Guid
- 读 SourcePath + 当前 `.metex` 设置（或覆盖 options）
- 失败：不破坏旧 cook 文件（写临时再替换）或明确回滚策略（Impl 定一种）

### Load

- 仅 cooked；legacy PNG Load 见 Migration

### Pixels

- 只读视图；调用方不释放底层除非 API 说明所有权

## Failure Model

| 失败 | 行为 |
|------|------|
| 源缺失 / 损坏 | Import/Recook 失败；保留旧 `.metex` |
| 未知 semantics | 拒绝 |
| legacy PNG 在过渡关闭后 Load | 明确错误并提示迁移 |
| 无 RHI 的 Load | 测环境可只解析 cook；Editor 正常路径要 RHI |

## Alternatives & Trade-offs

### Option A（推荐）— L1 `.metex` = 设置 + cooked 像素

- **Pros：** 运行时不依赖 PNG；与「提升为引擎资产」一致；通道语义一次 bake 进像素。
- **Cons：** 文件变大；需迁移与双路径；Atlas 要改取像素方式。

### Option B — L0 仅 meta 设置，Load 仍解 PNG

- **Pros：** 改动小。
- **Cons：** **不满足**「不再只看 raw」；源丢失即无法 Load；否决为正式目标（可作极短增量 spike，非本 Feature Done）。

### Option C — 仅 GPU swizzle（Godot L8）不 cook 文件

- **Pros：** 省磁盘。
- **Cons：** 仍以 PNG 为身份；设置难持久化；与 L1 目标不符。可作 Color 的**额外**运行时优化，不替代 `.metex`。

**选定：A。**

## Integration Impact

- `AssetTypeRegistry`：Texture2D 扩展 → `.metex`；文件对话框更新。
- NativeCopy：去掉图片 Accepts 或互斥。
- CB 缩略图：对 `.metex` 走 cook/加载预览。
- GuidRef：迁移时 **保持 Guid**，避免全项目坏引用。
- minCraft：重新导入块贴图或跑迁移后灰度 Color 正常。

## Migration / Compatibility

| 阶段 | 行为 |
|------|------|
| **S0–S2** | 新导入只产 `.metex`；Load **仍接受** legacy Content PNG（警告日志） |
| **S3** | 提供迁移：`PNG+meta` → 写入 `.metex`、设 SourcePath（可将 PNG 移到 Sources 或复制）、**保留 Guid**、更新 Registry 路径；可选批量命令 |
| **S4+** | 文档宣布 legacy Load Deprecated；开关或后续 Feature 再硬删 |

兼容承诺：迁移工具保留 Guid；不承诺永久双路径。

## Verification Strategy

- 单测：1ch PNG + Color → cook 后通道 3/4 且 R=G=B；Mask → R8。
- 单测：Load `.metex` 不打开源 PNG（源可临时改名仍能 Load）。
- 单测/手验：改 semantics Recook 后行为切换。
- 回归：RGB/RGBA 导入视觉与 Guid 引用场景。
- Atlas：成员为 `.metex` 时 Rebuild 不发红（Color）。
- Editor smoke：Import → Inspector 改 → 保存 → 重开项目。


## 自动导入（监视 Assets/**）

**目标：** 不预设「用户必须把图放进 Sources」；监视**项目 Content 根（通常即 Assets/）**下新增的 raw 图，按格式特征默认导入为 .metex。仍走 **同一** Product / Importer，禁止 RegisterAsset(png)。

### 触发与范围

| 规则 | 约定 |
|------|------|
| 监视根 | **项目 Content 根**（PathRegistry::GetProjectContentRoot()，即 CB 所看的 Assets 树） |
| 事件 | **新增** PNG/JPG/JPEG（MVP）；源更新自动 Recook 默认 Off（Q8） |
| 忽略 | .metex / .meta / 非图片；已是某 Texture SourcePath 的文件；Editor 自身写盘 mutation |
| Debounce | 合并短时间连续写（解压/批量拷贝） |
| 幂等 | 已有 .metex 且 SourcePath→该文件 → 跳过创建 |
| 开关 | AutoImportTextures（**默认 On**，可关） |

### 「格式特征」默认（MVP）

| 特征 | 默认 |
|------|------|
| 1/3/4ch LDR | channelSemantics=Color（1ch 扩 RGB） |
| 3ch / 4ch | RGB8 / RGBA8；sRGB On |
| jpeg/jpg | Color；无 alpha |

**不**自动选 SingleChannel。

### 输出路径与源文件处置

- 默认：在**同目录**生成 oo.metex（+ .meta），SourcePath 指向同目录留下的 oo.png（raw **可以**继续躺在 Assets 下，但**不是**注册资产）。
- **不强制**搬进 Assets/Sources/（Sources 仍可供手动 Import / mesh 管线使用，非纹理强制习惯）。
- 目标 .metex 已存在且 SourcePath 不同 → 跳过并日志，不覆盖。

`	ext
手动 Import / asset.import  ──┐
                              ├→ Texture2D Product / Importer → .metex
Assets/** Watcher 自动导入 ───┘
`

## Content Browser（必须跟随）

CB **已经**按 Registry 列资产瓦片（FindAssetMetasUnderDirectory），不是「文件夹里每个文件都是资产」。F05 落地后合同：

| 项 | 约定 |
|----|------|
| Texture 瓦片 | **只显示**已注册的 .metex（AssetType=Texture2D） |
| Raw PNG/JPG | **不**再作为 Texture 注册 → **CB 资产区不展示**（即使文件仍在磁盘） |
| 目录树 | 仍扫真实文件夹；可含「只有 raw、尚无 .metex」的目录；自动导入开启后应很快出现 .metex |
| 缩略图 / 预览 | 对 .metex 走 Loader/cook 像素或轻量预览路径，**禁止**假定路径扩展名是 png |
| 过渡期 | legacy 仍注册的 Content PNG 可继续显示并打 Deprecated；迁移后消失 |
| 用户可见文案 | Import / 自动导入完成后 CB 刷新；可选状态栏「Imported foo.png → foo.metex」 |

**验收：** 新项目拖一张 PNG 进 Assets/Textures/ →（自动或手动）得到 .metex → CB 只见纹理资产瓦片，不见「又当资产又当源」的双份 Texture 身份。

## Open Questions

| # | 问题 | 挡实现？ | 默认倾向 |
|---|------|----------|----------|
| Q1 | `.metex` 单文件 vs JSON+`.bin` | 否 | 单文件分节或 JSON+bin，Impl 选一 |
| Q2 | 存量 PNG 是否自动迁移 | 否 | 先工具/命令；不静默改全盘 |
| Q3 | HDR / 非 8-bit | 否 | **Deferred** |
| Q4 | Color 是否允许「R8 + 强制 swizzle」省空间 | 否 | MVP **扩 RGB**；swizzle 优化后置 |
| Q5 | 与 ASSET-F04 并行 | 否 | F04 验收不堵 F05 Design；实现可并行 |
| Q6 | Generated Atlas 页是否本期落 `.metex` | 否 | **Out**；API 预留即可 |
| Q7 | Assets 自动导入默认开？ | 否 | **On**；可关 |
| Q8 | 源更新是否自动 Recook？ | 否 | MVP 仅**新增**；更新 Recook 默认 Off |
| Q9 | 叫 ImportService？ | 否 | **否** → `Texture2DImporter` |

## Slice Plan（草案）

| Slice | 目标 | 验收 |
|-------|------|------|
| **S01** | 格式 + Texture2DImporter + Product `Texture2D` + Registry `.metex` | 显式导入产 `.metex`；Load 不读 PNG |
| **S02** | ChannelSemantics Color/Mask + 单测 | 灰度 Color 不发红；Mask 为 R8 |
| **S03** | Inspector 设置 + Recook + Reimport | 改意图生效 |
| **S04** | Sources 自动导入（debounce + 默认 Color + 幂等） | 丢 PNG 进 Sources 得 `.metex`；可关 |
| **S05** | 像素 API；Atlas/预览；legacy PNG 警告 | Atlas 成员 `.metex` OK |
| **S06** | 迁移命令 + 文档短节 | Guid 保留样本迁移成功 |

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
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [ ] Unresolved questions resolved or explicitly deferred/accepted（Q1–Q6 默认已写，待你确认）

**Ready for implementation?** `no` — 待你确认默认倾向（尤其 Q1/Q2/Q4）后改为 Planned，并开 Implementation Plan。

## Acceptance Checklist

- [ ] Design review（L2）
- [ ] Implementation plan traces to design
- [ ] Progress / Registry 同步
- [ ] 新 Texture 导入 → `.metex`；运行时 Load 不依赖 raw
- [ ] Color / Mask 合同 + 可 Recook
- [ ] Atlas 像素路径与 Guid 迁移策略落地
- [ ] 无新增 MCP 粗 Tool；F03 Guid 契约保持
