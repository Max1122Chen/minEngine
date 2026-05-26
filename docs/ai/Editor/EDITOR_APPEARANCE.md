# Editor Appearance — 设计案

Last updated: 2026-05-26  
Status: **M0–M6b 已合入（M6c 未做）；CJK 可读显示后置 i18n**（见 [EDITOR_THEME_M6_DESIGN.md](./EDITOR_THEME_M6_DESIGN.md)）  
分支：`feat/editor-appearance`  
父文档：[Editor 平台化规划](./EDITOR_PLATFORM_PLAN.md)、[Editor Shell](./EDITOR_SHELL_DESIGN.md)  
关联：[GUI 开发 FAQ](./GUI_DEV_FAQ.md)、[Command History / Undo](./EDITOR_COMMAND_HISTORY.md)、[资源管线 R2](../Render/RESOURCE_PIPELINE_PLAN.md)

---

## 0) 一句话

**`Color` / `LinearColor` 按 `Transform` 同款 `ME_STRUCT` 建模**（可序列化 + 预留 **`ColorWidget`** / ImGui 色板）；建立 **Dark/Light 可切换主题**；**`PropertyWidgets`** 统一 Inspector（含嵌套、`TransformWidget`、Specifier/Meta、Undo/Dirty）；**`ObjectPtrWidget`** 基于 **Asset 索引 / Class 过滤** 分流；**`Font` 在本计划内资产化**（M5）；审批通过后再从 **M0** 写代码。

---

## 1) 用户意图摘要

| # | 意图 |
|---|------|
| U1 | `Color` / `LinearColor` 与 **`Transform` 同级 struct 建模**，非裸 primitive |
| U2 | 预留 **`ColorWidget`**，使用 ImGui **`ColorEdit3` / `ColorEdit4`**（含拾色器） |
| U3 | UE/Unity 暗色工具风；**Dark + Light** 内置 + JSON 自定义；用户 **仅配配色** |
| U4 | Inspector **嵌套层级**；PropertyWidgets 覆盖 primitive + 引用 + Transform |
| U5 | Transform 专用 Widget；欧拉 ↔ 四元数预留；**MarkDirty / Undo** 回调 |
| U6 | **Specifier / Meta** 尽早（对齐 UE） |
| U7 | Vector 样式采纳；引用 **先下拉**，Browse 后置 |
| U8 | **`Font` 纳入本计划资产化**，并明确开工时间点 |
| U9 | **`ObjectPtrWidget`** 后续支持按 **ClassInfo** 过滤；Asset 走 **AssetManager 分类型索引**，避免扫全 `ObjectManager` |
| U10 | 旧 `UI/Widgets/*` 不沿用；其余设计基本同意 |

---

## 2) 目标与非目标

### 2.1 目标

| # | 目标 |
|---|------|
| G1 | `Color`、`LinearColor`：`ME_STRUCT` + 反射 + JSON（与 `Transform` 一致） |
| G2 | `ColorWidget` / `LinearColorWidget`（ImGui `ColorEdit*`） |
| G3 | `EditorThemePalette`（字段类型 `LinearColor`）+ Dark/Light + `.mesettings` |
| G4 | `EditorAppearance` + `PropertyEditPolicy` + `PropertyWidgets` + `TransformWidget` |
| G5 | Inspector 嵌套布局；Object/Asset 引用选择器架构落地 |
| G6 | **`Font` 资产**：`AssetType == "Font"`、`Font`/`FontResource` 类、扫描/加载、Editor 消费 |
| G7 | `AssetManager` **按 AssetType 的 Meta 索引**（注册时维护，查询 O(1) 类型桶） |
| G8 | 与 E1 Inspector 平台化对齐：Widgets 在 Drawer 之下 |

### 2.2 非目标

| # | 非目标 |
|---|--------|
| NG1 | 用户配置 ImGui `StyleVar`、控件外观、Table 列宽 |
| NG2 | 主题可视化编辑器 |
| NG3 | 引用 **Browse / 拖拽 / Ping**（第一版） |
| NG4 | 保留并扩展 `DraggableOverlay` / `MultiSelectFilterDropdown` |
| NG5 | Material Graph 节点画布主题 |
| NG6 | Icon font merge |
| NG7 | HDR `LinearColor`、色域、物理正确拾色（后续） |

### 2.3 并行纪律

| 规则 | 说明 |
|------|------|
| 分支 | `feat/editor-appearance` |
| 路径 | `Editor/**`、`Runtime`（Color、Font 资产、AssetManager 索引、ProjectSettings）、`docs/ai/Editor/**` |
| 审批门 | **用户审批本设计 v1 后** 才启动 **M0（Color struct）** |

---

## 3) 架构分层

```text
Inspector / Drawer
       → PropertyEditPolicy (Specifier, Meta, Context: SceneInstance | AssetDefaults)
       → PropertyWidgets
              ├─ TransformWidget
              ├─ ColorWidget / LinearColorWidget
              ├─ ObjectPtrWidget → PropertyReferencePicker
              │       ├─ Asset path: AssetManager (per-type index)
              │       └─ Object path: ObjectManager + Class filter (后续)
              └─ primitives…
       → EditorAppearance (theme tokens, font atlas from Font asset)
```

---

## 4) `Color` 与 `LinearColor`（对齐 `Transform`）

### 4.1 建模原则（拍板 2026-05-25）

| 原则 | 说明 |
|------|------|
| **存储真源** | 资产、主题、属性等 **底层一律 `LinearColor`**（线性 RGBA float） |
| **`Color` 角色** | **sRGB 展示/交换** 用的 `uint8` struct（类似 UE `FColor`），**不是**持久化主类型 |
| **Runtime 无 ImGui** | `Color` / `LinearColor` **禁止** include imgui、**禁止** `ToImVec4` 等 UI 转换 |
| **sRGB ↔ Linear** | 仅在 **Runtime** `Color.cpp` 用标准 gamma 公式（`Color ↔ LinearColor`） |
| **Editor 拾色** | **编辑侧暴露 sRGB**（`ImGui::ColorEdit*` 的 float[4] 视为 sRGB 0–1）；读写仍落 **`LinearColor*`**；ImGui 转换在 **`Editor/src/UI/Property/EditorColorConversion.h`**（或 `Appearance/`） |

与 `Transform` 相同：`ME_STRUCT()` + `ME_GENERATED_BODY` + 分字段 `ME_PROPERTY`；头文件 `Runtime/Core/Math/Color.h`。

### 4.2 类型定义

```cpp
// 持久化 / 主题 / 材质属性 — 真源
ME_STRUCT()
struct LinearColor
{
    ME_GENERATED_BODY(LinearColor)
    ME_PROPERTY(EditAnywhere) float R = 0.0f;
    ME_PROPERTY(EditAnywhere) float G = 0.0f;
    ME_PROPERTY(EditAnywhere) float B = 0.0f;
    ME_PROPERTY(EditAnywhere) float A = 1.0f;

    Color ToColor() const;              // Runtime：线性 → sRGB uint8
    static LinearColor FromColor(const Color& srgb);
};

// sRGB 8-bit 视图；序列化可用，但主题与 Appearance 只用 LinearColor
ME_STRUCT()
struct Color
{
    ME_GENERATED_BODY(Color)
    ME_PROPERTY(EditAnywhere) uint8 R = 255;
    // G, B, A …
    LinearColor ToLinearColor() const;  // Runtime：sRGB → 线性
    static Color FromLinearColor(const LinearColor& linear);
};
```

### 4.3 序列化

- JSON：**对象 + 命名字段** `{ "R", "G", "B", "A" }`；主题 palette 字段类型为 **`LinearColor`**
- M0 验收：`LinearColor` / `Color` 经 `Serializer` JSON 往返
- Binary：M0 **不做**（O8）

### 4.4 `ColorWidget`（M3，Editor only）

| 步骤 | 位置 |
|------|------|
| 读 | `LinearColor` → `EditorColorConversion::ToSrgbEditFloats` → `ImGui::ColorEdit4`（**sRGB** 显示） |
| 写 | `ColorEdit4` 输出 sRGB float → `FromSrgbEditFloats` → 写回 **`LinearColor`** |
| Undo/Dirty | `PropertyEditSession` |

**不实现** `LinearColorWidget` 对线性的直接拾色（用户编辑永远看 sRGB）；内部存储仍线性。

**Property 类型：** 反射值为 `LinearColor` 时走 `ColorWidget`；若极少数字段为 `Color` struct，可先 `ToLinearColor` 再编辑或单独薄封装。

**禁止** 在 `Color` 类内嵌 ImGui 或 `ImVec4`。

---

## 5) 主题系统（摘要）

- `EditorThemePalette` 字段全部为 **`LinearColor`**
- 内置 **`DarkEngine`**、**LightEngine`**（M1 同时交付，**O3 已拍板**）
- `EditorAppearanceSettings`：`ThemePresetId` + `CustomPalette`；Custom 与预设 **merge 回退**（**O2 已拍板**）
- 圆角/间距：`EditorAppearance::ApplyStyleConstants()` 代码写死，沿用当前 `Editor.cpp` 数值（**O5 已拍板**）
- 运行时切换主题 **写回 `.mesettings`**（**O6 已拍板**）

---

## 6) PropertyWidgets 与 Inspector

### 6.1 嵌套层级

- `Transform` → **`TransformWidget`**（子树 + 缩进）
- 其它 `MEObjectProperty`（含 `Color`）→ 可折叠子树；**禁止**与父级字段同一缩进展平（修复 SceneComponent / Transform 观感）
- `TransformWidget`（M4 规则）
  - 默认展开：`TreeNode` 使用 `ImGuiTreeNodeFlags_DefaultOpen`
  - Root/非 Root：一个 `GameObject` 可能挂多个 `SceneComponent`，其中只有一个是 `RootComponent`
    - RootComponent：在 “Root Transform” 区绘制（避免组件表重复编辑）
    - 非 Root SceneComponent：在各自组件行绘制 `m_Transform`（允许单独编辑它们的 Transform）

### 6.2 Session / Policy（M2）

`PropertyEditSession` 承载 **`ContextKind`**、`OnMarkDirty`（Scene 已接 `MarkSceneDirty`；Material/资产默认面板后续补脏标记）。

`PropertyEditPolicy` 统一 **可见性 / 可编辑 / 只读** 与 **Meta 展示**（`DisplayName`、`Tooltip`；`ReadOnly=true` 等同不可编辑）。

#### 编辑语境（`EditorPropertyEditContextKind`）

| 语境 | 用途 | 典型调用方 |
|------|------|------------|
| **`SceneInstance`** | 关卡内实例属性（Hierarchy / Scene Inspector） | `PropertyEditSession::ForSceneEditor` |
| **`AssetDefaults`** | 非场景实例的「默认/创作」侧 | Material 节点 Details、`MaterialNodeDefPropertyDrawer`；将来资产默认 Inspector、工程设置 |

**拍板（2026-05-25）：** 不设单独的 `MaterialGraph` 枚举。Material 图节点参数属于 **默认侧创作**，与资产默认共用 **`AssetDefaults`**，以便 `EditDefaultsOnly` / `VisibleDefaultsOnly` 等与 UE 心智一致。

Specifier 映射（`PropertyEditPolicy`）：

| Specifier | `SceneInstance` | `AssetDefaults` |
|-----------|-----------------|-----------------|
| `Invisible` | 隐藏 | 隐藏 |
| `VisibleInstanceOnly` | 显示 | 隐藏 |
| `VisibleDefaultsOnly` | 隐藏 | 显示 |
| `EditInstanceOnly` | 可编辑 | 只读/禁用 |
| `EditDefaultsOnly` | 只读/禁用 | 可编辑 |
| `EditAnywhere` | 可编辑 | 可编辑 |
| `Transient` | 不可编辑 | 不可编辑 |

`CanEdit` 为 false 时，Scene Inspector 对行使用 `ImGui::BeginDisabled`；`IsReadOnly` = 显示但不可改。

### 6.3 Widget 清单（第一版）

| Widget | 覆盖 |
|--------|------|
| Bool / Int / Float / String | primitives |
| Vector2/3/4 | 分量色 + Reset |
| Enum | `MEEnum`（**M3‑Enum 后置**，反射缺 underlying type） |
| **Color / LinearColor** | **ColorWidget** |
| **Transform** | TransformWidget |
| **ObjectPtr** | **ObjectPtrWidget**（见 §7） |

### 6.4 `PropertyEditPolicy` + Meta

- Specifier：已有 `EditAnywhere`、`EditDefaultsOnly`、`Invisible` 等
- Meta 第一版键：`DisplayName`、`Tooltip`、`ClampMin/Max`、`UIMin/Max`
- 引用类扩展 Meta（§7.3）：`AllowedClasses`、`ReferenceKind`

## 6.5 M4：Inspector 嵌套 + TransformWidget + Scene Undo（按 propertyPath 粒度）

你这轮的诉求拆成 Q1/Q2/Q3/Q4，我把它们都落在这节里，确保实现时不会走偏。

### 6.5.1 Q3：默认展开

- `TransformWidget` 的 `TreeNode` 默认 **展开**：`ImGuiTreeNodeFlags_DefaultOpen`
- 其它嵌套 struct 子树默认也用 **DefaultOpen**（不做默认折叠）

### 6.5.2 Q2：一个 GO 多 SceneComponent，只有一个 Root，但其它 Transform 也要可编辑

- 一个 `GameObject` 可以挂多个 `SceneComponent`
- 其中只有一个 `RootComponent`
- M4 的显示与交互规则：
  - `RootComponent` 的 Transform：显示在 Inspector 顶部 “Root Transform” 区（使用 `TransformWidget`）
  - 非 Root `SceneComponent` 的 Transform：显示在各自组件行里（使用 `TransformWidget`）
  - 去重规则：**仅跳过 RootComponent 在组件表里的 `m_Transform` 绘制**，其它 SceneComponent 的 `m_Transform` 允许单独编辑

### 6.5.3 Q1：能不能“嵌套对象一个 property 一个 command”，而不是整包 struct？

可以，并且推荐按 **propertyPath 粒度**实现：
- 不满足的原因（当前系统的现状）：
  - Inspector Undo 当前以 `capturePropertyName` 为定位信息
  - 嵌套字段控件（如 `m_Transform.Position`）如果仍复用父级 `objectUndoContext`，就会退化成“整段 struct 被一个命令覆盖”
- 推荐实现策略：
  - 扩展 Undo 定位从单段 `propertyName` → **`propertyPath`**
  - 例如：
    - 顶层 Transform：`m_Transform`
    - 子字段：`m_Transform.Position` / `m_Transform.Rotation` / `m_Transform.Scale`
  - 当某个子控件完成编辑并结束交互时，只提交对应 `propertyPath` 的 before/after blob → 从而做到真正的“一个 property 一个 command”

> 备选（不改 propertyPath）：可以做到“每次子控件结束交互都提交一条 Command”，但 blob 可能仍是整 struct（不符合你提出的“不是整个 struct”）。

### 6.5.4 Q4：Undo 桥（按你现有代码链路逐步讲清楚）

M4 的 Undo 桥实现完全复用你当前 Inspector 的 before/after + Command 提交框架，只把“定位字段”从 `propertyName` 升级为 `propertyPath`。

#### A) 当前已存在的桥（现状链路）

1. `SceneEditorInspectorSource::DrawProperty(...)`
   - 控件绘制过程中，决定是否启用 undo capture
2. `SceneEditorInspectorSource::ApplyPropertyUndoCaptureHooks(...)`
   - 依赖 ImGui：
     - `Activated`：序列化 beforeBlob 并缓存到 `m_PropertyUndoBeforeByEditId[editId]`
     - `DeactivatedAfterEdit`：序列化 afterBlob 并调用 `TryPropertyUndoCommitImmediate(...)`
3. `SceneEditorInspectorSource::TryPropertyUndoCommitImmediate(...)`
   - 调用 `m_SceneEditor.SubmitSetObjectProperty(...)`
4. `SetObjectPropertyCommand` / `SceneEditor::ApplySetObjectProperty(...)`
   - Execute：写入 after
   - Undo：写入 before

#### B) M4 需要你补的“最小桥接点”

只要做下面三件事，propertyPath 粒度就能贯通：

1. 扩展 `PropertyUndoCaptureContext`
   - 新增 `propertyPath`（字符串）
   - 让 `SerializePropertyUndoBlob` / Apply 写回时使用 `propertyPath`

2. 修改递归绘制时的 context 生成（让嵌套字段每次都拿到“子字段自己的 propertyPath”）
   - 在 `DrawObjectProperty` 递归时维护 `currentPath`
   - 子字段生成：`childPath = currentPath + "." + childProperty.GetName()`
   - 每个子控件的 before/after 都用各自 `childPath`

3. 增加 Serializer/Apply 的“按路径写回”能力
   - 新增（或扩展）：
     - `Serializer::SerializePropertyByPath(...)`
     - `Serializer::DeserializePropertyByPath(...)`
   - 规则：用 `.` 分段遍历 owner 对象的属性树，直到最后一段才对该字段调用现有 Serialize/Deserialize

这样 Undo 桥最终满足：
- 一个 GO 多 SceneComponent：每个 Transform 都有自己的命令粒度
- 嵌套字段：每个控件结束编辑都提交“对应 propertyPath”的命令
- 默认展开：TransformWidget/嵌套 struct 默认展开，用户体验符合预期

---

## 7) 引用选择：`ObjectPtrWidget`

**详细设计（评审中）：** [OBJECT_PTR_WIDGET_DESIGN.md](./OBJECT_PTR_WIDGET_DESIGN.md)

### 7.1 摘要（v3，2026-05-25）

| 项 | 决定 |
|----|------|
| 入口 | **`ObjectPtrWidget`**；与 `PropertyValueWidget` 并列 |
| Allowed | **`const MEClass*`** 列表；Meta 将来只负责解析成指针 |
| 分流 | **`allowed->IsA(Asset::StaticClass())`** → `CollectAssetCandidates`；否则 → `CollectObjectCandidates`（**不用** `HasAssetTypeForClass`） |
| Asset 优先 | Asset 子类 **只**列 Meta 桶；已加载实例不走 Object 分支（`LoadAssetByPath` 更快） |
| Runtime | `AssetTypeRegistry` 增加 **`MEClass*` → AssetTypeId**；`FindAssetMetasByClass` |
| 结构 | `Collect*` 在 **`ObjectPtrWidget` 内** + `PropertyRefPicker`；无独立 Resolve 模块 |
| Registry 登记 | Builtin 用 **`T::StaticClass()`** 写入 `m_AssetTypeIdByClass` |
| Meta 第一版 | 预留、不解析 |
| **None** | **所有** `MEObjectPtr` 编辑 Combo **固定**首项 None（清空 `shared_ptr`）；无 `allowNone` 开关 |

### 7.2 与旧 §7 的差异

- 不再把「纯 Object 引用 UI」推到 M7；与 Asset 引用同一 Widget、同一 Allowed 模型。
- `DrawAssetRef` 重复实现待 M3.1 删除，由上述分层替代。

---

## 8) 字体：`Font` 资产化（纳入本计划）

**详细设计：** [FONT_ASSET_DESIGN.md](./FONT_ASSET_DESIGN.md)（M5a/M5b 已合入）

### 8.1 定位

- `Font` 是 **引擎可发现、有 Meta、可加载** 的资产（与 `Texture2D` 同级抽象）
- Editor UI 字体 = **消费 Font 资产**；游戏 UI 将来复用同一类型
- 英文为主；资产 / 设置上预留 **`bEnableCjkGlyphs`**（默认 false，仅合并 glyph range）
- **CJK 可读显示后置 i18n**：需含 CJK 字形的 Font 资产（Inter 无中文）；M5.1 不交付中文 UI 显示

### 8.2 摘要（与详细设计对齐）

| 项 | 决定 |
|----|------|
| Runtime | `Font : Asset`，**仅**缓存 `std::vector<uint8_t>` + 源扩展名；**无 ImGui**（**M5a ✓**） |
| 磁盘 | 源文件即 `.ttf` / `.otf`；**无** `.mefont` sidecar |
| AssetType | `"Font"`；Registry + `LoadAsset_Impl<Font>` + Scan/Import（**M5a ✓**） |
| Editor 排版 | **`EditorTypographyRole`**：Body=Inter Regular、Heading=Inter SemiBold 等；**每角色** `FontAssetGuid` + `SizePixels` |
| 工程设置 | `EditorAppearanceSettings.Typography`（角色表）；`bEnableCjkGlyphs` 作用于全部角色（**M5.1**） |
| Editor | 单 atlas 多 `ImFont*` + `EditorTypographyScope`；`Fonts[0]`=Body；移除 `FontGlobalScale` |
| per-asset Meta | **不做**；字号归属角色，非 Font 资产（见 FONT_ASSET_DESIGN §6） |

### 8.4 与 RESOURCE_PIPELINE 关系

- 在 [RESOURCE_PIPELINE_PLAN](../Render/RESOURCE_PIPELINE_PLAN.md) 中 **记一笔** Font 类型由 Appearance 里程碑引入（交叉链接即可，不必等 R2 全部完成）

### 8.5 图标字体

仍 **不做** Icon font；.toolbar 使用文字标签（见原 NG6）。

---

## 9) 实施里程碑（修订）

| 阶段 | 内容 | 开工依赖 | 验收 |
|------|------|----------|------|
| **—** | **设计审批** | 用户确认本文 | **已完成（2026-05-25）** |
| **M0** | `Color` / `LinearColor` struct + 反射 + JSON + Runtime sRGB 转换 | **已合入（`07c9734`）** | `Color.h/.cpp`、`Color.gen.*`、`uint8` codec；Editor 仅 `EditorColorConversion` |
| **M1** | `EditorThemePalette`、Dark+Light、`EditorAppearance`、`ProjectSettings.Appearance` | **已合入（`e71f622`）** | View→Theme Dark/Light；写入 `.mesettings` |
| **M2** | `PropertyEditPolicy`、`PropertyEditSession` | **已合入（`6648a0e`）** | 语境仅 `SceneInstance` \| `AssetDefaults`；Scene/Material Details 走 Policy；Meta DisplayName/Tooltip/ReadOnly |
| **M3** | PropertyWidgets（primitive、**ColorWidget**） | **已合入** | `PropertyPrimitiveWidgets` / `ColorWidget` / `PropertyValueWidget`；`LinearColor` 用 `StaticClass`；Material `ForAssetDefaults` + `MarkDirty` |
| **M3‑Enum** | `PropertyEnumWidget` 接入 | **已合入** | `PropertyEnumWidget` 已从 `PropertyValueWidget` 接线 |
| **M3.1** | `ObjectPtrWidget` + `PropertyRefPicker`（**Asset + Object**） | **已合入** | 见 [OBJECT_PTR_WIDGET_DESIGN.md](./OBJECT_PTR_WIDGET_DESIGN.md) |
| **M3-Enum** | `PropertyEnumWidget` 接线 | **已合入** | `MEPrimitiveProperty::GetEnum()` / `GetSize()` |
| **M4** | Inspector 嵌套 + `TransformWidget` + Scene Undo 接线 | M3.1 | **已合入**；TransformWidget；`propertyPath` 粒度 Undo |
| **M5a** | **`Font` 资产** + `LoadAsset_Impl` + scan | M1、M4 | **已合入**；Project 下 `.ttf`/`.otf` 可加载 |
| **M5b** | 排版角色 + 多字体 atlas + `EditorTypographyScope` + 代表性窗口接线 | M5a | **已合入**；Body/Heading 字重与字号可区分；无 `FontGlobalScale` |
| **M5.1** | CJK glyph 开关、全窗口排版、设置变更重建 atlas | M5b | **已合入**；CJK **可读显示** → **i18n** |
| **M6** | 收拢窗口散落 `PushStyleColor` | M1 | **已合入（M6c 图域后置）**；见 [EDITOR_THEME_M6_DESIGN.md](./EDITOR_THEME_M6_DESIGN.md) |
| **M7** | Object 引用 Picker（非 Asset，`AllowedClasses` + 注册表或受控枚举） | M4 + 需求 | 可选；不阻塞 Appearance 主线 |

**Font 开工时间点：** **M5**（在 M1 主题可用之后、M6 清扫之前）；Runtime Font 类与 AssetManager 扩展可与 M3.1 并行，但 **Editor 换字体** 不早于 M5。

**PR 建议：** M0–M1 | M2–M3 | M3.1（合并 AssetWorkflow 后）–M4 | M5–M5.1 | M6 。

---

## 10) 模块与文件（增补）

| 路径 | 职责 |
|------|------|
| `Runtime/Core/Math/Color.h` | `Color`, `LinearColor` |
| `Runtime/Resource/Font.h`（或 `AssetResources/FontResource.h`） | Font 资产 |
| `Runtime/Resource/AssetManager.*` | 类型桶索引 + Font LoadImpl |
| `Editor/src/UI/Property/PropertyEditTypes.h` | `EditorPropertyEditContextKind` |
| `Editor/src/UI/Property/PropertyEditPolicy.*` | Specifier + Meta |
| `Editor/src/UI/Property/PropertyEditSession.*` | Context + `OnMarkDirty` |
| `Editor/src/UI/Property/PropertyPrimitiveWidgets.*` | Primitive + Vector2/3/4 |
| `Editor/src/UI/Property/PropertyEnumWidget.*` | Enum 下拉 |
| `Editor/src/UI/Appearance/EditorTypographyScope.*` | 排版 RAII `PushFont` |
| `Editor/src/UI/Appearance/EditorWindowTypography.*` | 面板标题 Heading 字体 |
| `Editor/src/UI/Property/PropertyValueWidget.*` | Dispatches primitive / `LinearColor` (`IsA(StaticClass)`) |
| `Editor/src/UI/Property/ColorWidget.*` | ImGui ColorEdit → `LinearColor` |
| `Editor/src/UI/Property/ObjectPtrWidget.*` | `MEObjectPtrProperty` 入口 |
| `Editor/src/UI/Property/PropertyRefPicker.*` | Combo UI |
| `Runtime/.../AssetTypeRegistry.*` | `GetAssetTypeIdForClass`（M3.1 小扩展） |
| `Editor/src/UI/Appearance/EditorAppearance.*` | 主题 + Font atlas |

---

## 11) 已拍板项

| 项 | 决定 |
|----|------|
| Color 建模 | **ME_STRUCT**；**存储用 `LinearColor`**；`Color` = sRGB |
| ImGui 转换 | **仅 Editor**（`EditorColorConversion`）；Runtime **无** imgui |
| ColorWidget | M3；编辑 **sRGB**，写回 **LinearColor** |
| 主题 | Dark + Light；仅配色可配置；JSON 对象字段 |
| Open O1–O3,O5,O6 | 采纳建议默认 |
| Font | **本计划 M5 资产化**，非「将来再说」 |
| ObjectPtr | Asset → **类型桶索引**；Object → 后续 + Class 过滤 |
| 属性编辑语境 | **二分**：`SceneInstance` + `AssetDefaults`（含 Material 节点）；**无** `MaterialGraph` |
| 审批门 | **已批准；M0 已开工** |
| 其余 v0 设计 | 用户基本同意 |

---

## 12) 仍待确认（审批时可一并答复）

| ID | 问题 | 建议 |
|----|------|------|
| O7 | `Font` 类名：`Font` vs `FontAsset` vs `UIFont` | 类名 **`Font`**，`AssetType="Font"` |
| O8 | M0 是否同步 Binary codec | M0 仅 JSON；Binary 随 Serializer 里程碑 |
| O9 | 编辑 sRGB / 存 Linear | **已拍板**：Editor sRGB 显示，底层 `LinearColor` |
| O10 | 非 Asset 的 `ObjectPtr` 第一版是否禁用 UI | **是**（文案提示），M7 再做 |

---

## 13) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-25 | v0 初稿 |
| 2026-05-25 | **v1 迭代**：Color 对齐 Transform + ColorWidget；Font 纳入 M5；ObjectPtr / AssetManager 类型桶 + PropertyReferencePicker；审批门；Open 项更新 |
| 2026-05-25 | **拍板**：sRGB 编辑 / Linear 存储；ImGui 转换仅 Editor；M0 开工 |
| 2026-05-25 | **M2 设计**：移除 `MaterialGraph` 语境；Material 节点 Details 使用 `AssetDefaults`；补充 §6.2 语境表 |
| 2026-05-25 | **M3**：PropertyWidgets 分层；`LinearColor` 用 `MEClass::IsA(StaticClass)`；Material Details `PropertyEditSession::MarkDirty` |
| 2026-05-25 | **M3 诚实范围**：`PropertyEnumWidget` 仅 scaffold、未接线；**M3.1** 类型桶后置至 `AssetWorkflow` 分支合并后 |
| 2026-05-25 | **ObjectPtrWidget v2 设计**：Asset+Object、`AllowedClasses` 一统 |
| 2026-05-25 | **ObjectPtrWidget v3**：`MEClass*`、`IsA(Asset)` 分流、双 `Collect*` 扁平化；见 `OBJECT_PTR_WIDGET_DESIGN.md` |
| 2026-05-26 | **M5**：`FONT_ASSET_DESIGN.md` 详细设计（Font 数据结构、EditorAppearance、设置字段） |
