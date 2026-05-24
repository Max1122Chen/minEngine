# Editor 架构复盘与 Sub-Editor 方向

Last updated: 2026-05-24  
Status: **复盘定稿（无代码变更；后续设计对照基准）**  
父文档：[Editor 平台化规划](./EDITOR_PLATFORM_PLAN.md)  
关联：E1 Inspector、[Platform 路线图](../Platform/PLATFORM_ROADMAP.md)

---

## 0) 文档用途

- 记录 **2026-05-24** 对现有 `Editor` Application 的架构复盘（偏严厉，刻意暴露 bad design，便于规避）。
- 记录用户拍板方向：**`Editor` 降为 Shell；Scene / Material / 未来 Animation / Sprite 等为 Sub-Editor**。
- **不**包含具体类实现与工期；实现前需结合 UE / Unity / Godot 对标笔记。

---

## 1) 总判断

当前 Editor **不是「缺 Inspector 扩展点」**，而是 **还没有 Editor 架构**——只有：

- 一套能跑的 **Scene 关卡编辑器**（功能堆在 `class Editor`）
- **Material 模式**作为相对独立的岛（`MaterialEditor` + 专用窗口）

`InspectorWindow` 不是门面，是 **~600 行 GameObject 专用面板**。  
`MaterialDetailsWindow` + `MaterialNodeDefPropertyDrawer` 是 **第二套 Details UI**。

若在现有 `InspectorWindow` 上「加 Asset 分支」，会把 **第三、第四套 Details** 焊在一起，比现在还难维护。

**用户结论（拍板）：** 早期为立竿见影把 SceneEditing 堆进 `Application` 类 `Editor` 是合理过渡；现在应改为 **Sub-Editor 思路**，`Editor` 只做上层管理。

---

## 2) 现状问题清单（按严重度）

### P0 — `Editor` 是 God Object

`Editor` 同时承担：

| 职责 | 现状位置 |
|------|----------|
| Application / ImGui / 主循环 | `Editor::Initialize` / `Run` |
| Project 开闭 | `OpenProject`（`CloseProject` TODO） |
| GameObject 选择 + 改名 + Component CRUD | `m_SelectedGameObject*` + 20+ 方法 |
| Scene dirty / Save / 增删 GO | `Editor` 直接调 `SceneManager` |
| ViewportClient 工厂 | `m_ViewportClients` + `static_cast` |
| `MaterialEditor` 生命周期 | `unique_ptr<MaterialEditor>` |
| 窗口标题、布局重置 flag | `dockLayoutInitialized`、`requestResetLayout` |
| 组件类型列表缓存 | `InitializeAllComponentTypeNames` |

**后果：**

- 新编辑目标（Asset、Graph Node、Skeleton…）只能继续 **往 `Editor.h` 加 API**。
- 几乎所有窗口 `#include "Editor.h"` → **编译耦合 + 心智耦合**。
- Inspector 无法独立演进，只能 `GetSelectedGameObject()`。

**代码锚点：** `minEngine/Editor/src/Editor.h`、`Editor.cpp`。

---

### P0 — 没有正式「选择模型」

```text
uint64_t m_SelectedGameObjectId;
GameObject* m_SelectedGameObject;   // 裸指针，与 ID 双轨
```

问题：

1. **仅支持 GO**；Asset / Component 作为主选或子选无正统入口。
2. **`SyncSelectionWithScene()` 缺陷**：ID 仍在 map 中时直接 `return`，**不刷新指针**；自动改 ID 时 **不更新 `m_SelectedGameObject`**。
3. 场景卸载 / 重载后，裸指针 **无失效契约**。
4. Hierarchy 与 Inspector **各有一套 F2 改名** 状态与 buffer。

**后果：** Content Browser、Graph 选 Node 只能走 `MaterialDetailsWindow` 等旁路。

---

### P1 — `InspectorWindow` 名不副实

实际职责：

- GO 头信息 + 改名
- Add Component UI
- 全套反射 Property 绘制（primitive / vector / enum / object ptr）
- `DrawAssetRef`（且代码内 TODO「要更 generic」）
- Component 右键菜单

**并行第二套：** `MaterialDetailsWindow` + `MaterialNodeDefPropertyDrawer`。

**缺失：** Drawer 注册、Target 抽象、Preview 插槽、Command 边界。

---

### P1 — `EditorUIMode` 是布局级 hack

- 切换 Scene ↔ Material → **整 dock 拆掉重建**。
- 窗口 `SetOpen(false)` 隐藏，非独立 Editor 实例。
- Material 模式无 Shared Inspector / Content Browser 位。
- 每增加一种 Asset Editor → enum + 第三套 layout + 第三组 Window suite → **组合爆炸**。

对标：UE 为 **多 Asset Editor + 共享 Content Browser / Details**，非全局 enum 切 Layout。

---

### P2 — Viewport / Preview 分裂

- Scene：`SceneEditingViewportClient` → Active Scene
- Material：`MaterialPreviewViewportClient` → 独立 Preview Scene
- 异构 map + `dynamic_cast`；Material client 内判断 `UIMode != MaterialEditing` 则 return

**无** 统一 `PreviewHost` / `PreviewSession` → Inspector 内嵌预览无挂载点（见 E2）。

---

### P2 — 数据流：窗口直连 Runtime 改数据

```text
ImGui → InspectorWindow → Editor::RenameGameObject → GO::SetName → MarkSceneDirty
```

无：事务边界、只读策略、PropertyChanged → Preview 刷新 的统一通道。  
Material 稍好（Session + debounce compile），但 Details UI 仍直接改 `Material&`。

---

### 相对正确的岛（应保留并抽象）

| 模块 | 评价 |
|------|------|
| `MaterialEditor` | Session + Preview + 核心无 ImGui → **Sub-Editor 雏形** |
| `EditorWindow` + `EditorGUIManager` | 窗口注册 → Shell 可复用 |
| `EditorWindowSuite::Shared` | 概念正确，未充分使用 |
| 反射 Property 绘制 | 方向对，应抽 **共享 PropertyEditor**，非 Inspector 私有 |

---

## 3) Sub-Editor 方向（用户拍板 + 架构师建议）

### 3.1 目标形态

```text
                    ┌─────────────────────────────────────┐
                    │  EditorShell (Application)          │
                    │  bootstrap · GUIManager · 输入路由    │
                    │  SelectionService · CommandHistory*   │
                    │  SubEditor 注册 / 激活 / 多窗口*      │
                    └──────────────┬──────────────────────┘
                                   │ 窄接口
         ┌─────────────────────────┼─────────────────────────┐
         ▼                         ▼                         ▼
  SceneEditor              MaterialEditor              (future)
  · 场景 GO/Component       · 已有 Session+Preview       AnimationEditor
  · Hierarchy 语境          · Graph + Details            SpriteEditor
  · Scene Viewport          · Material Preview           …
         │                         │
         └─────────────┬───────────┘
                       ▼
              Shared Shell 服务
              · InspectorPanel (E1)
              · ContentBrowser (后置)
              · Console / MainMenu
              · PropertyEditor (共享控件)
```

\* CommandHistory、多 OS 窗口为 Shell 级能力，Sub-Editor 消费而非各自实现。

### 3.2 `EditorShell` 应拥有什么

| 属于 Shell | 不属于 Shell（下沉 Sub-Editor） |
|------------|----------------------------------|
| Engine / ImGui / 窗口系统 bootstrap | GO 增删改、Scene Save 逻辑 |
| `EditorGUIManager`、Dock、全局菜单框架 | Material compile、Graph 编辑 |
| **Sub-Editor 生命周期**：Register / Activate / Deactivate | Viewport 内具体 picking、gizmo |
| **输入路由**：Focus 栈、快捷键消费优先级 | 各 Editor 专有快捷键表（向 Shell 注册） |
| **SelectionService**（或 `EditorSelection` 唯一真相源） | 「选中 GO」vs「选中 Node」的语义由 Sub-Editor 写入 Selection |
| **CommandHistory / Undo**（P3；Shell 持有栈，Sub-Editor 提交 Command） | 具体 Command 类型在 Sub-Editor 或 Runtime 域 |
| Project / PathRegistry 会话边界 | Asset 导入细节（E3 + Shell 调 FileDialog） |
| 未来：**多 Editor 窗口 / OS 窗口** 抽象 | 每个 Sub-Editor 的 panel 布局预设 |

**关于 CommandBuffer / CommandHistory 是否在 Shell：** **是。**  
理由（对标 UE `FTransaction`、Unity Undo）：Undo 栈是 **跨 Panel、跨 Sub-Editor** 的全局用户期望；若各 Sub-Editor 自带栈，切模式丢历史、无法统一 Ctrl+Z。Sub-Editor 只 **构造并 Push `IEditorCommand`**，Shell 执行/合并/Redo。

### 3.3 Sub-Editor 契约（待详细设计）

每个 Sub-Editor 建议具备：

| 能力 | 说明 |
|------|------|
| `GetId()` | 如 `"Scene"`、`"Material"` |
| `CanActivate()` / `Activate()` / `Deactivate()` | 切换时释放 focus、viewport |
| `GetWindowSuite()` 或 `GetPreferredLayout()` | 向 GUIManager 提供 dock 预设 |
| `Tick()` / `Draw()` 或注册 Window 列表 | Material 已部分如此 |
| `GetSelectionContext()` | 写入 Shell Selection 的语境 |
| `RegisterCommands()` | 向 Shell 注册快捷键 |
| `OpenAsset(AssetMeta)`（可选） | Content Browser 双击入口 |

**SceneEditor** = 从 `Editor` **迁出** 现有 GO/Scene/Hierarchy/SceneViewport 逻辑。  
**MaterialEditor** = **已基本符合**，需对齐 Sub-Editor 接口、去掉对 God `Editor` 的宽依赖。  
**AnimationEditor / SpriteEditor** = 未来按同一契约添加，**不** 扩 `EditorUIMode` enum。

### 3.4 `EditorUIMode` 的命运

- **短期：** 映射为 `ActiveSubEditorId` 的别名（兼容现有菜单）。
- **长期：** 废弃 enum；改为 `EditorShell::ActivateSubEditor("Material")` + 可选 layout preset id。

---

## 4) 与其他重构项的依赖关系

```text
E0  Editor Shell + Sub-Editor 拆分     ← 当前最优先（本文档）
      ↓
E1  Inspector（Selection + Drawer）    依赖 Shell SelectionService
E2  Previewer                          Sub-Editor 与 Inspector 共用 PreviewHost
E3  AssetManager                       Shell 调 Import；Sub-Editor OpenAsset
E4  FileDialog                         Shell 提供 IFileDialogService
      ↓
Content Browser / Content 双击 Open
P3  Undo                               Shell CommandHistory
```

**Inspector 重构（E1）不应先于 E0 大量写代码**：否则 Drawer 仍会注入 God `Editor`。  
**最小并行：** 设计 `EditorSelection` 类型，与 E0 接口一并定稿。

---

## 5) E1 避坑清单（Inspector 专用）

| 不要做 | 要做 |
|--------|------|
| `InspectorWindow::OnDraw` 堆 `if/else` | `InspectorTarget` + Drawer 注册表 |
| Inspector `#include Editor.h` 调 20 个方法 | 窄接口 `IInspectorHost`（Selection + Commands + PreviewHost） |
| 为 Asset 再写一套 Property 绘制 | 共享 `PropertyEditor`；Material Details 迁入 |
| GO 改名逻辑留在 Inspector | `SelectionService` 或共享 RenameHandler |
| Inspector 内嵌 Preview 实现 | Preview **插槽**；E2 提供 `IPreviewWidget` |
| 裸 GO 指针作选择 | GUID / weak_ptr / handle + 失效回调 |

---

## 6) 迁移策略（原则，无代码）

1. ** strangler fig**：先 Introduce `EditorShell` + `ISubEditor`，`SceneEditor` 包一层现有逻辑 **搬方法**，行为不变。
2. **MaterialEditor 对齐接口**，成为第二个 `ISubEditor` 参考实现。
3. **SelectionService** 替代 `m_SelectedGameObject*`，修 `SyncSelectionWithScene` 类 bug。
4. **Inspector** 改为读 Selection；`GameObjectDrawer` 从现 `InspectorWindow` 搬迁。
5. 删除 God `Editor` 上已迁走的 API；`Editor` 改名或 typedef 为 Shell（可选）。

**禁止：** 在未拆 Shell 前堆 Content Browser + Asset Inspector + 第三套 Property UI。

---

## 7) 开放问题（后续与 UE/Unity/Godot 对标后拍板）

- Sub-Editor 是 **单例**（同时只激活一个）还是 **多 Tab 并存**（UE 式多 Asset 编辑器）？
- SceneEditor 关闭时 Selection 是否自动切到 Shell 空选？
- Shared 窗口（Inspector、Content Browser）在 Sub-Editor 切换时 **Drawer 是否自动切换**（同一窗口，不同 Target 类型）？
- CommandHistory 是否按 **Domain** 分栈（Scene vs Material）还是全局单栈？
- 多 OS 窗口：Sub-Editor 是否可 detach 到独立 OS 窗口（ImGui viewport / 原生窗口）？

---

## 8) 参考代码（过渡实现，待替换）

| 路径 | 说明 |
|------|------|
| `Editor/src/Editor.h` | God Object，迁移动作清单来源 |
| `Editor/src/Editor.cpp` | 主循环、选择、Scene CRUD |
| `Editor/src/UI/EditorWindows/InspectorWindow.*` | 非门面 Details |
| `Editor/src/Material/MaterialEditor.*` | Sub-Editor 参考岛 |
| `Editor/src/EditorGUIManager.*` | Shell GUIManager |
| `Editor/src/EditorUIMode.h` | 待废弃的布局 enum |

---

## 9) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-24 | 初版：God Object 复盘 + Sub-Editor / EditorShell 方向 |
