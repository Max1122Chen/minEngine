# Agent 转交 — Editor Asset Workflow 线

Last updated: 2026-05-25  
Status: **活跃 — 供 Composer / Agent 首条消息引用**  
分支：`feat/editor-asset-workflow`  
Worktree：`d:\Dev\GitRepo\minEngine-asset-workflow`  
基线：`07c9734`（含 `feat/editor-appearance` 的 Color M0）

---

## 0) 一句话任务

在 **独立 worktree** 上实现 **E3 AssetManager 扩展 + E4 FileDialog + AssetWorkflow 编排**（Content Browser UI 后置）；**依赖并保留** appearance 线的 `Color` / `LinearColor` M0，**不**改 appearance 专属 UI 路径。

---

## 1) 环境与并行纪律

| 项 | 值 |
|----|-----|
| 本线 worktree | `d:\Dev\GitRepo\minEngine-asset-workflow` |
| 本线分支 | `feat/editor-asset-workflow` |
| 并行 appearance 线 | `d:\Dev\GitRepo\minEngine` → `feat/editor-appearance` |
| 共同祖先 | `f429879`（master / Inspector undo E1.3+） |
| 本线已合并 | `07c9734` — `Color` + `LinearColor`（appearance M0） |

**规则：**

- 只在 **asset-workflow worktree** 内改代码；不要动 `d:\Dev\GitRepo\minEngine` 工作区。
- 禁止 `git add .`；只 stage 本任务相关文件。
- appearance 线继续 M1+（主题、PropertyWidgets）；本线 **拥有** `AssetManager` 变更 API / 事件 / 类型桶，appearance 将来 **消费** 只读索引。
- 合并顺序建议：本线 E3 稳定 → appearance rebase → 再开 Content Browser UI 子模块。

---

## 2) 必读文档（按顺序）

1. `docs/ai/PROJECT_CONTEXT.md`
2. `docs/ai/Editor/EDITOR_PLATFORM_PLAN.md` — **§ E3、E4、§2 依赖顺序**
3. `docs/ai/Editor/EDITOR_SHELL_DESIGN.md` — `AssetWorkflowModule`、`EditorServiceModule`
4. `docs/ai/Platform/ContentBrowser/CONTENT_BROWSER_DESIGN.md` — 产品意图（UI 后置）
5. `docs/ai/Editor/EDITOR_APPEARANCE.md` — **仅读 § G7 / M3.1**（类型桶与 appearance 的约定，本线 E3 一并实现）

---

## 3) 路径所有权

### 3.1 允许修改

| 区域 | 路径 |
|------|------|
| AssetManager | `minEngine/minEngine/src/Runtime/Resource/AssetManager.{h,cpp}`、`AssetMeta.*` |
| 资产类型表 | 新建 `Runtime/Resource/AssetTypeRegistry.*` 或等价（E3 单一来源） |
| FileDialog | 新建 `Editor/src/Services/FileDialog/`（`IFileDialogService` + Win32 实现） |
| AssetWorkflow | `Editor/src/Services/AssetWorkflowModule.{h,cpp}` |
| Shell 注册 | `Editor/src/Editor.{h,cpp}`（仅模块注册 / 注入服务，小 diff） |
| 设计 / 会话 | `docs/ai/Platform/ContentBrowser/`、`docs/ai/sessions/`、`docs/ai/PROGRESS_LOG.md`（任务结束时简短条目） |

### 3.2 禁止修改（交给 appearance 线）

| 区域 | 路径 |
|------|------|
| Color 类型 | `Runtime/Core/Math/Color.{h,cpp}`、生成的 `Color.gen.*` |
| Property / 主题 UI | `Editor/src/UI/Property/**`、`EditorAppearance/**` |
| 文档 | `docs/ai/Editor/EDITOR_APPEARANCE.md`（只读） |

### 3.3 共享只读依赖（已在本线基线）

- `Color` / `LinearColor` — 本线 **不扩展**；若 E3 测试需要序列化样例，用现有类型即可。
- `FindAssetMetasByType` — 已存在于 `AssetManager.h`；E3 应改为 **注册时维护类型桶**（O(1) 桶 + 稳定迭代），对齐 appearance G7。

---

## 4) 实施分期（本 agent 范围）

### Phase A — E3 设计 + 最小 API（先做）

**目标：** Registry 变更能力 + 事件，供 Editor / 未来 Content Browser 订阅。

| 交付 | 说明 |
|------|------|
| `AssetRegistryChange` | 枚举：`Registered` / `Unregistered` / `Moved` / `MetaUpdated` / `Reimported` |
| 订阅 API | `AssetManager::Subscribe(OnRegistryChanged)` 或等价 delegate/multicast |
| 变更操作 v0 | `ImportAsset(sourcePath, destDir)`、`DeleteAsset(path)`、`MoveAsset(old, new)` — 磁盘 + `.meta` + 内存 Registry 一致 |
| 类型桶 | `m_AssetMetasByType`（或 `unordered_map<string, vector<GUID>>`）；`RegisterAsset` / `CacheMeta` 维护；`FindAssetMetasByType` 走桶 |
| 类型表 | 扩展名 ↔ AssetType 单一来源（FileDialog filter、Scan、Loader 共用） |
| 验收 | 单元/手动：Import 后事件触发、按类型查询不扫全表、Delete 后 GUID 映射清除 |

**非目标（Phase A）：** 依赖图、异步 Reimport、Addressables 路径抽象。

### Phase B — E4 FileDialog

| 交付 | 说明 |
|------|------|
| `IFileDialogService` | `OpenFiles`、`SaveFile`、`SelectFolder`；filter 来自 E3 类型表 |
| `Win32FileDialogService` | Windows 原生；Editor 启动时注册到 `IEditorContext` 或 Service 定位器 |
| 验收 | 菜单或临时调试入口能选文件/文件夹并返回路径 |

### Phase C — AssetWorkflow 编排

| 交付 | 说明 |
|------|------|
| `AssetWorkflowModule` 扩展 | `ImportAssetDialog()`、`DeleteSelectedAsset()` 等；内部：FileDialog → E3 Import →（可选）OpenAsset |
| 与现有 `OpenAsset` | 保持 `MaterialEditor` / `SceneEditor` 路由；不引入全局 Selection enum |
| 验收 | 从 OS 对话框 Import 到 `Assets/` 后 Registry 刷新；双击 meta 仍能 `OpenAsset` |

### Phase D — Content Browser（后置，本任务可选 stub）

- 仅当 A–C 稳定后：新建 `ContentBrowserModule` 骨架 + 空面板 + 订阅 E3 事件。
- **不要**在 E3 未定前堆 Browser UI 细节（见 `CONTENT_BROWSER_DESIGN.md`）。

---

## 5) 第一步（Agent 开工清单）

1. 阅读 `AssetManager.cpp` 现有 `ScanAssets` / `RegisterAsset` / `CacheMeta` / `FindAssetMetasByType`。
2. 阅读 `AssetWorkflowModule.cpp` 现有 `OpenAsset` 路由。
3. 在 `docs/ai/Platform/ContentBrowser/` 或 `docs/ai/sessions/` 写 **E3 v0 接口草案**（200 行内：事件签名、Import/Delete/Move 语义、与 `.meta` 不变量）。
4. **用户确认草案后** 再改 `AssetManager` — 先设计后编码。
5. Phase A 第一个 PR 切片建议只含：**类型桶 + Registry 事件 + `ImportAsset`（单文件）**，不含 FileDialog。

---

## 6) 现有代码锚点

```text
AssetManager.h     — Registry、Load/Save、FindAssetMetasByType（待优化为桶）
AssetWorkflowModule — OpenAsset → MaterialEditor / SceneEditor
EDITOR_SHELL_DESIGN — AssetWorkflowModule 为 EditorServiceModule，ContentBrowser 后置
Color.h (M0)       — 只读依赖；LinearColor 为持久化真源（appearance 约定）
```

---

## 7) Composer 首条消息（复制粘贴）

```markdown
你在 minEngine 的 **asset-workflow 专用 worktree** 工作。

- 路径：`d:\Dev\GitRepo\minEngine-asset-workflow`
- 分支：`feat/editor-asset-workflow`（基线含 appearance Color M0）
- 转交文档：`docs/ai/sessions/EDITOR_ASSET_WORKFLOW_AGENT_HANDOFF.md`

任务：实现 E3 AssetManager 扩展 + E4 FileDialog + AssetWorkflow 编排（Content Browser UI 后置）。

纪律：
- 只改转交文档 §3.1 允许路径；禁止改 Color / Property UI / EDITOR_APPEARANCE.md
- 先读转交文档 §2 必读文档，再读 AssetManager 与 AssetWorkflowModule 现状
- Phase A 先写 E3 v0 接口草案，等我确认后再写 AssetManager 代码
- 第一个代码切片：类型桶 + Registry 变更事件 + ImportAsset（单文件）
- 不要 git add .；遵循 cpp-style skill；C++ 优先成员函数而非文件级 static 辅助

完成后更新 `docs/ai/PROGRESS_LOG.md` 一条简短记录。
```

---

## 8) appearance 线同步说明

当 `feat/editor-appearance` 有新 commit 时，在本 worktree 执行：

```powershell
git fetch origin
git merge feat/editor-appearance
# 或：git rebase feat/editor-appearance
```

若冲突集中在 `AssetManager.*`，以 **本线 E3 设计** 为准，appearance 只保留对 `FindAssetMetasByType` 的调用。
