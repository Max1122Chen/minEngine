# Editor Appearance — 设计案

Last updated: 2026-05-25  
Status: **拍板 v1 — M0/M1/M2 已合入；M3 核心 Widgets 已实施（待 commit）；Enum / M3.1 后置**  
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

### 8.1 定位

- `Font` 是 **引擎可发现、有 Meta、可加载** 的资产（与 `Texture2D` 同级抽象）
- Editor UI 字体 = **消费 Font 资产**；游戏 UI 将来复用同一类型
- 英文为主；资产 / 设置上预留 **`bEnableCjkGlyphs`**（默认 false）

### 8.2 Runtime 形状（示意）

```cpp
ME_CLASS()
class Font : public Asset
{
    ME_GENERATED_BODY(Font)
    // 内存：TTF/OTF 字节或 stb 解码结果；ImGui 不负责持有文件路径
};

// AssetType: "Font"
// 扩展名: .ttf, .otf
// Meta: 可选 DefaultSizePixels, bEnableCjkGlyphs
```

- `AssetManager::LoadAsset_Impl<Font>`：读文件 → 缓存字节 → `EditorAppearance` 用 `AddFontFromMemoryTTF`
- `InferAssetTypeFromExtension` / `LoadAssetByMeta_Internal` 增加 Font 分支

### 8.3 Editor 消费

- `EditorAppearanceSettings`（或 `EditorFontSettings`）字段改为：
  - `GUID UiFontAssetGuid`（工程覆盖）
  - `float UiFontSizePixels`（全局字号配置项）
  - 引擎默认：`EngineDefaultAssets/Editor/Fonts/DefaultUI.ttf` 对应 **内置 Font 资产** 或启动时注册默认 Meta
- 移除长期依赖 `io.FontGlobalScale = 1.5f`
- CJK：若 `bEnableCjkGlyphs`，`AddFontFromMemoryTTF` 传 `GetGlyphRangesChineseFull()`（**接口预留**）

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
| **M3** | PropertyWidgets（primitive、**ColorWidget**） | **已实施（待 commit）** | `PropertyPrimitiveWidgets` / `ColorWidget` / `PropertyValueWidget`；`LinearColor` 用 `StaticClass`；Material `ForAssetDefaults` + `MarkDirty`；**Enum 未接入**（见下） |
| **M3‑Enum** | `PropertyEnumWidget` 接入 | **后置** | 反射补全 enum **underlying type / size** 后再从 `PropertyValueWidget` 启用；仓库保留 scaffold，**未接线** |
| **M3.1** | `ObjectPtrWidget` + `PropertyRefPicker`（**Asset + Object**） | **已实施（待 commit）** | 见 [OBJECT_PTR_WIDGET_DESIGN.md](./OBJECT_PTR_WIDGET_DESIGN.md) |
| **M3-Enum** | `PropertyEnumWidget` 接线 | **已实施（待 commit）** | `MEPrimitiveProperty::GetEnum()` / `GetSize()` |
| **M4** | Inspector 嵌套 + `TransformWidget` + Scene Undo 接线 | M3.1 | Transform 缩进；属性 Undo |
| **M5** | **`Font` 资产** + `LoadAsset_Impl` + scan + `EditorAppearance` 从 Font 加载 | M1（可与 M3 并行） | 工程可指定 Font GUID；英文清晰 |
| **M5.1** | 工程 `UiFontSize`、CJK 开关接线（无 glyph 也不崩） | M5 | 配置项生效 |
| **M6** | 收拢窗口散落 `PushStyleColor` | M1 | grep 硬编码减少 |
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
| `Editor/src/UI/Property/PropertyEnumWidget.*` | **Scaffold only**（未接入 `PropertyValueWidget`） |
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
