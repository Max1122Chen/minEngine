# UI-F09 / SPR-F08 — UI 与 Sprite 表达力整体设计 — Design Spec

## Meta
- **ID:** `UI-F09`（主轴）· `SPR-F08`（共享语义分节，见 §H6）
- **Type:** Feature（能力面补齐 + 字段/样式归属 + 材质参数槽位地基 + 文字度量层）
- **Complexity:** `L2`（跨 Components / ScreenUI 渲染路径 / Sprite 材质工厂 / 字体度量 / 反射序列化 / 编辑器 Inspector 五个面；含 3 个「锚点二选一」判定与 1 套共享数学层）
- **Status:** `Approved`（2026-10-21）—— 主人**批准全部决策项按推荐值执行**（D1–D10）；实现从 **S01 FillAmount** 起，按切片表逐片交付、每片独立验收
- **Owner:** project maintainer
- **Last updated:** 2026-10-21 **v2**（主人审批通过：D1–D10 全部按推荐；状态转 Approved）
- **行号基线:** 本文件所有 `file:line` 断言均取自撰写当日的工作树（含 CORE-F43 / 任务书 #5 的在途改动，例如 `WidgetComponent.h` / `TextMeshBuilder` 邻近文件尚未提交）。实现开工前若这些文件已提交并发生行号漂移，需按符号名重新定位后再引用。
- **Related:**
  - 能力登记 [UI-F09_UI_SPRITE_EXPRESSION_CAPABILITY_BACKLOG.md](./UI-F09_UI_SPRITE_EXPRESSION_CAPABILITY_BACKLOG.md)（U1–U11 / S1–S7 的唯一真源；本文件不改它）
  - [CORE-F43 GO/Component active 与 visible 语义](../Core/CORE-F43_GO_COMPONENT_ACTIVITY_AND_VISIBILITY_DESIGN.md) —— 可见性/激活语义正在分批实现，本设计的字段必须与它正交（§H7）
  - `FEATURE_REGISTRY.md:166-167`（两行登记，本 Feature **不改**该文件）
- **Depends on:** `WidgetComponent::FillSceneProxy`（`Framework/Components/WidgetComponent.cpp:323-390`）、`SpriteQuadMesh`（`Render/Sprite/SpriteQuadMesh.h:12-25`）、`SpriteMaterialFactory`（`Render/Sprite/SpriteMaterialFactory.cpp:18-278`）、`TextMeshBuilder::Build`（`UI/Font/TextMeshBuilder.cpp:10-103`）、`FontGlyphAtlas` 度量（`UI/Font/FontGlyphAtlas.cpp:79-81`）、反射生成器（`scripts/minEngine_header_tool.py`）
- **Blocks:** 无
- **Does not block:** 任何进行中 Feature

## TL;DR

1. **能力取舍**：`U1 FillAmount`、`U2 文字垂直对齐`、`U3 9-slice`、`S1 Sprite Pivot` 四项进**本轮**（S01–S04）；`U4/U5/U6/U7/U8`、`S2/S3/S4/S5/S6` **后置**；`U9/U10` 归主线、`U11` 主人已裁定排除、`S7` 不独立实现（并入 U3 的数学层）。
2. **架构主线**：**语义共享在纯函数层，字段归属在组件层**。UI 与 Sprite 共用同一套「切片 / 填充 / 度量」数学（新的无状态几何函数集），但字段各自挂在 `ImageComponent` / `TextComponent` / `SpriteComponent` 上，**不引入嵌套样式结构体**（理由与代价见 §H1）。
3. **实现锚点**：FillAmount 走**代理矩形 + UV 同步缩放**（零着色器改动，纹理密度不变，不是拉伸）；9-slice 走**CPU 9-quad 顶点缓冲**（复用文字已跑通的 local-pixel-mesh 路径，零新材质变体）。两者的着色器方案都已评估并列了推翻条件（§H3 / §H4）。
4. **材质参数槽位**：本轮**不新增任何槽位**；只把隐式槽位表外提并取消 `translucent ? 4 : 3` 分叉，同时把「改了参数不生效」的根因（`Material::SetScalarParameter` 对未知名字静默丢弃，`Material.cpp:568-572`）变成可自动断言的不变式（§H2）。
5. **文字**：垂直对齐只需要 `lineHeight`（= `ascender - descender`，`FontGlyphAtlas.cpp:79-81` 已算出但 descender 被丢弃）与 1 个追加枚举 + 1 个字段；换行/溢出**本轮不纳入**（§H5）。
6. **兼容**：磁盘读取路径是 `skipUnknownField=true`（`AssetSerializeHelper.cpp:79-82`、`PrefabUtilityDetail.h:39-43`），因此「追加字段、缺省取默认」在**旧场景/prefab 上是零迁移**；风险不在缺字段，而在**新增枚举值 + 旧引擎 + strict 路径**（§C1–C3）。

## Scope

### In
- U1–U11 / S1–S7 的**三档裁剪**（本轮 / 后置 / 不做）与每条的「为什么」。
- 字段与样式归属的取舍（扁平字段 vs 嵌套样式结构体），含 Inspector 与反射序列化代价。
- 材质标量参数**槽位表**方案与四条不变式（含模板缓存 key 维度）。
- FillAmount 与 9-slice 的**实现锚点**判定：图内 UV 重映射 vs 独立顶点缓冲，各自推荐与推翻条件。
- 文字能力的**层次归属**（字体度量 / 网格构建 / 组件）与垂直对齐的度量需求修正。
- 与 CORE-F43 的正交边界：**哪些是呈现参数、哪些是开关**。
- 序列化兼容论证与旧场景/prefab 的迁移结论。
- 可独立验收的切片表（每片：做什么 / 结束仍绿 / 可回退 / 自动 oracle）。
- Failure Model 与 Verification（可脚本化 oracle 与明确的「目视项」分界）。

### Non-goals（明确不做，见 §Deferred）
- 不写任何实现代码；本文件是唯一交付物，**不改源码 / CMake / 注册表 / bugs / CORE-F43 / backlog / 外仓**。
- 不做 U11（alpha 遮罩命中）：主人已在另一条线把它与遮挡语义一起处理，本设计**不预留字段、不定义语义**（§能力三档 U11 行）。
- 不重新定义 U9/U10（已在任务书 #5 主线，命中路径已存在）。
- 不做跨域排序层统一（S4）、不做图集 margin/extrude（S5）、不做帧动画（S6）。
- 不做每 widget 材质实例数量的重构；只保证新增能力**不放大**实例数（本轮方案本身不新增材质变体）。

## Reader quick start
1. 先读 **§能力三档**：这份文档对每条能力只回答一个问题——「现在做 / 以后做 / 不做，为什么」。
2. 契约只读三处：**§H2 四条槽位不变式**、**§H7 呈现参数 vs 开关**、**§状态机或契约 I1–I9**。
3. 实现顺序只看 **§切片表**（S01 = FillAmount，唯一有真实需求）；争议看 **§待主人审批的决策清单**（D1–D10）。

## Pre-flight

| 项 | 结论 |
|----|------|
| **Complexity** | **L2**：三个独立锚点判定 + 一个共享数学层 + 反射/序列化面 + 每片可独立回退 |
| **Prerequisites（渲染面）** | `WidgetSceneProxy` 已是 widget 呈现数据的唯一载体（`Render/SceneProxies/WidgetSceneProxy.h:20-42`）**sound**，可承载填充分解；`SpriteQuadMesh` 是进程级单元四边形（4 顶点 / 6 索引，`SpriteQuadMesh.h:20-25`）**sound**；per-widget 顶点缓冲路径已有先例（文字，`WidgetComponent.cpp:334-352`）**sound** |
| **Prerequisites（材质面）** | 模板按 blend mode 缓存、实例只派生不编译（`SpriteMaterialFactory.cpp:133-171`、201-219）**sound**；标量槽位是**隐式常量**（`SpriteMaterialFactory.cpp:31`）→ 需外提（§H2）；`SetScalarParameter` 对未知名字**静默丢弃**（`Material.cpp:568-572`）→ 这是「改了不生效」的根因 |
| **Prerequisites（文字面）** | 度量只暴露 `Ascender` / `LineHeight`（`FontGlyphAtlas.h:38-39`），descender 在 `FontGlyphAtlas.cpp:80` 算完即弃；`TextMeshBuilder` 是**单行、顶对齐**（`TextMeshBuilder.cpp:39-40`） |
| **Prerequisites（序列化面）** | 磁盘读：`skipUnknownField=true` + `strictTypeCheck=false`（`AssetSerializeHelper.cpp:79-82`、`PrefabUtilityDetail.h:39-43`）；枚举按**名字**存取（`Serializer_Legacy.cpp:219-229`、382-396） |
| **Tech-debt risk** | **low–medium**：本轮四项都不改着色器、不改布局算法；主要风险是「新字段被误用为开关」与「未来切片误配槽位」——前者用 §H7 + I6 挡住，后者用 §H2 的槽位表 + oracle 挡住 |
| **WIP** | 与任何进行中 Feature 无文件重叠；本设计**只落一个 .md**（UI 与 Sprite 同文，理由见 §H6） |
| **Philosophy** | 呈现能力是**参数**，不是开关（开关属 CORE-F43）；两域共用的语义放在**纯函数**里，不放在共享结构体里（结构体会污染序列化路径，函数不会） |
| **Recommendation** | S01（FillAmount）→ S02（文字垂直对齐）→ S03（9-slice）→ S04（Sprite Pivot，可选）→ S05（槽位地基 + 兼容收尾）。S01 之后排期压力即消失，S02/S03 可任意顺序 |

## Problem

### P1 — 三项缺失不是孤立缺口，而是「呈现参数」这一层整体没有落点
`ImageComponent` 只有 4 个属性（`ImageComponent.h:63-76`：SourceMode / Texture / Sprite / Color），`TextComponent` 只有 5 个（`TextComponent.h:67-80`：Content / Font / FontSize / Color / HorizontalAlignment）。所有「这张图怎么铺、这段字放在框里的哪」都没有表达位——`ResolveDrawUVRect()` 只能返回**整块** UV（`ImageComponent.cpp:93-105`），文字基线被硬编码为「顶对齐」（`TextMeshBuilder.cpp:39-40`）。

### P2 — 消费方的真实痛点只有一处，且它现在是 20 段贴图拼的
进度条/血条用 20 段白贴图 + `SetActive` 拼接（backlog:17）。这值得一个真实的 `FillAmount`：不是「顺便补」，而是把「按比例揭示一张图的一部分」变成引擎能力。

### P3 — UI 与 Sprite 走同一个材质工厂，但参数是隐式的
`ScreenUIMaterialFactory::CreateInstance` 直接转发到 `SpriteMaterialFactory::CreateInstance(rhi, true)`（`ScreenUIMaterialFactory.cpp:39-42`），参数值统一由 `SpriteMaterialFactory::ApplyColorTextureAndUVRect`（`SpriteMaterialFactory.cpp:263-278`）按**字符串名字**写入。槽位号是散在 `PopulateSpriteMaterialGraph` 里的魔数（`SpriteMaterialFactory.cpp:31`、34、39、44、49、62、67、72、112）。任何新增参数只要漏掉「图里加节点 / 表里加槽位 / Apply 里加 set」三者之一，就**静默不生效**——`Material::SetScalarParameter` 在参数表里找不到名字时直接返回（`Material.cpp:568-572`）。

### P4 — 文字判「框内垂直位置」缺的不是 descender，而是被丢弃的度量
`FontGlyphAtlas` 只暴露 `GetAscender()` / `GetLineHeight()`（`FontGlyphAtlas.h:38-39`），而 `m_LineHeight = m_Ascender - descender`（`FontGlyphAtlas.cpp:79-81`）——**用来做顶/中/底对齐的正是 lineHeight**，descender 只是中间量。backlog:28 假设「需要字体降部度量」；本设计的结论是：对齐本身的充分度量已经存在，descender 只在做光学居中（cap-height/x-height）时才需要。

### P5 — 裁剪在本仓没有承载体，所以「遮罩类」能力现在不能便宜地做
`RHICmdSetViewport` 同时设置 viewport 与 scissor（`RHI.h:110-121`；Vulkan 实现里一次调用写两者，`VulkanRHI.cpp:2250-2256`），`UIDrawCommand` 只有 `Draw/StableOrder/CanvasSortOrder` 三个字段（`DrawCommands/UIDrawCommand.h:9-14`）。UI 命中只查矩形（`ScreenUIHitTester.cpp:143-163`）。任何「矩形裁剪」都要先拆这三处的耦合——这是 U6 后置的技术理由，不是排期借口。

## 能力三档（本轮 / 后置 / 不做）

> 「本轮」= 本设计授权实现的批次；「后置」= 已在设计里定好语义方向、等排期；「不做」= 本 Feature 永不实现，或归别的 Feature。

### UI 侧

| # | 能力 | 档 | 一句理由 |
|---|------|----|----------|
| U1 | FillAmount + 方向 | **本轮 S01** | 唯一有真实消费方痛感（backlog:17），且能在**零着色器改动**下做到精确（§H3） |
| U2 | 文字垂直对齐 | **本轮 S02** | 纯 CPU 布局数学，所需度量已存在（`FontGlyphAtlas.cpp:79-81`），改动限于 1 枚举 + 1 字段 + 1 个构参数 |
| U3 | 9-slice | **本轮 S03** | 需求侧判低优先（backlog:19），但它是 S3/S7 共享数学的载体：现在不做，将来必然长出两套切片实现 |
| U4 | 文字换行与溢出 | 后置 | 需要多行布局 + 省略号测量 + 逐行 quad 发射；先让 S02 把度量层立起来，再谈行断开不会有两次返工 |
| U5 | 文字描边/阴影 | 后置 | 只有两条路（二次 blit / SDF 或多次采样），都要动着色器与字体管线；无任何消费方需求 |
| U6 | 裁剪/遮罩 | 后置 | 前置条件是**拆掉** viewport/scissor 耦合（`RHI.h:110-121`）并同步 UI 命中（`ScreenUIHitTester.cpp:148-149`），这是独立 Feature 的量级（§P5） |
| U7 | 布局容器 | 后置 | 改的是 `UILayoutPass` 的整套算法与作者化语义，与本 Feature 的「呈现参数」不同层 |
| U8 | ScrollView | 后置 | 依赖 U6 + U7，且自带输入惯性与边界状态机 |
| U9 | 指针遮挡/穿透 | **不做（非本 Feature）** | 已在任务书 #5 主线；`m_bHitTestVisible` 与命中查询已存在（`WidgetComponent.h:71-72`、`ScreenUIHitTester.cpp:148`），此处重复定义只会制造第二真源 |
| U10 | 点击要求原地释放 | **不做（非本 Feature）** | 同上，主线在办 |
| U11 | alpha 遮罩命中 | **不做（主人已裁定排除）** | 主人已把它与遮挡语义放到另一条线；本设计**不预留字段**，避免两套命中语义并存 |

### Sprite 侧

| # | 能力 | 档 | 一句理由 |
|---|------|----|----------|
| S1 | Pivot / 轴心 | **本轮 S04（可选）** | 只影响模型矩阵与 AABB，零着色器零参数；是 Sprite 常规能力里唯一与其他项零耦合的一块 |
| S2 | Tiling / 平铺 | 后置 | 需要 wrap 采样状态 + 材质变体，且与 9-slice 抢同一条 UV 管线（同一 UV 不能既是重复又是切片） |
| S3 | Flip / 镜像 | 后置（最便宜） | 与 9-slice **强耦合**：翻转一张九宫格必须同时交换左右/上下边框，因此顺序上必须在 S03 之后；且无消费方需求 |
| S4 | 排序层与顺序 | 后置 | UI 已是 `CanvasSortOrder → StableOrder` 两级（`ForwardRenderer.cpp:1286-1296`），Sprite 走半透明距离排序（`ForwardRenderer.cpp:1215-1217`）；「统一口径」是跨域排序约定，不是字段，属独立设计 |
| S5 | 图集区域表达 | 后置（部分已存在） | 取 UV 已可用（`Sprite.h:47` `GetUVRectNormalized`）；缺的是 margin/extrude 语义，绑 ASSET-F01 |
| S6 | 帧动画 / 序列帧 | 后置 | 前置问题是「是否走 AnimationGraph」；未定就实现必是第二套时间轴 |
| S7 | 九宫格在 Sprite 侧 | **不做独立实现** | 并入 U3 的同一套纯函数（§H6）；Sprite 侧只多一个字段位点，不新开语义 |

## How

### H1 字段与样式归属 —— 扁平字段 + 共享纯函数层（不抽样式结构体）

**候选与代价**

| 方案 | 做法 | 代价 | 收益 |
|------|------|------|------|
| A（推荐） | 字段逐项直挂 `ImageComponent` / `TextComponent`，用**命名前缀**分组（`Fill*` / `Border*` / `VerticalAlignment`） | 字段数变多（Image 4→7，Text 5→6），Inspector 列表变长 | 零迁移风险；沿用现有 4 个属性的生成/序列化/override 已证明路径（`ImageComponent.gen.cpp:7-10`） |
| B | 抽 `UIImageStyle` / `UITextStyle` 结构体，作为 1 个 `ME_PROPERTY` 内嵌 | 属性路径变 `Style.BorderX`；prefab override 记录、`PropertyPath` 按名解析的整条链都要跟着改（`Serializer.cpp:1504-1537`）；Inspector 多一层嵌套 | 分组清晰、一次 dirty 调用 |
| C | 先 A，字段数超过阈值再迁 B | 迁移本身要有版本/兼容设计 | 延迟决策 |

**推荐 A**，三条依据：
1. **反射是代码生成的**：`ME_PROPERTY` 由 `scripts/minEngine_header_tool.py` 生成到 `Generated/Reflection/*.gen.cpp`（`ImageComponent.gen.cpp:1`），宏本身是空标记（`ReflectionAnnotations.h:12-22`）。方案 B 会引入嵌套结构的生成与序列化路径，属于**未验证能力**；方案 A 的路径已被现有 4 个字段验证。
2. **Inspector 没有分组元数据**：`ReflectionAnnotations.h` 里没有任何 category/group/tooltip 宏，属性按 `ForEachPropertyInHierarchy` 的声明顺序通用绘制（`SceneEditorInspectorSource.cpp:388`、431）。抽结构体**换不来分组收益**，只换来一层嵌套路径。
3. **可见性压缩已有现成手段**：确实需要藏字段时用 `PropertySpecifier::Invisible`（`MEProperties.h:71`，CORE-F43 已核实它能隐藏字段并挡住 PropertyPath 写入），不需要为此重构归属。

**代价的正面处置**：把「语义」放纯函数，字段只是入口。新增一组无状态函数（位置与命名在实现期定，建议放 `UI/Image/` 下）：

- `ComputeFilledRect(rect, uvRect, origin, amount) -> {rectOut, uvOut}`
- `ComputeNineSliceQuads(rect, uvRect, borderPx) -> 9 x {rect, uv}`（含退化钳制）
- `ComputeTextBaselineY(widgetHeight, lineHeight, ascender, verticalAlignment) -> float`

UI 与 Sprite 都调这些函数 → **一份语义，两处入口**（§H6）。

**推翻条件**：出现**第二个消费方**需要整套参数（内置控件库 / Lua 绑定 / 编辑器预览）；或 9-slice+fill+radial+Flip 合计字段 > 12 使 Inspector 不可读；或确认 `PropertyPath` / prefab override 已支持嵌套内联结构并愿意承担迁移。

**Inspector 显示约定（本轮新增字段适用）**：字段按语义块相邻声明（`SourceMode/Texture/Sprite/Color` 之后：`FillAmount/FillOrigin`，再 `Border*`），保证编辑器里读起来成组；派生量（例如 9-slice 是否需要重建网格）不建字段，用私有 dirty 位（已有先例：`TextComponent.h:82-84`）。

### H2 材质参数槽位 —— 外提槽位表，本轮不新增槽位

**现状（隐式槽位）**

| 槽位 | 参数名 | 出现条件 | 证据 |
|------|--------|----------|------|
| 0/1/2 | `TintR/G/B` | 恒定 | `SpriteMaterialFactory.cpp:62`、67、72 |
| 3 | `Opacity` | **仅 translucent** | `SpriteMaterialFactory.cpp:112` |
| base+0..3 | `UVRectU0/V0/U1/V1` | 恒定，`base = translucent ? 4 : 3` | `SpriteMaterialFactory.cpp:31`、34、39、44、49 |

着色器侧：标量参数被翻译成 `u_ScalarParams[i]`（`GLSLMaterialTranslatorImpl.cpp:346-348`），**数组长度 = 已用最大槽位 + 1**（`GLSLMaterialTranslatorImpl.cpp:371-381`），绑定表由「已用槽位集合」推导（`GLSLMaterialTranslatorImpl.cpp:101-108`、466-467）。

模板缓存：按 `(RhiSerial, translucent)` 两维缓存（`SpriteMaterialFactory.cpp:133-150`、174-199），实例由 `CreateRuntimeInstance` 派生并复制参数表、**不重编译**（`Material.h:60-68`）。

**方案（本轮）**

1. 新增 `Render/Sprite/SpriteMaterialSlots.h`：一个 `enum class ESpriteScalarSlot { TintR = 0, TintG, TintB, Opacity, UVRectU0, UVRectV0, UVRectU1, UVRectV1 }` + 一张「槽位 → 参数名」常量表；`PopulateSpriteMaterialGraph` 与 `ApplyColorTextureAndUVRect` 都从这张表取值，不再写字面量。
2. **取消 blend mode 分叉**：`Opacity` 在 opaque 图里也占槽位 3（节点存在、不连线），于是 UV 恒为 4..7。收益：`translucent ? 4 : 3` 这个「加参数时必须重算」的分支消失。
3. **本轮不新增任何材质参数**：S01/S03 都走 CPU 路径（§H3/§H4），所以 0 个新槽位、0 个新材质变体、0 次额外实例分配（回应 backlog:58 的第 6 问题）。

**四条不变式**

- **I-S1（只追加）**：槽位号只追加，永不重排、永不复用。旧号腾空就保持空洞，接受数组多出一个元素的浪费。
- **I-S2（三处同源）**：图节点、槽位表、`Apply*` 写入三者由同一张表驱动；**任何参数名不在表里 = 编译期不可表达**。这张表的存在就是为了消灭「加了节点忘了 set」的静默失败（`Material.cpp:568-572`）。
- **I-S3（缓存 key 含变体）**：模板缓存 key 必须含「图变体」维度。图语义一变必须换 variant（而不是复用旧模板）。当前 key 只有 `(RhiSerial, translucent)`，本轮因为不改图所以不受影响；任何未来新增参数的切片**必须**同时扩这个 key。
- **I-S4（槽位稠密）**：数组大小由最大槽位推导，因此槽位必须连续、少空洞；新参数一律追加在当前最大值之后。

**可自动断言的 oracle（不需要 GPU）**：新增一个纯 CPU 的图描述访问器（例如 `SpriteMaterialFactory::DescribeScalarParameters() -> {name, slot}[]`，由 `PopulateSpriteMaterialGraph` 与槽位表共同产生），测试断言：
- 表中每个名字在图的 ScalarParameter 节点集合里**恰好出现一次**，且槽位号与表一致；
- 槽位集合连续（`0..N-1` 全部被用）；
- `ApplyColorTextureAndUVRect` 写入的名字集合 == 表的名字集合（用同一张表计算，断言「无遗漏」）。

**推翻条件**：若实现期发现 `MaterialEdGraph` 的节点参数无法在无 RHI 环境下构建（`PopulateSpriteMaterialGraph` 目前是匿名命名空间内的函数，`SpriteMaterialFactory.cpp:18-129`），则该 oracle 退化为「表自身的静态一致性测试」+「需要 GPU 的参数存在性测试」（后者已有 API 先例：`MaterialRenderProxy::GetScalarParameterValue`，用在 `Tests/Suites/MaterialInstanceTest.cpp:25`）。

### H3 FillAmount 的实现锚点 —— 代理矩形 + UV 同步缩放（CPU，零着色器改动）

**三个候选**

| 方案 | 做法 | 精确性 | 改动面 |
|------|------|--------|--------|
| **F-CPU（推荐）** | 在 `WidgetComponent::FillSceneProxy` 里按 `FillAmount` 收缩**呈现矩形**，同时对 `m_UVRect` 做**同比例**收缩（比例 f），再交给现有 model matrix + 着色器 UV lerp | **精确**：可见区宽 `w*f`、UV 跨 `(u1-u0)*f` ⇒ 纹理密度不变，观感是「裁」不是「压」 | 组件 + 代理字段；着色器 0 改动 |
| F-DISCARD | 着色器里按 uv 与 `FillAmount` 比较丢弃片段 | 边缘硬、无半像素问题；但需要**新材质变体 + 新槽位** | 图 + 槽位表 + 变体 key |
| F-MESH | 为每个填充状态生成独立顶点缓冲（半宽 quad，UV 不变） | 最精确 | 每 widget VB/IB + 重建时机管理 |

**推荐 F-CPU**，理由：
1. **零着色器改动**是本轮最关键的成本约束：新增参数需要动槽位表 + 变体 key + 一次重编译（§H2 I-S3），而 F-CPU 完全绕开这条最容易失败的链。
2. **语义正确且可证明**：设可见区宽 `w1 = w*f`，UV 跨 `du1 = du*f`，每像素纹理跨度 `w1/(du1*Wtex) = w/(du*Wtex)` 与 f 无关 ⇒ 不是拉伸。
3. **与布局解耦**：收缩只发生在**代理**（`WidgetSceneProxy.h:22-25` 的 `m_TopLeftPx/m_SizePx/m_UVRect`），`WidgetComponent::m_ComputedRect` 不动 ⇒ 不触碰 `UILayoutPass`，也不影响 CORE-F43 的布局/命中文档（那边已裁定「隐藏节点布局仍算、命中断」）。
4. **与既有数据流完全同构**：代理矩形在 `BuildScreenUIQueue` 里统一做 letterbox 映射后进入 model matrix（`ForwardRenderer.cpp:1249-1281`），UV 经 `lerp(uvMin,uvMax,attrib)` 生效（`SpriteMaterialFactory.cpp:90-100`）。填充只是把这两个值改写一次。

**注意点（必须写进实现约束）**
- **顺序**：先算「整块 UV」（`ResolveDrawUVRect`，`ImageComponent.cpp:93-105`），再套填充比例——否则 Sprite 子区域 + 填充会二次缩放。
- **方向**：单一枚举 `EUIImageFillOrigin { Left = 0, Right, Bottom, Top }`（追加到 `UI/UITypes.h`，与 `EUITextHorizontalAlignment`（`UITypes.h:56-63`）同风格）；`Left` 表示水平方向自左向右揭示，方向由枚举隐式携带，不引入第二个 Mode 枚举。
- **退化**：`amount <= 0` → 不发射绘制（代理 `m_bVisible=false`，与现有可见位一致，`WidgetComponent.cpp:386-389`）；`amount >= 1` → 与不加字段时**逐位一致**（这是默认值兼容的硬要求）。
- **边缘**：线性过滤下切口处会有 1 像素混色；九宫格填充（边框固定、只填中段）**不在本轮**——那需要 F-DISCARD 或 F-MESH（§Deferred）。
- **只对 Image 生效**：文字走 local-pixel 网格，其模型矩阵只做平移（`ForwardRenderer.cpp:1273-1277`），填充对文字无意义。

**推翻条件**：① 需求要求「边框不动、只填内部」（进度条套美术边框）——F-CPU 会把边框一起压缩，此时必须转 F-DISCARD；② 需求要求径向/角度填充——矩形缩放无法表达，转 F-DISCARD 或新几何；③ 若 S03 已把 Image 变成 CPU 9-quad 网格，则填充可与网格切片共用同一份几何生成，此时 F-MESH 的边际成本降到接近 0，应重估。

### H4 9-slice 的实现锚点 —— CPU 9-quad 顶点缓冲（复用 local-pixel-mesh 路径）

**两个候选**

| 方案 | 做法 | 精确性 | 改动面 |
|------|------|--------|--------|
| **S-CPU（推荐）** | 生成 9 个子 quad 的顶点/索引（角块原尺寸、边块单向拉伸），UV 直接烘进顶点；走已有的 per-widget VB/IB + `m_bLocalPixelMesh` 路径 | **精确**（边框按整数像素、UV 端点落在纹素边界） | 新几何生成器 + `ImageComponent` 边框字段 + 代理路径分支；**0 新材质参数** |
| S-SHADER | 单个 unit quad，在片元里按 widget 像素尺寸做分段 UV 重映射 | 需要处理半纹素偏移与 `WidgetPxSize` uniform；边界易出现半像素误差 | 新图变体 + 3~5 个新槽位 |

**推荐 S-CPU**，理由：
1. **路径已存在且被验证**：文字就是「per-widget VB/IB + local-pixel 布局 + 只做平移的模型矩阵」（`WidgetComponent.cpp:334-352`、`ForwardRenderer.cpp:1273-1277`），`TextMeshBuilder` 也在做同样的逐 quad 发射（`TextMeshBuilder.cpp:42-100`）。这不是新机制，是把已有机制用第二次。
2. **不改着色器 = 不碰 §H2 的整条风险链**（槽位表、变体 key、模板重编译）。
3. **仍是单次 draw call**：9 quad = 36 顶点 / 54 索引，一次 `DrawIndexed`；不是 9 次 draw。
4. **性能可控**：只在「尺寸或边框变化」时重建（与文字的 `m_LastWidgetSize` 同构，`TextComponent.h:82-84`）。

**实现约束**
- **I-Q1（UV 恒等）**：走 local-pixel 网格 + 烘进顶点的 UV 时，材质 UV 参数必须是 `(0,0,1,1)`；否则着色器会二次重映射（`SpriteMaterialFactory.cpp:90-100`）。文字已隐含依赖此约定（`TextComponent.cpp:139` → `ScreenUIMaterialFactory::ApplyColorAndTexture` → 恒等 UV，`ScreenUIMaterialFactory.cpp:50-55`）——这条要写成显式不变量并加断言。
- **零边框退化**：四边都为 0 → 走原来的单 quad 路径（`SpriteQuadMesh`），保证旧场景**逐位一致**。
- **钳制**：`border.x + border.z > size.x` → 按比例压到恰好放下（不产生负宽度 quad）；这是唯一的语义自由度，写进测试。
- **半像素**：边框取整到整数像素再算 UV；非整数边框按最近整数处理并记录（避免缝线）。
- **与填充的交互**：本轮两者不同时生效（填充 = 全图缩放；9-slice = 中段拉伸）。同时开启的行为定义为「先 9-slice 切出中段、再按 f 收缩中段矩形」——**这是 §Deferred**，本轮不实现、不做承诺。

**推翻条件**：① 同屏 9-slice widget 数量级上升导致 VB churn 成为热点（需 profile 证据）；② 需要圆角/圆形裁剪/渐变边框——那必须走片元路径；③ 若 S5（图集 extrude）先落地，边块需要外扩取样，CPU 方案的 UV 计算要跟着改（可接受但需重估）。

### H5 文字能力的层次归属

**三层划分（现状即如此，本设计只填格）**

| 层 | 位置 | 本轮新增 | 不变量 |
|----|------|----------|--------|
| 字体度量 | `FontGlyphAtlas` | 暴露 `GetDescender()`（`m_Descender` 新字段；`FontGlyphAtlas.cpp:80` 已算出即弃） | 度量只读、随 atlas（font x pixelSize）缓存，与 `FontGlyphAtlasCache` 生命周期一致（`TextComponent.cpp:103-116`） |
| 网格构建 | `TextMeshBuilder::Build` | 新增入参 `EUITextVerticalAlignment`（纯函数，可测） | 不持有状态、不查组件、不依赖 RHI |
| 组件 | `TextComponent` | `ME_PROPERTY` 字段 + Setter + `MarkTextMeshDirty()`（既有模式，`TextComponent.cpp:65-74`） | 字段只做「存值 + 弄脏」，算法不在组件里 |

**垂直对齐算法（写成公式，实现按此）**
设 `H` = widget 高、`A` = ascender、`L` = lineHeight（= A - descender）：
- `Top`：`baselineY = A`（**当前行为，保持默认值**——`TextMeshBuilder.cpp:39-40`）
- `Middle`：`baselineY = (H - L) / 2 + A`
- `Bottom`：`baselineY = H - L + A`

因为只有 `L` 参与，**对齐不需要完整 descender 支持**；descender 的用途是光学居中（cap-height/x-height 校正），列为可选增强（D6）。

**为什么换行/溢出不在本轮**：它把「单行、一次 pen 前进」变成「行断开 + 逐行发射 + 溢出处省略号测量 + 高度超限策略」，还要新增「内容变化 ⇒ 需要重排」的依赖（现在只有 font/size/content/alignment → dirty，`TextComponent.cpp:18-74`）。它值得一份独立切片，且**必须先有 U2 的度量层**才有干净落点。本轮明确不做、不做承诺。

**层次上的一个硬约束**：对齐与 baseline 计算只能出现在 `TextMeshBuilder`（纯函数层），**不得**下沉到 `FontGlyphAtlas`（那是字体事实，不是布局决策），也**不得**上浮到 `WidgetComponent`（它只负责把 widget 尺寸喂进来，`WidgetComponent.cpp:334-337`）。

### H6 UI 与 Sprite 为什么同文（SPR-F08 分节而非另起一篇）

三条共享事实：
1. **同一个材质工厂**：UI 材质是 Sprite 材质的直接转发（`ScreenUIMaterialFactory.cpp:39-42`、58-68），参数槽位、模板缓存、`Apply*` 写入路径**完全同一份代码**（`SpriteMaterialFactory.cpp:263-278`）。
2. **同一套几何**：UI widget 与 SpriteComponent 共用进程级 `SpriteQuadMesh`（`SpriteQuadMesh.h:10-25` 明确写着 shared by SpriteComponent (world) and ScreenUI widgets）。
3. **同一套待共享语义**：9-slice（U3/S7）、图集子区域取 UV（`Sprite.h:47` vs `ImageComponent.cpp:99`）、排序（各自一套，见 S4）——它们的语义必须一次定义。

因此：**一份文档、两个分节**。若将来「排序层统一」被批准并需要跨 RenderScene 的排序约定，那部分是独立 Feature 的量级，届时另起 `docs/ai/Sprite/SPR-F08_*.md` 并链回本篇；当前不满足「必须独立成文」的条件（无独立字段、无独立实现、无独立风险）。

**Sprite 侧分节（SPR-F08 部分）要点**
- **S1 Pivot（本轮 S04，可选）**：`SpriteComponent` 现有 5 个属性（`SpriteComponent.h:60-77`），无 pivot；默认 `(0.5, 0.5)` 表示「中心 = 现在行为」，保证旧场景一致；实现落点是模型矩阵与 `GetBoundingBox`（`SpriteComponent.h:51`）。Oracle：纯矩阵/AABB 数学测试。
- **S7 九宫格**：**不新开实现**，改用 U3 的 `ComputeNineSliceQuads`；Sprite 侧只多一个 `BorderPx` 字段位点。这条是本设计对「两套实现」的正面回答。
- **S3 Flip**：必须在 9-slice 之后（翻转要交换左右/上下边框），本设计给顺序不给实现。
- **S2/S4/S5/S6**：后置，理由见三档表。

### H7 与 CORE-F43 的关系 —— 呈现参数 vs 开关

**判据**：一个字段若会改变「这个节点在不在域里、能不能被点到、要不要参与级联」，它是**开关**，归 CORE-F43；若只改变「已经在域里的东西长什么样」，它是**呈现参数**，归本设计。

| 类别 | 字段 | 归属 | 本次是否新增 |
|------|------|------|--------------|
| 开关（可见轴） | `SceneComponent::m_bVisible`（CORE-F43 S01 落地）、`Component::m_bActive` | CORE-F43 | **否** |
| 开关（命中轴） | `WidgetComponent::m_bHitTestVisible`（`WidgetComponent.h:71-72`；字段 `:116` 目前无 `ME_PROPERTY`，是否持久化由主线定） | 任务书 #5 主线 | **否** |
| 呈现参数 | `FillAmount` / `FillOrigin` / `Border*` / `VerticalAlignment` / `Pivot` | **本设计** | 是 |

**三条硬约束**
- **I6**：呈现参数**不得** gate 绘制。`FillAmount == 0` 的效果是**不发射绘制**（代理 `m_bVisible=false`），**不是** `SetActive(false)` / `SetVisible(false)`。理由：后者会改写作者化开关、触发 CORE-F43 的级联与缓存失效，并在命中语义上留下第二个真源（CORE-F43 的 I1「级联永不写回字段」正是为此而立）。
- **I7**：呈现参数**不得**进入布局。填充/切片只改代理矩形与 UV，`m_ComputedRect` 与 `UILayoutPass` 不动（这也是 CORE-F43 D4「隐藏节点布局仍算」前提不被破坏的原因）。
- **I8**：本设计**不新增任何布尔开关**。需要「关掉视觉」时用 CORE-F43 的可见性轴；需要「关掉部分视觉」时用呈现参数（`FillAmount=0`、`Color.a=0` 都是呈现）。

## 状态机或契约

不变量（实现期按此逐条守）：

- **I1（默认不变）**：所有新字段的默认值必须让渲染结果与今天**逐位一致**（`FillAmount=1`、`Border=(0,0,0,0)`、`VerticalAlignment=Top`、`Pivot=(0.5,0.5)`）。
- **I2（追加型）**：字段只追加；枚举只追加；**禁止重排**既有枚举值（`EUIImageSourceMode` / `EUITextHorizontalAlignment` / `EUIAnchorPreset` 等，`UITypes.h:56-74`）。
- **I3（先整块 UV 后填充）**：填充比例作用在「已解析出的整块 UV」之上，不得与 Sprite 子区域二次相乘。
- **I4（UV 恒等）**：local-pixel 网格路径必须把材质 UV 参数设为 `(0,0,1,1)`（见 §H4 I-Q1）。
- **I5（槽位四律）**：§H2 的 I-S1..I-S4。
- **I6/I7/I8**：§H7。
- **I9（dirty 传播）**：任何呈现参数 setter 必须走「同值早退 + `MarkRenderStateDirty`」的既有模式（`ImageComponent.cpp:13-77`、`TextComponent.cpp:18-74`），不得静默改字段。

## Failure Model

| # | 失效模式 | 触发条件 | 表现 | 缓解 |
|---|----------|----------|------|------|
| F1 | 参数名漂移 | 图里有节点、`Apply*` 没 set（或反过来） | **静默不生效**，无日志（`Material.cpp:568-572`） | I-S2 单表驱动 + §H2 的 oracle |
| F2 | 模板陈旧 | 图语义变了但复用旧模板 | 新参数被着色器忽略 | I-S3 变体进 key；`ClearTemplateCache`（`SpriteMaterialFactory.cpp:245-253`）在热重载时调用 |
| F3 | 几何/UV 不同步 | 只缩放矩形、没同步 UV | 进度条看起来被「压扁」 | I3 + 纹理密度断言（§V1） |
| F4 | UV 双应用 | local-pixel 网格 + 非恒等 UV 参数 | 图像被裁成一小块 | I4 + 断言 |
| F5 | 缝线/半像素 | 9-slice 边框非整数像素 | 切片间可见细线 | 边框取整 + 端点对齐纹素 |
| F6 | 填充边缘混色 | 线性过滤 + 非纹素对齐切口 | 1 像素过渡 | 接受（与主流引擎一致）；必要时后续上 F-DISCARD |
| F7 | 旧场景读不回 | 磁盘路径误用 `skipUnknownField=false` | missing field 硬失败（`Serializer.cpp:1286-1296`） | §C1：磁盘读已确认为 true；新字段不得出现在 transient/PIE 独占路径上 |
| F8 | 旧引擎读新场景 | 新枚举值 + `strictTypeCheck=true` | 该字段读失败（`Serializer_Legacy.cpp:390-396`） | 磁盘路径 `strictTypeCheck=false` ⇒ 降级为「跳过 + 保留默认」（`SerializationTypes.h:18-19`）；仍应在变更记录里标注「新场景需要 ≥ 某版本」 |
| F9 | 呈现参数被当开关 | 有人用 `FillAmount=0` 去实现「隐藏面板」 | 命中/级联语义错位 | I6 + Inspector tooltip（文案级，非机制级） |

## Verification

**V1 — 纯函数 oracle（无 RHI、无编辑器，进 `minEngineTests`）**
- `ComputeFilledRect`：`amount in {0, 0.25, 0.5, 1}` x `origin in {Left,Right,Bottom,Top}` x 退化 UV；断言 ① 可见区域尺寸按轴 `= size*f`；② **纹理密度不变** `w1/du1 == w/du`；③ `amount==1` 时输出与输入逐位相等。
- `ComputeNineSliceQuads`：`border=0` ⇒ 1 个 quad 且与整块矩形/UV 相等（旧行为等价）；border 刚好一半 ⇒ 中心块宽 0 不出 quad；border 超过一半 ⇒ 按比例钳制、无负尺寸；9 块矩形**无缝且不重叠**（相邻边界相等，边框为整数时可精确断言）。
- `ComputeTextBaselineY`：`Top` ⇒ `= ascender`（与 `TextMeshBuilder.cpp:40` 现有行为一致）；`Middle`/`Bottom` 用已知 `H/L/A` 断言；`L > H` 时的钳制行为（贴顶）。

**V2 — 组件层 oracle（无 RHI）**
- 默认值：新建 `ImageComponent` / `TextComponent` / `SpriteComponent` 断言新字段默认值 == I1。
- clamp：`SetFillAmount(-1) ⇒ 0`、`SetFillAmount(2) ⇒ 1`；`NaN` 定义为 0（写进测试）。
- 反射面：新字段在反射里可见且 `EditAnywhere`（`ForEachPropertyInHierarchy` 计数），可序列化（round-trip 一次）。

**V3 — 材质槽位 oracle（见 §H2）**：表 ↔ 图 ↔ Apply 三方一致；槽位连续；名字唯一。

**V4 — 缓存不变量**：`SpriteMaterialFactory::GetCacheStats().TemplateCompiles`（`SpriteMaterialFactory.cpp:17-24`、221-226）在本轮改动后**不得增加**（本轮 0 新变体）。这条同时是「没有引入编译风暴」的回归门。

**V5 — 目视项（明确不可脚本化，列清单不做门禁）**
- 进度条 0/25/50/75/100 五档截图；9-slice 拉伸到 3 种尺寸的角块不变形；文字三档垂直对齐在同一 widget 内的相对位置。
- 说明：`WidgetComponent::FillSceneProxy` 需要 `RenderSystem` + RHI（`WidgetComponent.cpp:291-305`、309-313），**代理级断言在无 GPU 的测试环境里拿不到**，因此 V5 只做人工清单，不假装它是 oracle。

**V6 — 兼容回归（进 `minEngineTests`）**
- 旧 `.mescene` / prefab 资产（挑仓库里现存的含 `ImageComponent`/`TextComponent` 的资产）加载成功、新字段取默认、无 error 日志。
- 枚举名序列化回归：新增枚举值不影响既有值名字。

**门禁**：V1–V4、V6 进 `scripts/verify.ps1`（单门禁）；V5 归档为交付说明里的截图清单。

## 兼容与迁移

### C1 缺字段（旧场景 → 新引擎）
磁盘读取路径已确认宽松：`AssetSerializeHelper::ReadObjectFromFile` 用 `skipUnknownField=true, strictTypeCheck=false`（`AssetSerializeHelper.cpp:79-82`），prefab 磁盘读取同样（`PrefabUtilityDetail.h:39-43`），语义在 `SerializationTypes.h:19-22` 明确写着「true = 缺字段保留默认」。反序列化循环按 `EnterField` 命中，缺字段直接返回 true（`Serializer.cpp:1286-1296`）。
⇒ **追加字段零迁移**：旧 `.mescene` / `.meprefab` 不需要改写，新字段取构造默认值（I1 保证渲染不变）。

### C2 内存克隆 / PIE / 临时缓冲（strict 路径）
`kPrefabCloneOptions` 与 PIE 克隆用 `skipUnknownField=false, strictTypeCheck=true`（`PrefabUtilityDetail.h:33-37`、`SceneDuplicator.cpp:14-17`），且 `Serializer` 在这些入口强制 false（`Serializer.cpp:1624`、1646、1716、1751）。
⇒ 这些是**同版本进程内**的写-读配对，字段必然同时存在，不构成兼容风险；但**新字段不得只存在于该路径**（即不得把字段标成 `Transient` 而仍期望它被存盘——`Transient` 会被跳过，`Serializer.cpp:1274-1277`）。

### C3 枚举与版本
- 枚举按**名字**写（`Serializer_Legacy.cpp:219-229`）与按**名字**读（382-396）⇒ 追加枚举值对**新引擎读旧盘**完全安全。
- **旧引擎读新盘**：若 JSON 里出现它不认识的枚举名，`FindByName` 返回 null ⇒ 读失败，但在磁盘路径下被 `strictTypeCheck=false` 降级为「跳过 + 保留默认」（`SerializationTypes.h:18-19`）。
⇒ 结论：追加枚举可接受；但要在变更记录里注明「含新枚举值的资产需要 ≥ 该版本引擎，否则该字段回落到默认」。**禁止重排**仍成立，且理由是纪律性的：重排会让任何 `static_cast<int>`（含日志、Lua、编辑器下拉索引）悄悄错位，而这不会在序列化测试里暴露。
- 版本闸门：`ValidateDiskSchema` 只在 `schemaVersion > 支持版本` 时失败（`DiskVersion.h:18-21`）。**本轮不 bump schema 版本**（追加字段不需要，且 bump 会让新盘在旧引擎上被直接拒收，得不偿失）。

### C4 编辑器与 prefab override
- Inspector 通用绘制（`SceneEditorInspectorSource.cpp:388`、431）+ 代码生成（`ImageComponent.gen.cpp:1`）⇒ 新字段**自动出现**，实现期只需重跑 `scripts/minEngine_header_tool.py`。这是一步工程动作，不是设计风险。
- prefab override 以**属性名**为单位记录属性路径；扁平字段沿用现有名字空间，不引入 `Style.*` 前缀路径（§H1 方案 A 的直接收益）。

## 切片表

> 「结束仍绿」= `scripts/verify.ps1` 单门禁全绿；「可回退」= 该片独立 `git revert` 后其余片仍自洽；「oracle」= 可自动断言者，标注「目视」者不计入门禁。

| 切片 | 做什么 | 结束仍绿 | 可回退 | 自动 oracle |
|------|--------|----------|--------|-------------|
| **S01 FillAmount（第一片，唯一有真实需求）** | 新 `EUIImageFillOrigin`（追加）；`ImageComponent` 加 `FillAmount`(float, 默认 1) + `FillOrigin` + setter（clamp + dirty）；`ComputeFilledRect` 纯函数；`WidgetSceneProxy` 承载填充后的矩形/UV（直接写既有 `m_TopLeftPx/m_SizePx/m_UVRect`，不新增冗余字段）；`FillSceneProxy` 按比例收缩 | 既有 UI 测试（`ScreenUIImageTest` / `ScreenUITextTest` / `ScreenUIHitTest` / `UILayoutTest` + `SpriteTranslucencyTest`）不变；V1/V2 新增 | 是：字段默认 1、纯追加，删字段即回到今天 | V1（含纹理密度等式）、V2（clamp/默认）、V6（旧场景） |
| **S02 文字垂直对齐** | `FontGlyphAtlas` 加 `m_Descender` + getter；`EUITextVerticalAlignment`（追加，默认 Top）；`TextComponent` 字段 + setter + dirty；`TextMeshBuilder::Build` 加参数，baseline 按 §H5 公式 | 既有 `ScreenUITextTest` 全绿（Top 路径逐位一致） | 是：默认 Top = 今天 | V1（baseline 公式）、V2（默认/枚举可见） |
| **S03 9-slice（CPU 9-quad）** | `ImageComponent` 加 `BorderPx`(Vector4, 默认 0)；`ComputeNineSliceQuads` 纯函数；`WidgetComponent` 在边框非零时走 per-widget VB/IB（local-pixel 路径，I4 恒等 UV），为零时走原单 quad | 既有 UI 测试全绿；零边框路径逐位一致 | 是：默认 0 边框走老路径 | V1（9 块无缝/退化/钳制）、V2、V4（`TemplateCompiles` 不增） |
| **S04 Sprite Pivot（可选）** | `SpriteComponent` 加 `Pivot`(Vector2, 默认 0.5,0.5)；模型矩阵与 `GetBoundingBox` 按 pivot 偏移 | `SpriteRuntimeConsumeTest` / `SpriteTranslucencyTest` 全绿 | 是：默认 0.5 = 今天 | V1（矩阵/AABB 数学） |
| **S05 槽位地基 + 兼容收尾** | `SpriteMaterialSlots.h` 外提 + 取消 `translucent?4:3` 分叉；`DescribeScalarParameters` 只读访问器；旧场景/prefab 兼容回归用例；文档与注册表状态同步 | `verify.ps1` 全绿；V3/V4/V6 新增 | 是：纯重构，行为等价（参数值不变） | V3（三方一致）、V4（编译次数不增）、V6 |

**为什么 FillAmount 排第一**：它是 backlog:17 唯一有实际痛感的项；S01 完成后消费方的 20 段拼贴即可退场，其余三项都是体验改善，可任意排序。S05 放最后是因为它是给**下一批**（radial / clip / tiling）铺路的卫生工作，不阻塞任何一片。

## Deferred

| 项 | 触发条件（什么时候重新排期） |
|----|------------------------------|
| U4 换行/溢出 | S02 落地后，且有真实多行文本需求（对话框/日志/物品描述） |
| U5 描边/阴影 | 有可读性需求（例如 UI 叠在动态背景上），且愿意接受材质变体成本 |
| U6 裁剪/遮罩 | 先做「viewport/scissor 解耦」独立设计（`RHI.h:110-121`、`VulkanRHI.cpp:2250-2256`）+ UI 命中矩形裁剪（`ScreenUIHitTester.cpp:148-149`） |
| U7 布局容器 | 手工摆位成为主要成本时；与 `UILayoutPass` 的算法演进合并设计 |
| U8 ScrollView | U6 + U7 都落地后 |
| 9-slice + FillAmount 同时生效 | 有「美术边框 + 内部填充」需求时；届时按 §H4 推翻条件③ 重估锚点 |
| 径向/角度填充 | 有环形进度需求时；走 F-DISCARD 变体（需 §H2 全套地基） |
| S2 Tiling | 有地形/背景重复需求，且已接受 wrap 采样状态引入 |
| S3 Flip | S03 落地后（需交换边框），且有消费方需求 |
| S4 排序层统一 | 出现 UI 与 Sprite 交错排序的真实需求；独立设计 |
| S5 图集 margin/extrude | ASSET-F01 推进时 |
| S6 帧动画 | 「是否走 AnimationGraph」定案后 |
| U11 alpha 遮罩命中 | 主人另一条线（与遮挡语义一起）；本设计不留字段 |

## 假设清单（无需审批，直接按此执行）

1. 磁盘场景/prefab 读取路径会一直保持 `skipUnknownField=true`（`AssetSerializeHelper.cpp:79-82`、`PrefabUtilityDetail.h:39-43`）。若某条新读取路径改成 strict，则该路径下新增字段会硬失败——实现期若新增读取入口，必须显式带这两个 option。
2. `PropertyPath` 与 prefab override 以属性名为单位、不支持嵌套内联结构（`PropertyPath.cpp` 与 `Serializer.cpp:1504-1537` 的按名解析为证）⇒ 本设计采用扁平字段。
3. `WidgetSceneProxy` 是 widget 呈现数据的唯一真源，`FillSceneProxy` 是唯一装配点（`WidgetComponent.cpp:323-390`）⇒ 填充/切片只改这里，不改布局。
4. 文字单行是当前唯一渲染形态（`TextMeshBuilder.cpp:42-100`）⇒ 垂直对齐只改 baseline，不改行数。
5. 本轮不新增材质标量参数（S01/S03 均 CPU 路径）⇒ 不需要扩变体 key；S05 只做等价重构。
6. 消费者（`minCraft*`）只读；本设计涉及的任何 API 追加都必须保持现有符号与行为。

## 待主人审批的决策清单

> 每条：选项 / 推荐 / 一行理由 / 推翻条件。**默认按推荐执行；被推翻的那条改后再开工。**

| # | 决策 | 选项 | 推荐 | 一行理由 | 推翻条件 |
|---|------|------|------|----------|----------|
| **D1** | 字段归属 | (a) 扁平字段 + 前缀 (b) 嵌套 `UIImageStyle` / `UITextStyle` | **(a)** | 反射生成器与 Inspector 都不支持分组（`ReflectionAnnotations.h:12-22`、`SceneEditorInspectorSource.cpp:388`），抽结构只换来嵌套属性路径与 override 迁移成本 | 出现第二消费方要整套参数，或字段数 > 12 |
| **D2** | FillAmount 锚点 | (a) 代理矩形 + UV 同步缩放 (b) 着色器 discard (c) 独立 VB | **(a)** | 零着色器改动 + 纹理密度可证明不变 + 与布局解耦 | 需求要求「边框固定只填内部」或径向填充 |
| **D3** | 9-slice 锚点 | (a) CPU 9-quad (b) 片元分段 UV | **(a)** | 复用文字已跑通的 per-widget VB 路径，且不碰材质槽位/变体链 | 同屏 9-slice widget 数量级上升并被 profile 证明是热点；或需要圆角 |
| **D4** | 槽位表 | (a) 外提 `SpriteMaterialSlots.h` 并取消 `translucent?4:3` 分叉 (b) 维持现状 | **(a)** | 分叉是「加参数必踩」的坑；外提后「改参数不生效」可自动断言 | 实现期发现 opaque 图多一个未连线节点会改变编译产物（需实测） |
| **D5** | 本轮是否新增材质变体 | (a) 不新增 (b) 为 Fill 新增 discard 变体 | **(a)** | 本轮两片都走 CPU，新增变体是纯风险无收益 | D2/D3 被推翻转着色器方案时，本条自动失效 |
| **D6** | 是否暴露 descender | (a) 暴露 `GetDescender()`（1 字段 + 1 getter） (b) 不动 | **(a)** | 对齐不需要它（`lineHeight` 已足够），但光学居中和将来的换行需要，成本近乎零 | 需要「零新增 API」的严格冻结期 |
| **D7** | 换行/溢出 | (a) 后置 (b) 纳入本轮 | **(a)** | 它把单行布局变成多行排版，回归面比 S02 大一个量级，且无消费方需求 | 出现真实多行文本需求且 S02 已交付 |
| **D8** | Sprite Pivot | (a) 纳入本轮 S04 (b) 后置 | **(a)** | 零耦合、零着色器、默认值保持旧行为；让 SPR-F08 有可交付项 | 本轮排期紧则降为后置，不影响 S01–S03 |
| **D9** | Fill 方向编码 | (a) 单枚举 `EUIImageFillOrigin{Left,Right,Bottom,Top}` (b) Mode + Origin 两枚举 | **(a)** | 方向由枚举隐式携带，少一个枚举面、少一处可写错的组合 | 径向填充获批（需再加 Mode，届时用追加方式） |
| **D10** | Flip 的排期位置 | (a) 9-slice 之后 (b) 提前 | **(a)** | 翻转九宫格必须交换边框，顺序颠倒会先做出一版错的 UV 语义 | 若 S03 被砍，Flip 可独立提前（那时无边框可交换） |

**审批方式**：逐条回「D# 同意 / 改为 (x)」即可；未提及的按推荐执行。审批通过后，本文件状态改 Approved，实现从 S01 开始。

## 变更记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v1 | 2026-10-20 | 首版：能力三档（本轮 U1/U2/U3/S1；后置 U4–U8 + S2–S6；不做 U9/U10/U11/S7）、字段归属（扁平 + 纯函数层）、槽位表方案与四条不变式、两个锚点判定、文字分层、CORE-F43 正交边界、切片表 S01–S05、决策清单 D1–D10。**仅设计，未授权实现。** |
