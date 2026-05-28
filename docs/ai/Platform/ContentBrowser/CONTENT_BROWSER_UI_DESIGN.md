# Content Browser — UI 体验设计（P6.1）

Last updated: 2026-05-26  
Status: **P6.1 主体已合入**；**§2.6 P6.1-polish 已实现**（`ContentBrowserWindow` 面包屑 + Tile 网格）  
前置：[ASSET_PIPELINE_P6_API.md](./ASSET_PIPELINE_P6_API.md)（数据与窗口框架）  
依赖：[EDITOR_APPEARANCE.md](../../Editor/EDITOR_APPEARANCE.md)、[EDITOR_THEME_M6_DESIGN.md](../../Editor/EDITOR_THEME_M6_DESIGN.md)  
父路线：[EDITOR_PLATFORM_PLAN.md](../../Editor/EDITOR_PLATFORM_PLAN.md) §7

---

## 0) 背景

| 阶段 | 交付 | UI |
|------|------|-----|
| **P6（asset-workflow）** | `AssetTreeModel`、`ContentBrowserModule`、`ContentBrowserWindow`、Registry 订阅、Import/Refresh、Inspector 最小路由 | **故意裸 ImGui**；左树**仅目录**、右**三列表格**（与产品预期不符，P6.1 改） |
| **P6.1（当前）** | 窗口 UX 定稿 + Appearance 接入 + Inspector 修复 | 见 §2–§5 |
| **P7** | 默认 Dock、Import 到当前目录、Editor 打开路由 | 产品集成 |

**合并说明：** `feat/editor-asset-workflow` + Appearance M0–M6b 均已合入 `master`（`25552f9`）。

---

## 1) 用户拍板（2026-05-26）

| # | 决策 |
|---|------|
| **U1** | **右侧**主区为 **图标网格（Tile Grid）**；格子内预留 **Icon 位**（暂不接 Icon font，可用占位框 + 资产名） |
| **U2** | **面包屑路径**（当前 `ProjectContentRoot` 相对路径，可点击祖先跳转） |
| **U3** | 工具栏 **仅 `Import` + `Refresh`**；**搜索 / 类型过滤** → **后置选做** |
| **U4** | **`Delete` 从工具栏移除**；随 **右键上下文菜单** 再引入（与 Move/Reveal 等一并规划） |
| **U5** | **不要底栏摘要**（`DrawSelectionSummary` 删除）；选中信息只在 **Inspector** |
| **U6** | **左侧树 = 目录 + 该目录下已注册资产**（文件夹与资产同为树节点）；**不是**「左仅目录 + 右列表」的 P6 分割 |
| **U7** | **双击资产**：**不**打开 SubModule Editor；仅调用 **预留接口 + `Log`**；真实 **Editor 路由** 另立设计（P7 / E1 后） |
| **U9** | **Dock：** Scene 编辑布局将 Content Browser 放在 **Hierarchy/Inspector 下方，且在 Console 右侧**（贴合 Unity 习惯）；**Material Editor 默认不显示**（可通过 Window 菜单再打开 — 若后续支持） |
| **U10** | 单击目录仅做导航，**不清空当前 Asset 选中**；AssetMetaInspector 保留上一资产内容 |
| **U11** | **P6.1-polish — 面包屑**：扁平路径条；背景贴近 CB 面板；**无圆角**；**Caption** 字号；段可点（不用默认 `ImGui::Button` 样式） |
| **U12** | **P6.1-polish — Tile**：外框 **更宽**（见 §2.6 尺寸）；结构为 **竖矩形外框 → 正方形 Icon 占位 → 单行 AssetName**；名称 **单行 + 省略号**，限宽不超出 tile |

---

## 2) P6.1 目标布局（定稿）

```text
┌─ Content Browser ─────── EditorWindowTypography: Heading ─────────────┐
│ [Import] [Refresh]                                                    │
│  Assets > Materials > Examples          ← 面包屑（可点击）            │
├─ Content tree (~28–32%) ─┬─ Asset tile grid (剩余宽度) ─────────────────┤
│  ▼ Assets                │  ┌────┐ ┌────┐ ┌────┐                      │
│    ▼ Materials           │  │ ▢  │ │ ▢  │ │ ▢  │  ← Icon 占位          │
│      MyMat.mat           │  │Name│ │Name│ │Name│                      │
│      SubFolder           │  └────┘ └────┘ └────┘                      │
│        Other.png         │  （当前目录 flat grid；滚动）                 │
│  ▶ Textures              │                                            │
└──────────────────────────┴────────────────────────────────────────────┘
```

### 2.1 左栏 — 统一内容树（`ContentTree`）

| 项 | 说明 |
|----|------|
| 节点类型 | **目录节点**（可展开）+ **资产叶子**（对应该目录下、Registry 已注册且有 `.meta` 的资产） |
| 数据来源 | 扩展 `AssetTreeModel`（或 `ContentTreeModel`）：在 `DirectoryNode` 上挂 `std::vector<const AssetMeta*>`，Rebuild 时按路径填入 |
| 交互 | 单击目录 → 更新 **当前路径** + 面包屑 + **右侧网格**；单击资产 → `SetSelectedAsset` + Inspector |
| 展开 | 目录默认按磁盘结构；资产显示 **DisplayName**（`AssetMeta::AssetName`） |
| 视觉 | `EditorThemeScope` + Body 字体；目录与资产叶节点样式可区分（缩进 / 占位 icon 框，仍不接 Icon font） |

**与 P6 差异：** P6 左树仅 `BuildDirectoryNodeRecursive` 文件系统目录；资产只在 `GetAssetsInCurrentDirectory()` 供右表使用。P6.1 **必须在树模型中合并资产节点**。

### 2.2 右栏 — 图标网格（`AssetTileGrid`）

| 项 | 说明 |
|----|------|
| 范围 | **当前面包屑目录**下的资产（与 Registry 一致，D12：仅已注册资产） |
| 单元格 | 固定 tile 尺寸；上方 **Icon 占位区**（灰色框或首字母，预留后续 Icon / 缩略图 E2）；**详细布局见 §2.6** |
| 选中 | 单选；palette `Selection` 描边/底色；与树选中同步 |
| 双击 | `OnAssetTileActivated(meta)` → **仅 Log** + 空实现 hook（§3.2） |
| 移除 | P6 的 `Table`（Name/Type/Guid 列） |

### 2.3 工具栏与面包屑

| 控件 | 行为 |
|------|------|
| **Import** | 保持 `ImportAssetDialog()`（Import 目标目录 **P7** 再改当前路径） |
| **Refresh** | `RebuildDirectoryTree` + 重建当前目录网格；清空选中 |
| **面包屑** |  segments 来自 `m_CurrentDirectoryRel`；点击 segment → `SetCurrentDirectory`；`Assets` 根可点击；**视觉见 §2.6.1** |

### 2.6) P6.1-polish — 面包屑与 Tile 视觉规格（2026-05-26 拍板）

> **实现状态：** 已合入 `ContentBrowserWindow.cpp`（`DrawBreadcrumbLink` / `DrawAssetTile`）。  
> **动机：** 当前面包屑用默认 `Button`（高对比 + 圆角）；Tile 的 Icon 区非正方形、名称换行与外框未对齐。

#### 2.6.0 拍板参数（用户确认）

| 参数 | 决策 | 备注 |
|------|------|------|
| Tile 外框宽度 | **128 px**（偏宽，优于 96 px 密排方案） | 与当前实现 `tileWidth` 一致，便于迁移 |
| Asset 名称 | **单行 + 省略号** | 不用多行 `TextWrap`；超长用 `RenderTextEllipsis` 或等效 |
| 面包屑 | U11 | 见 §2.6.1 |
| Tile 结构 | U12 | 见 §2.6.2 |

#### 2.6.1 面包屑（`DrawBreadcrumb`）

| 项 | 规格 |
|----|------|
| 控件形态 | **扁平可点击段**，不用全局 `ImGui::Button` 默认样式 |
| 推荐实现 | `InvisibleButton` + `DrawList` 绘制 hover 底；或段级 `PushStyleVar(FrameRounding,0)` + Button 色设为 `ChildBg` / `PanelBackground` 同级 |
| 背景色 | 默认与 CB **Child 背景**一致；hover：`Selection` 或 `FieldBackgroundHovered`（低 alpha） |
| 圆角 | **0**（`FrameRounding = 0`） |
| 字号 | `EditorTypographyScope` → **`EditorTypographyRole::Caption`** |
| 分隔符 `>` | `TextMuted`，非按钮；`SameLine` 小间距 |
| 交互 | 与 §2.3 相同：`Assets` → 根；segment → `SetCurrentDirectory(prefix)` |
| 后置 | 路径过长时的中间省略（`A/.../C`）— **不在本 polish** |

#### 2.6.2 Tile 网格（`DrawAssetTileGrid`）

**单元结构（自上而下，均在固定外框内）：**

```text
┌─ tile outer: 128 × H ──────────┐
│ pad (4)                          │
│   ┌─ icon: 120 × 120 ─┐        │  S = outerWidth - 2×pad
│   │  占位（方框，无圆角） │        │
│   └────────────────────┘        │
│   gap (4)                        │
│   AssetName（单行，居中，省略）   │  可用宽 = 128 - 2×pad = 120
│ pad (4)                          │
└──────────────────────────────────┘
```

| 常量 | 值 | 说明 |
|------|-----|------|
| `kTileOuterWidth` | **128** | 列宽基准 |
| `kTilePadding` | **4** | 外框内边距 |
| `kIconSize` | **120** | `outerWidth - 2×padding`，**正方形** |
| `kIconLabelGap` | **4** | Icon 与名称间距 |
| `kTileOuterHeight` | **152**（建议） | `2×pad + icon + gap + captionLine + pad`；实现时按 Caption 行高微调 |
| `kTileSpacing` | **8** | 列/行间距（可从现 12 略收紧，实现时定） |

| 项 | 规格 |
|----|------|
| 命中区域 | 整个 **outer 矩形**（`InvisibleButton` 或覆盖大小的 `Selectable`） |
| Icon 绘制 | `AddRectFilled` + `AddRect` 边框；**rounding = 0**；水平居中于 outer |
| 名称 | **单行**；`labelMaxWidth = kTileOuterWidth - 2×kTilePadding`；**水平居中**；超出 **`…` 省略** |
| 裁剪 | `PushClipRect(outerMin, outerMax)`，文字与 Icon 不得画出 outer |
| 选中 | `palette.Selection` 外框描边和/或 outer 内低 alpha 铺底；与树选中同步不变 |
| 双击 | 保持 `ActivateAssetFromBrowser` + Log only |

**与当前代码差异（实施时对照）：**

| 现状 | 目标 |
|------|------|
| Icon 为宽条（约 112×48）+ `rounding=4` | 120×120 方框，`rounding=0` |
| `PushTextWrapPos` 多行换行 | 单行 `RenderTextEllipsis` |
| `Selectable` 110px 高与内容叠绘 | 固定 outer 高 + 分层绘制 |

#### 2.6.3 实现范围与文件

| 文件 | 变更 |
|------|------|
| `ContentBrowserWindow.cpp` | `DrawBreadcrumb`、`DrawAssetTileGrid`；可抽 `DrawBreadcrumbSegment` / `DrawAssetTile` |
| `ContentBrowserWindow.h` | 可选：`ContentBrowserViewMetrics` 常量 struct |
| Appearance | 面包屑 Caption；tile 选中/边框用现有 palette（§6 A4）；**不新增 TypographyRole** |

**本 polish 不做：** 缩略图（E2.3b）、Icon font、面包屑路径省略、CB 窗口级 `BeginPanel`（可同 PR 顺手，非必须）。

#### 2.6.4 Polish 验收（目视）

- [x] 面包屑段与 CB 右栏背景融合；无默认灰按钮感；无圆角；Caption 字号
- [x] 每 tile：128px 宽外框；Icon **正方形**；名称 **单行省略** 且不溢出
- [x] 单击选中 / 双击 Log 行为不变（代码路径保持）
- [ ] Dark / Light 主题下可读（需 Editor 目视）

### 2.4 明确不做（P6.1）

| 项 | 阶段 |
|----|------|
| 搜索框、类型过滤 Combo | **后置选做** |
| 工具栏 Delete | **右键菜单** 阶段 |
| 底栏 `DrawSelectionSummary` | **删除** |
| 双击打开 Material/Scene Editor | **P7 / Editor 路由设计** |
| Icon font / 真实缩略图 | Icon + 缩略图策略见 **[CB_TILE_DISPLAY_DESIGN.md](../../Editor/CB_TILE_DISPLAY_DESIGN.md)**（C1 Icon Font → C2/C3 缩略图） |
| PropertyWidgets 画 Meta | **E1** |
| Preview 嵌入 | **E2** |

---

## 2.5) Dock 与模块可见性（拍板 U9）

### 目标布局（Scene Editing）

在现有 `BuildSceneEditingLayout` 基础上，将 **Content Browser** 放在 **Hierarchy / Inspector 下方，且在 Console 右侧**：

```text
┌─ Viewport (main) ────────────────┬─ right column ─────────────────┐
│                                  │ ┌ Hierarchy ─┬─ Inspector ─┐ │
│                                  │ └────────────┴─────────────┘ │
├─ Console ────────────────────────┼─ Content Browser (full width) ┤
│                                  │                               │
└──────────────────────────────────┴───────────────────────────────┘
```

**实现要点（P6.1-layout）：**

| 项 | 做法 |
|----|------|
| Dock 注册 | `DockBuilderDockWindow("Content Browser", …)`；窗口 **ImGui 标题** 须与字符串一致 |
| 分栏顺序 | ① 自 `main` 分出 `rightColumn` + `console`；② 自 **`rightColumn`**（整列）`Split Down` → 上：`Hierarchy|Inspector`，下：`Content Browser`。**禁止** 仅自 `hierarchyArea` 向下拆（否则 CB 只占 Hierarchy 列宽） |
| Material 隐藏 | `ContentBrowserWindow::GetOwnerModuleId()` 返回 `SceneEditor::kModuleId`（`"Scene"`），复用 `EditorWindow::IsVisibleForActiveModule` |
| 切 Module | `ActivateSubModule(Material)` 时 CB **不绘制**；`SetContentBrowserInspectorActive(false)` 已在切工程/失焦路径考虑，**切 Material 时应清 CB Inspector 抢占** |
| Material 布局 | `BuildMaterialEditingLayout` **不** dock Content Browser（保持现状） |

### 与 P7 关系

- 默认 Dock 位 + Scene-only 可见性 → **本拍板，P6.1-layout 实施**。
- 用户手动 Save Layout、Material 下是否允许 Window 菜单打开 CB → **P7**（可选）。

---

## 3) 行为与接口

### 3.1 选择同步

```text
树选中资产 ──┐
             ├──► AssetWorkflowModule::SetSelectedAsset(meta)
网格选中资产 ──┘         │
                         ▼
              InspectorWindow → AssetWorkflowInspectorSource
              （共享 Inspector 停靠窗，§4）
```

- 选中目录仅更新导航（当前路径 + 面包屑 + 右侧网格）；**不清空**当前 Asset 选择；Inspector 继续显示最近一次选中的资产 Meta。

### 3.2 双击 — 预留接口（不打开 Editor）

```cpp
// 建议命名（Editor / AssetWorkflowModule）
void OnContentBrowserAssetActivated(const AssetMeta& meta, ActivateAssetIntent intent);
// P6.1 实现：ME_LOG_INFO(...); 不调用 OpenAsset() / ActivateSubModule
```

- P6 现有 `OpenAsset()` **保留**，Content Browser **不再调用**（网格/树双击改走新 hook）。
- 后续 **Editor 路由设计** 再接通（按 `AssetType` → SubModule、是否切 layout）。

### 3.3 右键菜单（后置，与 Delete 同批）

- Delete、Reveal in Explorer、Rename/Move 等 **不在 P6.1**。
- 设计占位：上下文菜单挂在树节点与 tile 上。

---

## 4) 已知缺陷：Asset Inspector 出现在 Debug 窗

### 4.1 现象

选中 Content Browser 资产时，Meta 四行出现在 **`Debug##Default`（或标题含 Debug 的浮动窗）**，而不是停靠的 **Inspector** 面板。

### 4.2 原因（代码审查）

| 组件 | `ImGui::Begin("Inspector")` |
|------|-----------------------------|
| `InspectorWindow::OnDraw` | 有选中时 **不** `Begin`，只调 `source->DrawInspector()` |
| `SceneEditorInspectorSource` / `MaterialEditorInspectorSource` | `DrawInspector` 内 **`EditorWindowTypography::BeginPanel(..., "Inspector")`** ✓ |
| `AssetWorkflowInspectorSource::DrawInspector` | **仅** `ImGui::Text`，**无** `BeginPanel` ✗ |

无当前窗口时，ImGui 将控件落入 **默认 Debug 窗口**。

### 4.3 P6.1 修复要求（验收 blocking）

- [ ] `AssetWorkflowInspectorSource::DrawInspector` 与 Scene 一致：在 **`EditorWindowTypography::BeginPanel`** 内绘制（或由 `InspectorWindow` 统一 `BeginPanel` 且各 Source **不再**二次 `Begin` — 二选一，推荐前者与 Scene 对齐）。
- [ ] 选中资产时 Meta 显示在 **Inspector 停靠窗**；不出现额外 Debug 窗。
- [ ] Content Browser 聚焦 + `SetContentBrowserInspectorActive` 逻辑保持。

---

## 5) 模型与文件（实施指引）

| 变更 | 说明 |
|------|------|
| `AssetTreeModel` | `DirectoryNode` 增加 `AssetChildren`；`RebuildDirectoryTree` 按目录填充 meta 指针 |
| `ContentBrowserWindow` | 重写 `DrawDirectoryTree`（资产叶）、`DrawAssetList` → `DrawAssetTileGrid`；增 `DrawBreadcrumb`；删 `DrawSelectionSummary`；工具栏删减 |
| `AssetWorkflowModule` | 可选 `OnContentBrowserAssetActivated`；`DrawInspector` 修 BeginPanel |
| Appearance | §6 接入清单 |

### 6) Appearance 接入清单

| # | 项 |
|---|-----|
| A1 | `EditorWindowTypography::BeginPanel` 窗口标题 |
| A2 | 工具栏 `Body` + `Import`/`Refresh` 间距 |
| A3 | 左右 `Child` 面板 `PanelOverlay` / Border token |
| A4 | 树节点 / tile 选中 `Selection` token |
| A5 | 空目录 / 空网格 `TextMuted` |
| A6 | **无**底栏 |

---

## 7) 验收标准（P6.1 Done）

- [ ] 左树可见 **目录 + 资产**；右为 **图标网格**（Icon 占位）
- [ ] 面包屑可导航；工具栏仅 Import + Refresh
- [ ] 无底栏摘要；工具栏无 Delete
- [ ] 双击资产 **仅 Log**，不切换 SubModule
- [ ] Asset Meta 在 **Inspector** 窗显示（§4 修复）
- [ ] Dark/Light 主题下与 Hierarchy 等窗口视觉一致
- [ ] Scene 默认布局 **右下角** Dock Content Browser；Material 默认 **不可见**（`GetOwnerModuleId == Scene`）

---

## 8) 后置选做（记录，非 P6.1）

- 搜索、类型过滤
- 右键菜单（含 Delete）
- Import 到当前面包屑目录
- Tile 缩略图（E2）、Icon font
- `OpenAsset` 路由与双击打开 Editor
- 面包屑中间路径省略（§2.6.1 后置）

---

## 9) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-26 | 初稿：P6 vs P6.1；Appearance 清单 |
| 2026-05-26 | **用户拍板**：左树含资产、右网格、面包屑、工具栏、无底栏、双击仅 Log；§4 Inspector 缺陷 |
| 2026-05-26 | **Dock U9**：Scene 中位于 Hierarchy/Inspector 下方、Console 右侧；Material 默认隐藏 |
| 2026-05-26 | **补充拍板**：Dock 具体为 Hierarchy/Inspector 下方且在 Console 右侧；单击目录不清空 Inspector 资产 |
| 2026-05-26 | **§2.6 P6.1-polish**：面包屑扁平 Caption / 无圆角；Tile 128×方 Icon + 单行省略号；**U11/U12**；实现待审批 |
| 2026-05-26 | **§2.6 实现**：`ContentBrowserWindow` — `DrawBreadcrumbLink`、`DrawAssetTile`、ViewMetrics |
