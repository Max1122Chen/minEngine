# Editor 平台化 — 总体规划

Last updated: 2026-05-24  
Status: **规划（先对标学习，再分项详细设计）**  
父文档：[Platform 路线图](../Platform/PLATFORM_ROADMAP.md)

---

## 0) 立场

- **现有 Editor 视为过渡实现**（`EditorUIMode`、GO-only Inspector、`MaterialPreviewViewport` 等验证过 UX，但不是长期骨架）。
- **不以「修补 Content Browser」为心智模型**，而是建立可支撑 Content Browser、专用 Asset Editor、Undo、后续脚本的 **Editor 基础层**。
- **Content Browser 是消费方之一**，排在 Inspector / Previewer / Asset 基础设施 / FileDialog 之后或与之并行，但 **不先于基础层闭门定稿**。
- **当前阶段只记录目标项与开放问题**；分项 `*_DESIGN.md` 在广泛参考 UE / Unity / Godot 后再写。

---

## 1) 四大板块 + Editor Shell（主要目标）

> **2026-05-24 补充：** 架构复盘与 Sub-Editor 方向见 [EDITOR_ARCHITECTURE_REVIEW.md](./EDITOR_ARCHITECTURE_REVIEW.md)。**E0（Editor Shell 拆分）优先于 E1 大量实现。**

### E0 — Editor Shell 与模块体系（当前最优先）

**目标：** **`class Editor` 保留类名**；领域 **`EditorSubModule` → `SceneEditor` / `MaterialEditor`**；横切 **`EditorServiceModule`**（Console、OpenAsset）；**选择 per SubModule**。

| 目标项 | 说明 |
|--------|------|
| Editor | bootstrap、模块注册表、`ActivateSubModule` |
| EditorSubModule | 互斥激活、自管 layout/窗口/选择/InspectorSource |
| EditorServiceModule | Console、AssetWorkflow（OpenAsset）；**非** SubModule |
| GUIManager | 仅 Register/工厂；layout 归 Module |

**分项设计：** [EDITOR_SHELL_DESIGN.md](./EDITOR_SHELL_DESIGN.md) v2

---

### E1 — Inspector 体系

**目标：** Inspector 作为 **统一门面**，按「当前编辑语境」路由到不同 **Drawer 后端**。

| 目标项 | 说明 |
|--------|------|
| 编辑语境模型 | 定义 `EditorContext` / `InspectorTarget`（如：Scene 中 GO+Component、Project 中 Asset、Material 会话中 Graph 节点等） |
| Drawer 注册与生命周期 | 可扩展的 `IInspectorDrawer`（或等价物）；按 Target 类型 / 优先级选择 Drawer |
| 与选择系统解耦 | Hierarchy、Content Browser、Graph 等 **选择源** 只更新 Target；Inspector 只读 Target |
| 属性编辑一致性 | 复用 Reflection 绘制；只读 vs 可编辑策略；与 Undo（P3）预留接口 |
| 多 Target / 空态 | 无选择、多选、Unsupported 类型的 UX |

**暂不决定：** Drawer 是否按窗口拆分、Details 与 Preview 是否同一 Panel（见 E2）。

**对标学习（待读）：**

| 引擎 | 入口 |
|------|------|
| UE | `IDetailsView` / Property Editor、`FPropertyEditorModule`、Details 定制 |
| Unity | `Editor` + `CustomEditor` / `PropertyDrawer`、`SerializedObject` |
| Godot | `EditorInspector`、`EditorPlugin` 扩展 Inspector |

**开放问题：**

- Component 与 Asset 是否共用同一 Inspector 窗口，还是 Tab？
- Material Graph 节点 Inspector 与 Asset Inspector 是否同一套 Drawer 树？

---

### E2 — Previewer 统一化

**目标：** 建立 **可分类、可组合** 的预览体系，避免每种资产复制一套 `MaterialPreviewViewport`。

| 目标项 | 说明 |
|--------|------|
| 预览 taxonomy | **Scene 型**（Mesh、Material、Skeleton、Animation… 需 3D 世界） vs **Flat 型**（Texture、Text、Sprite、曲线… 2D/直出） vs **None/Metadata-only** |
| PreviewScene | 轻量、与 Active Scene **隔离** 的预览世界（光照、相机、占位网格）；生命周期与域 GC 对齐 [内存管理](../Platform/MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) |
| PreviewViewport | 渲染到 RT；供 Inspector 嵌入或独立窗口；统一相机/光照默认值 |
| PreviewSession / Host | 谁创建/销毁 PreviewScene；多 Inspector 是否共享同一 Session |
| Material Editor 关系 | **倾向：组合通用 Scene Preview + Material 专用扩展**（而非继承整条 Viewport）；Editor 内 Preview 可增 capability（换 mesh、调光、编译刷新） |

**暂不决定：** 缩略图生成是否复用 PreviewScene；Animation 预览是否独立 Timeline。

**对标学习（待读）：**

| 引擎 | 入口 |
|------|------|
| UE | Asset Editor Viewport、`FAdvancedPreviewScene`、`UThumbnailManager` |
| Unity | Preview Render Utility、`AssetPreview`、Model/Material Inspector 预览 |
| Godot | 子 viewport 预览、Import dock 3D 预览 |

**开放问题：**

- Inspector 内嵌 Preview 的尺寸/刷新策略 vs 专用 Preview 窗口？
- `MaterialEditor` Preview 是否与 Content Browser 选中 Material 共用 `PreviewSession`？

---

### E3 — AssetManager 基础设施

**目标：** Runtime 侧 **资产注册表 + 变更能力 + 变更感知**，Editor 与 Content Browser 均依赖此层。

| 目标项 | 说明 |
|--------|------|
| Registry 模型 | Path ↔ Meta ↔ GUID ↔ 可选 Loaded 实例；与磁盘一致性的不变量 |
| 变更感知 | Import / Delete / Move / Reimport / Meta 变更 → **事件或 delegate**（Editor 订阅刷新） |
| 变更操作 | 原子 API：Import、Delete、Move/Rename、（可选）External 引用 |
| 扫描策略 | 全量 Scan vs 目录增量 vs 单文件注册 |
| 类型与扩展名 | 单一来源的类型表（Browser 过滤、FileDialog filter、Loader 共用） |
| 与 ObjectManager | 加载资产为引擎根；Delete 与 cache / GC 策略一致 |

**暂不决定：** 依赖图（Asset Registry Dependencies）、异步 Reimport、Addressables 式路径抽象。

**对标学习（待读）：**

| 引擎 | 入口 |
|------|------|
| UE | `IAssetRegistry`、`FAssetRegistry`、`AssetTools`、Redirectors |
| Unity | `AssetDatabase`、Import Pipeline、`AssetPostprocessor` |
| Godot | `EditorFileSystem`、资源 UID、`ResourceImporter` |

**开放问题：**

- Meta 是否始终 sidecar（`.meta`）？Engine 默认资产是否同一套 Registry？
- Delete 时场景/材质内引用如何报告（v0 WARN vs 阻塞）？

---

### E4 — 跨平台 FileDialog

**目标：** Editor 通过 **抽象接口** 调用 OS 文件选择，不绑定 Win32 实现。

| 目标项 | 说明 |
|--------|------|
| `IFileDialogService` | OpenFile(s)、SaveFile、SelectFolder；filter 与 E3 类型表联动 |
| 平台实现 | Windows 原生优先；后续 macOS / Linux（NFD 等） |
| 与 Import 流程 | Dialog → 目标目录（Content Browser 当前路径）→ E3 `Import` |

**暂不决定：** 是否引入第三方库（如 nativefiledialog）还是各平台薄封装。

**对标学习（待读）：**

| 引擎 | 做法 |
|------|------|
| UE | `IDesktopPlatform`、Slate File Picker |
| Unity | `EditorUtility.OpenFilePanel` 等 Editor-only API |
| Godot | `EditorFileDialog`、`DisplayServer` file dialog |

---

## 2) 板块依赖（建议顺序）

```text
        E0 Editor Shell + Sub-Editor 拆分
              ↓
E1 Inspector ←── E2 Previewer
              ↑
    E3 AssetManager + E4 FileDialog
              ↓
    Content Browser / Asset 双击 Open
P3 Undo（Shell CommandHistory）
```

- **E3 + E4** 可先做接口与 Win32 实现，不依赖 UI 细节。
- **E1 + E2** 强相关，设计阶段建议 **一起读 UE Details + Preview Scene**，再定 Inspector 是否内嵌 Preview。
- **Content Browser** UI：依赖 E3 事件 + E1 选择 +（可选）E2 缩略图（后期）。

---

## 3) Content Browser 的定位（产品意图保留，实现后置）

用户期望（摘要，细节见 [ContentBrowser/CONTENT_BROWSER_DESIGN.md](../Platform/ContentBrowser/CONTENT_BROWSER_DESIGN.md) §0）：

- Shared 窗口；Project `Assets/` 树；类型过滤；Import；选中 → Inspector；双击 → Asset Editor；右键 Delete/Move。

**实现前提：** E1（Asset Target Inspector）、E3（Registry + 变更）、E4（Import 对话框）；E2 提供 Inspector 内预览；Editor 路由与 Focus 可作为 **E5（Editor Shell 重构）** 另文，或与 E1 合并讨论。

---

## 4) 建议的后续工作流（当前阶段）

1. **对标阅读**（每板块 0.5–1 天笔记，写入 `docs/ai/Editor/research/` 或各 `*_DESIGN.md` 的「Research」节）。
2. **拍板 E1↔E2 边界**（Inspector 是否包含 Preview 区；PreviewSession 归属）。
3. **写分项设计草稿**：`INSPECTOR_DESIGN.md`、`PREVIEWER_DESIGN.md`、`ASSET_REGISTRY_DESIGN.md`、`FILE_DIALOG_DESIGN.md`（尚未创建）。
4. **再拆实施里程碑**（不在本文档写具体类名与工期）。

---

## 5) 与 Platform 路线图关系

| 原优先级 | 调整后 |
|----------|--------|
| P2 Content Browser | **P2 Editor 平台化**（E1–E4 + Content Browser 消费） |
| P3 Undo | 仍后置；E1/E3 API 预留 Command 边界 |
| P4 MEFunction / P5 Lua | 不变 |

---

## 6) 参考（当前代码，过渡实现）

- `InspectorWindow` — GO-only，待 E1 替换心智
- `MaterialPreviewViewport` / `MaterialEditorPreview` — E2 参考样本
- `AssetManager::ScanAssets` — E3 起点
- `EditorUIMode` / `EditorGUIManager` — Editor Shell，待 E5 或随 E1 演进
