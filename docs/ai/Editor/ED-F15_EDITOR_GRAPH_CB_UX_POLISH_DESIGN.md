# Editor Graph / Content Browser UX Polish — Design Spec

## Meta
- **ID:** `ED-F15`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/editor`
- **Related:** Material Graph（imgui-node-editor）；`SmGraph`（AnimGraph）；Content Browser tiles；`EditorAppearance` / theme palette

## TL;DR
对齐 AnimGraph 画布导航到 MaterialGraph 习惯，刷新 Material 节点视觉（UE 风格 chrome），补齐 CB 文件夹 tile 的选中高亮与图标，并让 Inspector 组件行图标与 AddComponent 共用同一套 `ComponentTypeUiCatalog` 解析（修复命名空间导致的 puzzle 回退）。小 Feature、产品面 UX，不碰命令栈 / 资产契约。

## Scope
- **In:**
  1. **AnimGraph（`SmGraph::Widget`）导航对齐 MaterialGraph**
     - 空画布：**右键拖拽平移**；**左键拖拽框选**
     - 右键：拖动超阈值 → Pan；单击无拖动 → 保留现有 Context Menu
     - Middle / Alt+LMB 可保留为次要平移手势（兼容）
     - **框选 MVP：** 仍为单 `Selection`（选中框内一个 State）；**不**扩展多选模型（已确认）
  2. **MaterialGraph 节点外观** — 彩色 Header 条 + 主题色 Body/边框/选中描边；Pin 图标略整理；颜色走 `EditorAppearance` palette + 现有 `HeaderColor`
  3. **CB 文件夹 tile** — FA 文件夹图标；单击选中边框高亮（双击仍进入目录）
  4. **Inspector 组件行图标** — 与 AddComponent 一致，使用 `ComponentTypeUiCatalog` 已注册 glyph（见 §3.4）
- **Out:**
  - AnimGraph **多选数据模型** / 批量删除
  - Save As / Scene 多开（仍属 ED-F11 余量）
  - 完整克隆 UE Material Editor（自定义 Pin 形状、comment bubble、reroute 等）
  - AnimGraph 节点 chrome 大改（本次只对齐导航）
  - EditorSettings 持久化选中态（ED-F10）
  - 为未登记类型批量补 icon 表（仅修解析；缺表仍 puzzle）

## Reader quick start
1. 本文件：边界与契约
2. 代码入口：
   - `Editor/src/UI/SmGraph/SmGraphWidget.{h,cpp}`
   - `Editor/src/UI/EditorWindows/MaterialGraphWindow.cpp` + `MaterialGraphNodeRegistry.*`
   - `Editor/src/UI/EditorWindows/ContentBrowserWindow.{h,cpp}`
   - `Editor/src/UI/Appearance/EditorAssetTypeIcons.*`（文件夹 glyph 可复用 / 扩展）
   - `Editor/src/Services/ComponentTypeUiCatalog.*` + `SceneEditorInspectorSource.cpp`（组件行 icon）

---

## 1) 背景与目标

| Pain | 目标体验 |
|------|----------|
| Material / Anim 画布手势不一致，切换 SubEditor 要改肌肉记忆 | Anim ≈ Material：RMB pan + LMB marquee |
| Material 节点只有纯文本标题，`HeaderColor` 几乎未用，像早期原型 | 贴近 UE：顶栏色块 + 深色机体 + 清晰选中框 |
| CB 文件夹 tile 无图标、无选中框，资产 tile 却有 | 文件夹与资产 tile 视觉权级接近 |
| Inspector 组件行几乎全是 puzzle，AddComponent 同类型却正确 | 两边共用同一解析；已注册类型显示表内 icon |

---

## 2) 现状

| 区域 | 现状 |
|------|------|
| Material | `ax::NodeEditor` 默认导航（RMB pan + LMB 框选）；节点 `BeginNode` 内仅 `Text` + 控件，`HeaderColor` 未绘制 |
| AnimGraph | `SmGraph`：Middle / Alt+LMB → Pan；RMB → Context Menu；空处 LMB → 清选中，无 marquee；`Selection` 单对象 |
| CB 文件夹 | `DrawDirectoryTile` → `DrawTileVisual(..., selected=false)` 且 `iconAssetMeta=nullptr` → 空方块 |
| Inspector icon | `DrawComponentHeaderRow` 已调 `ComponentTypeUiCatalog::DrawIcon(*appearance, classInfo)`，但 **MEClass 路径解析有 bug**（见 §3.4） |

---

## 3) 方案

### 3.1 AnimGraph 导航（W1）

**契约：**

| 手势 | 行为 |
|------|------|
| RMB down → drag ≥ `kPanDragThresholdPx`（建议 3–5px） | `Mode::Pan`（同现有 Scroll 公式） |
| RMB click（未超阈值）在空/节点/边 | 现有 Context Menu 路径不变 |
| LMB down 在空处 → drag | `Mode::BoxSelect`：画 marquee；mouseup 命中 State 节点则 `SetSelection` **一个**（优先面积最大或 z 最后绘制） |
| LMB 在节点/环/边 | 现有 DragNode / LinkDrag / 选边 不变 |
| Middle / Alt+LMB | 仍可启动 Pan（次要） |

**实现要点：** `Mode` 增 `BoxSelect`；绘制用 canvas 空间矩形；不扩展 `Selection` 结构。

### 3.2 Material 节点视觉（W2）

**契约（UE-ish MVP）：**

1. `EndNode` 后（或 `BeginNode` 内首部）用 `GetNodeSize` / item rect 画：
   - **Header 条**：`style.HeaderColor`（可略降饱和 / 固定高度 ~22px）
   - **Body**：`palette.PanelBackground` 或略深变体
   - **边框**：默认 `Border`；选中时 `Selection` 或语义高亮色加粗
2. 标题改在 Header 内白色/高对比文字（`TextPrimary` on header）
3. 同步 `Ed::GetStyle()` 若干字段（NodeRounding、NodeBorderWidth、NodeBg…）到当前 palette，避免与 ImGui theme 脱节
4. `HeaderColor` 仍可哈希生成；可选：按类别（Math / Texture / Parameter / Output）固定色板 — **本期允许保留哈希**，先保证 chrome 结构正确

### 3.3 CB 文件夹 tile（W3）

**契约：**

1. 单击文件夹 tile → 记 `m_SelectedDirectoryRelativePath`（或等价），清资产选中；绘制 `selected=true` 边框（与资产同一 `DrawTileVisual` 路径）
2. 图标：`ICON_FA_FOLDER`（Solid）填入 icon 槽；可挂 `EditorAssetTypeIcons::GlyphForFolder()` 以免散落
3. 双击仍 `SetCurrentDirectory`；进入后可选清除「文件夹选中」或保留 — **推荐进入后清除**

### 3.4 Inspector 组件图标（W4）

**根因（代码已核实）：**

- 反射类名是 `#TYPE` → `"minEngine::StaticMeshComponent"`（见 `ME_REFLECTION_CLASS_DEFINE_BEGIN`）。
- `ResolveIconGlyph(string_view)`：先 `GetShortTypeName`（剥 `::`）再查表 → AddComponent **正确**。
- `ResolveIconGlyph(MEClass*)`：对 `current->GetName()` **直接** `LookupIconExact`，从不剥命名空间；继承链上全是 `minEngine::…` → 全 miss → **puzzle**。
- Inspector 用 MEClass 重载；AddComponent 用 string 重载 → 表观不一致。

**契约：**

1. `ResolveIconGlyph(const MEClass*)` 在每级超类上使用 `GetShortTypeName(current->GetName())` 再 `LookupIconExact`（与 string 路径一致）。
2. 继承回退语义保持：叶子未登记时仍可命中 `LightComponent` / `SceneComponent` 等基类表项。
3. Inspector 仍走 `DrawIcon(appearance, classInfo)`；不在 Inspector 侧复制第二套表。
4. Out：不为本期未覆盖类型（如 UI `ButtonComponent`）补全表项，除非顺手且表已有明显空缺。

### 3.5 数据流 / 模块边界

- 仅 Editor UI；不改 Runtime Material / Anim 资产格式
- 不新增 Agent/Debug 命令
- Theme：读 `EditorAppearance`，不新增独立 Material 主题文件（除非颜色表膨胀再拆）
- 组件 icon：仍静态 `ComponentTypeUiCatalog`，不进 Runtime reflection metadata

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Anim 框选扩展多选 + 批量删 | 对齐 Material 更完整 | 牵 Inspector / Delete / Bridge，超「小 feat」 | **Out / 后续** |
| B. 框选只选一个（本期） | 手势一致、改动面小 | 多节点框选仍弱 | **选用** |
| C. 去掉 Middle/Alt pan | 更纯 | 破坏已适应的人 | **保留为次要** |
| D. Material 全盘仿 UE 自定义绘制 | 最像 | 工作量大、易与 node-editor 布局打架 | **Out**；本期 chrome overlay |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| RMB pan 与 Context Menu 抢手势 | 菜单打不开或误平移 | 拖拽阈值；仅超阈值进 Pan |
| 框选单选与用户「框选=多选」预期不符 | 失望 | Design Out 写清；Progress 标明；后续可开 ED-Fnn 多选 |
| Node chrome 与 imgui-node-editor 内部 Bg 叠色 | 发脏/双重边框 | 调 `GetStyle().NodeBg` 透明或统一由我们画 |
| 文件夹选中与资产选中双通道 | Inspector 空 | 选文件夹时 `SetSelectedAsset(nullptr)`；Inspector 空态可接受 |
| MEClass 短名假设再变 | icon 再次全 puzzle | 单点修在 `ResolveIconGlyph(MEClass*)`；与 string 路径共用 `GetShortTypeName` |

---

## 6) 验收标准

- [x] AnimGraph：RMB 拖动画布平移；单击 RMB 仍出菜单
- [x] AnimGraph：空处 LMB 拖出选框；松开后选中框内一 State（或清空）
- [x] Middle / Alt+LMB 平移仍可用
- [x] Material：节点可见彩色 Header + 主题 Body；选中有清晰描边；Dark/Light 不崩
- [x] CB：文件夹有图标；单击有选中边框；双击进入
- [x] Inspector：已登记类型（如 StaticMesh / Camera / Light）行首 icon 与 AddComponent 一致，非 puzzle
- [x] Editor 构建 OK；Progress 记一笔
- [x] Material 跟进：Pin 空心/实心、粗连线、右键 Add/Rename/Delete（手测通过）

## 7) Status note

**Done（2026-09-15）：** W1–W4 + Material UE-ish 跟进与右键菜单已落地并手测通过。

---

## 建议切片

| Wave | 内容 | 验证 |
|------|------|------|
| **W1** | SmGraph RMB pan + BoxSelect MVP | 手测 AnimGraph |
| **W2** | Material node chrome + style sync | 手测 Material Dark/Light |
| **W3** | CB folder icon + selection | 手测 Content Browser |
| **W4** | `ResolveIconGlyph(MEClass*)` 短名对齐 | Inspector vs AddComponent 对比 |

可同 PR 落地；实现顺序建议 **W4（最小）→ W1 → W3 → W2**（W2 视觉迭代最多）。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | 登记 ED-F15；三件套 UX + Anim 框选 scope cut（单选 MVP） |
| 2026-09-15 | 纳入 Inspector 组件 icon；根因 `MEClass` 全名未剥 `::`；确认 Anim 单选 |
| 2026-09-15 | 实现 W1–W4：SmGraph 导航、Material chrome、CB 文件夹 tile、ResolveIconGlyph 短名 |
| 2026-09-15 | Material：UE-ish padding/Pin 空心实心/粗连线；右键 Add/Rename/Delete |
| 2026-09-15 | 验收勾选；Status → **Done** |
