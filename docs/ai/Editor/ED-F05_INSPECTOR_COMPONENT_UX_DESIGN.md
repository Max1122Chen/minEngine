# ED-F05 — Inspector / Component UX

## Meta
- **ID:** `ED-F05`
- **Type:** Feature
- **Status:** Done（S00–S04；S05 Deferred）
- **Owner:** project maintainer
- **Last updated:** 2026-09-09
- **Branch:** `feat/editor`
- **Depends on:** `CORE-F07`（反射展示名切词，Done）；`CORE-F06`（Component Active，Done）；Editor CommandStack / `IEditorCommand`（已有）；Content Browser / Asset registry（已有）
- **Related:** [Implementation](./ED-F05_INSPECTOR_COMPONENT_UX_IMPLEMENTATION.md) · [ED-F02](./ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [ED-F04 Console](./ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md) · [CORE-F07](../Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)

## TL;DR

优化日常编辑手感（改名退出、Add 图标/搜索、组件头、重排、CB 内联重命名）。视口图标 Deferred。  
**图标关联：Editor 静态表 `ComponentTypeUiCatalog`（非 Runtime registry）** — 详见 §10.2。

> **落地：** S00–S04 已在 `feat/editor` 实现并通过 Editor 构建；手测清单见 §Acceptance。

---

## Pre-flight（2026-09-09）

| 项 | 结论 |
|----|------|
| 扫描 | Inspector / Hierarchy rename / Add 列表 / `m_Components` / FA7 / `ReflectionDisplayNames` / Content Browser 资产树 |
| 前置 | **sound** |
| 债风险 | **low–medium** — 重命名失焦与 ImGui 竞态；CB 重命名需改盘文件名 + meta/引用策略要说清边界 |
| WIP | DX 并行，不挡 ANIM；Hierarchy 树拖拽他分支已做 |
| 建议 | **Go**（Review 通过后）— 顺序 S00→S01→S02→S03→S04；**S05 视口图标 Deferred** |

---

## Scope

### In
- GO / Component **重命名退出**：Esc **或** 点击其他空白处 → **取消编辑、关闭输入框、不提交**；仅 **Enter** 提交
- Add Component：**类型图标** + **文本搜索**；补齐 `ME_CLASS(Abstract)`（承接 ED-F02-S05）
- **右键「Add Component」**：根级菜单项 → **次级子菜单**（含**搜索** + 类型列表）；与 Inspector **同构服务**；**可见范围先仅 Inspector GO**（Header / Components 空白；不含 Hierarchy，见 §10.9）
- Component 标题：**icon + 实例名（不切词）+ 类型展示（切词、去 `Component` 后缀）**；实例名可改
- Component **列表顺序**（↑↓ 或拖拽）+ Undo + 落盘一致
- **Content Browser 内联重命名**（资产显示名/文件名，见 §10.5）

### Out
- **视口世界空间类型图标** → **Deferred**（原 S05；体量大，后置）
- Hierarchy **父子树 / 拖拽挂接** — **他分支已实现**；本 Feature 不重复、不合入冲突时再对接
- Duplicate GO、F-focus、组件折叠记忆、选中 outline — **暂无需求 / 时机不宜**
- 通用 Icon 资产管线 / 自定义贴图 Icon Editor
- 改 PropertyPath 用实例名（仍按 **类型名** / `@`）
- Console `rename` 扩到 Component
- Material Preview SkyBox（ED-F02-S03）

## Reader quick start
1. 本文件 §1–9 — 行为契约与已确认决策  
2. **§10 — 实现设计（图标关联、API、文件触碰）** ← 审阅实现时优先读  
3. [Implementation](./ED-F05_INSPECTOR_COMPONENT_UX_IMPLEMENTATION.md) — 切片 DoD  
4. 代码入口：见 §10.7

---

## 1) 背景与目标

### Pain
- 重命名时 Esc / 点空白无法可靠退出输入框，或误提交（ImGui deactivate 竞态）。
- Add Component 纯文字；Abstract 标注不全；右键 Add 埋在 Create 下且 Hierarchy 无入口。
- 组件头只有类型短名；同类型多实例难辨；需要图标 + 可读类型。
- 组件顺序不可调。
- Content Browser 改名路径绕、易打断资产工作流。

### 成功长什么样
- Esc 或点空白 = **取消并关闭**；Enter = 提交。
- Add 列表有图标 + 可搜索；无 Abstract 基类；右键 **Add Component ▸ 类型**（同构服务）。
- 组件头：`[icon] MyLight  Point Light`（名不切词；类型切词且无 `Component` 后缀）。
- 可重排非 Root；Save/Load + Undo 正确。
- CB 可内联重命名资产（边界见 §3.8）。

---

## 2) 现状

| 区域 | 状态 | 位置 |
|------|------|------|
| Hierarchy / 组件 rename | Esc 易被 deactivate 抢先；点空白退出不可靠 | `HierarchyWindow` / Inspector |
| Add Component | 短类型名；已跳过 Abstract specifier | `SceneEditor*` |
| Abstract 标注 | 多数基类未标 | 各 Component 头 |
| Component 标题 | 仅 `GetShortTypeName` | `SceneEditorInspectorSource` |
| `MEObject::m_Name` | Component 已继承；Add 常空名 | `MEObject` / `GameObject` |
| 类型切词 | `InsertCamelCaseWordBreaks` 已有 | `ReflectionDisplayNames` |
| 组件顺序 | vector；无 Editor Move API | `GameObject` |
| CB 重命名 | **无**内联改名（后端已有 `RenameAsset`） | `ContentBrowserWindow` / `AssetManager` |
| 右键 Add Component | **已有**但仅 Inspector GO Header；嵌在 **Create ▸ Add Component ▸ 类型**；Hierarchy **故意关闭** | `SceneAddComponentMenuProvider` |
| 上下文菜单架构 | Typed Context + Action/Provider；三域分离 | `ContextMenu/*` · [EDITOR_CONTEXT_MENU_DESIGN](./EDITOR_CONTEXT_MENU_DESIGN.md) |
| 视口类型图标 | 无 → **本 Feature Deferred** | — |
| Hierarchy 父子拖拽 | **他分支已有** | （对接时再记） |

---

## 3) 方案

### 3.1 总原则

1. Editor 体验为主；Runtime 仅补 `MoveComponent` 等必要保序 API。
2. **实例名 ≠ 类型身份** — `m_Name` 展示/改名；`@` / PropertyPath 仍用类型名。
3. 复用 FA、切词 helper、`IEditorCommand`。
4. 视口图标整刀后置，不阻塞前序切片。
5. Forward-only：旧「仅类型头」展示不保留双轨。

### 3.2 模块边界

```text
┌──────────────────── Editor ─────────────────────────────────┐
│ HierarchyWindow / Inspector   rename dismiss (Esc|空白取消) │
│ SceneEditorInspectorSource    header / Add / reorder          │
│ ComponentTypeUiCatalog        type → FA icon + TypeDisplay    │
│ AddComponentPicker            同构列表示意（Inspector/菜单）  │
│ SceneAddComponentMenuProvider 右键 Add Component ▸ 类型       │
│ ContentBrowserWindow          inline asset rename             │
│ *Command                      Rename* / MoveComponent         │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────── Runtime ────────────────────────────────┐
│ GameObject               m_Components + MoveComponent         │
│ MEObject::m_Name         instance name (serialized)           │
│ ReflectionDisplayNames   camel-case for TypeDisplay only      │
│ AssetManager / meta      CB rename 落盘（S04 细边界）        │
└─────────────────────────────────────────────────────────────┘
```

### 3.3–3.8 行为摘要

| 切片 | 行为契约（已确认） | 实现细节 |
|------|-------------------|----------|
| S00 | Esc / 点空白 = 取消；仅 Enter 提交 | **§10.1** |
| S01 | Add：图标 + 搜索 + Abstract；**右键 Add Component 子菜单** | **§10.2 + §10.9** |
| S02 | 头：`icon + name + TypeDisplay` | **§10.3** |
| S03 | `m_Components` 重排；Root 置顶 | **§10.4** |
| S04 | CB 内联重命名 | **§10.5** |
| S05 | 视口图标 | **Deferred**（§10.6 仅提纲） |

### 3.9 与 ED-F02 / 他分支

| 项 | 关系 |
|----|------|
| ED-F02-S05 Abstract / 下拉图标 | **并入 S01** |
| ED-F02-S03 SkyBox preview | 仍属 ED-F02 |
| Hierarchy 父子拖拽 | **他分支已实现** — Out |
| ED-F04 PropertyPath | 不改 |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 失焦 = 提交（常见 DCC） | **不用** — 点空白 = 取消 |
| 独立 `m_EditorLabel` | **不用** — 用 `m_Name` |
| 视口图标与 S00 并行 | **不用** — Deferred |
| CB 只改显示名不改文件名 | **不用** — 对齐磁盘名 |
| 图标进 Runtime `ME_CLASS` metadata | **不用（本期）** — 见 §10.2 |
| 图标 JSON/外部配置 registry | **不用（本期）** — 类型少，C++ 表足够 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| ImGui 失焦/Esc 竞态 | 统一 cancel 标志；GO/Component/CB 同一 helper |
| 用户以为改名改了 `@` 路径 | 类型段始终可见；文档注明 |
| Move 破坏 Root | Root 置顶 + API 校验 |
| CB 改名断引用 | GUID 优先；路径引用写清 Out；目视清单 |
| 与他分支 Hierarchy 合并冲突 | 本 Feature 不改树拖拽；合并时只接显示 |

---

## 6) 验收标准（当前 Scope）

> 2026-09-09 维护者手测通过（含验收反馈 polish + 对齐修复）。`verify.ps1` 未作为本批门禁。

- [x] GO/Component：Esc **或** 点空白 → 取消并关闭；Enter → 提交
- [x] Add Component：图标 + 搜索；无 Abstract；右键 **Add Component ▶**（**仅 Inspector**，子菜单含搜索）
- [x] 组件头：`[icon] name TypeDisplay`；名不切词；类型切词且无 `Component`；Undo
- [x] 重排：Save 重开一致；Root 不参与；Undo
- [x] CB 内联重命名：Enter 成功；Esc/空白取消；重名拒绝；**F2 + 右键 Rename**
- [ ] `.\scripts\verify.ps1` 通过（本批未跑；不挡合入）
- [ ] （Deferred）视口图标 — 不挡 Feature 前序 Done

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done**（S00–S04）；S05 Deferred |
| Blocked reason | — |
| Unblock condition | S05 需维护者显式提优 |
| Next check | 已手测；待 commit 合入 |
| Owner | project maintainer |

---

## 8) 明确不做（本阶段）

| 项 | 说明 |
|----|------|
| 视口世界图标 | **Deferred（S05）** |
| Hierarchy 父子拖拽 | 他分支已有 |
| Duplicate GO / F-focus / 折叠记忆 / outline | 暂无需求或时机不宜 |
| Add 搜索 | **已在 S01** |

---

## 9) 已确认决策

| ID | 决策 |
|----|------|
| **A** | 空名：恢复原名并关闭（不提交空串） |
| **B** | 类型展示去 `Component` 后缀 + 切词 |
| **C** | 实例名 = `MEObject::m_Name`；**名不切词** |
| **D** | 名空：名段回退 `TypeDisplay` |
| **E** | Add 默认名：去后缀短类名（不切词），重名加序号 |
| **F** | Root 置顶，不重排 |
| **G** | 视口图标整刀 **Deferred**（原 Play 开关随 S05） |
| **H** | 不含 Duplicate / F-focus；**含 CB 内联重命名（S04）** |
| **I** | 顺序：**S00 → S01 → S02 → S03 → S04**；**S05 Deferred** |
| **J** | 点空白 / 非 Enter 失焦 = **取消**（与 Esc 同），仅 Enter 提交 |
| **K** | 组件头格式：**icon + name + TypeDisplay** |
| **L** | 图标：Editor 静态表 + 基类回退 |
| **M** | 未登记默认 `ICON_FA_PUZZLE_PIECE` |
| **N** | 重排仅 ↑↓ |
| **O** | CB 改名无 Undo |
| **P** | Inspector Add UI 实现时选稳妥方案（**须含搜索**） |
| **Q** | 右键 Add：**仅 Inspector GO**（Header / Components 空白）；Hierarchy 暂不含 |
| **R** | 次级菜单**要有搜索**（顶部 Filter 输入框） |
| **S** | 根级 `Add Component ▶`，去掉 Create 外壳 |

---

## 10) 实现设计（供审阅）

> 本节回答「打算怎么做」：落点文件、API 草图、数据流、与备选方案对比。开码时可微调，**语义契约以 §9 为准**。

### 10.0 公共工具（跨切片）

| 工具 | 位置（建议） | 职责 |
|------|--------------|------|
| `InlineRenameField`（名可改） | `Editor/src/UI/Widgets/InlineRenameField.h/.cpp` | 画 `InputText`；处理 Enter / Esc / 失焦；返回 `Commit` / `Cancel` / `Editing` |
| `ComponentTypeUiCatalog` | `Editor/src/Services/ComponentTypeUiCatalog.h/.cpp` | 类型 → Icon glyph + TypeDisplay；供 Add 列表与组件头共用 |
| `FormatTypeDisplayName` | Runtime `ReflectionDisplayNames` 或 Editor 薄封装 | 短类名 → 去 `Component` → 驼峰切词（**仅类型展示**） |
| `GetShortTypeName` | 已有于 `SceneEditorInspectorSource` | 去命名空间；可上提与 Catalog 共用 |

**不**新建全局单例「IconRegistry 服务模块」；Catalog 为普通 Editor 工具类（可 `static` 查询函数或轻量实例），与 `ContentBrowserWindow::ResolveAssetTypeIconGlyph` 同级。

---

### 10.1 S00 — 内联重命名退出

#### 问题根因（现状）

`HierarchyWindow` 大致逻辑：

```text
if (committed || IsItemDeactivatedAfterEdit()) → SubmitRename  // 提交
else if (IsKeyPressed(Escape)) → clear renaming id           // 取消
```

Escape 常先触发 InputText deactivate，于是 **先走提交**，Esc 分支无效或输入框状态错乱。点空白同理走 `DeactivatedAfterEdit` → 现网等于「提交」。

#### 目标状态机

```text
        BeginRename(buffer = oldName)
                │
                ▼
           ┌─ Editing ─┐
     Enter │           │ Esc 或 失焦(非 Enter)
           ▼           ▼
        Commit       Cancel
     (Submit*)    (关框；不 Dirty；不 Undo)
```

#### API 草图

```cpp
// Editor/src/UI/Widgets/InlineRenameField.h
enum class InlineRenameResult { Editing, Commit, Cancel };

struct InlineRenameDrawParams
{
    char* Buffer = nullptr;
    size_t BufferSize = 0;
    bool RequestFocus = false; // 进入编辑首帧 SetKeyboardFocusHere
};

InlineRenameResult DrawInlineRenameField(const InlineRenameDrawParams& params);
```

**判定规则（建议实现）：**
1. 本帧若 `IsKeyPressed(Escape)` → 记 `cancel = true`（须在处理 deactivate **之前**读键，或用 `ImGuiInputTextFlags_Callback*` / 帧初采样）。
2. `EnterReturnsTrue` / `committed` → `Commit`（若 buffer 空或仅空白 → 按确认 A：视为 Cancel 语义：恢复原名关框，不调用 Submit）。
3. `IsItemDeactivatedAfterEdit()` 且非本帧 Enter 提交 → `Cancel`（覆盖点空白、点其它控件）。
4. 否则 `Editing`。

调用方：

```text
switch (DrawInlineRenameField(...)) {
  case Commit: SubmitRename...(buffer); clear editing id; break;
  case Cancel: clear editing id; break; // 不 Submit
  case Editing: break;
}
```

#### 接入点

| UI | 改谁 | 提交仍走 |
|----|------|----------|
| Hierarchy GO | `HierarchyWindow` | 现有 `SceneEditor::SubmitRenameGameObject` / `RenameGameObjectCommand` |
| Inspector Component 名 | `SceneEditorInspectorSource`（S02 引入编辑态） | 新 `RenameComponentCommand` |
| Inspector GO 名（若已有/将有） | 同上 helper | 同 Hierarchy |
| CB 资产名 | S04 `ContentBrowserWindow` | `AssetManager::RenameAsset` |

#### 待你可问的点
- ImGui 版本下 `DeactivatedAfterEdit` 与 Esc 的精确帧序：实现时用 Editor 手测钉死；若 Esc 仍误 Commit，改为 `InputText` callback 吞掉 Esc 并 `ClearActiveID`。

---

### 10.2 S01 — Add Component 图标关联（核心）

#### 结论（推荐默认）

**用 Editor 侧 C++ 静态表（`ComponentTypeUiCatalog`），不是 Runtime 反射 registry，也不是 JSON。**

| 方案 | 做法 | 优点 | 缺点 | 本期 |
|------|------|------|------|------|
| **A. Editor 静态表** | `unordered_map` / 数组：短类名 → `ICON_FA_*` | 与 CB `ResolveAssetTypeIconGlyph` 一致；无 codegen；Core 不进「图标意见」 | 新类型要改一处表 | **选用** |
| B. `ME_CLASS` metadata `Icon=...` | 反射 ClassMetadata | 类型旁标注 | 要动标注/生成；图标是 Editor 观感进 Runtime | 不用 |
| C. JSON/配置 registry | 启动加载 | 可热改 | 过重；路径/加载又一套 | 不用 |
| D. 仅按基类硬编码 `if (IsA<Light>)` | 无表 | 快 | 同基类无法区分 Point/Spot | 可作 **继承回退**，不作唯一来源 |

#### Catalog 数据结构

```cpp
// Editor/src/Services/ComponentTypeUiCatalog.h
struct ComponentTypeUiInfo
{
    const char* IconGlyph = nullptr; // ICON_FA_* 字面量，指向静态串
    // TypeDisplay 运行时算，不进表
};

class ComponentTypeUiCatalog
{
public:
    /** short or reflected class name → glyph；未命中走基类链再默认 */
    static const char* ResolveIconGlyph(const Reflection::MEClass* componentClass);

    /** PointLightComponent → "Point Light" */
    static std::string MakeTypeDisplayName(const Reflection::MEClass* componentClass);
    static std::string MakeTypeDisplayName(std::string_view reflectedOrShortName);

    /** Add 默认实例名：去 Component 后缀、不切词，如 "PointLight" */
    static std::string MakeDefaultInstanceName(const Reflection::MEClass* componentClass);
};
```

#### 解析算法（图标）

```text
ResolveIconGlyph(class):
  1. key = ShortName(class->GetName())   // 去 minEngine:: 等
  2. if (table.count(key)) return table[key]
  3. walk SuperClass while IsA(Component):
       if (table.count(ShortName(super))) return that
  4. return ICON_FA_PUZZLE_PIECE  // 或 ICON_FA_CUBE — 实现时定一个默认
```

**表内容（v1 手写，示例）：**

| ShortName | Glyph（示意） |
|-----------|----------------|
| `DirectionalLightComponent` | `ICON_FA_SUN` |
| `PointLightComponent` | `ICON_FA_LIGHTBULB` |
| `SpotLightComponent` | `ICON_FA_LIGHTBULB`（或更贴切的 FA） |
| `SkyBoxComponent` | `ICON_FA_CLOUD_SUN` |
| `AudioComponent` | `ICON_FA_VOLUME_HIGH` |
| `AudioListenerComponent` | `ICON_FA_HEADPHONES` |
| `StaticMeshComponent` | `ICON_FA_CUBE` |
| `BoxColliderComponent` / `Sphere…` / `Capsule…` | `ICON_FA_VECTOR_SQUARE` 等 |
| `RigidBodyComponent` | `ICON_FA_WEIGHT_HANGING` |
| `LuaComponent` | `ICON_FA_CODE` |
| `CameraComponent`（若有） | `ICON_FA_VIDEO` |

未列出的具体类型：走基类（如未登记的 `XxxLightComponent` → `LightComponent` 若登记）→ 默认。

**基类行（可选）：** 表中可登记 `LightComponent`、`ColliderComponent` 等 **Abstract** 类型键，仅用于继承回退；Add 列表仍不展示 Abstract。

#### TypeDisplay 算法

```text
MakeTypeDisplayName(name):
  short = GetShortTypeName(name)
  if short ends with "Component": strip suffix
  return InsertCamelCaseWordBreaks(short)   // 升为 Reflection 共享 API 或 Catalog 内复制 CORE-F07 逻辑
```

例：`minEngine::PointLightComponent` → `PointLight` → `Point Light`。

#### Add 下拉 UI 改造

**现状：** `ImGui::BeginCombo` + `Selectable(短类名)`。

**目标：**
1. Combo 上方或内嵌 `InputText` Filter（子串匹配 `TypeDisplay` 与 short name，大小写不敏感）。
2. 每行：`PushFont(SolidIconFont)` + `Text(glyph)` + `SameLine` + `Selectable(TypeDisplay)`。
3. 类型列表来源仍为 `SceneEditor::GetAllComponentTypeNames()`（反射枚举）；**过滤**继续跳过 Abstract；S01 给真正抽象基类补 `ME_CLASS(Abstract)`，并逐步去掉 `InitializeComponentTypeNames` 里对 `PrimitiveComponent` / `LightComponent` 的 **硬编码名字黑名单**（改靠 specifier）。

图标字体：复用 `EditorAppearance::GetAssetIconSolidImFont()`（与 Play Toolbar / CB 相同）。

#### 同构服务（Inspector UI ≠ 菜单 UI）

抽出 **`AddComponentPicker`**（名可调；也可是 Catalog 上的一组静态绘制函数）：

| 职责 | 说明 |
|------|------|
| 数据 | `GetAllComponentTypeNames()` + `ComponentTypeUiCatalog` |
| `DrawInspectorAddSection(...)` | Filter + 列表/Combo + Add 按钮（面板内） |
| `DrawContextSubMenu(...)` | 在已打开的 `BeginMenu("Add Component")` **内部**：顶部 **搜索 Filter** + 类型行（icon + TypeDisplay）；与 Inspector **同构数据** |
| 执行 | 一律 `SelectGameObject(id)` + `SubmitAddComponentToSelectedGameObject` |

**同构 = 同一类型源、同一图标/TypeDisplay、同一 Submit 路径**；布局可不同（面板 vs 菜单）。

右键交互与可见性矩阵见 **§10.9**（不埋在 Create 下）。

#### 待你可问的点
- 是否接受「新 Component 忘了加表 → 默认图标」？
- 是否希望某类型强制用 Regular 而非 Solid（CB 有 Solid/Regular 分流）？MVP 建议 **一律 Solid**。

---

### 10.3 S02 — 组件头与改名

#### 展示布局（CollapsingHeader 区域）

现状：`CollapsingHeader(GetShortTypeName + "##id")`，左侧另有 Active checkbox。

目标行（同一行示意）：

```text
[✓ Active]  [FA icon]  [InstanceName 或 InputText]  [TypeDisplay(disabled)]  [↑][↓]
```

- icon：`Catalog::ResolveIconGlyph(component->GetClass())`
- InstanceName：只读时 `TextUnformatted(m_Name)`；空则显示 `MakeTypeDisplayName` 作名段回退
- TypeDisplay：`TextDisabled` / 次要色；**不可点改**
- 进入改名：双击名段，或右键菜单「Rename」（若已有 context menu 可挂）

Header 与 ImGui `CollapsingHeader` 抢点击：MVP 可用自定义 header 行（`TreeNode` / `Selectable` + 箭头）避免双击名与折叠冲突；或名段用独立 `InvisibleButton`/`InputText` 热区。实现时优先 **名段独立控件**，折叠点放在行首箭头。

#### 默认名（Add 时）

在 `SubmitAddComponentToSelectedGameObject` / `AddComponentCommand::Execute` 成功后：

```text
defaultName = Catalog::MakeDefaultInstanceName(class)  // "PointLight"
if conflict on same GO: append " 2", " 3", ...
component->SetName(defaultName)
```

#### Undo

```cpp
class RenameComponentCommand : public IEditorCommand
{
  // SceneEditor&, goId, component stable id 或 index+type,
  // oldName, newName
};
```

稳定指向：优先 `MEObject` GUID/ID（若 Component 有）；否则 `(ownerGoId, componentIndex)` 并在 Execute 时校验类型名。对齐现有 `RenameGameObjectCommand` 风格。

#### PropertyPath

不读 `m_Name` 选组件；改名不影响 `GO@PointLightComponent.x`。

---

### 10.4 S03 — 组件重排

#### Runtime API

```cpp
// GameObject.h
bool MoveComponent(Component& target, size_t newIndex);
```

语义：
1. 在 `m_Components` 找到 `target`；erase 后 insert 到 `newIndex`（先按 erase 校正 index）。
2. 若 `target` 是 `m_RootComponent` → **return false**（不允许动）。
3. 不改变 SceneComponent attach 父子；只改 vector 序。
4. 序列化已按 `m_Components` 顺序写读 → Save/Load 自然保持（实现时用现有 Scene 存一份验证）。

#### Editor

- `MoveComponentCommand(goId, fromIndex, toIndex)` 或存 component 身份 + 旧/新 index。
- Inspector：每个非 Root 组件头 **↑ / ↓**；边界 disable。
- 拖拽排序：可同切片用 ImGui payload；若工期紧则 S03 只做按钮，拖拽为 S03b（仍属本 Feature 可选，默认 MVP=按钮）。

#### Root 展示

Inspector 已有独立「Root Transform」块；Components 列表里 Root 仍可能出现（现状会画）。S03 约定：列表内 Root **无 ↑↓**；若与「Root Transform」重复感强，可另刀隐藏列表内 Root 的属性重复 — **本切片不强制改**，只禁重排。

---

### 10.5 S04 — Content Browser 内联重命名

#### 已有后端

`AssetManager` 已有：

```cpp
bool RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError);
bool MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError);
```

（会搬文件 + `.meta` + registry 路径更新。）

#### UI 流程

```text
F2 / 右键 Rename（树叶或瓦片） /（可选）双击名
  → ContentBrowserModule::RequestBeginAssetRename（右键经 EditorActionId::Rename）
  → InlineRenameField（§10.1 契约）
  → Enter: AssetManager::RenameAsset(meta.AssetPath, newFileName, err)
  → 失败: 提示 outError，保持选中
  → 成功: 刷新 AssetTreeModel / 选中新路径
```

入口：`F2` 与 **右键 Rename** 为 MVP 必达；与 Hierarchy/Inspector 共用 `EditorActionId::Rename`。
#### MVP 边界（实现时钉死）

| 做 | 不做 |
|----|------|
| 当前目录叶子资产 | 文件夹改名（可跟 `RenameAsset` 能力再开） |
| 保留扩展名（只改 stem） | 改扩展名 |
| 同目录冲突 → 拒绝 | 全项目路径字符串替换 |
| 无 CommandStack Undo（文件系统难） | 或仅日志；**默认无 Undo** |

引用：GUID 引用不断；若存在纯路径字符串引用，文档注明「改名后可能需手动修」——开 S04 前用 Grep 确认项目内引用方式写进切片笔记。

#### 树刷新

改名后调用现有 CB 刷新路径（`AssetTreeModel` rebuild 或局部 patch）；避免只改 UI 标签不改盘。

---

### 10.6 S05 — 视口图标（Deferred 提纲）

后置时再详设。方向：Edit 模式在 `SceneEditingViewportClient` post-draw 中，对白名单 Component 取世界位置 → 投影 → 画 `Catalog` 同款 glyph。Play 默认不画。

---

### 10.7 文件触碰一览（预计）

| 切片 | 主要文件 |
|------|----------|
| S00 | `HierarchyWindow.*`；新建 `InlineRenameField.*` |
| S01 | `ComponentTypeUiCatalog.*`；`AddComponentPicker.*`；`SceneEditorInspectorSource`；`SceneContextMenuProviders.*`；Abstract 标注；`InitializeComponentTypeNames` |
| S02 | `SceneEditorInspectorSource.*`；`RenameComponentCommand.*`；`ReflectionDisplayNames.*`（切词导出）；AddCommand 设默认名 |
| S03 | `GameObject.*`；`MoveComponentCommand.*`；Inspector ↑↓ |
| S04 | `ContentBrowserWindow.*`；复用 `InlineRenameField` + `AssetManager::RenameAsset` |
| S05 | Deferred：`SceneEditingViewportClient.*` 等 |

---

### 10.8 仍请你拍板的实现向问题（非 §9 行为）

| ID | 问题 | 草案默认 |
|----|------|----------|
| **L** | 图标关联用 Editor 静态表 + 基类回退？ | **是（§10.2 A）** |
| **M** | 未登记类型默认 glyph？ | `ICON_FA_PUZZLE_PIECE`（可改） |
| **N** | S03 重排 UI？ | **仅 ↑↓**；拖拽可选后续 |
| **O** | CB 改名 Undo？ | **不做**（仅成功/失败提示） |
| **P** | Inspector Add 用 Filter+Combo 还是 popup？ | 实现选更稳的一种 |
| **Q** | 右键 Add Component 可见范围？ | **仅 Inspector GO Header / Components 空白**；Hierarchy **暂不含**（可后开） |
| **R** | 次级菜单是否带**搜索**？ | **要** — 子菜单顶部 Filter 输入框（子串匹配 TypeDisplay / 短类名）；与 Inspector 同构数据，UI 可更紧凑 |
| **S** | 取消 Create 外壳，根级 `BeginMenu("Add Component")`？ | **是**（已拍板） |

---

### 10.9 上下文菜单健全性 + 右键 Add Component

#### 10.9.1 现状评估（是否健全？）

**结论：架构方向健全，可支撑「不同地方不同菜单」；产品规则与个别实现瑕疵在本 Feature 收紧即可，不必重做系统。**

| 维度 | 评估 |
|------|------|
| 模型 | **Typed Context 袋**（Hierarchy / SceneInspector / ContentBrowser）+ HitKind；窗口只 `Populate + BuildAndDraw` — 与 [EDITOR_CONTEXT_MENU_DESIGN](./EDITOR_CONTEXT_MENU_DESIGN.md) 一致 |
| 扩展 | 静态 `IEditorAction` + 动态 `EditorActionProvider` — 类型列表走 Provider，正确 |
| 分域可见性 | `ctx.Find<T>()` + hit kind 门控 — **不同右键面已能出不同菜单** |
| Add Component 现状 | `SceneAddComponentMenuProvider` **仅** Inspector `GameObjectHeader`；Hierarchy 在 M4.1 **故意排除** |
| UX 嵌套 | 现网：`Create ▸ Add Component ▸ 类型` — **多一层 Create**，不符合「先出现 Add Component，再点开次级」 |
| 已知瑕疵 | ① Components 空白复用整份 GO Header 菜单（含 Delete/Rename）；② `EnsureSectionOpen` 只支持 Create；③ 静态/Provider 双 `BeginMenu("Create")` 风险（今天两域不共弹窗，暂无双 Create）；④ CB ActionId 复用（无关本 Feature） |

#### 10.9.2 目标交互（理解对齐）

```text
右键命中「可 Add 的 GO 上下文」
  → 根菜单一项：Add Component ▶
  → 点击/悬停打开次级菜单（ImGui::BeginMenu）
       → **顶部搜索框** + 类型行：icon + TypeDisplay
       → 点击类型 → SubmitAddComponent（Undo 同 Inspector）
```

- 根菜单**不**直接铺满类型。
- **不**再包一层「Create」。
- 面板 Add UI 与菜单 UI 可不同；**服务同构**（Catalog + 类型列表 + Submit）。

#### 10.9.3 可见性矩阵（**已拍板 Q**：先仅 Inspector）

| 右键面 | Context | Add Component ▶ |
|--------|---------|-----------------|
| Hierarchy **GO 行** | `GameObjectItem` | **暂不**（后置；改门控即可加回） |
| Hierarchy **空白** | `Blank` | 否 |
| Inspector **GO Header** | `GameObjectHeader` | **显示** |
| Inspector **Components 空白** | 复用 Header 上下文 | **显示** |
| Inspector **Component 头** | `Component` | 否（保留 Remove） |
| Content Browser | CB context | 否 |

#### 10.9.4 实现要点

1. 改写 `SceneAddComponentMenuProvider`：门控仍为 Inspector `GameObjectHeader`（**暂不**扩 Hierarchy）；**去掉** `EnsureSectionOpen(Create)`；根级 `BeginMenu("Add Component")`；体内调用 `AddComponentPicker::DrawContextSubMenu`。
2. **次级菜单带搜索（确认 R）：** 子菜单顶部 `InputText` Filter；过滤 `TypeDisplay` / 短类名（大小写不敏感）；下列 `icon + TypeDisplay`。Filter 状态可用 `static`/菜单打开帧重置（打开菜单时清空），避免脏过滤残留。
3. 执行路径保持：`SelectGameObject` + `SubmitAddComponentToSelectedGameObject`。
4. 仍用 Provider，不新增静态 `EditorActionId::AddComponent`。

#### 10.9.5 与旧 M4.1 / Hierarchy

- Hierarchy 右键 Add：**本期仍不挂**（与维护者拍板一致；旧 M4.1 禁令暂保留）。
- 日后若要 Hierarchy：只扩 Provider 门控 + 从 `HierarchyMenuContext` 取 GO id，无需第二套 UI。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-09 | Draft：登记 ED-F05；初稿 A–I |
| 2026-09-09 | Review：点空白=取消；头=`icon+name+类型`；视口图标 Deferred；升入 CB 内联重命名；Hierarchy 树拖拽 Out（他分支）；收窄 §8 |
| 2026-09-09 | 增补 **§10 实现设计**：图标用 Editor Catalog 静态表；S00–S04 API/文件/数据流；待确认 L–P |
| 2026-09-09 | 拍板：L–P/S 默认；**Q=仅 Inspector**；**R=次级菜单要搜索**（Filter=搜索框） |
| 2026-09-09 | S04 补齐 CB **右键 Rename**（与 F2 同源） |
