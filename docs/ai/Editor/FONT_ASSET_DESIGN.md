# Font 资产 — 设计案（M5 / M5b 排版角色）

Last updated: 2026-05-26  
Status: **M5a/M5b/M5.1 已合入** | **CJK 可读显示（字体资产）后置 i18n**  
父文档：[EDITOR_APPEARANCE.md](./EDITOR_APPEARANCE.md) §8、§9  
关联：[EDITOR_SHELL_DESIGN.md](./EDITOR_SHELL_DESIGN.md)、[ASSET_PIPELINE_P1_API.md](../Platform/ContentBrowser/ASSET_PIPELINE_P1_API.md)

---

## 0) 一句话

**`Font` = 与 `Texture2D` 同级的 `Asset` 子类**（磁盘 `.ttf` / `.otf`，Runtime 仅字节缓冲）；Editor 通过 **`EditorTypographyRole`（排版角色）** 为不同 UI 语义绑定 **不同 Font 资产 + 不同默认字号**（如正文 Inter Regular、标题 Inter SemiBold），在 **同一 ImGui 字图集** 内烘焙多 `ImFont*`，绘制时用 **`PushFont` / `EditorTypographyScope`** 切换；工程设置存 **角色表**，而非单一全局 `UiFontAssetGuid`。

---

## 1) 目标与非目标

### 1.1 目标

| # | 目标 | 阶段 |
|---|------|------|
| G1 | `Font` 类 + `AssetManager` 扫描/加载 | **M5a ✓** |
| G2 | `AssetTypeRegistry` 登记 `.ttf` / `.otf` | **M5a ✓** |
| G3 | `LoadAsset_Impl<Font>` → `m_FontFileBytes` | **M5a ✓** |
| G4 | **`EditorTypographyRole`**：语义化 UI 排版槽（Body、Heading 等） | M5b |
| G5 | 每角色：**`FontAssetGuid` + `SizePixels`**；默认可分别指向 Inter Regular / SemiBold | M5b |
| G6 | `EditorAppearance::RebuildUiFontAtlas`：为 **每个已配置角色** 调用 `AddFontFromMemoryTTF`；移除 `FontGlobalScale` | M5b |
| G7 | **`EditorTypographyScope`**（或等价 API）：RAII `PushFont`/`PopFont`，避免散落魔法 | M5b |
| G8 | 关键窗口先接线（菜单、面板标题、Inspector 区段标题、正文） | M5b |
| G9 | 工程 `.mesettings` 序列化 **`EditorTypographySettings`** | M5b |
| G10 | **`bEnableCjkGlyphs`**：构建 atlas 时对所有角色 **同一套** glyph range 合并策略 | M5.1 |
| G11 | 改角色字体/字号后重建字图集；缺字/坏 GUID **不崩溃** | M5.1 |

### 1.2 非目标

| # | 非目标 |
|---|--------|
| NG1 | Runtime / `Font` **include imgui** 或持有 `ImFont*` |
| NG2 | Icon font、toolbar 图标字体 |
| NG3 | 游戏内 UI 运行时消费（类型预留） |
| NG4 | `.mefont` sidecar、Font Inspector 编辑 |
| NG5 | FreeType、子集化、SDF、可变字体轴 |
| NG6 | **任意控件** 在 `.mesettings` 里逐条配字体（过细）；M5b 只认 **固定角色枚举** |
| NG7 | M5b **不要求** 一次 grep 改完所有窗口；允许分波次接线，但 **基础设施一次到位** |
| NG8 | **CJK 可读 UI 显示**（含 Noto 等含字形 TTF、locale 选字体）：**后置 i18n**；M5.1 仅提供 `bEnableCjkGlyphs` + atlas 合并字集，**不保证** Inter 等西文字体下中文正常显示（缺字仍为 `?`） |

---

## 1.3 设计取向（为何用「排版角色」而非单一字体）

| 维度 | 看法 |
|------|------|
| **产品** | 标题 SemiBold + 正文 Regular 是常见编辑器层级，比全局 `FontGlobalScale` 或「全 UI 一个 TTF」更接近现代 UI（Figma / VS Code token 思路）。 |
| **技术** | ImGui **原生支持** 单 atlas 多 `ImFont*` + `PushFont(font, size)`；不同字重 = 不同 TTF 文件 = 多次 `AddFontFromMemoryTTF`，与现有 `Font` 资产模型一致。 |
| **工程** | 用 **固定角色枚举** 约束复杂度：设置里是「Body 用哪个 GUID、多大」，而不是每个 `ImGui::Text` 写 GUID。 |
| **风险** | ① 漏 `PushFont` 的控件仍走 `Fonts[0]` 默认体 — 需约定 **默认体 = Body**；② 多角色 × CJK 字集 → atlas 变大 — M5b 先英文 glyph，CJK 在 M5.1 统一开关；③ 全仓库改绘制点 — **M5b 先基础设施 + 代表性窗口**，M6 可顺带清扫。 |

**不推荐回退的方案：** 仅用一个 Inter Regular，标题靠 `PushFont(NULL, 更大字号)` — **无法实现 SemiBold 字重**（除非再 MergeMode 同 family，复杂且易踩 ImGui 合并坑）。用户明确要求 Regular / SemiBold 分文件，故 **每字重至少一个角色槽**。

---

## 2) 架构分层

```text
磁盘:  Project/Content/Fonts/Inter-Regular.ttf
       Project/Content/Fonts/Inter-SemiBold.ttf
          │
          ▼
AssetManager → std::shared_ptr<Font>  (仅字节, 无 ImGui)
          │
          ▼  (仅 Editor)
EditorAppearanceSettings.Typography[]
  角色 Body     → { Guid(Inter-Regular), SizePx=14 }
  角色 Heading  → { Guid(Inter-SemiBold), SizePx=16 }
          │
          ▼
EditorAppearance::RebuildUiFontAtlas()
  foreach role with valid Font:
    AddFontFromMemoryTTF(bytes, role.SizePixels, cfg, glyphRanges)
  Fonts[0] := Body 的 ImFont*   // ImGui 默认字体 = 正文
          │
          ▼
绘制:  EditorTypographyScope scope(Heading);
       ImGui::Text("Hierarchy");   // SemiBold 16px
       // scope 析构 PopFont
```

**原则（不变）：**

- 真源在磁盘 TTF/OTF；Runtime 中立。
- ImGui / atlas / `PushFont` 仅在 Editor。

---

## 3) `Font` 数据结构（Runtime，M5a 已定稿）

路径：`minEngine/minEngine/src/Runtime/Resource/Font.h`（**已实现**）。

```cpp
ME_CLASS()
class Font : public Asset
{
    // m_FontFileBytes, m_SourceExtension
    // GetFontFileBytes(), GetSourceExtension(), IsValid()
};
```

`FontLoader::LoadFromAssetMeta` + `LoadAsset_Impl<Font>` + Registry **已合入**（见 M5a commit 待用户提交）。

**刻意不放：** `ImFont*`、per-asset 默认字号（字号归属 **排版角色**，见 §6）。

---

## 4) `FontLoader` 与 `AssetManager`（M5a）

与 §3 实现一致：`FontLoader` 从 `AssetMeta` 解析绝对路径，二进制读入 `m_FontFileBytes`。

扫描 / Import：`.ttf` / `.otf` → `AssetType="Font"`。

---

## 5) 引擎与工程默认字体（Inter）

### 5.1 推荐默认资产（方案 A）

用户计划将字体放在 **Project**；引擎默认仍建议提供回退，便于零配置开箱：

| 角色 | 建议文件 | 建议默认字号 |
|------|----------|----------------|
| **Body** | `Inter-Regular.ttf`（或 `Inter_24pt-Regular.ttf`） | **14** px |
| **Heading** | `Inter-SemiBold.ttf`（或 `Inter_24pt-SemiBold.ttf`） | **16** px |
| **Subheading**（可选，M5b 可先做 2 角色） | 同 Heading 或 Regular | **15** px |
| **Caption**（可选） | 同 Body | **12** px |
| **MenuBar**（可选） | 同 Body | **13** px |

路径约定（引擎回退）：

```text
{EngineDefaultAssetsRoot}/Editor/Fonts/Inter-Regular.ttf
{EngineDefaultAssetsRoot}/Editor/Fonts/Inter-SemiBold.ttf
```

工程内建议：

```text
{ProjectContentRoot}/Fonts/Inter-Regular.ttf
{ProjectContentRoot}/Fonts/Inter-SemiBold.ttf
```

`.mesettings` 中角色 GUID 为 Zero 时：`EditorAppearance` 按 **路径解析 Meta → Load Font**；失败则该角色回退到 **Body 的字体** + 仍用角色自己的 `SizePixels`，并 `ME_CORE_WARN`。

### 5.2 仓库状态

- M5a 代码已就绪；**TTF 由用户放入 Project**（已沟通）。
- 引擎 `EngineDefault/Editor/Fonts/` 可后续补交 Inter（许可证允许再分发的前提下）。

### 5.3 不再使用「单一 DefaultUI.ttf」

原 v1 的 `DefaultUI.ttf` + `UiFontAssetGuid` **废弃**，由 **角色表 + 按角色回退** 替代（§6）。

---

## 6) 排版角色与工程设置

### 6.1 `EditorTypographyRole`（枚举，Editor 或 Runtime 均可放；建议 Runtime 以便 `.mesettings` 反射）

```cpp
ME_ENUM()
enum class EditorTypographyRole : uint8_t
{
    Body = 0,       // 正文、属性值、列表项
    Heading,        // 面板标题、窗口标题、一级区段
    Subheading,     // CollapsingHeader、二级区段（可选 M5b）
    Caption,        // 辅助说明、小字
    MenuBar,        // 主菜单（可选 M5b）
    Count
};
```

**M5b 最小集：** `Body` + `Heading`（满足 Inter Regular / SemiBold 需求）。其余角色可在同一 atlas 构建循环中预留，默认复制 Body 的 GUID 与字号常量。

### 6.2 `EditorTypographySlot`

```cpp
ME_STRUCT()
struct EditorTypographySlot
{
    ME_GENERATED_BODY(EditorTypographySlot)

    /** Zero → 使用引擎/工程默认路径解析（§5）。 */
    ME_PROPERTY()
    GUID FontAssetGuid{};

  /** 烘焙进 atlas 的像素字号；0 = 使用该角色的引擎默认（见 EditorTypographyDefaults.h）。 */
    ME_PROPERTY()
    float SizePixels = 0.0f;
};
```

### 6.3 `EditorTypographySettings`（写入 `EditorAppearanceSettings`）

```cpp
ME_STRUCT()
struct EditorTypographySettings
{
    ME_GENERATED_BODY(EditorTypographySettings)

    /** 长度 = EditorTypographyRole::Count；下标与枚举值对应。 */
    ME_PROPERTY()
    std::vector<EditorTypographySlot> Slots;

    /** M5.1：为 true 时，所有角色 AddFont 时合并中文全字集。 */
    ME_PROPERTY()
    bool bEnableCjkGlyphs = false;
};
```

`EditorAppearanceSettings` 增加：

```cpp
ME_PROPERTY()
EditorTypographySettings Typography{};
```

**序列化：** 首次打开旧 `.mesettings`（无 Typography）时，`LoadFromAppearanceSettings` 填充 **内置默认表**（Inter 路径 + 上表字号）。

### 6.4 与旧字段对照

| 旧（v1） | 新（v2） |
|----------|----------|
| `UiFontAssetGuid` | 删除；改为 `Typography.Slots[Body]` |
| `UiFontSizePixels`（全局） | 删除；改为 **每角色 `SizePixels`** |
| `bEnableCjkGlyphs` | 保留在 `EditorTypographySettings` |

### 6.5 为何仍不做 per-asset Meta

字号、字重组合属于 **UI 语义（角色）**，不是字体文件固有属性；同一 `Inter-Regular.ttf` 可同时用于 Body(14) 与 Caption(12)，靠 **不同 `SizePixels` 两次 AddFont** 即可（ImGui 按 size 烘焙不同 `ImFont*`）。

---

## 7) `EditorAppearance` 消费（Editor only）

### 7.1 职责

| API | 职责 |
|-----|------|
| `RebuildUiFontAtlas()` | 清空 atlas；按角色加载 `Font` 并 `AddFontFromMemoryTTF`；`Fonts[0]=Body` |
| `GetImFont(EditorTypographyRole)` | 返回已烘焙指针；无效则 Body |
| `LoadFromAppearanceSettings` | 主题 + 重建 atlas |
| `EditorTypographyScope` | 构造 `PushFont(GetImFont(role), 0.0f)`，析构 `PopFont` |

成员建议：

```cpp
std::array<ImFont*, static_cast<size_t>(EditorTypographyRole::Count)> m_RoleFonts{};
ImFont* m_BodyFont = nullptr;  // 冗余缓存 = m_RoleFonts[Body]
```

### 7.2 `RebuildUiFontAtlas` 流程

1. `io.Fonts->Clear()`；`io.FontGlobalScale = 1.0f`。
2. 解析 **Body** 的 `Font`（必须先成功或 `AddFontDefault()` 兜底）。
3. 对每个 `role in [0, Count)`：
   - 取 `slot = Typography.Slots[role]`（缺省则用 `EditorTypographyDefaults`）。
   - `sizePx = slot.SizePixels > 0 ? slot.SizePixels : Defaults::GetSize(role)`。
   - `font = ResolveFontAsset(slot.FontAssetGuid, role)`（含 §5 路径回退）。
   - 若 `font` 无效且 `role != Body` → 复用 Body 的 **字节** 但保留 **role 的 sizePx**（降级 warn）。
   - `AddFontFromMemoryTTF(..., sizePx, &cfg, ranges)` → `m_RoleFonts[role]`。
4. `io.Fonts->Build()`；销毁/重建 backend 字体纹理。
5. **`m_RoleFonts[Body]` 设为 `io.Fonts->Fonts[0]`**（保证未 Push 的控件是正文）。

**CJK（M5.1）：** `bEnableCjkGlyphs` 为真时，每个 `AddFont` 使用 **相同** 的合并 glyph range 列表（实现时按仓库 imgui 版本选 `MergeMode` 或多次 range 指针）。

### 7.3 绘制约定

| UI 语义 | 角色 | 示例 |
|---------|------|------|
| 窗口标题、Dock 标题 | `Heading` | `Hierarchy`, `Inspector` |
| `CollapsingHeader` 一级 | `Heading` 或 `Subheading` | Scene 组件块 |
| 属性名、普通 `Text` | `Body` | Details 行 |
| 灰字说明 | `Caption` | Tooltip 风格辅助行 |
| 主菜单项 | `MenuBar` | `File`, `Edit` |

**M5b 验收最低线：** MainMenu + 至少一个面板标题 + Inspector 一处正文/标题对比可见字重差异。

### 7.4 ImGui 1.92 注意

- 使用 `PushFont(imFont, 0.0f)` 保持 **AddFont 时烘焙的 LegacySize**（每角色已含字号）。
- **禁止** 用 `GetFontSize()` 传给 `PushFont`（会双重缩放）。
- 同一 TTF、不同角色不同 `SizePixels` → **两次 `AddFontFromMemoryTTF`**，得到两个 `ImFont*`（符合预期）。

---

## 8) 文件清单（实施参考）

| 路径 | 阶段 |
|------|------|
| `Runtime/Resource/Font.h`、`FontLoader.*` | M5a ✓ |
| `Runtime/.../EditorTypographyRole.h`（或并入 Settings 头文件） | M5b |
| `Runtime/.../EditorAppearanceSettings.h` | M5b 增 `Typography` |
| `Editor/.../EditorTypographyDefaults.h` | M5b 默认 GUID/路径/字号 |
| `Editor/.../EditorTypographyScope.h` | M5b |
| `Editor/.../EditorAppearance.*` | M5b `RebuildUiFontAtlas` |
| `Editor/src/Editor.cpp` | M5b 去掉 `FontGlobalScale` |
| 各 `EditorWindows/*` | M5b 分批 `EditorTypographyScope` |

---

## 9) 验收标准

### M5a（已完成）

- [x] `.ttf` / `.otf` Scan 为 `Font`
- [x] `LoadAsset<Font>` 返回非空字节
- [x] Runtime 无 imgui

### M5b

- [x] atlas 含至少 **Body + Heading** 两种 `ImFont*`（Inter Regular / SemiBold）
- [x] 未 Push 时 UI 为 **Body**；标题处 Push **Heading** 后字重明显变粗
- [x] 无 `io.FontGlobalScale = 1.5f`
- [x] `.mesettings` 可保存/加载 `Typography`（切主题 / CJK 开关时写入）
- [x] Heading 的 GUID 无效时不崩溃（回退 warn）

### M5.1

- [x] `bEnableCjkGlyphs` 对所有角色生效（`GetGlyphRangesChineseFull` 合并）
- [x] View→Typography→Enable CJK Glyphs 切换后重建 atlas
- [x] 主要 Editor 窗口排版清扫（MainMenu、Hierarchy、Inspector、Console、Toolbar、Viewport、Material）
- [ ] 手改 `.mesettings` 各角色 `SizePixels` 后自动重建（需重开工程或后续 Appearance UI）

**CJK 显示（后置）：** M5.1 的 `bEnableCjkGlyphs` 只扩展 **glyph range**；当前默认 Inter **无 CJK 字形**，开启开关仍无法正确显示中文。含 CJK 的 **Font 资产 + 工程/locale 策略** 在 **i18n 里程碑** 再做（见 NG8）。

---

## 10) 实施分期建议

| 步骤 | 内容 | 说明 |
|------|------|------|
| **M5a** ✓ | Font + Loader + Registry + AssetManager | 已实施 |
| **M5b-core** | `EditorTypographyRole`、`EditorTypographySettings`、`RebuildUiFontAtlas`、`EditorTypographyScope`、去 FontGlobalScale | 基础设施 |
| **M5b-wire** | MainMenu、Hierarchy/Inspector 标题等代表性 `PushFont` | 可同 PR 或紧跟 |
| **M5.1** | CJK、设置项重建、更多窗口清扫 | 与 M6 部分重叠可接受 |

---

## 11) 待审批项（v2）

| ID | 问题 | 建议 |
|----|------|------|
| **F1** | 是否放弃单一 `UiFontAssetGuid` | **是**，改为角色表 |
| **F2** | M5b 最少几个角色 | **Body + Heading**；Subheading/Caption/MenuBar 预留枚举，默认同 Body 或按表 |
| **F3** | 默认字号 | Body **14**，Heading **16**（可微调） |
| **F4** | 同 TTF 不同字号 | **两次 AddFont**（不同 `SizePixels`），不靠 `FontGlobalScale` |
| **F5** | 标题 SemiBold | **独立 Font 资产**（`Inter-SemiBold.ttf`），不用假粗体 |
| **F6** | CJK | **M5.1** glyph range + 开关；**可读显示** → **i18n**（CJK Font 资产） |
| **F7** | 引擎默认 Inter | 用户 Project 优先；引擎 `EngineDefault/.../Fonts/` 作 Zero-GUID 回退 |
| **F8** | 全窗口一次性改完 | **否**；M5b-wire 分波次，避免巨型 PR |

---

## 12) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-26 | M5 详细设计初稿 |
| 2026-05-26 | **v2**：M5a 标记完成；M5b 改为 **排版角色** 多字体/多字号；废弃单一 `UiFontAssetGuid`；Inter Regular/SemiBold 默认策略 |
| 2026-05-26 | **M5.1**：CJK glyph 合并、`SetCjkGlyphsEnabled`、全窗口 `EditorWindowTypography` / `EditorTypographyScope` |
| 2026-05-26 | **备注**：CJK **可读显示**（字体资产）后置 **i18n**；M5.1 不解决 Inter 缺字 |
