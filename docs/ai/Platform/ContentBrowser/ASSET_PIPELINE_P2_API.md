# Asset Pipeline — P2 接口定稿（审批用）

Last updated: 2026-05-25  
Status: **已实现**（`--asset-manager-test`）  
前置：**P1 已合并于本地** `8c5958d`（`feat/editor-asset-workflow`）；远程 push 由你自行执行  
父文档：[ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §4.3、§9 P2  
P1 定稿：[ASSET_PIPELINE_P1_API.md](./ASSET_PIPELINE_P1_API.md)

---

## 0) P1 完成度核对

| P1 项 | 状态 |
|-------|------|
| `AssetTypeRegistry` + Builtin 类型 | ✅ |
| `m_AssetMetasByType` + `FindAssetMetasByType` / `ByRuntimeClass` | ✅ |
| `Subscribe` / `Unregistered` / `Registered` / `MetaUpdated` | ✅（`Moved` 未发射） |
| `ImportAsset` | ✅ |
| 相对 `AssetPath` + `ResolveAssetAbsolutePath` | ✅ |
| 去掉 Editor `EngineDefault` Registry 扫描 | ✅ |
| `DeleteAsset` / `MoveAsset` | ❌ → **P2** |
| `CloseProject` 清空 Registry | ❌ → **P2 建议纳入** |
| `InvalidateLoadedAsset(path)` / `Reimported` 事件 | ❌ → **P2 可选** |

**说明：** 「P1 推完」= **本线 P1 代码与本地 commit 已完成**；`git push` 未在本次会话执行（分支暂无 upstream）。

---

## 1) P2 交付边界

### 1.1 包含

| 项 | 说明 |
|----|------|
| **`DeleteAsset`** | 删磁盘（资产 + `.meta`）+ `UncacheMeta` + `Unregistered` |
| **`MoveAsset`** | 重命名/移动资产与 meta；Registry 键迁移；`Moved` 事件；**GUID 不变** |
| **`RenameAsset`**（可选薄封装） | `MoveAsset(old, newParent / newFileName)` 语法糖 |
| **Scene 联动** | 删除 `Scene` 类型资产时 `SceneManager::UnregisterScene` |
| **工程切换清理** | `CloseCurrentProject` → 清空 `AssetManager` Registry / 缓存 / 订阅保留或清空（见 §6） |
| **加载缓存迁移** | `MoveAsset` 时迁移 `m_LoadedAssetCache` 键 |
| **验收** | 单元/手动：删/移后事件、GUID 查找、场景引用行为符合 §4 |

### 1.2 不包含（P3+）

- `IFileDialogService` / NFD
- `ProjectAssetWatcher` / efsw
- `ContentBrowserModule`
- 引用扫描阻塞删除（v0 仍 **WARN only**，D5 已拍板）
- Undo 命令包装
- `Reimported` 事件与自动 reload（可 P2.1 小项，默认 **Defer**）

---

## 2) 修改文件（预计）

| 文件 | 动作 |
|------|------|
| `Runtime/Resource/AssetManager.h` | 公开 `DeleteAsset`、`MoveAsset`；（可选）`RenameAsset` |
| `Runtime/Resource/AssetManager.cpp` | 实现 + 私有 `MoveCachedLoadedAssetKey` |
| `Runtime/Function/Framework/Project/ProjectManager.cpp` | `CloseCurrentProject` 调 Registry 清理 |
| `Runtime/Function/Framework/Scene/SceneManager.*` | 仅 **调用** 已有 `UnregisterScene` |
| `docs/ai/PROGRESS_LOG.md` | P2 完成后一条 |
| （可选）`Editor/...` 临时调试菜单调用 Delete | **非必须**；可 P4 与 AssetWorkflow 一起做 |

**不修改：** `UI/Property/**`、`Color.*`、`AssetTypeRegistry` 表项（除非 Move 后扩展名变化导致改类型 — v0 **禁止**跨类型 Move）

---

## 3) 公开 API 定稿

### 3.1 `DeleteAsset`

```cpp
bool DeleteAsset(const std::string& assetPath, std::string& outError);
```

| 参数 | 语义 |
|------|------|
| `assetPath` | 工程相对或绝对路径；内部 `NormalizeProjectRelativeAssetPath` |
| `outError` | 失败原因（英文，日志同源） |

**流程（顺序固定）：**

```text
1. projectRelative = Normalize(path)
   → 空：outError = "invalid or out-of-project path"; return false

2. meta = FindAssetMetaByPath(projectRelative)
   → null：outError = "asset not registered"; return false

3. LogReferenceWarningsIfAny(meta.Guid)   // v0：仅 ME_CORE_WARN，不阻塞

4. absolute = ResolveAssetAbsolutePath(projectRelative)
   metaAbsolute = BuildMetaAbsolutePath(projectRelative)

5. 若 exists(absolute) → remove(absolute)
   若 exists(metaAbsolute) → remove(metaAbsolute)
   （remove 失败 → outError，return false；Registry 不变）

6. EvictLoadedAssetCache(projectRelative)

7. UncacheMeta(projectRelative)   // 已含 Unregistered 事件

8. 若 meta.AssetType == "Scene" → SceneManager::UnregisterScene(meta.AssetName)

9. return true
```

**与 Watcher 分工（P5）：** 磁盘已被外部删掉时，用 **`UnregisterAsset(path)`**（P2 新增，见 §3.3）只清 Registry，不 `remove` 文件。

### 3.2 `MoveAsset`

```cpp
bool MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError);
```

| 参数 | 语义 |
|------|------|
| `oldPath` / `newPath` | 工程相对或绝对；均须落在 `ProjectContentRoot` 下 |
| 扩展名 | **必须相同**（`path.extension()`）；否则失败 — 避免 AssetType 与 Loader 错位 |
| 目标已存在 | 失败（与 Import 一致，D4） |

**流程：**

```text
1. oldRel = Normalize(oldPath); newRel = Normalize(newPath)
   任一为空 → false

2. oldRel == newRel → true（no-op，不发 Moved）

3. FindAssetMetaByPath(oldRel) 必存在

4. 校验 newRel 不在 Registry（除非 no-op）

5. 校验 extension 相同；newRel 父目录存在或可创建（v0：父目录必须已存在）

6. absoluteOld/New, metaOld/New paths

7. rename(asset file); rename(.meta file)
   任一步失败 → 尽力回滚 rename（best-effort）；outError；return false

8. 更新 map 内 AssetMeta：
   - 从 m_AssetRegistry[oldRel] 节点取出 meta，改 AssetPath/AssetName，插入 [newRel]，擦除 [oldRel]
   - m_AssetPathByGuid[guid] = newRel
   - 类型桶：RemoveFromTypeBucket(old meta copy) + AddToTypeBucket(updated)  // 或 MoveMetaInRegistry 单函数

9. MoveLoadedAssetCacheKey(oldRel, newRel)

10. 写回 meta 文件（AssetPath=newRel, AssetName=stem, Guid 不变）

11. Broadcast Moved { OldPath=oldRel, NewPath=newRel, Guid, AssetTypeId }

12. return true
```

**不发 `MetaUpdated`**：路径变更用 **`Moved`** 专用事件（D3 同步语义不变）。

### 3.3 `UnregisterAsset`（Registry-only，供 Watcher / 外部已删文件）

```cpp
bool UnregisterAsset(const std::string& assetPath, std::string& outError);
```

| 项 | 说明 |
|----|------|
| 行为 | 仅 `EvictLoadedAssetCache` + `UncacheMeta`；**不**删磁盘 |
| 文件仍在盘上但未注册 | 下次 `ScanAssets` / `RegisterAsset` 可再纳入 |
| P2 实现 | 与 `DeleteAsset` 共享私有收尾 `RemoveFromRegistry(projectRelative)` |

### 3.4 `RenameAsset`（可选）

```cpp
bool RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError);
```

- `newFileName` 仅文件名（如 `MyMesh.obj`），新路径 = `oldPath` 父目录 / `newFileName`。
- 实现为一次 `MoveAsset` 调用。

### 3.5 `ClearProjectRegistry`（工程生命周期）

```cpp
void ClearProjectRegistry();
```

| 调用点 | 说明 |
|--------|------|
| `ProjectManager::CloseCurrentProject` | 在 `ClearProjectRoots()` **之前或之后** 调用（建议之后，避免路径解析歧义） |
| 行为 | `m_AssetRegistry.clear()`；`m_AssetPathByGuid.clear()`；`m_AssetMetasByType.clear()`；`m_LoadedAssetCache.clear()` |
| 订阅者 | **保留** `m_Subscribers`（Editor 模块不重建） |

**不发事件：** 批量 `Unregistered` 噪声大；Close 后下次 `OpenProject` + `ScanAssets` 整表 `Registered`。

---

## 4) 私有辅助（成员函数，非文件级 static）

```cpp
void AssetManager::EvictLoadedAssetCache(std::string_view projectRelativePath);
void AssetManager::MoveLoadedAssetCacheKey(std::string_view oldRel, std::string_view newRel);
bool AssetManager::LogReferenceWarningsForDelete(const AssetMeta& meta) const;  // v0 stub WARN
bool AssetManager::MoveRegistryEntry(std::string_view oldRel, std::string_view newRel, AssetMeta& inOutMeta);
```

---

## 5) 事件契约（P2 补充）

| Kind | 何时发射 | P2 前 |
|------|----------|-------|
| `Unregistered` | `UncacheMeta` / `UnregisterAsset` | ✅ 已有 |
| `Moved` | `MoveAsset` 成功 | ❌ |
| `MetaUpdated` | `RegisterAsset` 更新已有 | ✅ |
| `Reimported` | — | Defer P2.1 |

`AssetRegistryChange` 字段不变；`Moved` 时 `OldPath`/`NewPath` 均为**工程相对路径**。

---

## 6) 行为与风险（已拍板延续）

| ID | 决策 | P2 行为 |
|----|------|---------|
| D5 | Delete 不阻塞 | `LogReferenceWarningsForDelete` 仅 WARN |
| D4 | 目标存在则失败 | Move/Import 一致 |
| GUID | Move 不变 | meta 与场景 `$guid` 引用保持 |
| Scene | Delete Scene 资产 | `UnregisterScene(AssetName)` |
| 类型 | Move 禁止改扩展名 | 改扩展名 = 先 Delete + Import |

**已知限制（文档化）：**

- Delete 后已加载 `MEObject` 仍可能活在 `ObjectManager` 至 GC；场景里 GUID 引用解析失败 → 与 P1 相同。
- `CloseProject` 不清订阅者；若回调缓存 path 指针，Open 新工程后需自行刷新（Browser P6 处理）。

---

## 7) 验收标准

| # | 检查 |
|---|------|
| 1 | `DeleteAsset("Meshes/BasicShapes/cube.obj")` 后磁盘无 obj/meta，Registry 无该 path，`FindAssetMetaByGuid` 失败 |
| 2 | 订阅者收到 `Unregistered`，`OldPath` 为相对路径 |
| 3 | `MoveAsset` plane→`Meshes/BasicShapes/ground.obj` 非法（改扩展名）失败 |
| 4 | `MoveAsset` 同目录重命名后 GUID 不变，场景 mesh 引用仍可 resolve |
| 5 | `MoveAsset` 后 `m_LoadedAssetCache` 新键可 `LoadAsset` 命中缓存（若此前加载过） |
| 6 | `CloseCurrentProject` 后 Registry 为空；再 `OpenProject` 仅含扫描结果 |
| 7 | Delete `Scenes/test.mescene` 后 `SceneManager` 无对应 `RegisterScene` 项 |

---

## 8) 实施顺序（P2 内部分 PR 可选）

| 切片 | 内容 |
|------|------|
| **P2a** | `ClearProjectRegistry` + `ProjectManager` 挂钩 |
| **P2b** | `DeleteAsset` + `UnregisterAsset` + Scene 联动 |
| **P2c** | `MoveAsset` + `Moved` 事件 + cache 键迁移 |
| **P2d** | （可选）`RenameAsset` + Editor 调试菜单一项 |

建议 **一个 commit** 亦可（体量 < P1）。

---

## 9) 审批清单

- [ ] **A.** 同意 P2 范围（Delete + Move + Clear on Close + UnregisterAsset）
- [ ] **B.** 同意 Move 禁止改扩展名
- [ ] **C.** 同意 `CloseProject` 清 Registry 不发批量事件
- [ ] **D.** `Reimported` / 自动 reload 推迟到 P2.1 或 P5
- [ ] **E.** `RenameAsset` 薄封装要 / 不要

批准后回复 **「P2 API 批准」**，再动 `AssetManager` 代码。

---

## 10) 审批后文档

- 更新 `ASSET_PIPELINE_DESIGN.md` §9 勾选 P2 定稿链接  
- `ASSET_PIPELINE_P1_API.md` §11 指向本文  
- P3：[ASSET_PIPELINE_P3_API.md](./ASSET_PIPELINE_P3_API.md)
