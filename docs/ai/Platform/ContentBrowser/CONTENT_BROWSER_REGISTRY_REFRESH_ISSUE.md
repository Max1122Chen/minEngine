# Content Browser — Import/Delete 触发「全量刷新」问题记录

Last updated: 2026-05-27  
Status: **open**（后续优化，非阻塞 M1 右键菜单）  
关联：`AssetManager`、`AssetTreeModel`、`ProjectAssetWatcher`、右键菜单 M1（[EDITOR_CONTEXT_MENU_DESIGN.md](../../Editor/EDITOR_CONTEXT_MENU_DESIGN.md)）

---

## 1) 现象

- Content Browser 执行 **Import**、**Delete**（及工具栏 Refresh）后，目录树与 Tile 列表明显「整库刷新」，资产多时卡顿。
- 体感像 `ScanAssets` 全量扫描；实际 **Editor 主动 Import/Delete 并不调用 `ScanAssets`**，但 UI 重建成本接近全量。

---

## 2) 结论（分层）

| 层级 | 是否「全量」 | 说明 |
|------|--------------|------|
| **`AssetManager::ImportAsset` / `DeleteAsset`** | **否（注册表单条增量）** | `RegisterAsset` → `CacheMeta` + 一次 `BroadcastChange`；`DeleteAsset` → `UncacheMeta` + 一次 `Unregistered` |
| **`AssetManager::ScanAssets`** | **是** | 递归扫 Content 目录，每个文件 `RegisterAsset`；由 **`ProjectAssetWatcher::RunFullRescan`** 等触发，非右键 Import/Delete 主路径 |
| **`AssetTreeModel`（CB UI）** | **是（整树重建）** | 订阅方对 **任意** `AssetRegistryChange` 都调用 `RebuildDirectoryTree()` |

**根因概括：** 注册表变更已是增量通知，但 **Content Browser 模型把每条通知都当成整树重建**；且 **业务代码在订阅之外再次 `RebuildDirectoryTree()`**，造成重复。

---

## 3) 代码路径（便于后续改）

### 3.1 Runtime — 单条变更（正常）

- `ImportAsset`：`copy_file` → `RegisterAsset` → `CacheMeta` → `BroadcastChange(Registered)`  
  见 `minEngine/minEngine/src/Runtime/Resource/AssetManager.cpp`
- `DeleteAsset`：删磁盘 → `UncacheMeta` → `BroadcastChange(Unregistered)`
- `SuppressExternalSyncScope`：Import/Delete/Move 内开启，用于 **丢弃** `ProjectAssetWatcher` 防抖队列中的事件（非延后重放）

### 3.2 Editor — 整树重建（问题集中点）

**订阅回调（每条变更都重建）：**

```text
AssetTreeModel::OnRegistryChange
  → Registered / Unregistered / Moved / MetaUpdated / Reimported
  → RebuildDirectoryTree()                    // 递归 directory_iterator + 每节点扫全表
  → 若与当前目录相关 → RebuildCurrentDirectoryAssetList()
```

文件：`minEngine/Editor/src/Services/ContentBrowser/AssetTreeModel.cpp`

**`RebuildDirectoryTree` 成本：**

- `BuildDirectoryNodeRecursive`：每个目录节点调用 `AssetManager::FindAssetMetasUnderDirectory(rel)`。
- `FindAssetMetasUnderDirectory`：遍历 **整个** `m_AssetRegistry`（O(资产总数) × 目录数）。

**重复刷新：**

| 操作 | 额外全量重建 |
|------|----------------|
| Import N 个文件 | N 次 `OnRegistryChange` 重建 + `ImportAssetDialog` 末尾再 `RebuildDirectoryTree` 一次 |
| Delete（右键 Action） | 1 次 `UncacheMeta` 重建 + `RefreshContentBrowser` 再重建 |
| Refresh 按钮/菜单 | 显式 `RebuildDirectoryTree`（预期，但与上叠加） |

相关文件：

- `minEngine/Editor/src/Services/AssetWorkflowModule.cpp` — `ImportAssetDialog` 循环后重建
- `minEngine/Editor/src/ContextMenu/Actions/ContentBrowserBuiltInActions.cpp` — `RefreshContentBrowser`
- `minEngine/Editor/src/UI/EditorWindows/ContentBrowserWindow.cpp` — 工具栏 Refresh

### 3.3 Watcher — 真·全量 Scan（独立路径）

- `ProjectAssetWatcher::RunFullRescan()` → `AssetManager::ScanAssets(m_WatchedRoot)`
- 触发条件：debounce 批次内文件事件 > `kFullRescanFileThreshold`（32）或目录类事件等  
  见 `minEngine/Editor/src/Services/AssetWatch/ProjectAssetWatcher.cpp`
- Editor Import/Delete 期间 **抑制 Watcher**，一般 **不会** 在当帧再 Scan；但 Scan 发生时仍会对每个文件 `RegisterAsset` 并广播，若未合并通知，仍会 N 次触发 UI 重建。

---

## 4) 后续优化方向（待拍板后实施）

1. **通知合并（Runtime 或 Model）**  
   - `AssetManager` 增加 batch scope（暂停广播 / 结束时一次 `BulkChanged`），或  
   - `AssetTreeModel` 在 Import 循环外由调用方 `BeginBatch` / `EndBatch` 只刷新一次。

2. **UI 增量更新**  
   - `OnRegistryChange` 按 `Kind` + `OldPath`/`NewPath` 只改受影响目录节点与当前列表，避免 `RebuildDirectoryTree()`。

3. **索引结构**  
   - 按目录维护 `path → vector<AssetMeta*>`，避免 `FindAssetMetasUnderDirectory` 每次扫全表。

4. **去掉重复调用**  
   - Import 循环内依赖订阅 **或** 末尾显式 Refresh，二选一；Delete Action 同理。

5. **与 P5 文档对齐**  
   - [ASSET_PIPELINE_P5_API.md](./ASSET_PIPELINE_P5_API.md) 已记录 Editor 写盘 vs efsw 重复同步；本 issue 侧重 **CB UI 全量重建**，二者可一并规划。

---

## 5) 验收（解决后）

- [ ] 单文件 Import/Delete：Content Browser **最多 1 次** 树/List 刷新（或可观测的批量刷新）
- [ ] 多文件 Import（如 10+）：**1 次** 批量刷新，无 10+ 次整树重建
- [ ] 大数据量项目（资产数 / 目录数见测试备注）下 Import/Delete 可接受延迟
- [ ] `ScanAssets` 全量路径行为不变或单独文档化，不与日常 Import 混淆

---

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-27 | 初稿：Import/Delete 后「全量更新」问题记录（分析自 M1 CB 上下文菜单联调） |
