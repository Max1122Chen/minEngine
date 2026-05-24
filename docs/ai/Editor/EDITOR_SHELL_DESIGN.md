# Editor Shell — 设计草稿

Last updated: 2026-05-24  
Status: **草稿 v2（命名与模块划分已按 2026-05-24 讨论修订）**  
父文档：[Editor 平台化规划](./EDITOR_PLATFORM_PLAN.md)  
前置：[架构复盘](./EDITOR_ARCHITECTURE_REVIEW.md)

---

## 0) 一句话

**`class Editor`**（保留类名）是 Application 入口与 **模块注册表**；领域编辑由 **`EditorSubModule` 派生类 `XXXEditor`**（SceneEditor、MaterialEditor…）承担；**Console、OpenAsset** 等横切能力由 **`EditorServiceModule`** 承担（**不是** SubModule）。**GUIManager 只做窗口工厂与注册**，窗口归属与布局由各 Module 管理。

---

## 1) 模块分类（核心修订）

```text
                         ┌─────────────────────────┐
                         │  Editor : Application    │
                         │  bootstrap · 模块注册表   │
                         │  ActiveEditorModule*     │
                         │  Tick 编排               │
                         └───────────┬─────────────┘
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           │                         │                         │
           ▼                         ▼                         ▼
   EditorSubModule            EditorServiceModule         (future)
   (领域编辑器，互斥激活)       (常驻服务，无互斥激活)
           │                         │
   ┌───────┼───────┐         ┌───────┴────────┐
   ▼       ▼       ▼         ▼                ▼
SceneEditor MaterialEditor  ConsoleModule   AssetWorkflowModule
AnimationEditor …           (Console UI)    (OpenAsset 路由)
                              ContentBrowserModule* …
```

| 类型 | 基类 | 派生命名 | 激活 | 选择 |
|------|------|----------|------|------|
| 领域编辑器 | `EditorSubModule` | `SceneEditor`、`MaterialEditor` | **互斥** `Activate` | **模块内自建** |
| 横切服务 | `EditorServiceModule` | `ConsoleModule`、`AssetWorkflowModule` | 常驻 / 无 Activate | 各自域（若有） |
| Shell | `Editor` | — | — | **不持有** 全局 Selection enum |

\* Content Browser 后置，归 `EditorServiceModule` 或独立 `ContentBrowserModule`。

**为何不共用 `EditorSubModule` 做 Console/OpenAsset：**  
SubModule 语义是「进入一种编辑域、切 layout、抢焦点」；Console 不应与 Scene 互斥。若强行继承同一基类，Activate/Deactivate 语义会污染服务模块。**建议 sibling 基类**，都注册在 `Editor` 上。

---

## 2) 目标与非目标

### 目标

| # | 目标 |
|---|------|
| G1 | 保留 **`class Editor`** 类名；拆掉 God Object 业务 API |
| G2 | **`EditorSubModule` + `XXXEditor`** 命名；Material 现有类对齐接口 |
| G3 | **选择集 per SubModule**；Inspector 向 **ActiveSubModule** 要「当前详情」 |
| G4 | 输入路由对标 UE：**Command 表 + Focus 链 + Viewport/Mode 优先** |
| G5 | **`EditorServiceModule`**：Console、OpenAsset 等不挂 Editor 公有 API |
| G6 | **GUIManager** 仅 Register/Create；Module 管窗口生命周期与 layout |
| G7 | CommandHistory 挂 Editor 或独立 UndoModule（P3 stub） |

### 非目标（E0）

- Inspector Drawer 实现（E1）
- 全局 `SelectionKind` 大 enum（**明确不做**）
- Content Browser UI、AssetManager 变更 API

---

## 3) `class Editor`（Shell，保留类名）

```cpp
class Editor : public Application
{
public:
    void Initialize(int argc, char** argv) override;
    void Run() override;
    void Shutdown() override;

    // 模块注册
    void RegisterSubModule(std::unique_ptr<EditorSubModule> module);
    void RegisterServiceModule(std::unique_ptr<EditorServiceModule> module);

    bool ActivateSubModule(std::string_view moduleId);  // "Scene", "Material"
    EditorSubModule* GetActiveSubModule() const;
    EditorSubModule* FindSubModule(std::string_view moduleId) const;

    template<typename T>
    EditorServiceModule* GetServiceModule();  // 或 FindServiceModule(id)

    EditorGUIManager& GetGUIManager();   // 工厂/注册 only
    IEditorContext& GetContext();        // 窄接口，供 Module 使用

    // Project 会话（可后续迁入 ProjectServiceModule）
    bool OpenProject(const std::string& projectPath);
    void CloseProject();

private:
    void TickEditor(float deltaTime);
};
```

**Editor 上不暴露：** `SelectGameObject`、`OpenAsset`、`GetConsole` — 由 SubModule / ServiceModule 提供。

---

## 4) `EditorSubModule` 与 `XXXEditor`

```cpp
class EditorSubModule
{
public:
    virtual ~EditorSubModule() = default;

    virtual std::string_view GetModuleId() const = 0;      // "Scene"
    virtual std::string_view GetDisplayName() const = 0;

    virtual void Register(IEditorContext& editor) = 0;     // 向 GUIManager 注册窗口
    virtual void Shutdown() = 0;

    virtual bool CanActivate() const { return true; }
    virtual void OnActivate(IEditorContext& editor) = 0;
    virtual void OnDeactivate(IEditorContext& editor) = 0;
    virtual void Tick(float deltaTime) = 0;

    // Layout：本 Module 拥有的 dock 预设与窗口可见性
    virtual void ApplyDefaultLayout(IEditorContext& editor) = 0;

    // 选择 + Inspector 桥接（见 §5）
    virtual IEditorInspectorSource* GetInspectorSource() = 0;

    // 输入：本 Module 的 Command 表 + Viewport 处理（见 §7）
    virtual void RegisterCommands(IEditorCommandRegistry& registry) = 0;
    virtual bool RouteViewportInput(EditorViewportClient& client, /* key event */) { return false; }

    // Asset 入口（可选）
    virtual bool CanOpenAsset(const AssetMeta& meta) const { return false; }
    virtual bool OpenAsset(const AssetMeta& meta) = 0;
};
```

**派生类（命名拍板）：**

| 类名 | ModuleId | 迁出来源 |
|------|----------|----------|
| `SceneEditor` | `Scene` | 现 `Editor` GO/Scene/Viewport/Hierarchy |
| `MaterialEditor` | `Material` | 现 `MaterialEditor` + Material 窗口（可 wrapper 后合并） |

未来：`AnimationEditor`、`SpriteEditor` — **不加 Sub 后缀**。

---

## 5) 选择模型：Per-Module，非全局 SelectionService

### 5.1 问题（用户指出）

全局 `SelectionService` + `EditorSelectionKind`（GO / Asset / Bone / Joint / …）会 **组合爆炸**。骨骼、Sprite 帧、Material 图节点等选择语义 **只属于对应 Editor**。

### 5.2 拍板方案

- **每个 `EditorSubModule` 自建选择状态**（`SceneEditorSelection`、`MaterialEditorSelection`…）。
- **Shell 不解释** 选择内容；只记录 **`ActiveSubModule`**。
- **Inspector**（E1）只认接口：

```cpp
class IEditorInspectorSource
{
public:
    virtual ~IEditorInspectorSource() = default;
    virtual bool HasInspectableSelection() const = 0;
    virtual void DrawInspector(IInspectorUI& ui) = 0;  // 或 PopulateDrawers()
};
```

- `SceneEditor::GetInspectorSource()` → 内部 GO/Component 选择。
- `MaterialEditor::GetInspectorSource()` → Material 资产 + Graph Node。

**ServiceModule 若需 Inspector**（如 Content Browser 选中 Asset）：  
`AssetWorkflowModule` 实现 `IEditorInspectorSource`；当 Content Browser 面板 **聚焦** 时，Inspector 的「当前 Source」= Focus 栈顶（见 §7），**不必**把 Asset 选进 SceneEditor。

### 5.3 与 UE 对照

| UE | minEngine |
|----|-----------|
| `USelection`（Actor/Component 世界选择） | `SceneEditor` 内选择 |
| Content Browser 选中资产 | `AssetWorkflowModule` / ContentBrowserModule |
| Persona 骨骼选择 | 未来 `AnimationEditor` 内选择 |
| Details 面板随 **Toolkit 焦点** 变 | Inspector 随 **InspectorSource 焦点链** 变 |

---

## 6) `EditorServiceModule`（横切服务）

```cpp
class EditorServiceModule
{
public:
    virtual ~EditorServiceModule() = default;
    virtual std::string_view GetModuleId() const = 0;

    virtual void Register(IEditorContext& editor) = 0;  // 注册 Console 等窗口
    virtual void Shutdown() = 0;
    virtual void Tick(float deltaTime) {}               // 可选

    // 无 OnActivate / OnDeactivate
};
```

| 模块 | 职责 |
|------|------|
| **`ConsoleModule`** | Console 窗口、日志订阅；Register 时 `GUIManager.RegisterWindow` |
| **`AssetWorkflowModule`** | `OpenAsset(meta)` 路由到 `CanOpenAsset` 的 SubModule；AssetOpener 注册表 |
| **`MainMenuModule`**（可选） | 菜单栏；或仍 Shared 窗口由 Editor 在 Register 阶段拉起 |
| **`ContentBrowserModule`**（后置） | 浏览 + 选中；Inspector 通过本模块的 InspectorSource |

**OpenAsset 不在 `Editor::OpenAsset`：**

```cpp
// AssetWorkflowModule
bool OpenAsset(const AssetMeta& meta);  // 遍历 SubModule + 已注册 Opener
```

Content Browser / 菜单 **只调 ServiceModule**，不碰 Editor 公有面。

---

## 7) 输入路由 — UE 做法与 minEngine 映射

### 7.1 UE 分层（简表）

| 机制 | 作用 |
|------|------|
| **`FUICommandInfo` + `TCommands<>`** | 声明命令与默认快捷键（模块 Startup 注册） |
| **`FUICommandList`** | 命令 → `Execute` / `CanExecute` 绑定；**可多份**（Global / Toolkit / Mode） |
| **Level Editor Global Actions** | 关卡全局快捷键（`FLevelEditorModule::GetGlobalLevelEditorActions()`） |
| **`FEditorModeTools::InputKey`** | Viewport 按键先走 **ToolsContext / InputRouter**，再 **FEdMode** |
| **Focus / Toolkit** | 菜单、Details 随 **当前 Asset Editor Toolkit** 变；各 Toolkit 自带 CommandList |
| **Slate 焦点** | 控件级输入由 Slate 焦点链处理（ImGui 等价：窗口/Viewport Hovered+Focused） |

要点：**不是** 一个全局 `InputRouter` 枚举所有快捷键；而是 **多 CommandList + Viewport 链式 Route + 焦点上下文**。

### 7.2 minEngine 建议（E0）

```text
EditorCommandRegistry（或 EditorInputHub）
  ├─ GlobalCommandList        ← Editor / MainMenu：Exit、Save Project
  ├─ ActiveSubModule Commands ← Activate 时 MapAction，Deactivate 卸载
  └─ ServiceModule Commands   ← Console 切换等

每帧 ProcessInput（Run loop，ImGui NewFrame 后）：
  1. 若 ImGui WantCaptureKeyboard 且非 Viewport 白名单 → 跳过 Global 快捷键
  2. Focused ViewportClient → ActiveSubModule::RouteViewportInput（UE FEdMode 等价）
  3. 否则 → ActiveSubModule CommandList
  4. 否则 → GlobalCommandList
```

**不在 Editor 类上挂 `ProcessInput` 业务**；`EditorInputHub` 由 ServiceModule 或 Editor 私有成员持有，SubModule 在 `RegisterCommands` 注册。

**与 ImGui：** Viewport 内 3D 操作需在 `WantCaptureKeyboard` 时对 **focused viewport** 例外（与现 SceneEditingViewportClient 行为一致）。

---

## 8) GUIManager：仅工厂与注册

### 8.1 职责切分

| 组件 | 职责 |
|------|------|
| **`EditorGUIManager`** | `RegisterWindow(unique_ptr<EditorWindow>)` → id 索引；**不**决定 Suite 可见性、**不** BuildDockLayout |
| **`EditorSubModule`** | `Register()` 时注册本域窗口；`ApplyDefaultLayout()` 内调 `ImGui::DockBuilder*` 或 LayoutPreset |
| **`EditorServiceModule`** | 注册 Console 等 Shared 窗口 |
| **`Editor`** | 持有 GUIManager；`ActivateSubModule` 时调用新 Module 的 `ApplyDefaultLayout` |

### 8.2 窗口归属

```cpp
// SceneEditor::Register
void SceneEditor::Register(IEditorContext& ctx)
{
    auto& gui = ctx.GetGUIManager();
    gui.RegisterWindow(std::make_unique<HierarchyWindow>(ctx, *this));
    gui.RegisterWindow(std::make_unique<SceneEditingViewportWindow>(ctx, *this));
    m_RegisteredWindowIds = { "hierarchy", "viewport", ... };
}

void SceneEditor::ApplyDefaultLayout(IEditorContext& ctx)
{
    BuildSceneEditingDockLayout(dockspaceId, m_RegisteredWindowIds);
}
```

**废弃：** `EditorGUIManager::SetUIMode` 隐藏窗口；改为 **ActiveSubModule 的 layout + 各 Module 声明哪些窗口在本域显示**。Shared 窗口（Console）由 ServiceModule 注册且 **不参与** SubModule Deactivate 关闭。

---

## 9) `IEditorContext`（Module 窄接口）

```cpp
class IEditorContext
{
public:
    virtual Engine& GetEngine() = 0;
    virtual EditorGUIManager& GetGUIManager() = 0;
    virtual EditorSubModule* GetActiveSubModule() = 0;
    virtual AssetWorkflowModule& GetAssetWorkflow() = 0;
    virtual ConsoleModule& GetConsole() = 0;
    // P3: virtual EditorCommandHistory& GetCommandHistory() = 0;
};
```

Module **不** 持有 `Editor&` 全量引用。

---

## 10) Activate 流程（修订）

```text
Editor::ActivateSubModule("Material")
  1. old->OnDeactivate(ctx)
  2. m_Active = materialEditor
  3. materialEditor->OnActivate(ctx)
  4. materialEditor->ApplyDefaultLayout(ctx)   // Module 自管 dock
  5. UpdateWindowTitle()  // Editor 或 ActiveSubModule 提供标题片段
```

**不** Clear 其他 Module 的内部选择（切回 Scene 时仍可保留上次 GO 选）；Inspector 只读 **Active** Module 的 InspectorSource。

---

## 11) 迁移分期（E0）

| 阶段 | 交付 |
|------|------|
| E0.1 | `EditorSubModule`、`EditorServiceModule`、`IEditorContext`；`ConsoleModule`、`AssetWorkflowModule` 空壳 |
| E0.2 | `SceneEditor` 迁 GO/Scene API；内嵌 `SceneEditorSelection` + InspectorSource |
| E0.3 | `MaterialEditor` 实现 `EditorSubModule`；`ActivateSubModule` 替代 `EditorUIMode` |
| E0.4 | GUIManager 瘦身；layout 迁入各 Module |
| E0.5 | `EditorInputHub` + Command 注册；Viewport Route |
| E0.6 | 删 Editor 上已迁 API |

---

## 12) E0 验收标准（修订）

- [ ] **`class Editor` 类名保留**
- [ ] 领域模块：**`EditorSubModule` → `SceneEditor` / `MaterialEditor`**
- [ ] Console、OpenAsset 经 **ServiceModule**，非 `Editor::` 公有方法
- [ ] **无** 全局 `SelectionKind` 大 enum；Inspector 读 ActiveSubModule 的 `IEditorInspectorSource`
- [ ] GUIManager **无** `SetUIMode` 式业务；layout 由 SubModule 应用
- [ ] 新增 `AnimationEditor` 只需 Register SubModule，**不改 Editor 源码**

---

## 13) 开放问题

| 问题 | 倾向 |
|------|------|
| ServiceModule 基类命名 | **`EditorServiceModule`**（与 SubModule 并列，不混用） |
| MainMenu 归谁 | `MainMenuModule` 或 Editor bootstrap 一次性注册 |
| Inspector 窗口归谁 | **Shared Service** 或独立 `InspectorModule`；Source 仍来自 Focus/ActiveSubModule |
| Undo 归谁 | `UndoServiceModule` 或 Editor 持有栈；SubModule 提交 Command |
| 切 SubModule 是否保留 inactive 模块选择 | **保留**（各 Module 私有） |

---

## 14) 参考

- [EDITOR_ARCHITECTURE_REVIEW.md](./EDITOR_ARCHITECTURE_REVIEW.md)
- UE：`FUICommandList`、`FEditorModeTools::InputKey`、`FAssetEditorToolkit`
- 现码：`Editor.h`、`MaterialEditor.h`、`EditorGUIManager.cpp`

---

## 15) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-24 | v1：EditorShell / ISubEditor / 全局 Selection |
| 2026-05-24 | v2：保留 Editor；EditorSubModule + XXXEditor；EditorServiceModule；Per-Module 选择；GUIManager 瘦身；UE 输入对照 |
