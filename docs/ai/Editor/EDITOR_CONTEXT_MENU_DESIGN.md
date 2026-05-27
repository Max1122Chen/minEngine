# Editor 上下文菜单系统 — 可扩展设计（重置版）

Last updated: 2026-05-27  
Status: **v2 已拍板**；**M0 Done**；**M1 CB 收口**（Delete / Import / Refresh；Reveal、Rename **暂缓**，见 §6.1、§15.4）  
关联推进：[`EDITOR_TASK_ROLLOUT_2026-05-27.md`](./EDITOR_TASK_ROLLOUT_2026-05-27.md)  
外部参考：[`docs/external/editor_context_menu_system_design.md`](../../external/editor_context_menu_system_design.md)、[`docs/external/ue_editor_context_menu_design.md`](../../external/ue_editor_context_menu_design.md)  
父路线：[`PLATFORM_ROADMAP.md`](../Platform/PLATFORM_ROADMAP.md) §10

### v2 审阅要点（请重点看）

| # | 拍板项 | 结论 |
|---|--------|------|
| 1 | 对外类型 | **`EditorContextMenuSystem`**（组合 Registry + Builder） |
| 2 | 归属 | **`Editor` 成员 + `IEditorContext::GetContextMenu()`**，与 `EditorCommandStack` 同级 |
| 3 | 不做什么 | **不**继承 `EditorServiceModule`；**不**进 `RegisterModules()` |
| 4 | 窗口调用 | 仅 `Populate*Context` + `GetContextMenu().BuildAndDraw` |
| 5 | 扩展注册 | `GetContextMenu().GetRegistry().Register(...)`（测试 / 插件） |
| 6 | MVP 范围 | M0–M3：骨架 → CB → Hierarchy/Inspector → 清理 Tools FileDialog |
| 7 | M0 状态 | **Done**（2026-05-27） |
| 8 | M1 CB 范围 | **Delete + Import + Refresh**；Reveal / Rename **不交付**（代码待删，见 §15.4） |

---

## 0) 设计立场（重置说明）

**不再采用**「单 struct + enum ActionId + 中央 switch 路由」的过渡方案。  
**采用** 现代编辑器通用模型（与 UE `ToolMenus` / `FToolMenuContext`、外部讨论稿一致）：

```text
右键命中
  → 窗口构建 EditorMenuContext（异构 Typed Context 对象集合）
  → EditorContextMenuSystem::BuildAndDraw(ctx)   // 对外唯一入口
        ├─ EditorActionRegistry::Query
        └─ EditorMenuBuilder::Draw
  → IEditorAction::Execute → IEditorCommand → EditorCommandStack
```

**核心句：** 右键菜单 ≠ 窗口里的 `if (entity) MenuItem("Delete")`；右键菜单 = **Context 上的 Action 查询结果**。

---

## 1) 归属与生命周期（v2 拍板）

### 1.1 对外：一个系统门面，对内：组合而非揉类

| 层级 | 类型 | 职责 |
|------|------|------|
| **对外 API** | `EditorContextMenuSystem` | 窗口只调 `BuildAndDraw`；可选 `RegisterBuiltInActions` |
| **对内（组合）** | `EditorActionRegistry` | 注册 / Query / FindById；**可单测，不碰 ImGui** |
| **对内（组合）** | `EditorMenuBuilder` | Query 结果 → Section / MenuItem；**纯 UI** |

**禁止：** 把 Registry 与 Builder 合并成「一个既注册又画菜单」的大类（难测、难扩）。

**推荐：** `EditorContextMenuSystem` 持有 `Registry` + `Builder`（`unique_ptr` 或成员），构造时注入依赖。

```cpp
class EditorContextMenuSystem
{
public:
    void RegisterBuiltInActions();  // Editor 启动时调用一次
    void Shutdown();                // 清空 Provider / 动态注册（工程关闭时）

    void BuildAndDraw(IEditorContext& editor, const EditorMenuContext& ctx);

    EditorActionRegistry& GetRegistry();
    const EditorActionRegistry& GetRegistry() const;

private:
    EditorActionRegistry m_Registry;
    EditorMenuBuilder m_Builder;  // 构造时绑定 m_Registry
};
```

### 1.2 不继承 `EditorServiceModule`（与 CommandStack 同级）

**决策：** 上下文菜单是**横切基础设施**，不是带 `GetModuleId()` 的可选功能模块。

| 对比 | `EditorServiceModule` 典型 | 上下文菜单系统 |
|------|---------------------------|----------------|
| 例子 | `ContentBrowserModule`、`AssetWorkflowModule` | `EditorCommandStack`、`EditorInputHub` |
| 特征 | 领域状态 / 编排 / 可 Tick | 全窗口共用、无独立 Dock 窗 |
| 访问 | `IEditorContext::GetContentBrowser()` 等 | **`IEditorContext::GetContextMenu()`** |

**不采用** `EditorContextMenuModule : EditorServiceModule` 的原因（简述）：

- 易被理解成「可插拔可选模块」，但 CB/Hierarchy/Inspector 都依赖它；
- `Editor` 已有 `CommandStack` / `InputHub` 直接成员 + Context getter，再增 ServiceModule 多一层「该走 Module 还是 getter」；
- 本系统通常**不需要** `Tick`。

**生命周期挂钩：**

```text
Editor::PostInitialize() / OpenProject 后
  → m_ContextMenu.RegisterBuiltInActions()

Editor::CloseProject() / Shutdown
  → m_ContextMenu.Shutdown()
```

与 `ApplyCommandStackSettingsFromProject()` 同级，**不**进入 `RegisterModules()` 的 Service 向量。

### 1.3 `IEditorContext` 访问点（唯一运行时入口）

```cpp
class IEditorContext
{
    // …
    virtual EditorContextMenuSystem& GetContextMenu() = 0;
    virtual const EditorContextMenuSystem& GetContextMenu() const = 0;
};
```

`Editor` 持有成员 `EditorContextMenuSystem m_ContextMenu;`，实现转发。

**窗口禁止**直接调用 `EditorMenuBuilder::Draw` 或自行 `Registry.Query`；默认路径仅 `GetContextMenu().BuildAndDraw(...)`。扩展注册可用 `GetContextMenu().GetRegistry().Register(...)`。

扩展注册（插件 / 子模块）可通过 `GetContextMenu().GetRegistry().Register(...)` 暴露。

### 1.4 代码布局

```text
minEngine/Editor/src/ContextMenu/
  EditorMenuContext.h/.cpp
  EditorActionRegistry.h/.cpp
  EditorMenuBuilder.h/.cpp
  EditorContextMenuSystem.h/.cpp
  Actions/                    # 内置 IEditorAction 实现
    DeleteAction.h/.cpp
    …
  Contexts/                   # Typed Context 对象
    ContentBrowserMenuContext.h
    HierarchyMenuContext.h
    …
```

---

## 2) 与 Unreal Editor 的对照（学习，不抄全量）

| UE 概念 | minEngine 对应（本设计） | 说明 |
|---------|-------------------------|------|
| `FToolMenuContext` | `EditorMenuContext` | 异构对象容器，非单一 enum |
| `UContentBrowserAssetContextMenuContext` 等 | `FContentBrowserMenuContext` 等 **小型 Typed Context** | 每窗口/场景一块，避免胖 struct |
| `UToolMenus` | `EditorContextMenuSystem`（统筹） | 对外单一入口，对标 Editor 级子系统 |
| `FToolMenuSection` | `EditorMenuBuilder` + `EditorMenuSection` | UI 只负责排版，不含业务 |
| `FUICommand` / `FUICommandList` | `EditorUICommand` + `EditorUICommandList`（二期） | 菜单、工具栏、快捷键共用同一命令身份 |
| `CanExecute` on Action | `IEditorAction::CanExecute` | 可用性统一判断 |
| `Execute` → Transaction | `IEditorAction::Execute` → `IEditorCommand` | 与现有 `EditorCommandStack` 对齐 |
| `TypedElementFramework` | **未来** `IEditorElementHandle` + Capability | 本设计预留，MVP 不实现 |

UE 要点：**菜单依赖 Context，不依赖窗口类**；窗口只负责 `AddContext` + `BeginPopup` + `BuildMenu`。

---

## 3) 分层架构

```text
┌─────────────────────────────────────────────────────────────┐
│  Windows (CB / Hierarchy / Inspector / 未来 Material Graph) │
│    Populate*MenuContext() → EditorMenuContext               │
│    ImGui::BeginPopupContext*                                │
│    context.GetContextMenu().BuildAndDraw(context, menuCtx)  │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│  EditorContextMenuSystem（Editor 成员，IEditorContext 暴露）    │
│    BuildAndDraw → Builder.Draw(Registry.Query(ctx))           │
│    RegisterBuiltInActions / Shutdown                          │
└───────────────┬─────────────────────────────┬─────────────────┘
                │                             │
    ┌───────────▼──────────┐      ┌───────────▼──────────┐
    │ EditorMenuBuilder     │      │ EditorActionRegistry  │
    │ （纯 UI）              │      │ Register / Query      │
    └───────────┬──────────┘      └───────────┬──────────┘
                │                             │
                └──────────────┬──────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│  IEditorAction → IEditorCommand → EditorCommandStack（已有）   │
└─────────────────────────────────────────────────────────────┘
```

---

## 4) EditorMenuContext — 异构 Context 容器

### 4.1 为什么不用「一个大 struct」

错误方向（技术债）：

```cpp
struct EditorContextMenuContext {
    GameObject* GO;
    const AssetMeta* Asset;
    Component* Comp;
    // … 每加一种右键场景就改这里
};
```

与 UE 一致的做法：**多个小型 Typed Context Object 的组合**。

### 4.2 容器 API（概念）

```cpp
class EditorMenuContext
{
public:
    template<typename TContext>
    void Add(std::shared_ptr<TContext> ctx);  // 或 unique_ptr + 类型擦除

    template<typename TContext>
    TContext* Find() const;

    template<typename TContext>
    const TContext* Find() const;

    template<typename TContext>
    std::vector<TContext*> FindAll();            // 多选（M0：按值返回 vector，非 span）

    template<typename TContext>
    std::vector<const TContext*> FindAll() const;
};
```

**M0 行为：** `Find` / `FindAll` 对同类型多条 `Add` 均可见；`Find` 返回**第一条**。

### 4.3 首批 Typed Context（MVP 范围）

| Typed Context | 提供方 | 典型字段 |
|---------------|--------|----------|
| `ContentBrowserMenuContext` | CB 树/Tile/空白处 | `SelectedAssets`, `CurrentDirectoryRel`, `bClickedOnFolder`, `HitKind` |
| `HierarchyMenuContext` | Hierarchy | `SelectedGameObjects`, `bClickedEmpty` |
| `SceneInspectorMenuContext` | Scene Inspector | `InspectedObject`, `HoveredComponent`, `SelectionKind` |
| `EditorSelectionMenuContext`（可选） | 各窗口共用 | 与全局 Selection 同步的句柄，减少重复 |

**规则：** Action 只 `Find<ContentBrowserMenuContext>()`，**禁止** Action 内 `#include "ContentBrowserWindow.h"`。

### 4.4 与「能力（Capability）」的演进

| 阶段 | 模型 |
|------|------|
| **MVP** | Typed Context + 类型判断（`ctx.Find<HierarchyMenuContext>()`） |
| **Phase 2** | Context 内附带 `IDeletable` / `IRenameable` 接口指针 |
| **远期** | `EditorElementHandle` + 能力查询（对标 UE TypedElement，不阻塞 MVP） |

---

## 5) IEditorAction — 可注册、可扩展的动作

```cpp
class IEditorAction
{
public:
    virtual ~IEditorAction() = default;

    virtual EditorActionId GetId() const = 0;           // 稳定 ID，供 UICommand / 快捷键
    virtual const char* GetLabel(const EditorMenuContext& ctx) const = 0;
    virtual EditorMenuSectionId GetSection() const = 0; // "Asset" / "Edit" / "Create"

    virtual bool IsVisibleInMenu(const EditorMenuContext& ctx) const = 0;

    virtual bool CanExecute(const EditorMenuContext& ctx) const = 0;
    virtual const char* GetDisabledReason(const EditorMenuContext& ctx) const = 0;

    virtual void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const = 0;
};
```

**执行约定：** `Execute` **不**直接改 Scene/Asset；应 `editor.GetCommandStack().Execute(MakeCommand(...))` 或调用已有 `SceneEditor::Submit*`（内部已入栈）。**M0：** `Execute` 为 `const` 成员（Action 无实例可变状态）。

**M0 已提供：** `GetSortOrder()` 默认 `0`；`EditorActionId` 枚举含 MVP 全部 ID（见 `EditorActionIds.h`），尚未注册对应 Action 类。

### 5.1 动态 Action（Provider）

对标 UE「Add Component 子菜单」与外部讨论稿 `ActionProvider`：

```cpp
using EditorActionProvider = std::function<void(
    IEditorContext& editor,
    const EditorMenuContext& ctx,
    EditorMenuBuilder& builder)>;

// Registry 在 Query 静态 Action 之后，再调用已注册的 Provider
```

示例：`RegisterComponentAddProvider()` 遍历反射注册的 Component 类，向 builder 追加临时 `AddComponentAction(class)`。

---

## 6) EditorActionRegistry

```cpp
class EditorActionRegistry
{
public:
    void Register(std::unique_ptr<IEditorAction> action);  // 同 EditorActionId 忽略重复
    void RegisterProvider(EditorActionProvider provider);

    std::vector<const IEditorAction*> Query(const EditorMenuContext& ctx) const;
    void InvokeProviders(IEditorContext& editor, const EditorMenuContext& ctx,
                         EditorMenuBuilder& builder) const;

    const IEditorAction* FindById(EditorActionId id) const;

    void Clear();
    void ClearProviders();
};
```

**扩展性核心：** 新增「删除资产」「重命名文件夹」等 → **只注册新 Action 类**，**不改** `ContentBrowserWindow.cpp` 菜单分支。

**排序：** 每个 Action 带 `SortOrder` 或按 `Section` + `SortOrder` 排序；`Query` 返回已过滤、已排序列表。

**Query（方案 A，M1+）：** 返回 `IsVisibleInMenu(ctx)==true` 的 Action；`CanExecute` 由 `EditorMenuBuilder` 控制灰显与 Tooltip。

### 6.1 MVP 内置 Action 清单

> 由 `EditorContextMenuSystem::RegisterBuiltInActions()` 统一注册，而非分散在各窗口。  
> `EditorActionId` 枚举可预留未实现项；**未注册 = 菜单中不出现**。

#### Content Browser（M1 已交付 / 计划交付）

| EditorActionId | Section | 状态 | 说明 |
|----------------|---------|------|------|
| `Delete` | Edit | **M1 交付** | `AssetWorkflowModule::DeleteSelectedAsset` |
| `ImportAsset` | Asset | **M1 交付** | `ImportAssetDialog(CurrentDirectoryRel)`；目录/空白可点，资产项灰显（方案 A） |
| `Refresh` | Asset | **M1 交付** | 重建 `AssetTreeModel` |
| `RevealInExplorer` | Asset | **暂缓** | 需**跨平台** Shell API；不采用 Win32-only 临时实现（见 §15.4） |
| `Rename` | Edit | **暂缓** | CB 资产重命名需与 Hierarchy 体验 + `IEditorCommand`/Undo 统一设计后再做（见 §15.4） |

#### Hierarchy / Inspector（M2 已交付）

| EditorActionId | Section | 状态 | 说明 |
|----------------|---------|------|------|
| `Delete` | Edit | **M2 交付** | `DeleteGameObjectCommand`；与 CB 共用 `EditorEditActions::DeleteEditorAction` |
| `Rename` | Edit | **M2 交付** | Hierarchy：`RequestBeginRenameGameObject` + 内联；Inspector：`BeginRenameGameObjectInInspector` |
| `Duplicate` | Edit | **M2 占位** | 菜单可见，`CanExecute=false`（Command 未实现） |
| `FocusInViewport` | View | **M2 占位** | 菜单项「Frame in Viewport」；Viewport `FocusSelection` 未接线 |

#### 其它

| EditorActionId | 说明 |
|----------------|------|
| `OpenAsset` | 依赖 P7 路由；注册后 `CanExecute` 控制 |

**暂缓项仍保留在 `EditorActionIds.h` 中**，便于 M2+ 注册，避免改枚举顺序。

---

## 7) EditorMenuBuilder — 纯 UI

```cpp
class EditorMenuBuilder
{
public:
    explicit EditorMenuBuilder(EditorActionRegistry& registry);

    void Draw(IEditorContext& editor, const EditorMenuContext& ctx);

private:
    EditorActionRegistry& m_Registry;
    void DrawAction(IEditorContext& editor, const IEditorAction& action,
                    const EditorMenuContext& ctx, bool enabled);
};
```

**说明：** `BuildAndDraw` 落在 `EditorContextMenuSystem`；Builder 负责 `Query` → 绘制静态项 → `InvokeProviders`。

**窗口侧唯一模板：**

```cpp
if (ImGui::BeginPopupContextItem())
{
    EditorMenuContext menuCtx;
    PopulateContentBrowserContext(menuCtx, /* hit data */);

    m_Context.GetContextMenu().BuildAndDraw(m_Context, menuCtx);
    ImGui::EndPopup();
}
```

Hierarchy **不知道** 有哪些菜单项；只 `PopulateHierarchyContext`。

### 7.1 Section 排版（M0：扁平菜单，非子菜单）

| `EditorMenuSectionId` | 显示名（`GetEditorMenuSectionDisplayName`） |
|-----------------------|---------------------------------------------|
| `Edit` | Edit |
| `Asset` | Asset |
| `Create` | Create |
| `View` | View |

**M0 实现：** 同一右键菜单内**扁平** `MenuItem`；`Section` 变化时插入 `ImGui::Separator()`（**不**使用 `BeginMenu` 嵌套 Section 子菜单）。同一 Section 内按 `SortOrder` 排列。

未来 `Create` 下可由 **Provider** 生成子项或子菜单（M4）。

---

## 8) EditorUICommand（二期，与菜单同源）

为避免菜单、工具栏、快捷键三套逻辑：

```cpp
struct EditorUICommand
{
    EditorActionId ActionId;
    const char* DisplayName;
    // 未来：DefaultChord
};

class EditorUICommandList
{
    void MapAction(EditorActionId id, /* shortcut, toolbar slot */);
    bool TryExecute(EditorActionId id, IEditorContext& editor, const EditorMenuContext& ctx);
};
```

MVP 可只定 `EditorActionId` 枚举/constexpr，**UICommandList 壳子后接**；菜单已走 Registry 即满足低债。

---

## 9) 与 minEngine 现有模块集成

| 现有组件 | 集成方式 |
|----------|----------|
| `Editor` | 成员 `EditorContextMenuSystem m_ContextMenu`；`PostInitialize` / `OpenProject` 后 `RegisterBuiltInActions()` |
| `IEditorContext` | **`GetContextMenu()`**（唯一推荐运行时入口） |
| `EditorCommandStack` | 所有破坏性编辑必须 `Execute(unique_ptr<IEditorCommand>)` |
| `SceneEditor` | Hierarchy/Inspector 相关 Action 内部调用现有 `Submit*` |
| `AssetWorkflowModule` | CB：**Delete / Import**（Action 内调用）；Reveal、Rename **暂缓**（§15.4） |
| `InspectorModule` | Inspector 右键 `PopulateSceneInspectorMenuContext` |
| `EditorInputHub` | 二期：`GetContextMenu().GetRegistry().FindById` + Selection 构建 Context |

**删除：** Tools 菜单中 FileDialog 临时入口（S3）；Import 走 CB Action / 工具栏，不经 Tools 调试项。

**明确不做：** `EditorContextMenuSystem` **不**继承 `EditorServiceModule`（见 §1.2）。

---

## 10) 分阶段实施（结构一次到位，功能分批）

| 阶段 | 交付 | 说明 |
|------|------|------|
| **M0 骨架** | ✅ 见 §15 | 2026-05-27 |
| **M1 CB** | ✅ Context + 右键 + **Delete / Import / Refresh**；Reveal、Rename **撤出** | 2026-05-27 收口 |
| **M2 Hierarchy + Inspector** | 对应 Typed Context + Delete/Rename/Duplicate/Focus | 与 Scene Command 对齐 |
| **M3 清理** | 移除 Tools FileDialog 入口；文档验收 | |
| **M4** | `ActionProvider`（如 Add Component 列表）、Section 子菜单 | |
| **M5** | `EditorUICommandList` + 快捷键 | 与 `EditorInputHub` 统一 |
| **远期** | Capability / `EditorElementHandle` | 对标 TypedElement |

**MVP（M0–M3）已包含正确架构**，不是「先写死再重构」。

---

## 11) 设计优势（为何值得多写一层）

1. **扩展成本低**  
   新菜单项 = 新 `IEditorAction` + `Register`，窗口零修改。后续 Material Graph、CB 右键「Reimport」同路径。

2. **与 UE / 现代编辑器心智一致**  
   Context 袋 + Registry + Command，团队与外部资料（含 UE 源码阅读）对齐，降低沟通成本。

3. **技术债可控**  
   避免每个窗口 `DrawContextMenu()` 里 if 地狱；避免 `enum ContextType` 无法表达多选/空白/Property 组合。

4. **Undo 单一真相**  
   Action 层薄，Command 层厚；菜单/工具栏/快捷键将来共用 ActionId，不会「菜单能 Undo、快捷键不能」。

5. **动态菜单可生长**  
   `ActionProvider` 支持反射驱动子菜单（Add Component、按 AssetType 过滤），无需改 ImGui 代码。

6. **测试友好**  
   `BuildActions(ctx)` 可单测：给定 Context 快照，断言 Query 结果与 CanExecute，无需 ImGui。

7. **为 Icon / 缩略图留接口**  
   `IEditorAction` 可增加 `GetIcon(const EditorMenuContext&)`；CB Tile 图标与菜单图标可共用 `AssetType` → 图标描述符表（独立设计，见 rollout B）。

8. **插件 / 脚本远期**  
   经 `GetContextMenu().GetRegistry().Register(...)` 扩展；与 P4/P5（MEFunction/Lua）同方向。

9. **归属清晰、API 不分裂**  
   单一 `GetContextMenu()`，避免「ServiceModule 还是 Context getter」双入口；与 `CommandStack` 基础设施一致。

---

## 12) 明确不做 / 暂缓（本系统范围内）

- **`EditorContextMenuModule : EditorServiceModule`**（本设计明确不采用，见 §1.2）

- **CB `RevealInExplorer`（暂缓）** — 不做 Win32 `ShellExecute` 级临时方案；待 Runtime/Editor **跨平台**「在文件管理器中显示」抽象后再注册 Action
- **CB `Rename`（暂缓）** — 不做模态 Rename 窗 + 直调 `AssetManager::RenameAsset`；待与 Hierarchy 内联/F2、`Rename*Command`、Undo 统一交互后再交付

- TypedElementFramework 全量（仅用 Typed Context 过渡）
- 反射自动生成 `UFUNCTION(EditorAction)`（P4 后考虑）
- Material Editor 图节点右键（单独 Context 类型，复用同一 Registry）
- CB 缩略图（E2.3b，与菜单系统解耦）

---

## 13) 验收标准

### M0（骨架，2026-05-27）

- [x] `EditorContextMenuSystem` + `EditorMenuContext` + `IEditorAction` + `EditorActionRegistry` + `EditorMenuBuilder` 存在于 `Editor/src/ContextMenu/`
- [x] `IEditorContext::GetContextMenu()`；`Editor` 成员 `m_ContextMenu`；**不**继承 `EditorServiceModule`
- [x] `PostInitialize` / `OpenProject` 成功 → `RegisterBuiltInActions()`；`CloseProject` / `Shutdown` → `Shutdown()`（清 Provider）
- [x] `EditorActionId` 枚举覆盖 MVP Action 名（实现于 `EditorActionIds.h`）
- [ ] `Registry::Query` 单测（Editor 无测试目标，**延后**）

### M1（Content Browser，2026-05-27 收口）

- [x] `Contexts/ContentBrowserMenuContext.h`；`Actions/ContentBrowserBuiltInActions.*`
- [x] CB 树目录 / 树资产 / Tile / 列表空白处 → `BuildAndDraw`
- [x] **Delete**、**Import…**、**Refresh**
- [x] `IsVisibleInMenu` + `CanExecute` 方案 A（Import 在资产项上灰显）
- [x] Import 目标目录 = 当前/右键目录（`ImportAssetDialog(rel)`）
- [x] ~~Reveal in Explorer~~ — **撤出**（§15.4）；代码已删
- [x] ~~CB Rename 模态框~~ — **撤出**（§15.4）；代码已删
- [ ] **Reveal**（跨平台 Shell）— 后续专章设计后再注册
- [ ] **CB Rename** — 与 Command/Undo + 统一 UX 后再注册

### M2（Hierarchy + Inspector，2026-05-27）

- [x] `Contexts/HierarchyMenuContext.h`、`Contexts/SceneInspectorMenuContext.h`
- [x] `Actions/EditorEditActions.*`（`Delete`/`Rename` 跨 CB + Scene）；`Actions/SceneBuiltInActions.*`（Duplicate/Focus 占位）
- [x] Hierarchy GO 项右键 → `BuildAndDraw`；空白处仍本地「Create Empty」
- [x] Inspector GO 标题右键 → `BuildAndDraw`；组件 Remove 仍本地（M2+ 可迁 Registry）
- [x] Delete/Rename 走现有 `SubmitRemoveGameObjectFromScene` / `RenameGameObjectCommand` 路径
- [ ] **Duplicate** Command + Undo
- [ ] **Frame in Viewport** 接 `SceneEditingViewportClient::FocusSelection`

### MVP 全量（M0–M3 完成后勾选）

- [x] Content Browser 经 `GetContextMenu().BuildAndDraw` 弹出菜单
- [x] Hierarchy / Inspector GO 右键经 Registry（组件 Remove 除外）
- [ ] 三窗口右键均由 `EditorMenuContext` + `Registry.Query` 驱动，无硬编码 `MenuItem("Delete")` 业务分支
- [ ] 新增实验性 Action 仅需 `Register`，无需改 CB/Hierarchy 源文件
- [ ] Delete/Rename 等走 `IEditorCommand` / 现有 Submit 路径
- [x] Tools 临时 FileDialog 入口已移除，Import 仍可用（File 菜单 Import Asset…）
- [x] 设计案链接 `docs/external/` 两份参考（文首已链）

---

## 15) M0 实现对照（2026-05-27）

> 供审批 M1 前核对；**以代码为准**的路径：`minEngine/Editor/src/ContextMenu/`。

### 15.1 与设计一致

| 项 | 说明 |
|----|------|
| 门面 | `EditorContextMenuSystem` 组合 `m_Registry` + `m_Builder`，对外 `BuildAndDraw` |
| 归属 | `Editor::m_ContextMenu`；`IEditorContext::GetContextMenu()` |
| 非 ServiceModule | 未进入 `RegisterModules()` |
| Context 袋 | `EditorMenuContext::Add` / `Find` / `FindAll`（`shared_ptr` + `type_index`） |
| Registry | `Register`、`RegisterProvider`、`Query`、`FindById`、`Clear` / `ClearProviders` |
| Provider 时机 | `EditorMenuBuilder::Draw` 在静态 Action 之后调用 `InvokeProviders` |
| 生命周期 | `PostInitialize` + `OpenProject` → `RegisterBuiltInActions`；`CloseProject` + `Editor::Shutdown` → `Shutdown()` |
| 目录 | `ContextMenu/` 下核心五件套 + `EditorActionIds`；`Actions/`、`Contexts/` **M1 起**再建 |

### 15.2 实现与设计差异（设计已按此更新；M1 可选代码调整）

| 主题 | 设计原述 | M0 实现 | M1 建议 |
|------|----------|---------|---------|
| `FindAll` 返回类型 | `std::span<…>` | `std::vector<…>` 按值返回 | 保持 vector 即可 |
| `Execute` | 非 const | **`const` 成员** | 设计已改；无需回退 |
| Section UI | 易被读成 Section **子菜单** | **扁平** MenuItem + Section 间 `Separator` | 设计 §7.1 已写明 |
| `Query` 过滤 | 未明确是否含灰显项 | **仅** `CanExecute==true` 才出现 | M1：拆 `IsVisibleInMenu` / `CanExecute`，或 Query 按 Context 匹配、Builder 灰显 |
| `RegisterBuiltInActions` | 启动注册一次 | **PostInitialize 与 OpenProject 均调用**；`Register` 按 Id **去重** | M1 注册内置 Action 时依赖去重即可 |
| `Shutdown` | 清空 Provider / 动态 | **`ClearProviders` only**；内置 Action 表保留 | 与「CloseProject 不关内置」一致 |
| 内置 Action | §6.1 清单 | **空表**（M0 占位） | M1 在 `RegisterBuiltInActions` 注册 |
| Typed Context | §4.3 三张表 | **未建** `Contexts/` | M1 `ContentBrowserMenuContext` |
| 单测 | M0 提及 Query 单测 | **未建** Editor 测试目标 | 延后或 M1 冒烟 |

### 15.3 菜单灰显（已拍板方案 A，M1 已实现）

- `IEditorAction::IsVisibleInMenu` — Context 匹配则显示  
- `CanExecute` — Builder `BeginDisabled` + `GetDisabledReason` Tooltip  
- 例：CB 上 Import 对资产项可见但灰显（`HitKind` 非目录/空白）

### 15.4 M1 范围回退：Reveal / Rename（2026-05-27）

试用结论：架构（Context + Registry + 方案 A）可用；下列两项**体验/平台未达标**，从 M1 交付中移除，**实现代码待下一任务删除**（设计案先更新）。

| 项 | 原 M1 实现 | 问题 | 后续方向 |
|----|------------|------|----------|
| **RevealInExplorer** | `EditorPlatformShell` + Win32 `explorer /select` | 非跨平台；与引擎分层不符 | Runtime 或 `IPlatformShell` 抽象（Win/macOS/Linux），再挂 `RevealInExplorer` Action |
| **CB Rename** | 模态 `Rename Asset` + `AssetManager::RenameAsset` | 与 Hierarchy F2/内联不一致；无 Undo | 统一「重命名」交互规范 + `IEditorCommand`（可参考 `RenameGameObjectCommand`）；CB 可内联或 F2，**禁止**临时模态 |

**M1 收口后 CB 右键仅：** Delete、Import…、Refresh。

**待删实现（代码清理，非本文档范围）：**

- `Shell/EditorPlatformShell.*`
- `ContentBrowserBuiltInActions` 内 Reveal / Rename 类及 `Register` 调用
- `AssetWorkflowModule`：`BeginRenameSelectedAsset` / pending 状态
- `ContentBrowserWindow`：`DrawPendingAssetRenamePopup` 及相关成员

---

## 14) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-27 | 初稿：轻量 Service + enum（已废弃） |
| 2026-05-27 | **重置**：UE/现代编辑器对齐；Context 袋 + Registry + Builder + Command；M0–M5 |
| 2026-05-27 | **v2**：`EditorContextMenuSystem` 门面；**不**继承 `EditorServiceModule`；`IEditorContext::GetContextMenu()` |
| 2026-05-27 | **M0 落地** + §15 实现对照；§13 拆 M0/MVP 验收；§4/§5/§6/§7 与代码对齐 |
| 2026-05-27 | **M1 CB**：ContentBrowserMenuContext、方案 A；首版含 Reveal/Rename（后撤回） |
| 2026-05-27 | **M1 收口**：Reveal/Rename 暂缓；§6.1 分表；§12、§15.4；CB 仅 Delete/Import/Refresh |
