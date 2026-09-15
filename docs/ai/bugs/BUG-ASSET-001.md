# BUG-ASSET-001 — 新建/保存资产触发 ProjectAssetWatcher 全盘 ScanAssets

## Meta
- **ID:** BUG-ASSET-001
- **Status:** Open
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-15
- **Last updated:** 2026-09-15
- **Affects:** `ProjectAssetWatcher` + `AssetManager::ScanAssets`；任意向 Content 写新文件且未正确 `EditorFilesystemMutationPass` 的路径（已确认：`PrefabUtility::SavePrefabAsset`）
- **Related Feature/Slice:** ED-F16 Create Prefab · `EditorFilesystemMutationPass` · TD-004（CB UI 重建，相关但非同一根因）

## TL;DR
Hierarchy **Create Prefab…**（或其它未挂 MutationPass 的落盘）后，efsw 把目录变更当成「需全量重扫」，`RunFullRescan` → `ScanAssets(Assets)`；Console 刷出大量 `Asset registered/updated`（看起来像「别处来的」日志），资产多时明显卡顿。

---

## 症状
1. 创建/保存新 `.meprefab`（或同类写盘）后短暂卡顿。
2. 日志出现：
   - `ProjectAssetWatcher: running full ScanAssets on '<Assets root>'`
   - 随后大量 `Asset registered` / `Asset updated`（`RegisterAsset` Info，对**每个**已有资产再跑一遍）
3. 常与 CB 双击 Prefab 时的 `TryOpenAsset` 日志同屏出现（已在 `feat/prefab` 补齐 Prefab 打开；全扫噪音仍属本 bug）。

## 期望
- Editor 主动写盘应走 **增量** `RegisterAsset` / watcher `RegisterOrUpdate`，**不**触发全 Content `ScanAssets`
- MutationPass 覆盖本次写入的文件与父目录，吞掉自触发 watcher 事件
- 仅在真正丢失同步（阈值目录风暴）时才 FullRescan，且应 batch / 降噪日志

## 复现
1. `feat/prefab` 打开 Editor，项目 Content 下已有若干资产
2. Hierarchy → Create Prefab… → 保存到 `Assets/`（或子目录）
3. 观察 Console：先可能有 Create 成功 Info，随后 full ScanAssets + 全库 `Asset registered/updated`

## 环境
- OS：Windows 10；efsw `ProjectAssetWatcher`
- Branch：`feat/prefab`；Create Prefab → `PrefabUtility::SavePrefabAsset`

## 根因（初步，待修时再钉死）

### A) Watcher 策略过激
`ProjectAssetWatcher::handleFileAction`：若事件路径 `is_directory` → **立刻** `RequestDebouncedFullRescan()`。  
`kDirectoryEventThreshold = 1`：防抖队列里 **只要 1 条** directory rescan 请求，`ProcessPendingActions` 就 `RunFullRescan()` → `AssetManager::ScanAssets(m_WatchedRoot)`。

Windows/efsw 在目录内 **新增文件** 时，常对**父目录**发 Modified；父目录被当成 directory 事件 → 单次 Create Prefab 即可触发全扫。

### B) Create Prefab 落盘未挂 MutationPass
对照：
- `AssetManager::CreateAsset<*>` / Import / Delete / Move：写盘前后 `NoteEditorFilesystemMutation(...)`（含父目录）
- `SceneEditor::SaveCurrentSceneAs`：对 `parent_path` + 文件 `NoteMutatedAbsolutePath`
- **`PrefabUtility::SavePrefabAsset`：** `create_directories` + `Serializer::ToFile` + `RegisterAsset`，**从未** `NoteEditorFilesystemMutation`

因此自触发的 directory / file 事件不会被 `ShouldIgnoreWatcherEvent` 吞掉。

### C) 「别处来的」日志从哪来
不是随机模块：来自 **全盘 Scan 路径**：

```text
ProjectAssetWatcher::RunFullRescan
  → AssetManager::ScanAssets(Assets)
    → recursive_directory_iterator
    → 每个可识别扩展名 RegisterAsset(...)
         → ME_LOG(LogAsset, Info, "Asset registered/updated ...")   // 每资产一条
```

故用户看到「一堆不像 Prefab 的注册日志」= 全库被重新 Register 的副作用，不是其它子系统自发扫描。

## 修复方向（后续合适时机；本期只登记）
1. **热修 Create Prefab：** `SavePrefabAsset`（及同类 Utility 写盘）对齐 CreateAsset：写前 Note 父目录 + 资产路径 + meta 路径。
2. **Watcher：** 评估 `kDirectoryEventThreshold`；目录 Modified 优先尝试增量而非 full Scan；full Scan 时 `BeginRegistryBroadcastBatch` + 降 Info 日志级别或仅 log 摘要。
3. 与 TD-004（CB 整树重建）区分：本 bug 是 **Runtime ScanAssets**，不是仅 UI model。

## 回归验证
- [ ] Create Prefab 后 **无** `running full ScanAssets`（正常单文件场景）
- [ ] Console **无**全库 `Asset registered/updated` 风暴
- [ ] 新 Prefab 仍出现在 CB（增量 Register / watcher RegisterOrUpdate）
- [ ] 外部工具往 Assets 丢文件仍能被 watcher 发现（增量或合理防抖全扫）

## 关联
- `AssetWorkflowModule::TryOpenAsset` Prefab 放行（同会话已补；非本 bug）
- [TD-004](../TECH_DEBT.md) — CB registry 刷新 UI 成本
- [CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md](../Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md)（Archived：UI 侧历史；本 bug 为 watcher Scan）
- `EditorFilesystemMutationPass` · `ProjectAssetWatcher.cpp` · `PrefabUtility::SavePrefabAsset`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | Open：Create Prefab 后全盘 Scan；钉 MutationPass 缺口 + directory threshold=1；说明 Scan 刷屏日志来源 |
