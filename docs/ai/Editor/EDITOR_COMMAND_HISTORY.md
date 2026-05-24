# Editor Command Stack（Undo / Redo）— 设计

Last updated: 2026-05-24  
Status: **v3.2（E1.3 Inspector 属性 Command 已落地）**  
父文档：[Editor Shell 设计](./EDITOR_SHELL_DESIGN.md)  
相关：`EditorCommandStack.h`、`IEditorCommand`、`EditorInputHub`、`ProjectSettings.Editor`

---

## 0) 一句话

**`EditorCommandStack`** 持有可逆编辑事务栈（**`IEditorCommand`**）；**`EditorInputHub`** 负责快捷键/菜单触发 Undo/Redo，不入栈。栈深度上限由 **项目级 `EditorSettings.MaxUndoStackDepth`** 配置。

**推进顺序（拍板）：** [序列化扩展 S1–S2](../Platform/Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) → E1.3 Inspector 属性 Command（Property blob）→ E1.4 Snapshot → E1.5 Material → E2 合并/Composite。

---

## 1) 命名（已定）

| 符号 | 含义 |
|------|------|
| **`IEditorCommand`** | 可逆编辑事务；`Execute` / `Undo` / `GetDescription` |
| **`EditorCommandStack`** | Undo/Redo 双栈 + `Execute` 入栈 |
| **`EditorCommandBinding`** | InputHub 菜单/其它绑定（**不是** `IEditorCommand`） |

---

## 2) 与 Shell 其它概念的分工

| 概念 | 类型 / 位置 | 职责 |
|------|-------------|------|
| **Editor Command** | `IEditorCommand` + `EditorCommandStack` | 捕获一次可逆状态变更 |
| **UI 快捷键** | `EditorInputHub` | Save、Exit；Undo/Redo 走 `ImGui::Shortcut(RouteGlobal)` |
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

### 3.3 栈深度 vs 快照体积

- 当前上限按 **条数**（`MaxUndoStackDepth`），**不按字节**。
- 单条 Command 可携带任意大小的 before/after（例如未来 JSON 快照）。
- **E1.4** 再评估：大快照是否占多条深度、或项目级字节预算（非 E1.3 阻塞项）。

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

### 4.1 E1.2 已落地（Scene）

| Command | 入口 | Undo 质量 |
|---------|------|-----------|
| `RenameGameObjectCommand` | Hierarchy / Inspector 重命名 | 完整 |
| `SetGameObjectTransformCommand` | Gizmo 松手 | 完整 |
| `AddEmptyGameObjectCommand` | Hierarchy 创建 | 完整 |
| `DeleteGameObjectCommand` | Hierarchy 删除 | **弱**：仅 name + Transform；Undo 新建 GO（新 ID），无组件/层级 |
| `AddComponentCommand` | Inspector 添加 | 完整（按类型移除） |
| `RemoveComponentCommand` | Inspector 移除 | **弱**：Undo 同类型空组件，**属性未恢复** |

代码目录：`minEngine/Editor/src/Commands/Scene/`。

### 4.2 E1.3 已入栈（Inspector 反射 Primitive）

| Command | 入口 | 说明 |
|---------|------|------|
| `SetObjectPropertyCommand` | Inspector 控件 `Activated` / `DeactivatedAfterEdit` | Binary before/after blob；**仅 `MEPropertyCategory::Primitive`** |

### 4.3 仍不入栈 / 弱覆盖

| 操作 | 现状 |
|------|------|
| Inspector **Object / ObjectPtr / Array** | 仍仅 dirty，无 Undo |
| Inspector **嵌套 Object 子字段**（如 Transform 内 Position） | 无 per-field Undo；Transform 位移请用 Gizmo |
| Save Scene / 打开项目 | 设计约定不入栈 |
| Material 图编辑 | **E1.5** |

**原则：** 一次交互结束再 `Execute`（与 Gizmo 松手、重命名回车一致）；Command 自包含可恢复状态。

**后置（E2）：** `TryMerge`（Gizmo/Slider 连续合并）、Composite 多步事务。

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
| `Ctrl+Z` / `Ctrl+Y` | `EditorInputHub::ProcessGlobalUndoRedoShortcuts()`，`ImGui::Shortcut(..., RouteGlobal)`；`WantTextInput` 时不抢焦点 |
| MainMenu Edit Undo/Redo | `CanUndo/CanRedo` + `Undo/Redo` |
| Save | 不入栈 |

---

## 8) 实现分期

| 阶段 | 内容 | 状态 |
|------|------|------|
| **E1.1** | 栈 + `EditorSettings` + 菜单 + 开/关项目、LoadScene 清栈 | **完成** |
| **E1.2** | Scene 结构编辑（Rename、Transform、GO/Component 增删） | **完成**（Delete/Remove 为占位，见 §4.1） |
| **S1–S2** | 序列化：BinaryArchive + 公开 Property API | **完成** |
| **E1.3** | Inspector 属性 Command（Binary property blob） | **完成** |
| **E1.4** | **Object / Component Snapshot**（E1.3 完成后再定稿设计并实现） | 待设计 |
| **E1.5** | Material 图/属性 Command | 待做 |
| **E2** | Preferences UI、`TryMerge`、Composite、菜单 `PeekUndoDescription` | 待做 |

```text
E1.2 手写 Command（已完成）
  → S1–S2 BinaryArchive + SerializeProperty（平台）
  → E1.3 Inspector property blob Undo
  → E1.4 通用快照（升级 Delete/Remove、ObjectPtr/Array）
  → E1.5 Material
  → E2 体验
```

---

## 9) E1.3 — Inspector 属性 Command（详细设计）

**前置：** [SERIALIZATION_BINARY_AND_PROPERTY_API.md](../Platform/Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) **S1 + S2**（`BinaryArchive` + 公开 `SerializeProperty` / `DeserializeProperty`）。

### 9.1 目标

- Inspector 中修改 **反射属性** 后，**Ctrl+Z** 可恢复。
- 与现有 `Apply*` / `Submit*` 一致；**before/after** 为 **Binary property blob**（非手写 JSON）。
- **不**在本阶段引入通用 Snapshot 类型。

### 9.2 非目标（留给 E1.4 / E1.5）

- 整 GameObject / 整 Component 序列化恢复（Delete GO、Remove Component 真恢复）。
- Material 图节点/连线。
- `TryMerge`（同一滑条拖很久仍可能多条 Undo，E2 再合并）。

### 9.3 Command：`SetObjectPropertyCommand`

**定位：** 单对象、单属性、**before / after** 各一份值。

| 字段 | 说明 |
|------|------|
| `m_OwnerGuid` | 属性所属 `MEObject`（多为 `Component`；含 Root 上 `m_Transform`） |
| `m_PropertyName` | 反射字段名（如 `m_Metallic`） |
| `m_Before` / `m_After` | 属性值的 JSON 表示（见 §9.4） |
| `m_SceneEditor` | 应用/撤销时写回场景 |

**描述：** `Set {ClassShortName}.{PropertyName}`（找不到对象时 Execute/Undo no-op，与现有 Command 一致）。

### 9.4 属性值编码（E1.3 范围）

**编码：** `Serializer::SerializeProperty` → `BinaryWriterArchive` → `std::vector<uint8_t>`；Undo 时 `DeserializeProperty` 写回 owner。

**v1 支持（与 Inspector `DrawPrimitiveProperty` 对齐）：**

- `int` / `int32` / `uint32`、 `float` / `double`、 `bool`
- `std::string`
- `Vector2` / `Vector3` / `Vector4`

**v1 明确不做（标为 E1.3 后续或 E1.4）：**

| 类别 | 原因 |
|------|------|
| `Object` / `ObjectPtr` | GuidRef / Instanced 语义；见序列化设计 §6.3 |
| `Array` | 整 property blob 可支持，Inspector 接线可后置 |
| `Invisible` / `Transient` | Inspector 已跳过，Command 也不接 |

**Transform：** Inspector 里 Root 的 `m_Transform` 若可编辑，走 **同一 Command**（JSON 存 Transform），与 Gizmo 的 `SetGameObjectTransformCommand` 并存（不同入口、同一字段）。

### 9.5 定位对象与属性

```text
ObjectManager::FindObject(ownerGuid)
  → MEObject*
  → ReflectionSystem 按 class 层级 FindProperty(propertyName)
  → property.GetMutable(owner)
```

- 不依赖 `Component` 裸指针跨帧（删除/重建后 GUID 仍稳定则 Undo 可定位；对象已销毁则 no-op）。
- 属性写回后：若 owner 为 `SceneComponent`，调用 `MarkRenderStateDirty()`（与 Inspector 现状一致）。

### 9.6 `SceneEditor` API

```cpp
bool ApplySetObjectProperty(const GUID& ownerGuid,
                              const std::string& propertyName,
                              const Json& value);

void SubmitSetObjectProperty(IEditorContext& context,
                             const GUID& ownerGuid,
                             const std::string& propertyName,
                             Json beforeValue,
                             Json afterValue);
```

- `Apply*`：直接写属性 + `MarkSceneDirty()` + 必要时 dirty render。
- `Submit*`：`Execute(SetObjectPropertyCommand{...})`；`before`/`after` 由调用方在交互边界采集。

### 9.7 Inspector 交互边界（ImGui）

与 Gizmo「松手一条 Command」对齐：

| 时机 | 行为 |
|------|------|
| `ImGui::IsItemActivated()` | 对该控件 `CapturePropertyToJson` → **session before** |
| 拖拽/输入中 | 只改内存 + `MarkSceneDirty`（**不入栈**） |
| `ImGui::IsItemDeactivatedAfterEdit()` | 再 capture **after**；若 `before != after` → `SubmitSetObjectProperty` |

实现落点：`SceneEditorInspectorSource` 各 `Draw*Property`，或共用的 `DrawPropertyWithUndo(...)` 包装，避免每个控件重复三行 ImGui 状态判断。

**注意：** `Checkbox` 可在 **一次点击** 内同时 Activated + DeactivatedAfterEdit，仍按上表提交一条 Command。

### 9.8 验收（E1.3）

- [ ] 修改选中 Component 的 float/int/bool/Vector 字段 → Undo 恢复旧值 → Redo 恢复新值
- [ ] 连续改两个不同属性 → 栈上两条，各 Undo 一次
- [ ] 视口聚焦时 Ctrl+Z 仍有效（沿用现有 Global Undo）
- [ ] 不支持类型仍只 dirty、不入栈（行为与现在一致，无静默失败）

### 9.9 建议文件

| 文件 | 职责 |
|------|------|
| `Commands/Scene/SetObjectPropertyCommand.{h,cpp}` | Command |
| （无单独 JSON 层） | 直接用 `Serialization::SerializeProperty` + Binary buffer |
| `SceneEditor.{h,cpp}` | `Apply*` / `Submit*` |
| `SceneEditorInspectorSource.cpp` | Activated / DeactivatedAfterEdit 接线 |

---

## 10) E1.4 — Object Snapshot（设计占位）

> **状态：** E1.3 完成并跑通 Inspector 属性 Undo 后，再写本节细案并开始编码。  
> 本节仅列方向，避免与 E1.3 的单属性 JSON 重复造轮子。

### 10.1 要解决的问题

| 场景 | E1.3 不足 | E1.4 目标 |
|------|-----------|-----------|
| Delete GameObject Undo | 只有 name + Transform | 恢复 GO + 组件列表 + 属性 +（若已有）父子关系 |
| Remove Component Undo | 空壳同类型组件 | 恢复删除前整组件状态 |
| Inspector ObjectPtr / Array | 未支持 | 统一走快照或专用子命令 |
| Material 大图 | — | E1.5 可复用同一 `EditorSnapshot` 容器 |

### 10.2 方向（待 E1.3 后定稿）

```text
EditorSnapshot（或 SceneEditSnapshot）
  CaptureGameObject(gameObjectId)   // Serializer + 子组件 GUID 列表
  CaptureComponent(ownerGuid)
  Restore(snapshot)                 // 反序列化 + Scene 索引更新
```

- 复用 **`Serialization::Serializer`** + 反射；与 `.mescene` 字段子集对齐，避免第二套格式。
- Command 内只存 **`std::string` 或 `Json` blob** + 元数据（描述用）。
- 升级 **`DeleteGameObjectCommand`** / **`RemoveComponentCommand`** 使用快照，而非扩写更多 ad-hoc 字段。

### 10.3 与栈策略（E1.4 设计时拍板）

- 单条快照是否计 1 条深度，或按大小折算（可选）。
- 超大 Material 图是否截断/拒绝入栈（与 E1.5 一起验收）。

---

## 11) E1.5 / E2（概要，细节后补）

### E1.5 Material

- 节点增删、连线、图属性；`MaterialEditor::Submit*` + `Material*Command`。
- 与 Scene **共用** `EditorCommandStack`；切换 SubModule 不清栈。
- 复杂图变更可依赖 **E1.4** 子图快照；若 E1.4 未就绪，可先做「单属性 JSON」与 Scene E1.3 同构。

### E2 体验

| 项 | 说明 |
|----|------|
| `TryMerge` | 同属性连续拖拽合并为一条 |
| `CompositeCommand` | 多步一次 Undo（粘贴、批量） |
| 菜单 | Edit → Undo 显示 `PeekUndoDescription` |
| Preferences | 项目外编辑 `MaxUndoStackDepth` |

---

## 12) 验收总表

### 已完成

- [x] `IEditorCommand` + `EditorCommandStack`
- [x] `MaxUndoStackDepth` ← `ProjectSettings.Editor`
- [x] `OpenProject` / `LoadScene` 清栈；`ActivateSubModule` 不清
- [x] Scene：Rename、Gizmo Transform、GO/Component 增删（部分弱 Undo）
- [x] Global Ctrl+Z/Y（视口聚焦可用）

### 待做

- [x] **E1.3** Inspector 属性 Undo（Primitive）
- [ ] **E1.4** Snapshot + 加强 Delete/Remove
- [ ] **E1.5** Material 命令
- [ ] **E2** Merge、菜单描述、Composite

---

## 13) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-24 | v1 草稿 |
| 2026-05-24 | v2：命名 `IEditorCommand`；E1.1/E1.2 Scene 子集 |
| 2026-05-24 | v3：同步 E1.2 全量 Scene 命令与弱 Undo；分期改为 E1.3 属性 → E1.4 Snapshot → E1.5 Material；补 E1.3 详细设计与 E1.4 占位 |
| 2026-05-24 | v3.1：E1.3 前置序列化 S1–S2；属性 blob 改为 Binary + `SerializeProperty` |
| 2026-05-24 | v3.2：`SetObjectPropertyCommand` + Inspector Primitive 接线 |
