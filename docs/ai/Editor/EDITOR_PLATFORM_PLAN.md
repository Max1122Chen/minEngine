# Editor 平台化 — 总体规划

## Meta

- **ID:** N/A
- **Status:** **Reference**
- **Owner:** project maintainer
- **Last updated:** 2026-05-26（正文）；Meta 2026-05-28
- **Related:** [EDITOR_SHELL_DESIGN.md](./EDITOR_SHELL_DESIGN.md), [ACTIVE_WORK.md](../ACTIVE_WORK.md)

> **Agent:** **Reference** — captures 2026-05-26 Editor platform intent and section checklist. **Not** an automatic backlog: items marked 未做 / 待做 / 当前 are optional future feats unless listed in ACTIVE_WORK or requested by the user. Verify “done” claims in code (e.g. Import to current Browser path, default Dock) before citing this plan.

## TL;DR

Editor shell, assets, Content Browser UI, Appearance, Scene Undo, and preview core are largely in place. Unified Inspector facade and several polish items remain **ideas**, not scheduled debt.

---

## 0) 立场

- **现有 Editor** 已具备 Shell（E0）、资产管线（E3/E4/P5–P6 数据层）、Appearance（M0–M6b）；**不再是**纯过渡 `EditorUIMode` 心智。
- **不以「修补 Content Browser」为唯一目标**，但 Browser **窗口 UI** 刻意延后到 Appearance 合入后再抛光（见 §3、§7）。
- **Content Browser** 基础设施（`AssetTreeModel`、Registry 订阅、Open/Delete/Import）**已完成**；列表/树/工具栏的 **工具风 UI** 为当前最近交付。
- 分项 `INSPECTOR_DESIGN.md` 仍待写；**E2** 见 [PREVIEWER_DESIGN.md](./PREVIEWER_DESIGN.md)（**v0.6，核心已实现**）；**Undo** 见 [EDITOR_COMMAND_HISTORY.md](./EDITOR_COMMAND_HISTORY.md)（**E1.1–E1.4 已验收**）。

---

## 1) 四大板块 — 实现状态（2026-05-26）

> 架构复盘：[EDITOR_ARCHITECTURE_REVIEW.md](./EDITOR_ARCHITECTURE_REVIEW.md)  
> Shell 分项：[EDITOR_SHELL_DESIGN.md](./EDITOR_SHELL_DESIGN.md)（**E0 已实现**）

### E0 — Editor Shell 与模块体系 — **已合入**

| 目标项 | 状态 |
|--------|------|
| `Editor` bootstrap、模块注册、`ActivateSubModule` | ✓ |
| `EditorSubModule`（Scene / Material）、互斥激活 | ✓ |
| `EditorServiceModule`（Console、`AssetWorkflow`、`ContentBrowser`） | ✓ |
| GUIManager 工厂；layout 归 Module | ✓（默认 Dock 位 **P7**） |

---

### E1 — Inspector 体系 — **过渡实现，待统一增强**

| 目标项 | 状态 |
|--------|------|
| 编辑语境 / `InspectorTarget` 模型 | 未做；现为 `InspectorWindow` + 多 `IEditorInspectorSource` 路由 |
| Drawer 注册 | 未做；Scene / Material / AssetWorkflow 各一套 Source |
| 选择解耦 | 部分：Hierarchy→Scene；CB 聚焦→`AssetWorkflow` Meta；Graph→Material Details |
| Property 一致性 | ✓ Appearance **PropertyWidgets** + `PropertyEditPolicy`（Scene/Material） |
| Asset Meta 展示 | **裸 `ImGui::Text`**（`AssetWorkflowInspectorSource`） |
| 多选 / Unsupported UX | 未做 |

**合并后路由（`InspectorWindow`）：** Content Browser 聚焦且有权选 → Asset Meta；否则 Active `EditorSubModule` → Scene/Material Source。

---

### E2 — Previewer 统一化 — **核心已合入**（子项延后）

| 目标项 | 状态 |
|--------|------|
| 薄 `PreviewScene` + Material 视口迁移 | ✓ E2.1 |
| Inspector 方槽 + `InspectorAssetInspection` | ✓ E2.2（Material Scene3D） |
| StaticMesh 检视 + `DefaultMaterial.memtl` | ✓ E2.3a |
| Texture2D Inspector 预览 | ⏳ **E2.2b 延后**（正交相机 + MaterialInstance） |
| CB Tile 缩略图 | ⏳ E2.3b |
| Material 视口飞行/轨道 | ⏳ E2.4 |

详见 [PREVIEWER_DESIGN.md](./PREVIEWER_DESIGN.md) §8。

---

### P3 — Editor Undo（Command Stack）— **首轮已合入**

| 目标项 | 状态 |
|--------|------|
| `EditorCommandStack` + 项目 `MaxUndoStackDepth` | ✓ E1.1 |
| Scene 结构编辑 Command（Rename/Transform/GO/Component） | ✓ E1.2 |
| BinaryArchive + 公开 Property 序列化 API | ✓ S1–S2 |
| Inspector 属性 Undo（`SetObjectPropertyCommand`） | ✓ E1.3 |
| GO/Component Snapshot Undo | ✓ E1.4 |
| Material 图/属性 Undo | ⏳ E1.5 |
| `TryMerge` / Composite / 偏好 UI | ⏳ Command E2 |

详见 [EDITOR_COMMAND_HISTORY.md](./EDITOR_COMMAND_HISTORY.md)。

---

### E3 — AssetManager 基础设施 — **首轮已合入**

| 目标项 | 状态 |
|--------|------|
| Path ↔ Meta ↔ GUID | ✓ 扫描 + sidecar `.meta` |
| 变更感知 | ✓ `Subscribe` + `ProjectAssetWatcher`（efsw） |
| Import / Delete | ✓ API + Editor 入口；Move/Rename UI 后置 |
| 类型表 | ✓ `AssetTypeRegistry`（Font、Texture、Material…） |
| 依赖图 / 异步 Reimport | 未做 |

文档：[ASSET_PIPELINE_P5_API.md](../Platform/ContentBrowser/ASSET_PIPELINE_P5_API.md)、[ASSET_PIPELINE_P6_API.md](../Platform/ContentBrowser/ASSET_PIPELINE_P6_API.md)

---

### E4 — 跨平台 FileDialog — **Win 首轮已合入**

| 目标项 | 状态 |
|--------|------|
| `IFileDialogService` | ✓ |
| NFD 实现 | ✓ Windows |
| Import 流程 | ✓ `ImportAssetDialog`（v0 目标目录仍多为固定 `Imported/`，**P6.1/P7** 可改当前 Browser 路径） |
| macOS / Linux | 未做 |

---

## 2) 板块依赖（修订顺序）

```text
        E0 Editor Shell                    [Done]
              ↓
    E3 AssetManager + E4 FileDialog        [Done 首轮]
              ↓
    P6  Content Browser 数据 + 窗口框架    [Done]
              ↓
    P6.1 Content Browser UI（Appearance）  [Done]
              ↓
    P3  Undo（E1.1–E1.4 + S1–S2）           [Done]
              ↓
    E2 Previewer 核心（E2.1–E2.3a）        [Done]
              ↓
    E1 Inspector 统一化增强                [当前]
              ↓
    P7  Dock / 菜单 / Import 路径等产品集成
```

**并行已完成：** [EDITOR_APPEARANCE.md](./EDITOR_APPEARANCE.md) **M0–M6b**（Color、主题、PropertyWidgets、Font、排版、主题色清扫）。

---

## 3) Content Browser 定位（更新）

**产品意图：** [CONTENT_BROWSER_DESIGN.md](../Platform/ContentBrowser/CONTENT_BROWSER_DESIGN.md)

| 层 | 状态 |
|----|------|
| **P6 基础设施** | ✓ `ContentBrowserModule`、`AssetTreeModel`、`Registry` 订阅、Open/Delete/Import、Inspector 最小路由 |
| **P6.1 窗口 UI** | ✓ 左树含资产、Tile 网格、面包屑、Appearance |
| **E1 资产 Inspector** | Meta 改 Property/Drawer；与 Scene Inspector 统一门面（**待做**） |
| **E2 Preview** | ✓ Material/StaticMesh Scene3D；Texture2D **延后**（E2.2b） |

**刻意分工（2026-05-26）：** asset-workflow 分支 **不** 接 Appearance API，避免与 `feat/editor-appearance` 冲突；合并 master 后由 **P6.1** 统一接 `EditorWindowTypography` / `EditorThemeScope`。

---

## 4) Editor Appearance 线（并行轨道，已合入 master）

独立里程碑，支撑全 Editor 工具风；**不替代** E1/E2 架构，但 **P6.1 / E1 属性 UI 直接依赖**。

| 阶段 | 内容 | 状态 |
|------|------|------|
| M0–M1 | `Color`/`LinearColor`、主题 Dark/Light、`EditorAppearance` | ✓ |
| M2–M4 | `PropertyEditPolicy/Session`、PropertyWidgets、`TransformWidget`、Scene Undo | ✓ |
| M3.1 | `ObjectPtrWidget`、Asset 引用 | ✓ |
| M5–M5.1 | Font 资产、排版角色、`EditorTypographyScope`、CJK glyph（显示后置 i18n） | ✓ |
| M6 | `EditorThemeScope`、语义色、窗口 `PushStyleColor` 清扫 | ✓（M6c 材质图节点色后置） |
| **M7** | Object 引用 Picker（非 Asset） | 可选 |
| **后续** | CJK 可读 UI（i18n）、Appearance 设置 UI、M6c 图域主题 | 未做 |

详见 [EDITOR_APPEARANCE.md](./EDITOR_APPEARANCE.md)、[FONT_ASSET_DESIGN.md](./FONT_ASSET_DESIGN.md)、[EDITOR_THEME_M6_DESIGN.md](./EDITOR_THEME_M6_DESIGN.md)。

---

## 5) 当前阶段工作流（2026-05-26 修订）

| 顺序 | 工作 | 状态 |
|------|------|------|
| **1** | **P6.1 Content Browser UI** | ✓ |
| **2** | **P3 Undo**（`EditorCommandStack`、E1.1–E1.4、S1–S2） | ✓ |
| **3** | **E2 Preview 核心**（`PreviewScene`、Inspector 检视、Material 视口） | ✓ |
| **4** | **E1 拍板 + 实现** | **当前** — `INSPECTOR_DESIGN.md`、`InspectorTarget`、Asset Meta Property |
| **5** | **P7** | 待做 — 默认 Dock、Import 到当前 Browser 路径、菜单 |
| **6** | **E2 延后子项** | E2.2b Texture / E2.3b CB 缩略图 / E2.4 Material 视口相机 |
| **7** | **Undo 延后** | E1.5 Material Command；Command E2 TryMerge/Composite |

---

## 6) 与 Platform 路线图关系

| 条目 | 状态 |
|------|------|
| P2 Editor 平台化 | **进行中**：E0/E3/E4/P6/P6.1/E2 核心 Done → **E1** → P7 |
| P3 Undo | **首轮 Done**（E1.5 / Command E2 延后） |
| P4 / P5 | 不变 |

---

## 7) 参考（当前代码）

| 模块 | 路径 / 说明 |
|------|-------------|
| Shell | `Editor.cpp`、`EditorSubModule`、`AssetWorkflowModule`、`ContentBrowserModule` |
| Content Browser | `ContentBrowserWindow.cpp`、`AssetTreeModel.*` |
| Inspector 路由 | `InspectorWindow.cpp` → Scene / Material / `AssetWorkflowInspectorSource` |
| Appearance | `EditorAppearance.*`、`EditorTypographyScope`、`EditorThemeScope` |
| Asset 核心 | `AssetManager.*`、`AssetTypeRegistry.*`、`ProjectAssetWatcher.*` |
| FileDialog | `FileDialogService`、`NativeFileDialogService` |
| Preview | `PreviewScene`、`InspectorAssetInspection`、`MaterialEditorViewportClient` |
| Undo | `EditorCommandStack`、`Editor/src/Commands/Scene/`、`SetObjectPropertyCommand` |

---

## 8) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-24 | 初稿：E0–E4 + CB 后置 |
| 2026-05-26 | 对齐 master：E0/E3/E4/P6 Done；Appearance M0–M6b；路线 P6.1→E1→E2；§4 Appearance 表 |
| 2026-05-26 | **P6.1 / P3 Undo / E2 核心** 标 Done；当前重点 E1 + P7；E2.2b/E1.5 等延后见 Platform §6 |
