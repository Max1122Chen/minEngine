# UI-F04 — ScreenUI Text / Font

## Meta
- **ID:** UI-F04
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-24
- **Related:**
  - [UI-F01](./UI-F01_UI_SYSTEM_DESIGN.md)（Canvas / Widget / Image）
  - [UI-F02](./UI-F02_SCREENUI_HITTEST_DESIGN.md) · [UI-F03](./UI-F03_SCREENUI_BUTTON_DESIGN.md)
  - [FONT_ASSET_DESIGN](../../Editor/FONT_ASSET_DESIGN.md)（Font 字节资产；**NG3** 故意不含游戏消费）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** UI-F01 Done；现有 Font Asset + FontLoader；ScreenUIPass
- **Blocks:** HUD 标签 / 菜单文字；不挡 Rich Text / Focus / Lua

## TL;DR

ScreenUI 今天只有 Image 色块，**没有游戏侧文字**。已有 Font 资产只缓存 TTF/OTF 字节，消费面在 Editor ImGui。本 Feature 增加薄 **TextComponent**（仿 Image：同 GO 上 Widget 几何 + Text 视觉），用引擎已链接的 **FreeType** 栅格化 → **每 Font×字号 一份 Glyph Atlas Texture**，在现有 **ScreenUIPass** 里以 **多 glyph 四边形** 绘制。不做 Rich Text / SDF / 完整排版引擎。

## Scope

### In（MVP）
- TextComponent：Content / Font 资产引用 / FontSize / Color / 简单水平对齐（Left|Center|Right）
- 同 GO 需 WidgetComponent（Layout + Hit 矩形）；缺则不画
- Runtime：**GlyphAtlas**（Font 字节 + pixel size → atlas Texture2D + glyph metrics 缓存）
- 栅格后端：**FreeType**（引擎已链接 reetype；**禁止**把 ImGui atlas / ImFont* 当游戏路径）
- 绘制：复用 ScreenUI Unlit Material 路径；**一 Text = 一动态 VB（N quads）= 一 MeshDrawCommand**（扩展 BuildScreenUIQueue / proxy，不新建 Pass）
- Hit：仍用 Widget ComputedRect（非整字精确命中）
- 单测：空串 / ASCII 度量 / atlas 不崩；可选 ASCII 目视 smoke
- ASCII + Basic Latin 为主；CJK **可渲染若 Font 含字形**，不做子集化/回退链

### Out（后置）
- Rich Text / 超链接 / 阴影·描边材质特化
- SDF / MSDF；可变字体轴；HarfBuzz shaping
- 自动换行 / 垂直对齐 / ellipsis / 本地化表
- 九宫格 Text；World-space Text
- Lua 绑定 Text；Focus 键盘输入
- 新建 TextSystem / 平行 UI Framework
- 改 Editor ImGui 字体管线（已是 FONT_ASSET 范围）

## Reader quick start
1. §Current：Image 画法 + Font 字节资产
2. §Proposed：TextComponent + GlyphAtlas + ScreenUI 多 quad
3. §Alternatives + Acceptance

---

## Problem

minCraft / Demo HUD 需要可读文字。当前只能贴图假字。Editor 有 Font，但 [FONT_ASSET_DESIGN NG3](../../Editor/FONT_ASSET_DESIGN.md) 明确不做游戏 UI 消费。UI-F01/F03 亦把 Text 后置。

## Requirements

- 作者可在 Scene 放 Text：字符串 + Font 资产 + 字号 + 颜色，Play 可见
- Edit 下 Layout 矩形正确（与 Image 同 Widget 模型）
- 缺 Font / 坏字节 / 缺字形：**不崩溃**（缺字可跳过或 ?）
- 不强迫游戏走 Editor / ImGui API

## Constraints

- **Capabilities, not Opinions**：只提供 Text 绘制机制；不规定 HUD 架构
- Runtime **不得** #include imgui 或持有 ImFont*
- 继续走 ScreenUIPass；不进 Opaque/Translucent 主队列
- Widget = Layout；视觉 = 专用 Component（对齐 F01 Image 切分）
- FreeType 已链入；MVP 优先复用，不新引 HarfBuzz

## Preferences

- API 形态对齐 Image（同 GO 复合）
- Atlas 按需增长；先固定上限（如 1024²）+ 溢出失败日志
- 字符串变更才重建 VB / 按需补 glyph

## Assumptions

- MVP 字串以 ASCII 为主足够验收；CJK 依赖用户选含字形的 TTF
- 一 Text 数百 glyph 足够 HUD；不做百万字批处理优化
- 现有 ScreenUI Material（Sprite Unlit）可绑 atlas + tint

## Non-Goals

见 §Out。

## Current Architecture

```text
Canvas.Tick → UILayoutPass → Widget.ComputedRect
Widget (+ Image) → WidgetSceneProxy → RenderScene.m_WidgetSceneProxies
ForwardRenderer::BuildScreenUIQueue → ScreenUIPass（像素 Ortho）
```

- ImageComponent：Texture × Color → 单 SpriteQuad
- Font：m_FontFileBytes only（Runtime/Resource/Font.h）
- FreeType：minEngine/CMakeLists.txt 已 PRIVATE link；当前主要服务 Editor imgui_freetype

## Proposed Architecture

```text
Font (bytes) ──► FontGlyphAtlasCache ──► Texture2D atlas + GlyphMetrics
                                              │
TextComponent (Content, FontRef, Size, Color) │
        │                                     │
        └─► TextMeshBuilder (N quads UV) ◄────┘
                │
         TextSceneProxy / extend WidgetSceneProxy
                │
         BuildScreenUIQueue → ScreenUIPass
```

### API（作者面）

| 类型 | 职责 |
|------|------|
| TextComponent | Content、Font、FontSize、Color、HorizontalAlignment；脏标重建 mesh |
| WidgetComponent | 几何 / Hit（不变） |
| Font | 仍只持字节；**不**持 GPU |

### Runtime 服务（建议名，实现可微调）

| 单元 | 职责 |
|------|------|
| FontGlyphAtlas | 单 Font×Size：FT_Face、atlas 像素、codepoint→UV/advance |
| FontGlyphAtlasCache | 全局/子系统级缓存；按 AssetGuid+size 复用 |
| TextMeshBuilder | 字符串 → 顶点（**Widget 局部像素**；见 §Implementation locks） |

**不**新建顶层 UITextSystem：挂在现有 UISystem 或 Render/UI 辅助命名空间即可。

### 绘制语义

- `finalRGB = white * tint`；glyph 灰度进 **alpha**（R 或 A 通道采样，见 locks）
- 对齐：ComputedRect 内水平 Left/Center/Right；垂直 **Top**
- 超出 Widget：**溢出**（裁剪后置）

## Responsibilities

| Unit | Responsible for | Not for |
|------|-----------------|--------|
| TextComponent | 作者属性 + 脏标 + 请求 mesh | Layout 算法；Hit 选举 |
| FontGlyphAtlas* | 栅格与 atlas | Scene / Pass |
| BuildScreenUIQueue | 收集 Text proxy 为 DrawCommand | 栅格化 |
| Font Asset | 字节真源 | GPU / ImGui |

## Boundaries

- **Allowed：** Text → Font asset；Text → Widget sibling；Atlas → Texture2D / RHI；Queue → ScreenUIPass
- **Forbidden：** Runtime → imgui；游戏强制 EditorTypographyRole；Text 绕过 ScreenUI 进主 MeshBatchCollector

## Dependencies

- Depends：UI-F01、Font/FontLoader、FreeType、ScreenUIPass、Sprite Unlit material factory
- Depended：minCraft HUD、未来 Focus/输入框
- New direction：Runtime UI **主动**用 FreeType（今日仅 Editor 侧重用）— 合理，因链路已在引擎内

## Ownership & Lifetime

- Atlas 缓存：引擎子系统或 UISystem 持有；Shutdown 释放 FT_Face / Texture
- TextComponent 不拥有 Atlas；只持弱引用/key
- Font Asset 卸载：atlas entry 失效并重建或丢弃（MVP：Guid 失效 → 下次绘制跳过 + log）

## State & Invariants

- Text 无 Widget 或面积≤0 → 不提交 draw
- Atlas 溢出 / FT 失败 → 不崩；该 Text 跳过或只画已缓存 glyph
- 同 Guid+Size 共享一份 atlas

## Data / Control flow

1. Layout：Canvas Tick → Widget rect
2. Text Tick/EndOfFrame：若 Content/Font/Size/rect 脏 → 确保 atlas glyphs → 重建 VB
3. Renderer：收集 proxy → ScreenUI sort（CanvasSortOrder, StableOrder）→ Draw

## API / Interface semantics

- SetContent / GetContent：UTF-8 std::string（MVP 按 codepoint 迭代；复杂 grapheme 后置）
- SetFont(AssetRef/Guid)：空 Guid = 不画
- Mutation：作者改属性 → dirty；**不**每帧全量栅格

## Failure behavior

| 失败 | 行为 |
|------|------|
| Font 未加载 / 空字节 | 不画；Warning 一次 |
| FreeType init/face 失败 | 同上 |
| Codepoint 无字形 | 跳过或替换 ?（实现选一，单测固定） |
| Atlas 满 | 停止新 glyph；Warning；已有字仍可画 |

## Alternatives considered

| 方案 | 结论 |
|------|------|
| **A. FreeType + Glyph Atlas + 多 quad（推荐）** | 与现有 Pass/Material 契合；可扩展 CJK |
| B. 整串烘焙成一张 Texture 再走 Image | 实现快，但改字重烘焙、内存差、对齐难 |
| C. 复用 ImGui font atlas | 违反 Runtime≠imgui；Editor 耦合 |
| D. 引入 HarfBuzz / SDF 一期做完 | 过重；违反 Prefer Simplicity |

## Complexity & risk

- **L2：** 新组件 + atlas + 队列扩展；无新 Pass / 无协议变更
- **Risk：** atlas 尺寸与 CJK；动态 VB 与现有单 quad proxy 模型扩展点要收窄文档

## Incremental plan

1. **S01** GlyphAtlas + FreeType 烟测（无 Scene）
2. **S02** TextComponent + 单测度量/不崩
3. **S03** ScreenUIQueue 多 quad 接线 + ASCII 目视
4. **S04** Docs / Registry Done

## Verification

- Suite screen-ui-text（名可调）：空串、ASCII advances、缺 Font 不崩
- Play：Canvas 内 Text 可见；Stop 无泄漏（手工 / 现有 teardown）
- 回归：screen-ui-coords / ui-layout / screen-ui-button 仍绿

## Acceptance

1. [x] Scene：Widget+Text+Font → Edit/Play 显示字符串（MCP accept_ui_text + 手验）
2. [x] 改 Content / Font / Color 经 Setter 刷新（Inspector 失焦提交 Content；ObjectPtr+Setter 走 Assign）
3. [x] 无 Font 不崩
4. [x] 不引入 imgui 依赖到 Runtime Text 路径
5. [x] Progress 记录验证命令


## Implementation locks（Design Review 2026-09-23）

开工前拍板（不再开放）：

1. **Image 与 Text 同 GO：** Widget 仅一条 visual proxy。MVP：**优先 Text**（存在可绘制 Text 时由 Text 填 proxy）；否则走现有 Image。同 GO 同时要「底图+字」→ 用两个 Widget（后置）。
2. **顶点空间：** Text VB 顶点在 **Widget 局部像素**（原点=ComputedRect 左上，Y 向下）。BuildScreenUIQueue 对 Text 使用 **Letterbox 映射后的平移（+均匀 scale）**，**不再**套用面向单位方块的 MakeWidgetModelMatrix 全缩放语义；Image 路径不变。
3. **缺字形：** 跳过该 codepoint（不插入 ?）；单测固定此行为。
4. **FontSize：** Canvas **参考分辨率像素**（与 Widget Size 同空间）。
5. **Edit 与 Play：** 与 Image 相同，Edit 视口也提交绘制（验收不限 Play）。
6. **灰度采样：** atlas 存灰度；材质/采样约定 **R（或 A）→ opacity，RGB=白 × Tint**；S03 若现有 Sprite Unlit 不够则加 ScreenUI Text 用材质变体，不改主 3D 材质。

## Open questions

1. ~~垂直对齐~~ → **Top**（已锁）
2. ~~超出 Widget~~ → **溢出**（裁剪后置）
3. ~~Atlas 键含 Color~~ → **否**
4. ~~纯色块字调试~~ → **不需要**
5. Atlas 初始尺寸 / 增长策略具体数字 — **Defer OK**（先 512 或 1024 方图）

## Design Review note

**Readiness: Ready with deferred items**（2026-09-23 Mode A）。方案 A 通过；见 §Implementation locks。可开 S01。

## 变更记录

| Date | Change |
|------|--------|
| 2026-09-23 | Draft：TextComponent + FreeType GlyphAtlas；复用 ScreenUIPass |
| 2026-09-23 | Design Review：Status→Planned；锁 Image/Text 优先级、局部像素 VB、缺字跳过、FontSize 空间 |
| 2026-09-24 | Implement Done + visual accept; Inspector string defer + ObjectPtr Setter Assign |
