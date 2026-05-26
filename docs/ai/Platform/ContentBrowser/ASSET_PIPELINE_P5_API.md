# Asset Pipeline — P5 接口定稿（审批用）

Last updated: 2026-05-26  
Status: **已实现**  
前置：**P2** Registry CRUD/events（`7758c60`）、**P4** `ImportAssetDialog`（`0f96c45`）  
父文档：[ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §5（R2 Watcher）、§9 P5、§10 D9  
P4 定稿：[ASSET_PIPELINE_P4_API.md](./ASSET_PIPELINE_P4_API.md)

---

## 0) 已拍板（本稿依据）

| # | 决策 |
|---|------|
| 1 | Watcher 仅存在于 **Editor**（`Editor/src/Services/AssetWatch/`），Runtime 不链 efsw |
| 2 | **线程模型 A**：efsw 回调线程 → 入队；**Editor 主线程** 消费并调 `AssetManager` |
| 3 | **Submodule** 路径：`minEngine/minEngine/Third-Party/efsw` |
| 4 | **链接**：仅 **`Editor` 目标** 链 efsw（与 NFD 不同；NFD 在 Runtime） |

---

## 0.1) P4 完成度核对

| 项 | 状态 |
|----|------|
| `ImportAssetDialog` + 菜单 | ✅ `0f96c45` |
| `ProjectAssetWatcher` / efsw | ❌ → **P5** |
| Content Browser UI | ❌ → **P6** |
| `Reimported` 事件 + 自动 reload 缓存 | ❌ → **P5.1 或 P6 前** |

---

## 1) P5 交付边界

### 1.1 包含

| 项 | 说明 |
|----|------|
| **Third-Party** | `efsw` git submodule @ 稳定 tag（建议 **1.4.0** 或与上游 release 对齐） |
| **CMake** | `add_subdirectory(efsw)`；`target_link_libraries(Editor PRIVATE efsw-static)`（静态链，避免 DLL 再拷一份） |
| **`ProjectAssetWatcher`** | `EditorServiceModule` + `efsw::FileWatchListener` |
| **生命周期** | `OpenProject` 成功后 `StartWatching(ProjectContentRoot)`；`CloseProject` / `CloseCurrentProject` 前 `StopWatching` |
| **事件→Registry** | 见 §4 映射表；仅调用 `AssetManager` **公开** API |
| **去抖** | 默认 **400 ms** 合并 burst（可配置常量） |
| **主线程消费** | `ProjectAssetWatcher::Tick(deltaTime)` 由 `Editor::Run` 每帧调用 |
| **回环抑制 v0** | `AssetManager` 本进程 `ImportAsset`/`DeleteAsset`/`MoveAsset` 期间递增抑制计数；Watcher 忽略抑制期内事件（见 §6） |
| **兜底** | 无法可靠解析的单次事件 → 记日志；目录级大批量变更可选触发 `ScanAssets`（见 §5） |

### 1.2 不包含（P5.1 / P6+）

- `AssetRegistryChangeKind::Reimported` 正式发射与 `m_LoadedAssetCache` 失效（P5.1）
- Content Browser 订阅与 UI 刷新（P6）
- Watcher 监听 `EngineDefaultAssetsRoot`
- 递归监听工程根目录以外路径
- Undo 包装 Watcher 触发的变更
- Linux/macOS CI 强制验收（P5 以 **Windows** 合入为准；他平台后续）

---

## 2) 分层与依赖

```text
efsw (Third-Party, submodule)
        ↑ link (Editor only)
ProjectAssetWatcher (Editor/Services/AssetWatch/)
        │ 队列 + debounce + 主线程 Tick
        ▼
AssetManager (Runtime) — RegisterAsset / UnregisterAsset / MoveAsset / ScanAssets
        │ Subscribe → (P6 Content Browser)
        ▼
AssetWorkflowModule / Import 等（不直接依赖 Watcher）
```

| 层 | 禁止 |
|----|------|
| Runtime `AssetManager` | 依赖 efsw / ImGui |
| `ProjectAssetWatcher` | 直接改 `m_AssetRegistry` |

---

## 3) 修改与新建文件（预计）

| 文件 | 动作 |
|------|------|
| `minEngine/Third-Party/efsw/` | **submodule**（用户或 agent 在审批后添加） |
| `minEngine/CMakeLists.txt` 或 `Editor/CMakeLists.txt` | `add_subdirectory(efsw)` + `target_link_libraries(Editor PRIVATE efsw-static)` |
| `Editor/src/Services/AssetWatch/ProjectAssetWatcher.h` | **新建** |
| `Editor/src/Services/AssetWatch/ProjectAssetWatcher.cpp` | **新建** |
| `Editor/src/Editor.{h,cpp}` | 注册模块；`OpenProject`/`CloseProject` 启停；`Run` 中 `Tick` |
| `Runtime/Resource/AssetManager.h` | `SuppressExternalSyncScope`；`RemoveMetaFileOnDisk`（P5.0） |
| `Runtime/Resource/AssetManager.cpp` | 抑制计数；`RemoveMetaFileOnDisk`；Import/Delete/Move 包裹 |
| `docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md` | P5 链接与 §5 对齐 |

**不修改：** `UI/Property/**`、`Color.*`、`FileDialog` Platform 层

---

## 4) `ProjectAssetWatcher` 公开形状（定稿）

```cpp
// Editor/src/Services/AssetWatch/ProjectAssetWatcher.h
class ProjectAssetWatcher : public EditorServiceModule, public efsw::FileWatchListener
{
public:
    static constexpr const char* kModuleId = "ProjectAssetWatcher";

    std::string_view GetModuleId() const override;
    void Register(IEditorContext& context) override;
    void Shutdown() override;

    void StartWatching(const std::filesystem::path& projectContentRoot);
    void StopWatching();

    void Tick(float deltaTime);  // Editor 主线程每帧

private:
    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                          const std::string& filename, efsw::Action action,
                          std::string oldFilename) override;

    void EnqueueFileAction(/* normalized fields */);
    void ProcessPendingActions();  // debounce 到期后执行

    // members: efsw::FileWatcher*, watch id, queue, mutex, debounce timer, IEditorContext*
};
```

---

## 5) efsw 事件 → `AssetManager` 映射（v0）

路径一律先转为 **工程相对路径**（与 P1b 一致）；非 `ProjectContentRoot` 下、或 `Normalize` 失败 → **忽略**。

| efsw `Action` | 文件类型 | AssetManager 调用 |
|---------------|----------|-------------------|
| **Add** | 可识别扩展名（非 `.meta`） | `RegisterAsset(absolutePath, inferredTypeId)` |
| **Modified** | 可识别扩展名 | 同 **Add**（v0 当 re-register；发 `MetaUpdated` 若已存在） |
| **Modified** | 仅 `.meta` | **忽略**（v0；资产文件未变） |
| **Delete** | 可识别扩展名 | `UnregisterAsset` + `RemoveMetaFileOnDisk`（**不** `DeleteAsset`，资产文件已由外部删除；见 §5.4） |
| **Delete** | 仅 `.meta` | **忽略** |
| **Moved** | old/new 均在工程内、扩展名不变 | `MoveAsset(oldRel, newRel, err)`；失败则 `Unregister(old)` + `RegisterAsset(new)` |
| **Moved** | 扩展名变化 | `Unregister(old)` + `RegisterAsset(new)`（等价删+增） |

**忽略规则（额外）：**

- 文件名以 `~` 开头、扩展名为 `.tmp` / `.bak` 等临时文件（常量表，可扩）。
- 目录事件：v0 **不**单独处理；若 efsw 报目录 Add/Delete，记入队列后触发 **`ScanAssets(contentRoot)`** 一次（去抖合并，见下）。

**批量 / 目录变更兜底：**

- 队列内在 debounce 窗口内若 ≥ `kDirectoryEventThreshold`（建议 1）次「需全量」标记，或单次 checkout 产生 > `kFullRescanFileThreshold`（建议 32）条文件事件 → 清空队列，改为一次 `ScanAssets(ProjectContentRoot)` + `ME_CORE_INFO`。

### 5.4 外部删除与 `.meta` 残留（P5.0 fix）

**问题（P5 初版瑕疵）：**

- 外部删除（资源管理器 / git）走 `UnregisterAsset`：只清 Registry，**不删盘**。
- 资产文件已不存在，但 `Assets/.../file.png.meta` 仍留在工程内。
- 下次同名文件再导入或 `RegisterAsset` 会 **优先反序列化旧 `.meta`**，可能沿用错误 GUID / 路径，与「新资产」语义不符。

**与 `DeleteAsset` 的边界：**

| API | 资产文件 | `.meta` | Registry |
|-----|----------|---------|----------|
| `DeleteAsset`（本进程） | 删除 | 删除 | `UncacheMeta` |
| 外部 Delete → Watcher | 已由用户删除 | **应删除**（P5.0） | `UnregisterAsset` |

**P5.0 修复（已实现，不扩大 `UnregisterAsset` 默认语义）：**

1. `AssetManager::RemoveMetaFileOnDisk(assetPath, err)` — 用既有 `Normalize` + `BuildMetaAbsolutePath`；若 meta 存在则 `remove`；不存在视为成功。
2. `ProjectAssetWatcher::ProcessUnregister`：在 `UnregisterAsset` 之后 **始终** 调用 `RemoveMetaFileOnDisk`（即使未注册，也清理孤儿 meta）。

**不改为：** Watcher 对外部删除调用 `DeleteAsset`（源文件已不存在，语义与失败路径均不合适）。

### 5.5 `AssetManager` 补强（待审批，未实现）

以下用于 git checkout / Watcher 漏事件等导致的 **历史孤儿 `.meta`**，与 P5.0 正交：

| # | 方案 | 说明 |
|---|------|------|
| **A** | `ScanAssets` / `OpenProject` 前 **孤儿 meta 清扫** | 递归 `ProjectContentRoot`：对每个 `*.meta`，若对应资产文件不存在则删除 meta（或打日志后删） |
| **B** | `RegisterAsset` 防御 | 注册时发现资产文件不存在但 meta 存在 → 视为陈旧，跳过加载旧 meta、按新资产生成 meta |
| **C** | 可选 API | `UnregisterAsset(..., UnregisterOptions{ .bRemoveMetaFromDisk })` 统一路径；Watcher 传 `true`（P5.0 已用独立 `RemoveMetaFileOnDisk`，是否合并待议） |

**建议顺序：** 先验收 P5.0；审批通过后实现 **A**（工程级卫生）+ 视需要 **B**（双保险）。

---

## 6) 回环抑制（本进程 CRUD）

**问题：** Editor 内 `ImportAsset` / `DeleteAsset` / `MoveAsset` 写盘 → efsw 再报事件 → 重复 Register/Unregister。

**v0 方案（采纳）：**

```cpp
// AssetManager — 公开或 friend Watcher 可读
void BeginSuppressExternalSync();
void EndSuppressExternalSync();
bool IsExternalSyncSuppressed() const;

// RAII 供 Editor/Runtime CRUD 使用
class AssetManager::SuppressExternalSyncScope { ... };
```

- `ImportAsset` / `DeleteAsset` / `MoveAsset` / `RegisterAsset`（若由 Editor 显式调用且需抑制）入口：`SuppressExternalSyncScope guard;`
- `ProjectAssetWatcher::ProcessPendingActions`：若 `IsExternalSyncSuppressed()` → **丢弃**本批或延迟到下一 debounce 窗口（v0：**丢弃**并 debug 计数即可）。

**Watcher 不抑制：** 外部资源管理器 / git 操作产生的事件。

---

## 7) 线程、队列与去抖

### 7.1 线程模型（拍板 A）

```text
[efsw 线程] handleFileAction → EnqueueFileAction (mutex + queue)
[Editor 主线程] Tick → 更新 debounce 计时 → ProcessPendingActions → AssetManager API
```

**禁止** 在 `handleFileAction` 内直接调用 `AssetManager`。

### 7.2 去抖

| 参数 | 值 |
|------|-----|
| `kDebounceMs` | **400**（与设计 §5.2 300–500 ms 中值一致） |
| 行为 | 每次 `Enqueue` 重置计时；`Tick` 中 `elapsed >= kDebounceMs` 时 `ProcessPendingActions` 一次 |

### 7.3 每帧预算

- `ProcessPendingActions` 单次最多处理 **64** 条文件级事件，超出留待下帧（防卡 UI）。

---

## 8) 生命周期挂钩（定稿）

| 时机 | 动作 |
|------|------|
| `Editor::RegisterModules` | 创建并 `Register` `ProjectAssetWatcher`（与 `AssetWorkflowModule` 同级） |
| `Editor::OpenProject` **成功**后 | `GetProjectAssetWatcher().StartWatching(PathRegistry::GetProjectContentRoot())` |
| `Editor::CloseProject` | `StopWatching()`（在 `ProjectManager::CloseCurrentProject` **之前**） |
| `ProjectManager::CloseCurrentProject` | 已有 `ClearProjectRegistry()`（P2） |
| `Editor::Shutdown` | `StopWatching()` + `Shutdown` |

**说明：** `OpenProject` 时 `ProjectManager` 已 `ScanAssets`；Watcher 在 Scan 之后启动，避免启动瞬间双倍全量（Stop 未运行时不应收到历史事件）。

---

## 9) efsw submodule 与 CMake

### 9.1 仓库

| 项 | 值 |
|----|-----|
| 上游 | https://github.com/SpartanJ/efsw |
| 建议 tag | **1.4.0**（实现前核对最新 release tag） |
| 路径 | `minEngine/minEngine/Third-Party/efsw/` |

```bash
git submodule add https://github.com/SpartanJ/efsw.git minEngine/minEngine/Third-Party/efsw
cd minEngine/minEngine/Third-Party/efsw
git checkout 1.4.0   # 或审批时指定的 tag
```

### 9.2 CMake（Editor，定稿）

在 **`Editor/CMakeLists.txt`**（推荐，与「仅 Editor 链」一致）：

```cmake
set(EFSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../minEngine/Third-Party/efsw)
if(EXISTS ${EFSW_DIR}/CMakeLists.txt)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST_APP OFF CACHE BOOL "" FORCE)
    set(EFSW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${EFSW_DIR} ${CMAKE_BINARY_DIR}/third-party/efsw EXCLUDE_FROM_ALL)
    target_link_libraries(${EDITOR_NAME} PRIVATE efsw-static)
    target_include_directories(${EDITOR_NAME} PRIVATE ${EFSW_DIR}/include)
endif()
```

| 选项 | 值 |
|------|-----|
| 链接目标 | **`efsw-static`**（单 EXE，少 DLL 依赖） |
| `EFSW_INSTALL` | OFF |
| `BUILD_TEST_APP` | OFF |

**`minEngine` Runtime 目标：** 不 `target_link_libraries(minEngine efsw)`。

### 9.3 平台依赖（实现对照）

| 平台 | P5 最低验收 |
|------|-------------|
| **Windows** | ✅ 主开发机 |
| Linux | inotify；需 dev 包时文档注明，不阻塞 Win PR |
| macOS | FSEvents；同上 |

---

## 10) P5 验收标准

| # | 检查 |
|---|------|
| 1 | 打开工程后 Watcher 启动；关闭工程后停止，无崩溃 |
| 2 | 资源管理器复制 `*.png` 进 `Assets/` → Registry 出现新项，`Registered`（日志或后续 Browser） |
| 3 | 资源管理器删除已注册资产 → `UnregisterAsset`，Registry 无该 path；对应 `.meta` 从磁盘删除（P5.0） |
| 4 | 本进程 **Import Asset** 不产生重复 Register 风暴（抑制生效） |
| 5 | git checkout 大批量变更 → 最终 Registry 与磁盘一致（允许走一次 `ScanAssets` 兜底） |
| 6 | `Editor.exe` 链 efsw；`libminEngine.dll` **不**依赖 efsw |

---

## 11) 实施顺序（P5 内）

| 切片 | 内容 |
|------|------|
| **P5a** | submodule + Editor CMake + 空 `ProjectAssetWatcher` 启停 |
| **P5b** | 队列 + debounce + `Tick` + Add/Delete 映射 |
| **P5c** | Move 映射 + `SuppressExternalSync` + Import 回环验证 |
| **P5d** | 目录/批量 `ScanAssets` 兜底 + 日志 |

建议 **1–2 个 commit**：`chore(third-party): add efsw` + `feat(editor): ProjectAssetWatcher with efsw`。

---

## 12) 审批清单

- [ ] **A.** 同意 P5 范围（efsw + `ProjectAssetWatcher` + 映射 + 抑制 + 兜底 Scan）
- [ ] **B.** 同意 **仅 Editor** 链 `efsw-static`（Runtime 不链）
- [ ] **C.** 同意线程模型 A + debounce **400 ms**
- [ ] **D.** 同意外部删除用 **`UnregisterAsset`**（非 `DeleteAsset`）
- [ ] **E.** 同意 `AssetManager::SuppressExternalSync` 回环抑制（v0）
- [ ] **F.** 同意 submodule 路径 `Third-Party/efsw`；tag **1.4.0**（或备注替代 tag）
- [ ] **G.** 同意 `Reimported`/缓存失效推迟 **P5.1**

批准后回复 **「P5 API 批准」**；可先加 submodule，再实现 P5a–P5d。

---

## 13) 审批后文档

- 更新 [ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §9、§11  
- [ASSET_PIPELINE_P4_API.md](./ASSET_PIPELINE_P4_API.md) 可增「P5 见 …」交叉链接
