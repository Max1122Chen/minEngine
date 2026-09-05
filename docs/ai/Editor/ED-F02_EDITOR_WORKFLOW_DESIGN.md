# ED-F02 — Editor Workflow

## Meta
- **ID:** `ED-F02`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Branch:** `master`（原 `feat/editor` 已合入）
- **Depends on:** `ASSET` 资产扫描/registry（已有）；`SceneManager` Load/Save（已有）；`MaterialEditor` Open/Save session（已有）
- **Related:** [Implementation](./ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · [EDITOR_SHELL_DESIGN](./EDITOR_SHELL_DESIGN.md) · [EDITOR_CONTEXT_MENU_DESIGN](./EDITOR_CONTEXT_MENU_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md)

## TL;DR

**工作流主路径已在 `master` 落地**（`AssetWorkflowModule` 编排：Content Browser 双击、File Open/New/Save As、dirty 确认、`CreateAsset<Scene/Material>`）。剩余体验项：**S03** Material Preview 仍缺 SkyBox 实体；**S05** 过滤逻辑已有，但 Component 基类多数未标 `ME_CLASS(Abstract)`。S04 视口局部 RMB 捕获已基本完成。

## Status note（In Progress）

| 字段 | 内容 |
|------|------|
| Landed | S00–S02（merge gate）+ S04（局部导航 capture） |
| Remaining | S03 SkyBox 内容；S05 Abstract **标注**补齐（过滤代码已在） |
| Code | `b3917ef` + `feat/editor` → `master` merge |

## Scope
- **In:**
  - Content Browser **双击** → 打开 Scene / Material（路由到既有 SubModule）
  - File 菜单：**Open Scene**、**New Scene**（最小版）、**Save** / **Save As**（Scene；Material 沿用现有快捷键）
  - 切换 Scene / Material / 退出 Editor 前的 **dirty 确认**（Save / Don't Save / Cancel）
  - Runtime：`AssetManager::CreateAsset<Scene>`、`CreateAsset<Material>`（写盘 + meta + Scene 注册）
  - Content Browser / File 菜单：**创建 Scene、Material**
  - Material Editor 预览：**SkyBox** 与 Scene 视口对齐
  - Scene 视口：**局部** RMB 导航鼠标捕获（不污染 ImGui 面板）
  - Inspector Add Component：**过滤 Abstract 基类**（反射 `ClassSpecifier::Abstract`）
  - 每刀 `verify.ps1`；涉及 Scene/Material 时记录人工目视步骤
- **Out（本 Feature 或近端不做）:**
  - Play Mode / Edit-Play 切换（`CORE-F05`）
  - 多 Scene 标签页、最近打开列表（Launcher 已有 `recent`）
  - 全资产类型创建向导（Texture、Mesh 等仍 **Import**）
  - Component 下拉 **图标**（S05 后半，可 merge 后单开）
  - Content Browser 内联重命名、拖拽移动、批量操作
  - VK Editor 专项 parity（`ED-F01` defer 项不阻塞本 Feature）
  - 项目级 Undo 跨 Scene 持久化

## Reader quick start
1. 本文件 — 编排层、dirty 策略、切片边界。
2. [Implementation](./ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md) — S00–S05 切片与 DoD。
3. 代码入口：`Editor/src/Services/AssetWorkflowModule.*`、`SubEditor/Scene/SceneEditor.*`、`UI/EditorWindows/MainMenuWindow.cpp`、`UI/EditorWindows/ContentBrowserWindow.cpp`、`Runtime/Resource/AssetManager.*`

---

## 0) Pre-flight（2026-09-01）

| 项 | 结论 |
|----|------|
| 扫描 | dirty/save 已在 SceneEditor、MaterialEditor；`AssetWorkflowModule::OpenAsset` 路由就绪；Scene `OpenAsset` stub；CB `ActivateAssetFromBrowser` 仅 log；`CreateAsset<T>` 无 Scene/Material 特化 |
| 前置 | `feat/editor` 已与 `master` 对齐；合并检查点要求本 Feature **至少 S01–S02 Done** |
| 债风险 | **low–medium** — Save As 与 `SceneManager::RegisterScene` 命名一致性；viewport 鼠标与 ImGui focus 竞态 |
| WIP | 无并行 Editor 工作流大改 |
| 建议 | **Go** — 先 S00+S01 打通主路径，再 S02 runtime 创建 |

> **2026-09-03：** Pre-flight 为历史记录。S01–S02 merge gate 已完成；见 Status note。


---

## 1) 背景与目标

### Pain
- 用户从 Content Browser 双击 `.mescene` / `.mematerial` 无反应。
- File → Open / New Scene 为 UI 占位，日常只能依赖启动时默认 Scene。
- 切换资产或关闭 Editor 时，未保存修改可能被静默丢弃。
- Material 预览缺 SkyBox，与 Scene 视口观感不一致。
- Scene 视口 RMB 导航时 **整窗** 隐藏光标，影响 ImGui 操作。
- Add Component 列表含 `PrimitiveComponent` 等不可实例化基类。

### 成功长什么样
- 双击 Content Browser 资产 → 进入对应 Editor 模式并加载内容。
- File 菜单可打开、新建、保存 Scene；Material 保持现有 Save 流程。
- 有未保存修改时切换/退出弹出确认。
- Material 预览有 SkyBox；视口 RMB 仅在聚焦视口时捕获鼠标。
- Component 下拉仅显示可添加的具体类型。

---

## 2) 现状

### 已有能力（2026-09-03 对照代码）

| 区域 | 状态 | 关键位置 |
|------|------|----------|
| Scene dirty / Save | Done | `SceneEditor` |
| 资产打开编排 | **Done** | `AssetWorkflowModule::TryOpenAsset` / `TryOpenSceneByPath` |
| Content Browser 双击 | **Done** | `ActivateAssetFromBrowser` → `TryOpenAsset` |
| File New / Open / Save As | **Done** | `MainMenuWindow::DrawFileMenu` |
| 未保存确认 | **Done** | `EditorUnsavedChangesDialog` |
| `CreateAsset` Scene/Material | **Done** | `AssetManager` + CB Create / New Scene |
| 视口局部 RMB 捕获 | **Done**（hover/focus 门控） | `SceneEditingViewportClient` |
| Material draw flags SkyBox | 已开 flag | `MaterialEditorViewportClient` |
| Preview 世界 SkyBox 实体 | **未做**（S03） | `PreviewScene::BuildDefaultSphereScene` 仍无 SkyBox |
| Abstract 过滤代码 | **有** | `SceneEditor::InitializeComponentTypeNames` |
| Component `ME_CLASS(Abstract)` | **多数缺失**（S05 余量；如 `PrimitiveComponent`） | 各 Component 头文件 |

### 数据流（已接通）

```text
UI (Menu / CB)
  → AssetWorkflowModule::TryOpenAsset / TryOpenSceneByPath / TryCreate*
      → UnsavedChanges dialog when dirty
      → SceneEditor::OpenAsset / OpenSceneByPath / MaterialEditor session
      → CommandStack.Clear (Scene) / Material session swap
```

---

## 3) 方案

### 3.1 总原则

1. **补接线，不另起文档模型** — dirty 仍由 `SceneEditor` / `MaterialEditor` 各自维护。
2. **编排集中** — 切换策略与用户确认放在 `AssetWorkflowModule`（或同级薄 helper），避免散落在 Menu / CB / SubModule。
3. **打开路径唯一** — File、Content Browser、（未来）快捷键都走同一 API。
4. **最小可用创建** — New Scene = 内存空场景 + 提示 Save As；Create Material = 默认图 + 写盘。
5. **切片可合并检查点** — S01+S02 为 merge gate；S03–S05 可后续在 `feat/editor` 或 merge 后迭代。

### 3.2 模块边界

| 模块 | 职责 |
|------|------|
| `AssetWorkflowModule` | **编排**：`TryOpenAsset`、`TryOpenSceneByPath`、`TryCreateScene`、`TryCreateMaterial`、`ConfirmDiscardIfDirty`；调用 SubModule |
| `SceneEditor` | Scene 文档：load/save/dirty；实现 `OpenAsset`；不直接弹 FileDialog |
| `MaterialEditor` | Material session：open/save/dirty；切换前由编排层查询 `Session.Dirty` |
| `MainMenuWindow` | 菜单项 → 调用 `AssetWorkflowModule` |
| `ContentBrowserWindow` | 双击 / Create 上下文 → 调用 `AssetWorkflowModule` |
| `AssetManager` | `CreateAsset<Scene/Material>`、`SaveAsset`（已有） |
| `SceneManager` | `LoadSceneByPath`、`RegisterScene`、`SaveCurrentScene`（已有） |
| `SceneEditingViewportClient` | 局部导航 capture；不改编排层 |
| `PreviewScene` + `MaterialEditorViewportClient` | SkyBox 内容与 draw flags |

**禁止：** 在 `MainMenuWindow` 内直接 `SceneManager::LoadSceneByPath` 并跳过 dirty 检查。

### 3.3 Dirty 确认策略

#### 触发点
- 打开另一 Scene / Material（含 Content Browser、File Open）
- File → New Scene
- Editor `RequestExit`（若当前有 dirty Scene 或 Material）

#### 对话框行为（首版）
- **Save** — 先保存当前文档（Scene：`SaveCurrentScene`；无路径则转 Save As；Material：`SaveActiveMaterial`），成功后再继续原操作。
- **Don't Save** — 丢弃 dirty，继续。
- **Cancel** — 中止，保持当前文档。

#### 实现要点
- 新增 `EditorUnsavedChangesDialog`（ImGui modal）或复用轻量 `ImGui::OpenPopup` 封装；返回三态枚举。
- `ConfirmDiscardIfDirty` 查询：
  - 若 `SceneEditor` 激活且 `IsSceneDirty()` → 提示 Scene
  - 若 `MaterialEditor` 有 open session 且 `Session.Dirty` → 提示 Material
  - 两者皆 dirty 时 **合并一句** 或 **先 Scene 后 Material**（首版：仅检查**即将被替换**的目标——打开 Scene 查 Scene dirty；打开 Material 查 Material dirty；Exit 查两者）

#### 不变量
- 加载新 Scene 后：`CommandStack.Clear()`（已有 `LoadScene` 行为）。
- 打开新 Material 前：`FlushPendingCompile()`（已有 `OnExitMode` 部分逻辑；编排层确保 session 切换一致）。

### 3.4 打开 Scene

#### `SceneEditor::OpenAsset`
```cpp
bool SceneEditor::OpenAsset(const AssetMeta& meta);
bool SceneEditor::OpenSceneByPath(IEditorContext& context, const std::string& projectRelativePath);
```
- 校验 `meta.AssetType == "Scene"`。
- 调用 `SceneManager::LoadSceneByPath(meta.AssetPath)`。
- 成功：`CommandStack.Clear()`、`SyncSelectionWithScene()`、`ClearSceneDirty()`。
- 失败：log + 返回 false，**不**清当前 Scene。

#### File → Open Scene
- `FileDialogService` 过滤 `.mescene`；初始目录 `ProjectContentRoot`。
- 选中后 `AssetWorkflowModule::TryOpenSceneByPath`（内部 dirty 确认）。

#### Content Browser
- `ActivateAssetFromBrowser` → `TryOpenAsset(meta)`（替换 log stub）。

#### `SceneEditor::CanOpenAsset`
- 实现为 `meta.AssetType == "Scene"`（与 Material 对称）。

### 3.5 创建资产（Runtime + Editor）

#### Runtime API（`AssetManager`）

```cpp
template<>
std::shared_ptr<Scene> CreateAsset<Scene>(const std::string& assetName, const std::string& directoryRel);

template<>
std::shared_ptr<Material> CreateAsset<Material>(const std::string& assetName, const std::string& directoryRel);
```

**契约：**
- `assetName`：不含扩展名的显示名；内部生成唯一 `AssetPath`（冲突时后缀 `_1`、`_2`…）。
- 写 `.mescene` / `.mematerial` + `.meta`；刷新 registry；Scene 额外 `SceneManager::RegisterScene`。
- Scene 默认内容：空 Scene（`EnsureRenderScene()`，无 GameObject 或仅可选默认相机——**首版空场景即可**）。
- Material 默认内容：空图或引擎默认材质模板（与 `MaterialEditor::EnsureDefaultSession` 对齐，**实现时二选一并在 Impl 写明**）。

#### Editor 入口
- File → New Scene：`TryCreateAndOpenScene`（内存创建 → 标记 dirty → 可选立即 Save As）。
- Content Browser 空白处右键 **Create → Scene / Material**（扩展现有 `EditorContextMenu` Create 区）。
- 创建成功后：`AssetTreeModel` refresh + 自动打开。

#### Save As（Scene，首版）
- 无 registry 路径的 Scene（New Scene 后）Save / Save As 弹出路径选择。
- 保存成功后：`RegisterScene`、更新 `m_SceneName` / meta 路径、`ClearSceneDirty()`。
- **Out of scope for v1：** 磁盘上重命名已存在资产并更新所有引用。

### 3.6 Material SkyBox（S03）

**根因：** `PreviewScene` 无 `SkyBoxComponent`；`MaterialEditorViewportClient` 未启用 `EnableSkyBox`。

**选用方案 A（推荐）：**
1. `PreviewScene::BuildDefaultSphereScene` 增加 SkyBox（与 `test.mescene` 或 EngineDefault 天空资源一致；可硬编码 GUID 与 sphere mesh 同模式，TODO 改扫描）。
2. `MaterialEditorViewportClient::EndFrame` flags 改为 `EnableSkyBox | EnablePostProcess`（是否与 Scene 视口完全一致可不加 `EnableDebugDraw`）。

**不选用方案 B：** 仅改 flags 不加组件——无 proxy 时可能仍无天空。

### 3.7 Viewport 鼠标约束（S04）

**问题：** `WindowSystem::SetCursorVisible(false)` 作用于 **整个 GLFW 窗口**，RMB 在视口按下后光标在 Inspector 等区域也消失。

**目标行为：**
- 仅当 **Scene 视口 focused + hovered** 且 **RMB down** 时进入 navigating；释放 RMB 或焦点离开视口则退出。
- 进入 navigating 时：禁用光标（或 `GLFW_CURSOR_DISABLED`）；退出时恢复。
- 若 ImGui `WantCaptureMouse` 且指针不在视口 image 上，**不**开始 navigating。

**实现触点：**
- `SceneEditingViewportClient::InputKeys` / `SetNavigating`：增加对 `m_FrameState`（hover/focus）与 ImGui IO 的判断。
- `Editor.cpp` shutdown 路径已 `SetCursorVisible(true)` — 保持。

**非目标：** 改 Playground 式 FPS；多视口 OS 窗口。

### 3.8 Component Abstract 过滤（S05）

1. **反射标注：** 为不可实例化基类加 `ME_CLASS(Abstract)`（至少 `Component` 的抽象中间层，如 `PrimitiveComponent`、`LightComponent` 等——以 `CreateDefaultInstance() == nullptr` 或现有层次为准）。
2. **过滤：** `SceneEditor::InitializeComponentTypeNames` 跳过 `classInfo->HasSpecifier(ClassSpecifier::Abstract)`。
3. **上下文菜单：** `SceneContextMenuProviders` Add Component 列表同步过滤。

**图标：** defer；下拉仍用 `GetShortTypeName`。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. `AssetWorkflowModule` 编排 | 改动集中、易测、CB/Menu 复用 | 模块略增职责 | **选用** |
| B. 新建 `EditorDocumentService` | 职责更单一 | 多一个服务、与 AssetWorkflow 重叠 | 不选用 |
| C. 各 SubModule 自行弹 dirty 对话框 | 分散 | 行为不一致、难维护 | 不选用 |
| SkyBox A：PreviewScene 加组件 | 与运行时一致 | 多一个 GUID 依赖 | **选用** |
| SkyBox B：仅改 draw flags | 改动最小 | 可能无效 | 不选用 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| New Scene 未 Save As 即切换 | 数据丢失 | dirty 确认 + 标题 `Untitled *` |
| `RegisterScene` 与 `AssetName` 不一致 | Save 失败 | Create/Save As 单一路径写 registry；单测或 smoke 覆盖 |
| 鼠标 capture 与 ImGui 抢焦点 | 面板难点击 | hover/focus 双条件；RMB release 必恢复 |
| Abstract 标注遗漏 | 仍出现基类 | 以 `CreateDefaultInstance` 为二次过滤（可选防御） |
| Material 编译中切换 | 崩溃/脏图 | 切换前 `FlushPendingCompile` |

---

## 6) 验收标准（Feature Done）

- [x] Content Browser 双击 Scene/Material 可打开对应编辑器
- [x] File Open / New Scene / Save / Save As（Scene）可用
- [x] Content Browser 或菜单可创建 Scene、Material 并自动打开
- [x] 切换 dirty 文档时确认框三选项行为正确；Cancel 不改变当前文档
- [ ] Material 预览可见 SkyBox（**S03** — flag 已开，缺 Preview 实体）
- [x] Scene 视口 RMB 导航局部捕获（**S04**）
- [ ] Add Component 列表无 Abstract 基类（**S05** — 需补标注）
- [x] Merge gate S01–S02 已合入 `master`
- [ ] 剩余切片完成后 `.\scripts\verify.ps1` + 目视收口 → Feature **Done**

---

## 7) 切片索引

| Slice | 摘要 | 优先级 | 状态 |
|-------|------|--------|------|
| S00 | Content Browser 双击 → `TryOpenAsset` | 高 | **Done** |
| S01 | Scene 打开 + File Open + dirty 确认 | 高 | **Done** |
| S02 | `CreateAsset` + New/Create Scene·Material | 高 | **Done** |
| S03 | Material Preview SkyBox | 中 | Remaining |
| S04 | Viewport 局部鼠标捕获 | 中 | **Done** |
| S05 | Abstract Component 过滤（含标注） | 低 | Partial（过滤有，标注缺） |

详见 [Implementation](./ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md)。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Registry 占位登记（双轨 backlog 审批稿） |
| 2026-09-01 | 扩写为正式 Design Spec（编排层、API、切片、风险） |
| 2026-09-03 | 对照 `master` 收口：S00–S02/S04 Done；S03/S05 余量；Branch=`master` |
