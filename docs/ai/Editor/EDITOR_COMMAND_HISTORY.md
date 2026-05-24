# Editor Command Stack（Undo / Redo）— 设计

Last updated: 2026-05-24  
Status: **v2（E1.1 + Scene 验证子集已实现）**  
父文档：[Editor Shell 设计](./EDITOR_SHELL_DESIGN.md)  
相关：`EditorCommandStack.h`、`IEditorCommand`、`EditorInputHub`、`ProjectSettings.Editor`

---

## 0) 一句话

**`EditorCommandStack`** 持有可逆编辑事务栈（**`IEditorCommand`**）；**`EditorInputHub`** 只负责快捷键/菜单触发 Undo/Redo，不入栈。栈深度上限由 **项目级 `EditorSettings.MaxUndoStackDepth`** 配置。

---

## 1) 命名（已定）

| 符号 | 含义 |
|------|------|
| **`IEditorCommand`** | 可逆编辑事务；`Execute` / `Undo` / `GetDescription` |
| **`EditorCommandStack`** | Undo/Redo 双栈 + `Execute` 入栈 |
| **`EditorCommandBinding`** | InputHub 快捷键/菜单绑定（**不是** `IEditorCommand`） |

---

## 2) 与 Shell 其它概念的分工

| 概念 | 类型 / 位置 | 职责 |
|------|-------------|------|
| **Editor Command** | `IEditorCommand` + `EditorCommandStack` | 捕获一次可逆状态变更 |
| **UI 快捷键** | `EditorCommandBinding` + `EditorInputHub` | Save、Exit、调用 `CommandStack::Undo/Redo` |
| **SubModule** | `SceneEditor`、`MaterialEditor` | 提供 `Apply*` / `Submit*`；不直接操作栈 |
| **配置** | `ProjectSettings.Editor`（`EditorSettings`） | `MaxUndoStackDepth` 等 |

```text
UI / Viewport
  → context.GetCommandStack().Execute(std::make_unique<ConcreteCommand>(...))
  → Stack 调用 cmd->Execute() 后压入 Undo 栈
```

---

## 3) 栈归属与生命周期

### 3.1 单栈、挂 Editor

- 经 **`IEditorContext::GetCommandStack()`** 访问。
- Scene / Material **共用**；`ActivateSubModule` **不清栈**。

### 3.2 何时 `Clear()`

| 事件 | 清栈 |
|------|------|
| `OpenProject` / `CloseProject` | **是** |
| **`LoadScene`（默认）** | **是**（经 `SceneEditor::LoadScene`） |
| `ActivateSubModule` | **否** |
| 修改 `MaxUndoStackDepth` | **否**（仅 `Trim` 栈底） |

---

## 4) `IEditorCommand`

```cpp
class IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual const char* GetDescription() const = 0;
};
```

**E1 已实现（Scene 子集）：**

- `RenameGameObjectCommand` — Hierarchy / Inspector 重命名
- `SetGameObjectTransformCommand` — Gizmo 拖拽结束（Translate/Rotate/Scale 统一为 Transform 快照）

**原则：** 交互结束再 `Execute`；Command 自包含 old/new 或 before/after 状态。

**后置：** `TryMerge`（Gizmo/Slider 连续合并）、Material 图命令。

---

## 5) `EditorCommandStack`

```text
Execute(cmd): cmd->Execute(); push undo; clear redo; trim to max depth
Undo():       pop undo → Undo() → push redo
Redo():       pop redo → Execute() → push undo
```

**API：** `SetMaxDepth`、`PeekUndoDescription`、`PeekRedoDescription`、`Clear`、`CanUndo/CanRedo`。

截断：超出 `m_MaxDepth` 时从 **栈底** 丢弃最老 Undo 条目。

---

## 6) `EditorSettings`（项目 `.mesettings`）

```cpp
struct EditorSettings {
    uint32_t MaxUndoStackDepth = 0;  // 0 → 引擎默认 100
};
struct ProjectSettings {
    std::string EditorDefaultSceneName;
    EditorSettings Editor;
};
```

引擎缺省：`EditorSettingsDefaults.h`（`kDefaultMaxUndoStackDepth = 100`，clamp 16–4096）。

`Editor::OpenProject` → `ApplyCommandStackSettingsFromProject()` + `Clear()`；`SceneEditor::LoadScene` → `Clear()`。

---

## 7) 与 InputHub / MainMenu

| 入口 | 状态 |
|------|------|
| `Ctrl+Z` / `Ctrl+Y` | GlobalCommand → `GetCommandStack()`（除 `WantTextInput` 外均处理，含视口聚焦） |
| MainMenu Edit Undo/Redo | 已接线 `CanUndo/CanRedo` |
| Save | 不入栈 |

---

## 8) 实现分期

| 阶段 | 状态 |
|------|------|
| E1.1 栈 + EditorSettings + 菜单 | **完成** |
| E1.2 Scene Rename + Transform | **完成（验证子集）** |
| E1.3 Material 图/属性 | 待做 |
| E2 Preferences UI、Merge、Composite | 待做 |

---

## 9) 验收（当前）

- [x] `IEditorCommand` + `EditorCommandStack` 命名
- [x] `MaxUndoStackDepth` 来自 `ProjectSettings.Editor`
- [x] `OpenProject` / `LoadScene` 清栈；`ActivateSubModule` 不清
- [x] Scene：Rename、Gizmo Transform 可 Undo/Redo
- [ ] Material 命令
- [ ] `TryMerge`、菜单显示 `PeekUndoDescription`

---

## 10) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-24 | v1 草稿 |
| 2026-05-24 | v2：命名定为 `IEditorCommand` + `EditorCommandStack`；`LoadScene` 默认清栈；E1.1/E1.2 Scene 子集落地 |
