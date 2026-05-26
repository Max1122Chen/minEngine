# Asset Pipeline — P6 接口定稿（审批用）

Last updated: 2026-05-26  
Status: **已实现并合入 master**（`25552f9` merge）；**Browser UI 抛光 → [CONTENT_BROWSER_UI_DESIGN.md](./CONTENT_BROWSER_UI_DESIGN.md)（P6.1）**  
前置：**P5** `ProjectAssetWatcher` + Registry 同步（`aab00b6`…`1158b02`）  
父文档：[ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §8（Content Browser）、§9 P6  
P5 定稿：[ASSET_PIPELINE_P5_API.md](./ASSET_PIPELINE_P5_API.md)

---

## 0) 已拍板 / 约束（本稿依据）

| # | 决策 |
|---|------|
| 1 | **P6 = Content Browser 数据 + 窗口框架**；默认 Dock / 菜单全集成归 **P7** |
| 2 | **D12**：列表仅显示 Registry 中、有 sidecar `.meta` 的已注册资产 |
| 3 | 磁盘目录树 + Registry 列表 **解耦**：树来自 `ProjectContentRoot` 文件系统；列表来自 `AssetManager` 查询 |
| 4 | **UI 视觉**：v0 仅用 **裸 ImGui**（`TreeNode` / `Selectable` / `Table` / `Text`）；**不**接 `EditorAppearance` 主题 token、**不**用 `UI/Property/**` 画 Meta |
| 5 | **Appearance 已合入 master（2026-05-26）**：**P6.1** 接主题/排版（见 [CONTENT_BROWSER_UI_DESIGN.md](./CONTENT_BROWSER_UI_DESIGN.md)） |
| 6 | **纪律**：不改 `UI/Property/**`、`Color.*`、`EDITOR_APPEARANCE.md`；C++ 成员函数优先 |

### 0.1) 与 Appearance 线的关系（2026-05-26 更新）

| 能力 | P6 v0（已合入） | P6.1（当前） |
|------|-----------------|--------------|
| 主题色、圆角、字体 | **未接**；裸 ImGui Style | 接 `EditorAppearance`、`EditorWindowTypography`、`EditorThemeScope` |
| Property 画 Meta | 裸 `Text` | 仍归 **E1**；P6.1 只抛光 Browser 窗口本体 |
| Inspector 路由 | Browser 聚焦 → `AssetWorkflowInspectorSource` | 保持；E1 再统一 Target |
| Focus | `SetContentBrowserInspectorActive` | 保持 |

**P6.1 设计案（已拍板 UX）：** [CONTENT_BROWSER_UI_DESIGN.md](./CONTENT_BROWSER_UI_DESIGN.md) — 左树含资产、右图标网格、面包屑；双击仅 Log；Inspector Debug 窗缺陷见该文档 §4。

---

## 1) P6 交付边界

### 1.1 包含

| 项 | 说明 |
|----|------|
| **`ContentBrowserModule`** | `EditorServiceModule`；`Register` 时注册 `ContentBrowserWindow` |
| **`AssetTreeModel`** | 目录树 + 当前路径 + 当前目录资产列表；订阅 Registry 增量刷新 |
| **`ContentBrowserWindow`** | `EditorWindow`；左右分栏 **框架**（树 / 列表 / 简易工具条） |
| **`AssetWorkflowModule` 扩展** | 选中资产、`DeleteSelectedAsset`、`IEditorInspectorSource`（只读 Meta） |
| **Registry 订阅** | `AssetManager::Subscribe` → `Registered` / `Unregistered` / `Moved` / `MetaUpdated` |
| **操作** | 双击 `OpenAsset`；Delete 调 `DeleteAsset`；Import 调既有 `ImportAssetDialog`（v0 仍 **固定 `Imported/`**，见 §7） |
| **Inspector 最小路由** | Browser 面板聚焦时 Inspector 显示资产 Meta（§6.4） |
| **可选 Runtime 辅助** | `FindAssetMetasUnderDirectory(relativeDir)`（若模型层不便扫私有表） |

### 1.2 不包含（P6.1 / P7 / 后置）

| 项 | 阶段 |
|----|------|
| 默认 Dock 位（Scene 右下） | **P7** |
| MainMenu 全面接线（除已有 Import） | **P7** |
| Import 到 **当前 Browser 目录** | **P6.1** 或 P7 |
| 缩略图、拖拽、多选、列排序、过滤搜索 | 后置 |
| Move/Rename UI | v1 |
| Undo 包装 Delete/Import | Command 线 |
| `Reimported` + `m_LoadedAssetCache` 失效 | **P5.1**（可与 P6 并行） |
| Appearance 主题 / Property Meta 绘制 | **merge 后 P6.1** |
| 引用检查阻塞 Delete | 后置 |
| 空目录隐藏（D12 扩展） | v1 |

---

## 2) 分层与依赖

```text
ContentBrowserWindow (ImGui 框架 only)
        │ 读/写
        ▼
AssetTreeModel ──subscribe──► AssetManager (Registry 事件)
        │ 选中 Meta
        ▼
AssetWorkflowModule ──OpenAsset──► EditorSubModule (Material / Scene …)
        │ DeleteSelectedAsset
        ▼
AssetManager::DeleteAsset
        ▲
ProjectAssetWatcher (P5，已存在)
```

| 层 | 禁止 |
|----|------|
| `AssetTreeModel` | 直接改 `m_AssetRegistry` |
| `ContentBrowserWindow` | 调用 `AssetManager` CRUD（经 `AssetWorkflowModule` / Model） |
| Runtime | 依赖 ImGui / Editor |

---

## 3) 新建与修改文件（预计）

| 文件 | 动作 |
|------|------|
| `Editor/src/Services/ContentBrowser/ContentBrowserModule.h` | **新建** |
| `Editor/src/Services/ContentBrowser/ContentBrowserModule.cpp` | **新建** |
| `Editor/src/Services/ContentBrowser/AssetTreeModel.h` | **新建** |
| `Editor/src/Services/ContentBrowser/AssetTreeModel.cpp` | **新建** |
| `Editor/src/UI/EditorWindows/ContentBrowserWindow.h` | **新建** |
| `Editor/src/UI/EditorWindows/ContentBrowserWindow.cpp` | **新建** |
| `Editor/src/Services/AssetWorkflowModule.{h,cpp}` | 选中、Delete、InspectorSource |
| `Editor/src/UI/EditorWindows/InspectorWindow.cpp` | **小改**：Browser 聚焦时优先 Asset Inspector（§6.4） |
| `Editor/src/Editor.{h,cpp}` | 注册 `ContentBrowserModule`（成员，与 `AssetWorkflow` 同级） |
| `Runtime/Resource/AssetManager.{h,cpp}` | **可选** `FindAssetMetasUnderDirectory` |
| `docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md` | P6 链接 |
| `docs/ai/PROGRESS_LOG.md` | 实现完成后追加 |

**不修改：** `UI/Property/**`、`Color.*`、`EDITOR_APPEARANCE.md`、`EditorAppearance.*`（除非 merge 后单独 P6.1）

---

## 4) 模块公开形状（定稿）

### 4.1 `ContentBrowserModule`

```cpp
class ContentBrowserModule : public EditorServiceModule
{
public:
    static constexpr const char* kModuleId = "ContentBrowser";

    std::string_view GetModuleId() const override;
    void Register(IEditorContext& context) override;
    void Shutdown() override;

    AssetTreeModel& GetModel();
    const AssetTreeModel& GetModel() const;

private:
    IEditorContext* m_Context = nullptr;
    std::unique_ptr<AssetTreeModel> m_Model;
    // ContentBrowserWindow 由 GUIManager 持有；Module 可缓存非拥有指针
};
```

### 4.2 `AssetTreeModel`（纯数据，无 ImGui）

```cpp
class AssetTreeModel
{
public:
    void ResetForProject(const std::filesystem::path& projectContentRoot);
    void Clear();

    void SetCurrentDirectory(std::string_view projectRelativeDir); // "" = root
    std::string_view GetCurrentDirectory() const;

    // 目录树：懒展开或一次性缓存（v0 可每次 Draw 前按需遍历，目录深时 P6.1 再缓存）
    struct DirectoryNode
    {
        std::string RelativePath;   // "" 或 "Meshes/BasicShapes"
        std::string DisplayName;   // 末段目录名
        std::vector<DirectoryNode> Children;
    };

    const DirectoryNode& GetDirectoryTreeRoot() const;

    // 当前目录下已注册资产（D12）
    const std::vector<const AssetMeta*>& GetAssetsInCurrentDirectory() const;

    void OnRegistryChange(const AssetRegistryChange& change);

    uint32_t SubscribeToAssetManager();
    void UnsubscribeFromAssetManager();

private:
    void RebuildDirectoryTree();
    void RebuildCurrentDirectoryAssetList();
    bool IsAssetInDirectory(std::string_view assetRelPath, std::string_view dirRel) const;

    std::filesystem::path m_ContentRoot;
    std::string m_CurrentDirectoryRel;
    DirectoryNode m_TreeRoot;
    std::vector<const AssetMeta*> m_CurrentAssets;
    uint32_t m_RegistrySubscriptionId = kInvalidAssetRegistrySubscriptionId;
};
```

**目录下资产枚举（v0 策略，二选一，实现时择一并在 PR 说明）：**

| 方案 | 做法 | 优缺点 |
|------|------|--------|
| **A（推荐）** | `AssetManager::FindAssetMetasUnderDirectory(dirRel)` | O(注册表)；需在 Runtime 加小 API |
| **B** | 合并各 `FindAssetMetasByType` 再按 path 前缀过滤 | 无 API 变更；类型多时略冗余 |

**`OnRegistryChange` 增量（v0 可简化）：**

| Kind | 动作 |
|------|------|
| `Registered` / `MetaUpdated` | 若 path 在当前目录 → 刷新列表；必要时重建树 |
| `Unregistered` | 从列表移除 |
| `Moved` | 旧/新 path 涉及当前目录 → 刷新列表 |
| 不确定 / 批量 | `RebuildCurrentDirectoryAssetList()` |

### 4.3 `ContentBrowserWindow`（UI 框架 — 视觉后置）

```cpp
class ContentBrowserWindow final : public EditorWindow
{
public:
    explicit ContentBrowserWindow(IEditorContext& context, AssetTreeModel& model);

    const std::string& GetId() const override;
    const std::string& GetTitle() const override;
    std::string_view GetOwnerModuleId() const override; // v0: 空 = 全局可见

    void OnDraw() override;

private:
    void DrawToolbar();
    void DrawDirectoryTree();
    void DrawAssetList();
    void DrawSelectionSummary(); // v0 可选：选中一行 Meta 摘要（ImGui::Text）

    AssetTreeModel& m_Model;
};
```

**窗口 Id 建议：** `"ContentBrowser"`；标题 `"Content Browser"`。

### 4.4 `AssetWorkflowModule` 扩展

```cpp
class AssetWorkflowInspectorSource : public IEditorInspectorSource
{
public:
    explicit AssetWorkflowInspectorSource(AssetWorkflowModule& owner);

    bool HasInspectableSelection() const override;
    void DrawInspector() override;

private:
    AssetWorkflowModule& m_Owner;
};

class AssetWorkflowModule : public EditorServiceModule
{
public:
    // … 已有 OpenAsset / ImportAssetDialog …

    void SetSelectedAsset(const AssetMeta* meta);       // nullptr = 清除
    const AssetMeta* GetSelectedAsset() const;

    void SetContentBrowserInspectorActive(bool active); // Browser 聚焦时 true
    bool IsContentBrowserInspectorActive() const;

    IEditorInspectorSource* GetInspectorSource();
    const IEditorInspectorSource* GetInspectorSource() const;

    void DeleteSelectedAsset();

private:
    const AssetMeta* m_SelectedAsset = nullptr;
    bool m_ContentBrowserInspectorActive = false;
    AssetWorkflowInspectorSource m_InspectorSource;
};
```

**`DrawInspector` v0（裸 ImGui，无 Property）：**

- 无选中：`TextUnformatted("No asset selected.")`
- 有选中：只读显示 `AssetName`、`AssetPath`、`AssetType`、`Guid`（`ToString()`）

---

## 5) `ContentBrowserWindow` UI 框架（不定视觉）

> **说明：** 本节只约定 **布局与交互**，颜色/字体/间距 **不** 写入 P6 实现要求；merge Appearance 后再对齐。

### 5.1 布局（ImGui）

```text
┌─ Content Browser ─────────────────────────────────────────────┐
│ [Import] [Delete] [Refresh]                    ← DrawToolbar  │
├──────────────────┬──────────────────────────────────────────┤
│  Directory Tree  │  Asset List (table)                       │
│  (Child 左 ~28%) │  (Child 右 ~72%)                          │
│  TreeNode*       │  Columns: Name | Type | Guid (short)      │
│                  │  Selectable rows, 单行选中                 │
├──────────────────┴──────────────────────────────────────────┤
│  Selection: (optional one-line summary)                       │
└───────────────────────────────────────────────────────────────┘
```

| 区域 | v0 控件 | 行为 |
|------|---------|------|
| 工具条 | `ImGui::Button` | Import → `ImportAssetDialog()`；Delete → `DeleteSelectedAsset()`；Refresh → `RebuildDirectoryTree` + 列表 |
| 目录树 | `TreeNode` / `TreeNodeEx` | 点击目录 → `SetCurrentDirectory`；根节点 = `Assets` 显示名 |
| 资产列表 | `BeginTable` 3 列 | 单击选中 → `SetSelectedAsset`；双击 → `OpenAsset` |
| 摘要条 | `Text` | 可选；与 Inspector 重复可省略 |

### 5.2 交互规则

- **单选** v0；无 Ctrl/Shift 多选。
- **Delete**：无选中 → 无操作；有选中 → `DeleteAsset`（本进程删盘 + meta，P2 语义）。
- **Open**：`MaterialEditor` / `SceneEditor` 既有路由；失败打日志。
- **Refresh**：不调用 `ScanAssets`（避免全量重扫风暴）；仅重建树 + 重查 Registry。全量 `ScanAssets` 仍由 OpenProject / Watcher 兜底负责。

### 5.3 视觉（**刻意留空 — Browser UI 展示需后期再设计**）

> **提示：** 当前仅为功能可用的 ImGui 线框；图标、缩略图、主题色、列表密度等 **Browser UI 展示** 待 master **Editor Appearance** merge 后单独立项（P6.1）设计，不阻塞 P7 集成。

| 项 | P6 | merge Appearance 后（P6.1） |
|----|-----|------------------------------|
| 选中行背景 | ImGui 默认 | `ImGuiCol_Header` / 主题 token |
| 字体 | 全局 `io.FontGlobalScale` | `EditorAppearance` 字体资产 |
| 图标 / 缩略图 | 无 | 后置 |
| Meta Inspector | `Text` | 可选 `Property` 只读 |

---

## 6) Inspector 与 Editor 挂钩

### 6.1 现状（P5 末）

`InspectorWindow` **仅** 读取 `GetActiveSubModule()->GetInspectorSource()`，Content Browser 选中 **不会** 出现在 Inspector。

### 6.2 P6 最小补丁（不依赖 Appearance）

`InspectorWindow::OnDraw` 伪代码：

```cpp
IEditorInspectorSource* source = nullptr;
AssetWorkflowModule& workflow = m_Context.GetAssetWorkflow();
if (workflow.IsContentBrowserInspectorActive()
    && workflow.GetInspectorSource()->HasInspectableSelection())
{
    source = workflow.GetInspectorSource();
}
else if (EditorSubModule* active = m_Context.GetActiveSubModule())
{
    source = active->GetInspectorSource();
}
// … DrawInspector or placeholder …
```

`ContentBrowserWindow::OnDraw` 末尾或 `Begin` 后：

```cpp
const bool browserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
m_Context.GetAssetWorkflow().SetContentBrowserInspectorActive(browserFocused);
```

### 6.3 P7 / Shell 完整 Focus 链（不在 P6）

- `EditorInputHub` 注册 Browser CommandList（Delete、Import、Refresh）
- 与 `EDITOR_SHELL_DESIGN` §7 完全对齐

---

## 7) 与 P4 Import 的衔接

| 项 | P6 v0 | 后续 |
|----|-------|------|
| 目标目录 | 仍 **`Assets/Imported/`**（`ImportAssetDialog` 不变） | P6.1：`ImportAssetDialog(destOverride)`，默认 `GetModel().GetCurrentDirectory()` |
| 导入后列表 | Watcher / `Registered` 事件刷新；若在 `Imported/` 且当前目录一致则立即可见 | — |

---

## 8) `AssetManager` 可选 API（方案 A）

```cpp
// AssetManager.h — P6 若采用目录枚举方案 A
std::vector<const AssetMeta*> FindAssetMetasUnderDirectory(
    std::string_view projectRelativeDirectory) const;
```

- `projectRelativeDirectory`：`""` 表示整个 `ProjectContentRoot` 下所有注册资产；`"Meshes"` 表示该目录 **直接** 子项（v0 **不** 递归子目录，与「当前文件夹」语义一致）。
- 实现：遍历 `m_AssetRegistry`，`Normalize` 后比较 `parent_path()` 与目录前缀。

---

## 9) 生命周期

| 时机 | 动作 |
|------|------|
| `ContentBrowserModule::Register` | `new AssetTreeModel`；`RegisterWindow(ContentBrowserWindow)`；`SubscribeToAssetManager` |
| `Editor::OpenProject` 成功 | `GetContentBrowser().GetModel().ResetForProject(contentRoot)` |
| `Editor::CloseProject` | `GetModel().Clear()`；`SetSelectedAsset(nullptr)` |
| `Shutdown` | `Unsubscribe`；`Shutdown` |

**说明：** P6 **不** 要求改 `OpenProject` 以外的 ProjectManager；若 Model 需在 Open 后初始化，由 `Editor::OpenProject` 成功分支调用（与 Watcher `StartWatching` 同级）。

---

## 10) P6 验收标准

| # | 检查 | 状态 |
|---|------|------|
| 1 | 打开工程后出现 **Content Browser** 窗口（菜单或默认可见；Dock 非必须） | ✅ |
| 2 | 左树展示 `Assets/` 下目录；点击目录，右侧列表仅显示该目录内 **已注册** 资产 | ✅ |
| 3 | 资源管理器复制 `*.png` 到当前目录 → 列表刷新（Watcher + Subscribe） | ✅ |
| 4 | 选中资产 → Inspector 显示 Meta 字段（Browser 聚焦时） | ✅ |
| 5 | 双击材质/场景 → 打开对应 SubEditor | ✅ |
| 6 | Delete 删除选中资产（盘 + Registry）；列表项消失 | ✅ |
| 7 | Import 仍进 `Imported/`；成功后在 `Imported` 目录下可见 | ✅ |
| 8 | **未** 修改 `UI/Property/**`、`Color.*`；**未** 依赖 `EditorAppearance` 头文件 | ✅ |
| — | **Browser UI 展示**（主题/图标/缩略图/布局密度） | ⏳ 后期再设计（P6.1，依赖 Appearance merge） |

---

## 11) 实施顺序（P6 内）

| 切片 | 内容 | 状态 |
|------|------|------|
| **P6a** | `AssetTreeModel` + 可选 `FindAssetMetasUnderDirectory` + Subscribe | ✅ |
| **P6b** | `ContentBrowserWindow` 框架（树 + 表 + 工具条） | ✅（UI 线框 only） |
| **P6c** | `AssetWorkflowModule` 选中 / Delete / InspectorSource + `InspectorWindow` 补丁 | ✅ |
| **P6d** | `ContentBrowserModule` + `Editor` 注册 / OpenProject Reset | ✅ |

---

## 12) 审批清单

- [x] **A.** 同意 P6 范围（Module + Model + Window 框架 + Subscribe + 选中/Delete/Open）
- [x] **B.** 同意 v0 **裸 ImGui**；**Browser UI 展示需后期再设计**（Appearance merge 后 P6.1）
- [x] **C.** 同意 Inspector **最小聚焦补丁**（§6.2），完整 Focus 链留 P7
- [x] **D.** 同意 Import v0 仍固定 `Imported/`（当前目录 Import 为 P6.1）
- [x] **E.** 采用目录资产枚举 **方案 A**（`FindAssetMetasUnderDirectory`）
- [x] **F.** 同意 **不** 做缩略图 / 拖拽 / Undo / Move-Rename UI

已批准并实现（2026-05-26）。

---

## 13) 文档同步

- [ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §9 P6 → ✅  
- [ASSET_PIPELINE_P5_API.md](./ASSET_PIPELINE_P5_API.md) §0.1 Content Browser → ✅
