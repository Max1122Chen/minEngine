# Asset Pipeline — 业务线详细设计（草案）

Last updated: 2026-05-25  
Status: **已拍板（§10、§14）— 可进入 P1 实现**  
Worktree / 分支：`minEngine-asset-workflow` / `feat/editor-asset-workflow`  
关联：[转交会话](../../sessions/EDITOR_ASSET_WORKFLOW_AGENT_HANDOFF.md)、[Editor 平台化 E3/E4](../../Editor/EDITOR_PLATFORM_PLAN.md)、[Shell / ContentBrowser](../../Editor/EDITOR_SHELL_DESIGN.md)、[产品意图](./CONTENT_BROWSER_DESIGN.md)、[appearance G7 类型桶](../../Editor/EDITOR_APPEARANCE.md#72-推荐方案采纳--略优于仅缓存)

---

## 0) 一句话

建立 **「磁盘 ↔ Registry ↔ Editor UI」** 闭环：Runtime `AssetManager` 提供可变更、可订阅的资产注册表；Editor 提供跨平台导入对话框、工程目录变更感知、以及 Content Browser 基本框架；`AssetWorkflowModule` 编排 Import / Open / Delete，**不**在本线改 Color / Property UI。

---

## 1) 端到端数据流

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                         Project Content Root                             │
│              PathRegistry::GetProjectContentRoot()  (Assets/)            │
│   *.png / *.memtl / …  +  sidecar *.meta  (GUID 真源)                    │
└───────────────┬───────────────────────────────┬─────────────────────────┘
                │ Scan / Register               │ OS 外部变更
                ▼                               ▼
┌───────────────────────────┐       ┌──────────────────────────┐
│      AssetManager         │◄──────│  ProjectAssetWatcher     │
│  Registry + 类型桶 + CRUD  │       │  (Editor, debounce)      │
│  Subscribe(变更事件)       │       └──────────────────────────┘
└───────────────┬───────────┘
                │ AssetRegistryChange
                ▼
┌───────────────────────────┐       ┌──────────────────────────┐
│   ContentBrowserModule    │       │   AssetWorkflowModule     │
│   TreeModel + 选中        │       │   ImportDialog / OpenAsset│
│   IEditorInspectorSource  │       │   DeleteSelected …        │
└───────────────┬───────────┘       └───────────┬──────────────┘
                │                               │
                │                               │ FileDialogService (Runtime Platform)
                ▼                               ▼
         Inspector / 双击 Open          IFileDialogService → NFD
```

```text
Runtime/Platform/FileDialog/FileDialogService  ←  Engine 生命周期
        ↑ 调用方：Editor（P3 验收 / P4 Import）、Playground、未来游戏逻辑
        ↑ filter 数据：AssetTypeRegistry::BuildFileDialogFilters()（Resource，无对话框）
```

**不变量（v0 已拍板）：**

| 项 | 规则 |
|----|------|
| Meta 真源 | 每个可识别资产文件有 sidecar `path.meta`；GUID 以 meta 为准 |
| **AssetPath 存储** | **工程相对路径**（相对 `ProjectContentRoot`，POSIX `/`，无 leading `/`）；见 §14 |
| Registry 键 | 与 meta 相同的 **相对路径** 字符串；`m_AssetPathByGuid` 二级索引 |
| 类型字符串 | `AssetMeta.AssetType` 与 `AssetTypeRegistry` 中 `AssetTypeId` 一致（如 `"Material"`） |
| Registry 范围 | **仅** `ProjectContentRoot`；引擎 `EngineDefaultAssetsRoot` **不进入** 本 Registry |
| 加载缓存 | `m_LoadedAssetCache` 键与 Registry 一致（相对 path）；IO 前 `ResolveAssetAbsolute` |

---

## 2) 分层与职责

| 层 | 组件 | 位置 | 职责 |
|----|------|------|------|
| **R0 类型表** | `AssetTypeRegistry` | `Runtime/Resource/` | 扩展名 ↔ AssetType ↔ 可选 MEClass；FileDialog filter、Scan、Loader 分流 **单一来源** |
| **R1 注册表** | `AssetManager` | `Runtime/Resource/` | Meta 索引、按类型桶、Load/Save、**Import/Delete/Move/Rename**、变更事件 |
| **R2 磁盘同步** | `ProjectAssetWatcher` | `Editor/src/Services/AssetWatch/`（建议） | 监听 **仅** `ProjectContentRoot`；**efsw**（§5.4）；debounce 后驱动 Registry |
| **P0 平台对话框** | `FileDialogService` + `IFileDialogService` | `Runtime/Platform/FileDialog/` | OpenFiles / SaveFile / SelectFolder；NFD 实现；**Editor/游戏** 调用 |
| **编排** | `AssetWorkflowModule` | `Editor/src/Services/` | Dialog → Import；选中 → Delete；`OpenAsset` 路由保持 |
| **UI 框架** | `ContentBrowserModule` + Window | `Editor/src/Services/ContentBrowser/` | 目录树 + 资产列表 + 选中；订阅 R1 事件刷新 |

**非目标（本业务线 v0）：** 依赖图、异步 Reimport 队列、Addressables 路径、缩略图、拖拽 Import、Undo 命令、引用完整性阻塞删除。

---

## 3) R0 — `AssetTypeRegistry`

### 3.1 API（草案）

```cpp
struct AssetTypeDescriptor
{
    std::string AssetTypeId;           // "Material", "Texture2D", …
    std::string RuntimeClassName;    // Reflection 类名，供 InferAssetTypeFromClassName
    std::vector<std::string> Extensions; // ".memtl", ".png", …
    std::string FileDialogFilterLabel;   // "Material (*.memtl)"
};

class AssetTypeRegistry
{
public:
    static AssetTypeRegistry& Get();

    void RegisterBuiltinTypes();  // 启动时一次
    const AssetTypeDescriptor* FindByExtension(std::string_view ext) const;
    const AssetTypeDescriptor* FindByAssetTypeId(std::string_view typeId) const;
    std::string InferAssetTypeFromExtension(const std::filesystem::path& path) const;
    std::string InferAssetTypeFromClassName(const std::string& className) const;
    std::vector<std::string> BuildFileDialogFilterSpec() const; // "Material (*.memtl)\0*.memtl\0…"
};
```

### 3.2 与现状迁移

- 将 `AssetManager::InferAssetTypeFromExtension/FromClassName` **委托**给 `AssetTypeRegistry`（成员函数调用，不新增文件级 static 表）。
- v0 内置类型与现 `AssetManager.cpp` 一致：`Texture2D`、`StaticMesh`、`Material`、`Shader`、`Scene`；**Font** 留给 appearance 线注册扩展，本线 Registry 预留 `RegisterType` 即可。

---

## 4) R1 — `AssetManager` 扩展

### 4.1 Registry 索引结构

```text
m_AssetRegistry:        path → AssetMeta (owned)
m_AssetPathByGuid:      GUID → path
m_AssetMetasByType:     AssetTypeId → vector<AssetMeta*>  // 非拥有指针，指向 map 内元素
m_LoadedAssetCache:     path → weak_ptr<MEObject>         // 已有
```

**桶维护（与 appearance G7 一致）：**

- `CacheMeta`：插入/更新桶；若 `AssetType` 变更则从旧桶移除再加入新桶。
- `UncacheMeta(path)`（私有）：从 `m_AssetRegistry`、`m_AssetPathByGuid`、类型桶移除。

**查询 API（v0）：**

```cpp
// 替换现有 O(N) 实现
std::vector<const AssetMeta*> FindAssetMetasByType(const std::string& assetTypeId) const;

// 可选：appearance / Browser 共用
void ForEachAssetMetaOfType(const std::string& assetTypeId,
                            const std::function<void(const AssetMeta&)>& visitor) const;
```

`FindAssetMetasByType` 的入参语义：**统一为 `AssetTypeId` 字符串**（如 `"Material"`）。  
类名入参仅通过 `InferAssetTypeFromClassName` 在调用方转换（Picker 侧），避免桶 key 二义性。

### 4.2 变更事件

```cpp
enum class AssetRegistryChangeKind : uint8_t
{
    Registered,
    Unregistered,
    Moved,       // 含 Rename（路径变、GUID 不变）
    MetaUpdated, // meta 文件内容变、路径不变
    Reimported   // 资产文件时间戳/内容变，v0 可与 MetaUpdated 合并上报
};

struct AssetRegistryChange
{
    AssetRegistryChangeKind Kind;
    GUID Guid;
    std::string OldPath;  // Moved/Unregistered 时有效
    std::string NewPath;  // Registered/Moved 时有效
    std::string AssetTypeId;
};

using AssetRegistryChangedCallback = std::function<void(const AssetRegistryChange&)>;

uint32_t Subscribe(AssetRegistryChangedCallback callback);
void Unsubscribe(uint32_t subscriptionId);
```

**投递语义（建议默认）：**

- **同步、在调用线程** 于 CRUD 成功返回前触发（实现简单，Browser 直接刷新）。
- 回调内禁止重入 `AssetManager` 写 API（文档约定）；必要时后续加 `DeferBroadcast`。

### 4.3 CRUD API 语义

#### `ImportAsset`

```cpp
struct ImportAssetResult
{
    bool bSuccess = false;
    std::string ErrorMessage;
    AssetMeta Meta;  // 成功时有效
};

ImportAssetResult ImportAsset(const std::filesystem::path& sourcePath,
                              const std::filesystem::path& destDirectory);
```

| 步骤 | 行为 |
|------|------|
| 校验 | `sourcePath` 存在；`destDirectory` 在 `ProjectContentRoot` 下（或解析为绝对路径后 `is_prefix_of`） |
| 类型 | `AssetTypeRegistry::InferAssetTypeFromExtension(source)`；未知扩展名 → 失败 |
| 复制 | `std::filesystem::copy_file` 到 `destDirectory / source.filename()`；重名 → **失败**（v0 不自动重命名） |
| 注册 | 对目标路径调用现有 `RegisterAsset(destPath, type)`（生成/加载 meta） |
| 事件 | `Registered` |
| Scene | 若 `AssetType == "Scene"`，同步 `SceneManager::RegisterScene`（与 `ScanAssets` 一致） |

**不**在 Import 时自动 `LoadAsset`；Editor 双击或显式 Open 再加载。

#### `DeleteAsset`

```cpp
bool DeleteAsset(const std::string& assetPath, std::string& outError);
```

| 步骤 | 行为 |
|------|------|
| 校验 | Meta 存在；路径在工程 Content 下 |
| 引用 | v0 **仅 WARN 日志**，不扫描场景/材质引用、不阻塞 |
| 缓存 | 移除 `m_LoadedAssetCache` 中该 path |
| 磁盘 | 删除资产文件 + `.meta` |
| Registry | `UncacheMeta` + `Unregistered` 事件 |

#### `MoveAsset` / `RenameAsset`

```cpp
bool MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError);
// RenameAsset = MoveAsset(same parent, new filename)
```

| 步骤 | 行为 |
|------|------|
| 磁盘 | `rename` 资产文件；`rename` 对应 `.meta`（`BuildMetaPath` 规则不变） |
| Meta | 更新 `AssetPath`、`AssetName`（stem）；**GUID 不变**；写回 meta 文件 |
| Registry | 从旧 path 键迁移到新 path；更新 `m_AssetPathByGuid`；桶内指针仍指向 map 内同一 `AssetMeta` |
| 缓存 | 旧 path 缓存项迁移或清除 |
| 事件 | `Moved`（`OldPath` / `NewPath`） |

#### `RegisterAsset` / `ScanAssets`（现有）

- 保留；成功 `CacheMeta` 后若为新 path 发 `Registered`，若已存在且 meta 字段修正发 `MetaUpdated`。
- `ScanAssets`：全量递归；用于 **OpenProject** 与 **Watcher 兜底全量刷新**（见 §5）。

### 4.4 与 `ObjectManager` / GC

- Delete v0 **不**从 `ObjectManager` 摘除已加载实例；依赖 weak cache 失效 + 后续 GC 域清理。
- 文档注明：若已加载资产仍被场景引用，会出现悬空 GUID 引用 — 与现有序列化行为一致，v0 WARN。

---

## 5) R2 — 工程目录变更感知

### 5.1 目标

用户或 VCS 在资源管理器中增删改文件时，Editor 内 Registry 与 Content Browser **无需重启**即可对齐磁盘。

### 5.2 组件：`ProjectAssetWatcher`

| 项 | 建议 |
|----|------|
| 生命周期 | `EditorServiceModule` 或 `AssetWorkflowModule::Register` 时创建；`ProjectManager::OpenProject` 后 `StartWatching(GetProjectContentRoot())`；`CloseProject` 停止 |
| 监听范围 | 仅 **当前工程** `Assets/`（`PathRegistry::GetProjectContentRoot()`） |
| 去抖 | 默认 **300–500 ms** 合并 burst（保存多文件、git checkout） |
| 忽略 | `*.meta` 单独变更可触发 `MetaUpdated`；临时文件 `~`、`*.tmp` 忽略 |

### 5.4 跨平台库选型（D9 — 已拍板）

| 库 | 许可 | 平台 | 说明 |
|----|------|------|------|
| **[efsw](https://github.com/SpartanJ/efsw)**（**采用**） | MIT | Win IOCP / Linux inotify / macOS FSEvents；失败时内置 **polling 回退** | 与 NFD 同级：Editor 链 Third-Party + CMake；vcpkg `efsw` 可用 |
| 自写轮询 | — | 全平台 | P5 前可作 stub；拍板后以 efsw 为主，不单独维护轮询逻辑 |
| libuv `uv_fs_event` | — | 有限 | 能力不如 efsw 完整 |
| Qt / Chromium base | — | — | 过重，不引入 |

**集成要点：** `ProjectAssetWatcher` 实现 `efsw::FileWatchListener`；`watch()` 在后台线程；回调 **投递到 Editor 主线程** 再 debounce 调 `AssetManager`（避免与 ImGui 同线程竞态）。

### 5.3 同步策略（增量 vs 全量）

**v0 推荐：分层策略**

| 场景 | 动作 |
|------|------|
| 可识别 **单文件** 创建/修改 | `RegisterAsset(path, inferredType)` |
| 单文件删除 | `DeleteAsset` 或仅 `UncacheMeta` + 事件（若文件已不存在则 **Unregister** 路径，不删盘） |
| 目录结构变更 / 批量未知 | `ScanAssets(contentRoot)` 全量（简单正确） |
| 本进程 CRUD 引起 | **抑制 Watcher 回环**：`AssetManager` 维护 `m_SuppressExternalSyncCount` 或 Watcher 忽略短时间内的自身路径 |

### 5.5 与 `AssetManager` 的边界

- Watcher 在 **Editor**；`AssetManager` 保持 Runtime，不依赖 ImGui/Editor。
- Watcher 只调用 **公开** `RegisterAsset` / `DeleteAsset` / `ScanAssets`，不直接改私有 map。

---

## 6) E4 — 跨平台 `IFileDialogService`（Runtime Platform）

> **P3 定稿：** [ASSET_PIPELINE_P3_API.md](./ASSET_PIPELINE_P3_API.md) — 实现落 `Runtime/Platform/FileDialog/`；**`minEngine` 链 NFD**；Editor 经 `Engine` / `FileDialogService::Get()` 调用。

### 6.1 分层

| 层 | 职责 |
|----|------|
| **Platform** | `FileDialogTypes`、`IFileDialogService`、`NativeFileDialogService`、`FileDialogService`（`NFD_Init`/`Quit`） |
| **Resource** | `AssetTypeRegistry::BuildFileDialogFilters()` — 资产扩展名 → `FileDialogFilter` |
| **Editor** | `IEditorContext::GetFileDialogService()` 转发；P4 `AssetWorkflowModule` 编排 Import |

**依赖：** `Platform → Core`；`Resource → Platform/FileDialogTypes`；`Editor → Runtime`。**禁止** Platform 依赖 Resource/Editor。

### 6.2 接口（摘要）

```cpp
struct FileDialogFilter
{
    std::string Label;
    std::string ExtensionSpec;  // NFD: "png,jpg" without dots
};

struct FileDialogRequest { /* Title, InitialDirectory, Filters, bAllowMultiple */ };
struct FileDialogResult   { bool bCancelled; std::vector<std::filesystem::path> Paths; };

class IFileDialogService { OpenFiles / SaveFile / SelectFolder; };

class FileDialogService
{
public:
    static FileDialogService& Get();
    void Initialize();  // NFD_Init
    void Shutdown();    // NFD_Quit
    IFileDialogService& GetImplementation();
};
```

- **Engine** `StartSystems` / `ShutdownSystems` 创建并初始化 `FileDialogService`（与 `AssetManager` 同级）。
- Editor **不**持有 `unique_ptr<IFileDialogService>` 实现。

### 6.2 实现选型

| 方案 | 说明 |
|------|------|
| **nativefiledialog-extended (NFD)** | 跨平台、MIT、无 HWND 依赖；适合「一条业务线要 Win/macOS/Linux」 |
| **Win32 薄封装 + NFD 非 Windows** | 当前主开发 Windows 时可先 Win32，但与你「跨平台导入」目标略背离 |
| **ImGui 内置 FileBrowser** | 非原生 OS 体验；不推荐作为主路径 |

**已拍板：** **引入 NFD**（`minEngine/Third-Party/nativefiledialog-extended/` + CMake **`minEngine` Runtime** 链接 `nfd`），全平台一条实现路径；游戏与 Editor 共用。

---

## 7) `AssetWorkflowModule` 编排

### 7.1 扩展 API

```cpp
class AssetWorkflowModule : public EditorServiceModule
{
public:
    bool OpenAsset(const AssetMeta& meta);           // 已有

    void ImportAssetDialog();                      // FileDialog → ImportAsset
    bool DeleteSelectedAsset();                    // 需当前选中 meta/path
    bool MoveSelectedAsset(const std::string& newPath); // v1 或 Browser 内 Rename

    void SetSelectedAsset(const AssetMeta* meta);  // Browser / 调试入口写入选中
    const AssetMeta* GetSelectedAsset() const;
};
```

### 7.2 `ImportAssetDialog` 流程

```text
1. InitialDirectory = ContentBrowser 当前文件夹，若无则 ProjectContentRoot
2. OpenFiles(filter = AssetTypeRegistry)
3. SelectFolder（可选第二步）或默认导入到当前 Browser 路径
4. 对每个文件 AssetManager::ImportAsset(src, destDir)
5. 失败汇总日志；成功项可选 OpenAsset（v0 默认 **不** 自动 Open）
```

### 7.3 `OpenAsset` 路由（不变）

- 遍历 `MaterialEditor` / `SceneEditor` 的 `CanOpenAsset` + `OpenAsset` + `ActivateSubModule`。
- **不**引入全局 Selection enum。

---

## 8) Content Browser 基本框架

### 8.1 模块结构

```text
Editor/src/Services/ContentBrowser/
  ContentBrowserModule.h/.cpp    // EditorServiceModule
  ContentBrowserWindow.h/.cpp    // EditorWindow
  AssetTreeModel.h/.cpp          // 纯数据：目录树 + 当前目录资产列表
```

### 8.2 `AssetTreeModel`

- **输入：** `PathRegistry::GetProjectContentRoot()` + `AssetManager` 查询。
- **目录树：** 仅展示磁盘目录结构（过滤：隐藏无引擎资产的空目录可选 v1）。
- **当前目录资产列表（D12）：** **仅** 已在 Registry 中、且有 sidecar `.meta` 的资产（按目录过滤 `Find` by path 前缀或维护目录索引）。
- **刷新：** 订阅 `AssetManager::Subscribe`；`Registered/Unregistered/Moved/MetaUpdated` 增量更新节点。

### 8.3 `ContentBrowserWindow`（v0 UI）

| 区域 | 行为 |
|------|------|
| 左 | `TreeNode` 目录树；选中 → 设置当前路径 |
| 右 | 当前路径下资产列表（名称、类型、GUID 简写）；无缩略图 |
| 选中 | `AssetWorkflowModule::SetSelectedAsset`；实现 `IEditorInspectorSource` 绘制 Meta 字段（只读 v0） |
| 双击 | `OpenAsset(meta)` |
| 右键 / 菜单 | Import（调 `ImportAssetDialog`）、Delete（`DeleteSelectedAsset`）；Move/Rename v1 |
| Dock | SceneEditing 默认右下（`ApplyDefaultLayout` 占位，可与 Shell E0 协调） |

### 8.4 Inspector 焦点

- `ContentBrowserModule::DrawInspector`：选中时显示 `AssetName`、`AssetPath`、`AssetType`、`Guid`；预览接 E2 后续。
- Focus 链：Browser 面板聚焦时 `Editor` 将 Inspector Source 切到本模块（见 `EDITOR_SHELL_DESIGN` §5.2）。

---

## 9) 实施顺序（建议拍板）

与「先设计后编码、分 PR」一致：

| 阶段 | 交付 | 验收 |
|------|------|------|
| **P0 设计** | 本文档拍板 | ✅ 2026-05-25 |
| **P1 E3 核心** | `AssetTypeRegistry` + 类型桶 + 事件 + `ImportAsset`（单文件） | Import 后事件、按类型查询不扫全表 |
| **P2 E3 CRUD** | `DeleteAsset` + `MoveAsset` + `RegisterAsset` 发事件 | 磁盘与 Registry 一致；Move 保 GUID |
| **P3 E4** | `Runtime/Platform/FileDialog` + **NFD**（Engine 生命周期） | Editor 菜单/Debug 调 `GetFileDialogService()` |
| **P4 编排** | `AssetWorkflowModule::ImportAssetDialog` + 选中 Delete | 对话框导入到 Assets |
| **P5 Watcher** | `ProjectAssetWatcher` + **efsw**（Editor-only） | 外部增删改 Assets → Registry 同步；Browser 刷新在 P6 |
| **P1b** | `AssetPath` 相对化 + 停止 Registry 扫描 EngineDefault | 见 §14；可与 P1 同 PR 或紧跟 |
| **P6 Browser** | `ContentBrowserModule` + Window + Model | 树+列表+选中+双击 Open |
| **P7 集成** | `Editor` 注册模块、默认 Dock、MainMenu Import | 端到端目视 |

**明确后置：** 缩略图、拖拽、Undo、引用检查阻塞删除、macOS/Linux Watcher 原生 API。

---

## 10) 决策表（2026-05-25 已全部采纳）

| ID | 结论 |
|----|------|
| D1–D7, D10–D11, D13–D15 | 按建议默认 **采纳** |
| **D8** | **NFD** 全平台 |
| **D9** | **efsw**（MIT，跨平台；见 §5.4） |
| **D12** | Browser **仅显示已注册、有 meta** 的资产 |

---

## 11) 路径与纪律（不变）

- 仅改 [转交 §3.1](../../sessions/EDITOR_ASSET_WORKFLOW_AGENT_HANDOFF.md) 允许路径；**禁止**改 `Color` / `Property` / `EDITOR_APPEARANCE.md`。
- C++：cpp-style；**成员函数** 优先；`AssetTypeRegistry` 单例与 `AssetManager` 协作。
- 第一个代码切片仍为：**类型桶 + Registry 事件 + `ImportAsset`（单文件）**（P1）。
- **P1 定稿：** [ASSET_PIPELINE_P1_API.md](./ASSET_PIPELINE_P1_API.md)（已实现 `8c5958d`）。
- **P2 定稿：** [ASSET_PIPELINE_P2_API.md](./ASSET_PIPELINE_P2_API.md)（已实现 `7758c60`）。
- **P3 定稿：** [ASSET_PIPELINE_P3_API.md](./ASSET_PIPELINE_P3_API.md)（**已实现**；submodule `nativefiledialog-extended` @ v1.3.0）。
- **P4 定稿：** [ASSET_PIPELINE_P4_API.md](./ASSET_PIPELINE_P4_API.md)（已实现 `0f96c45`）。
- **P5 定稿：** [ASSET_PIPELINE_P5_API.md](./ASSET_PIPELINE_P5_API.md)（**待审批**）。

---

## 12) 开放问题（剩余）

| 项 | 暂定 |
|----|------|
| Reimported | 外部覆盖资产文件 → Watcher 触发 `RegisterAsset`；`m_LoadedAssetCache` 失效该 path（P2+） |
| Browser vs Scene 选择 | Inspector 焦点链切换；**不清** Scene GO 选择（v0） |
| NFD 目录 | `minEngine/Third-Party/nativefiledialog-extended/` ✅ |

---

## 14) 双根资产与 `AssetMeta` 相对路径（已拍板）

### 14.1 问题

现况：`AssetMeta.AssetPath` 存 **绝对路径**（见 `MaterialIRSmoke.memtl.meta`）；`Editor` 启动时还对 `EngineDefaultAssetsRoot` 做 `ScanAssets`，与工程 `Assets/` 混在同一 Registry，导致：

- 换机器 / 克隆路径即 meta 失效；
- 「相对谁」歧义（Engine vs Project）；
- Content Browser / CRUD 边界不清。

### 14.2 原则（与你的想法一致）

```text
EngineDefaultAssetsRoot          ProjectContentRoot (…/Assets)
  模板 / Shader / IBL / 内置形状     工程拥有的、可 GUID 引用的资产
  PathRegistry 直接 Resolve          AssetManager Registry + .meta
  一般不写进工程 .meta               AssetPath 永远相对 ProjectContentRoot
```

| 消费方式 | 路径来源 | Registry |
|----------|----------|----------|
| 材质模板、glslinc、IBL、SkyBox | `GetEngineDefaultAssetsRoot()` / `ResolveEngineRelative` | **否** |
| 场景/材质/网格引用、Editor 浏览 | `ResolveProjectRelative(meta.AssetPath)` | **是** |
| 工程需要的「默认白图、示例 mesh」 | 应在 **工程** `Assets/` 内 | **是** |

**采纳：** 新建工程时（或首次 Open 缺省资源时）从 Engine 模板 **复制** 到 `Project/Assets/...`，生成 **新 GUID** 的 `.meta`；引擎目录不作为工程资产真源。

### 14.3 `AssetPath` 规范

- **磁盘 meta JSON：** `"AssetPath": "Materials/MaterialIRSmoke.memtl"`（相对 `ProjectContentRoot`）。
- **规范化：** `AssetManager::NormalizeAssetRelativePath` — 统一 `/`、去掉 `./`、禁止 `..` 逃出 ContentRoot。
- **解析：** `PathRegistry::ResolveProjectRelative(meta.AssetPath)` → 绝对路径，仅供 `filesystem` / Loader。
- **注册：** `RegisterAsset` 入参可为绝对或相对；写入 meta 前 **强制转为相对**。
- **迁移（forward-only）：** 读 meta 时若 `AssetPath` 为绝对且位于当前 `ProjectContentRoot` 下 → 重写为相对并 `saveMetaToFile`；若在 `EngineDefaultAssetsRoot` 下 → **不注册** 到工程 Registry（日志提示应 Copy 到工程）。

### 14.4 代码调整（P1b，与 asset-workflow 线）

| 位置 | 变更 |
|------|------|
| `Editor.cpp` | **移除** `ScanAssets(GetEngineDefaultAssetsRoot())` 进 Registry |
| `ProjectManager::OpenProject` | 保持只 `ScanAssets(GetProjectContentRoot())` |
| `PopulateEditorDefaultScene` / 测试资产 | 仅引用工程 `Assets/` 下 meta（已有 `MaterialIRSmoke` 可迁相对路径） |
| `Serializer` GUID 解析 | 仍用 GUID；路径仅用于 Load，相对化后更稳 |
| **新建** `ProjectTemplate::SeedDefaultAssets()` | 从 Engine 复制清单到 `Assets/Starter/`（**可 P7 或独立小任务**） |

### 14.5 与 UE 对照（学习用）

- UE **/Engine/** 与 **/Game/** 内容分离；可引用但 Cook 时 Game 资产进包。
- 你的方案 ≈ **Engine 只读模板 + Game(Content) 可编辑资产**；比「两套路径写进同一 meta」更简单，也利于 Git 协作。

### 14.6 风险

- 旧 scene / memtl 里若硬编码绝对 `AssetPath` 字符串（非 GUID）会断；本仓库主路径已是 **GUID 引用**，风险主要在 meta 与工具链。
- 引擎升级模板不会自动覆盖已复制到工程的文件（**有意为之**）；需要时可做「Replace from template」菜单（后置）。

---

## 13) 文档关系

| 文档 | 关系 |
|------|------|
| `CONTENT_BROWSER_DESIGN.md` | 保留产品意图；实现细节以本文为准 |
| `EDITOR_PLATFORM_PLAN.md` § E3/E4 | 本设计为其分项展开 |
| `EDITOR_ASSET_WORKFLOW_AGENT_HANDOFF.md` | 实施纪律与首切片范围 |
