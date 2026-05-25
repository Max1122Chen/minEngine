# Editor Command Stack（Undo / Redo）— 设计

Last updated: 2026-05-25  
Status: **v4.2（E1.4 Snapshot + E1.3+ Inspector Transform/AssetRef property Undo 已验收）**  
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
| **E1.4** | **Object / Component Snapshot**（Binary + 全反射字段） | **完成** |
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

- [x] 修改选中 Component 的 float/int/bool/Vector 字段 → Undo 恢复旧值 → Redo 恢复新值
- [x] 连续改两个不同属性 → 栈上两条，各 Undo 一次
- [x] 视口聚焦时 Ctrl+Z 仍有效（沿用现有 Global Undo）
- [x] 不支持类型仍只 dirty、不入栈（Array UI 未实现；见 §9.10）

### 9.10 Inspector Property Undo — 捕获策略（E1.3+ 设计，2026-05-25）

**问题（v4.1 前）：** `SetObjectPropertyCommand` 与 `SerializePropertyToBuffer` 已支持 Object / ObjectPtr blob；Primitive 可 Undo。Transform / AssetRef 失败是因为 **未触发 Submit**（ImGui session 与序列化属性名脱节），不是 Command 能力不足。

**目标：** 任意内嵌 **Object（struct）** 与 **ObjectPtr / AssetRef** 的有效编辑均能 **Commit** 一条 Command。

#### 9.10.1 `PropertyUndoCaptureContext`

```cpp
struct PropertyUndoCaptureContext {
    GUID ownerGuid;
    std::string ownerClassName;
    std::string capturePropertyName;  // Serializer 一级属性名，如 "m_Transform" / "m_Mesh"
};
```

- **Primitive / Vector：** `capturePropertyName = property.GetName()`。
- **内嵌 Object 子字段：** 仍序列化 **父 Object 整段**（如 `"m_Transform"`），与 Scene 存盘一致；不向 Serializer 传 `"m_Transform.x"` 路径。
- **ObjectPtr / Asset：** `capturePropertyName = "m_Mesh"` 等顶层指针字段名。

#### 9.10.2 统一 Capture API（`SceneEditorInspectorSource`）

| 函数 | 时机 |
|------|------|
| `TryPropertyUndoActivated(context, editId)` | `ImGui::IsItemActivated()` → `SerializePropertyToBuffer` → `m_PropertyUndoBeforeByEditId[editId]` |
| `TryPropertyUndoCommitAfterEdit(context, editId)` | `IsItemDeactivatedAfterEdit()` → after blob → `SubmitSetObjectProperty` |
| `TryPropertyUndoCommitImmediate(context, beforeBlob, afterBlob)` | Asset 选中时显式提交 |

`editId = ImGui::GetItemID()` **绑定真实控件**（每个 DragFloat、每个 Combo），不再用 Object 行「最后一个子控件」代表整行。

#### 9.10.3 分类型 UI

| 类型 | 绘制 | Commit 触发 |
|------|------|-------------|
| **Primitive / Vector** | `DrawProperty` + context | Activated / DeactivatedAfterEdit（同 E1.3） |
| **Object** | `DrawObjectProperty` 向子字段传 `owner` + `parentContext`（`capturePropertyName` = 父字段名） | 每个子控件 DeactivatedAfterEdit；**Object 行本身不 Capture** |
| **AssetRef** | `DrawAssetRef` | Combo `Activated` → before；`Selectable` 选中且 GUID 变化 → after + **立即 Submit** |
| **Array** | `CanUndo` → **false**（UI 未实现） | — |

#### 9.10.4 SerializerOptions（与 Restore 对齐）

`ApplySetObjectProperty` 与 Inspector Capture 共用：

- `skipUnknownField = false`
- `allowObjectPtrSerialization = true`

#### 9.10.5 验收

- [x] `m_Transform` 任意分量编辑 → Undo 整颗 Transform 回退
- [x] `m_Mesh` / `m_Material` 换资产 → Undo 恢复旧 GuidRef
- [x] Primitive 回归无退化
- [x] 连续改 Transform 与 Mesh → 栈上两条

**实现状态：** 已实现并手测通过（2026-05-25）。

### 9.9 建议文件

| 文件 | 职责 |
|------|------|
| `Commands/Scene/SetObjectPropertyCommand.{h,cpp}` | Command |
| （无单独 JSON 层） | 直接用 `Serialization::SerializeProperty` + Binary buffer |
| `SceneEditor.{h,cpp}` | `Apply*` / `Submit*` |
| `SceneEditorInspectorSource.cpp` | `PropertyUndoCaptureContext` + §9.10 分类型 Capture |

---

## 10) E1.4 — Object Snapshot（详细设计）

**状态：** Delete GO 快照与 Inspector Transform/AssetRef property Undo **已实现并验收**（2026-05-25）；Remove Component 快照代码已接（待单独手测）。  
**前置：** S1–S4 `BinaryArchive` + `SerializeProperty*` / `SerializeObjectToBuffer`（已完成）；E1.3 Primitive Undo（已完成）。  
**关联：** [SERIALIZATION_BINARY_AND_PROPERTY_API.md](../Platform/Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) §9（S3 Snapshot API）。

### 10.1 一句话

用 **`Serializer::Serialize` / `Deserialize` + `BinaryWriterArchive`** 对 **整棵 `MEObject` 反射子树**（与 `.mescene` 同规则）做 **Capture / Restore**；Command 只存 **二进制快照 + 少量元数据**；升级 **Delete GO / Remove Component** 为真恢复，并扩展 Inspector **ObjectPtr / Array** 走同一 Property blob 路径。

### 10.2 要解决的问题（E1.3 之后）

| 场景 | 现状 | E1.4 目标 |
|------|------|-----------|
| Delete GameObject → Undo | 仅 `name` + `Transform`，新建空 GO | 恢复 **组件列表 + 各组件反射字段 + GO 名/Guid + Root 链接** |
| Remove Component → Undo | 同类型 **空壳** 组件 | 恢复 **删除前整颗 Component**（含 Mesh/Material 等 GuidRef） |
| Inspector ObjectPtr / struct | Primitive + Transform + AssetRef 可 Undo | §9.10 `PropertyUndoCaptureContext`；Array UI 未实现 |
| Material 大图 Undo | — | **E1.5**；E1.4 只提供可复用 **`EditorObjectSnapshot`** 容器 |

### 10.3 非目标（本阶段不做）

- `.mescene` / `.memtl` 默认格式改 Binary。
- 快照 **增量 diff**、压缩、按字节折算栈深度（E2 / 后续）。
- **未反射** 的运行时字段：`GameObject::m_ID`、`PrimitiveComponent::m_SceneProxy`、`SceneComponent::m_AttachParent` / `m_AttachChildren`、各类 dirty 标志。
- **场景级** 父子 GO 层级（当前 `Scene` 为 **扁平** `m_GameObjects`，无 parent GO 字段；快照不虚构层级）。
- Material 图节点/连线 Command（**E1.5**）。

### 10.4 「全反射字段」范围（与 Scene 存盘对齐）

**编码路径：** `Serializer::SerializeObject_IterateProps` → 对 class hierarchy 上 **每个有 accessor 的 `MEProperty`** 调用 `SerializeProperty`（Primitive / Object / ObjectPtr / Array）。  
**与 Inspector 不同：** Serializer 仅跳过 **Transient**；**Invisible** 仍序列化（如 `m_Owner` → GuidRef），Inspector 不绘制 Invisible。

| 对象 | 会进快照的字段（示例） | 不进快照 |
|------|------------------------|----------|
| `MEObject` | `m_Name`, `m_Guid`（gen 反射） | `m_Outer`（无反射 accessor）、`m_Class` |
| `GameObject` | `m_RootComponent`, `m_Components`（`Instanced`） | `m_ID`（非反射，见 §10.6） |
| `Component` | `m_Owner`（Raw → 序列化为 **GuidRef**） | `m_bCanEverTick`, EoF 标记 |
| `SceneComponent` | `m_Transform` | `m_AttachParent`, `m_AttachChildren`, `m_bRenderStateDirty` |
| `StaticMeshComponent` | `m_Mesh`, `m_Material`, `m_CastShadow`, … | `m_SceneProxy` |
| 资产引用 | **GuidRef**（与 `test.mescene` 一致） | 不内嵌资产文件内容 |

**Instanced 语义（与 Scene 一致）：**

- `m_Components`：每个元素 **内联** 完整组件子树（`BeginObjectPtr` + 字段）。
- `m_RootComponent`：指向某组件的 **GuidRef**（与内联组件 `m_Guid` 一致）。
- `m_Mesh` / `m_Material`：指向已加载资产的 **GuidRef**；Restore 后 `ResolvePendingObjectRefs` → `ObjectManager` / `AssetManager`。

**验证基准：** 对场景中某 GO 做 Capture → 新 GO 上 Restore → Binary payload 与「`Serializer::Serialize(GameObject)` 再 Deserialize 到空 GO」结果一致（属性 + GuidRef 解析后 Mesh/Material 指针有效）。

### 10.5 数据模型：`EditorObjectSnapshot`

**文件：** `Editor/src/Commands/Scene/EditorObjectSnapshot.h`（或 `Editor/src/Serialization/EditorObjectSnapshot.h`）

```cpp
namespace minEngine::Editor
{
    // 魔数 'MESN' (minEngine SnapShot)
    constexpr uint32_t kEditorObjectSnapshotMagic = 0x4E53454Du;
    constexpr uint16_t kEditorObjectSnapshotVersion = 1;

    enum class EditorSnapshotKind : uint16_t
    {
        GameObject = 0,
        Component = 1,
    };

    struct EditorObjectSnapshot
    {
        uint32_t magic = kEditorObjectSnapshotMagic;
        uint16_t version = kEditorObjectSnapshotVersion;
        EditorSnapshotKind kind = EditorSnapshotKind::GameObject;

        // 捕获时写入，Restore 后由调用方更新（见 §10.6）
        uint64_t sourceRuntimeId = 0;
        GUID sourceRootGuid{};

        // 反序列化根类型，如 "minEngine::GameObject" / "minEngine::StaticMeshComponent"
        std::string rootClassName;

        // Serializer::Serialize(rootClassName, object, BinaryWriterArchive) 的完整输出
        std::vector<uint8_t> payload;

        // Component 快照专用（kind == Component）
        uint64_t ownerGameObjectId = 0;
        GUID ownerGameObjectGuid{};
        int32_t componentIndexInOwner = -1; // 可选，用于尽量插回原槽位；-1 表示 append
    };
}
```

**Envelope 布局（little-endian）：**

```text
[ magic u32 ][ version u16 ][ kind u16 ]
[ sourceRuntimeId u64 ][ sourceRootGuid 16 bytes ]
[ rootClassName: u16 len + utf8 ]
[ ownerGameObjectId u64 ][ ownerGameObjectGuid 16 ][ componentIndex i32 ]  // kind==Component 时有效；GO 快照填 0 / zero / -1
[ payloadSize u32 ][ payload bytes... ]
```

**Command 存储：** `std::vector<uint8_t> m_SnapshotEnvelope`（序列化整个 `EditorObjectSnapshot` 结构，或 struct 分字段存；实现选 **扁平 envelope** 便于单测 hex dump）。

### 10.6 平台 API（S3）：Object 级 Binary 缓冲

在 `Serializer` 上增加（与 `SerializePropertyToBuffer` 对称）：

```cpp
// Serializer.h — 公开 API
static SerializeResult SerializeObjectToBuffer(
    const std::string& rootClassName,
    const void* rootObject,
    std::vector<uint8_t>& outBuffer,
    const SerializerOptions& options = SerializerOptions{});

static SerializeResult DeserializeObjectFromBuffer(
    const std::string& rootClassName,
    void* outRootObject,
    const std::vector<uint8_t>& buffer,
    std::vector<PendingObjectRef>& outUnresolvedRefs,
    const SerializerOptions& options = SerializerOptions{});
```

**实现：** `BinaryWriterArchive` → `Serialize` → `TakeBuffer()`；读路径 `BinaryReaderArchive(buffer)` → `Deserialize` → `ResolvePendingObjectRefs`。

**`EditorObjectSnapshot` Capture / Restore 薄封装：**

```cpp
// EditorObjectSnapshotUtil.h（Editor 模块）
SerializeResult CaptureGameObject(const GameObject& go, EditorObjectSnapshot& out);
SerializeResult CaptureComponent(const Component& component, const GameObject& owner, int32_t index, EditorObjectSnapshot& out);

struct GameObjectRestoreResult {
    uint64_t restoredRuntimeId = UINT64_MAX;
    GameObject* restoredObject = nullptr;
};
GameObjectRestoreResult RestoreGameObjectToScene(Scene& scene, const EditorObjectSnapshot& snapshot);

struct ComponentRestoreResult {
    Component* restoredComponent = nullptr;
};
ComponentRestoreResult RestoreComponentToGameObject(GameObject& owner, const EditorObjectSnapshot& snapshot);
```

### 10.7 Capture 流程

#### 10.7.1 `CaptureGameObject`

```text
1. kind = GameObject, sourceRuntimeId = go.GetID(), sourceRootGuid = go.GetGuid()
2. rootClassName = go.GetClass()->GetName()
3. payload = SerializeObjectToBuffer(rootClassName, &go, default SerializerOptions)
4. 写入 envelope
```

**时机：** `DeleteGameObjectCommand::Execute` **之前**（对象仍存活、组件仍挂在 GO 上）。

**不在此阶段** 调用 `UnregisterObject`；删除仍由 `shared_ptr` 释放 + 现有 GC 路径处理。

#### 10.7.2 `CaptureComponent`

```text
1. kind = Component, ownerGameObjectId/Guid, componentIndexInOwner
2. rootClassName = component.GetClass()->GetName()
3. payload = SerializeObjectToBuffer(rootClassName, &component, ...)
   // 注意：owner 为 GO 时，m_Owner 序列化为 GuidRef；与 Scene 一致
```

**时机：** `RemoveComponentCommand::Execute` **之前**。

### 10.8 Restore 流程

#### 10.8.1 `RestoreGameObjectToScene`（Delete Undo）

**原则（与 Scene 内联 Instanced 一致）：** 不用 `CreateGameObject()`/`NewObject` 预分配 GUID；先 **空壳 + Deserialize**，再 **按 payload 内 `m_Guid` 注册**，最后 **Scene 分配新 runtime ID**。`m_Owner` 等字段走 **GuidRef + Resolve**，`PostRestore` 只做兜底与渲染刷新，不是主恢复路径。

```text
1. rootClass->CreateDefaultInstance() → shared_ptr<GameObject> go
2. go->SetOuter(scene)
3. DeserializeObjectFromBuffer(..., go.get(), payload, pendingRefs)
   // DeserializeObjectInstance 对 MEObject 派生类补 SetClass（CreateDefaultInstance  unlike NewObject）
   // SerializerOptions: skipUnknownField=false, allowObjectPtrSerialization=true
   // 内联 m_Components[]：每项 CreateDefaultInstance + 字段 + RegisterObject(component)
   // m_Owner → GuidRef(父 GO 快照 GUID)，此时尚未 Resolve
4. ObjectManager::RegisterObject(go)   // 使用 Deserialize 写回的 m_Guid
5. scene.InsertRestoredGameObject(go)  // 新 runtime ID，入 m_GameObjects / index
6. ResolvePendingObjectRefs(pendingRefs)   // 此时 FindObject(父 GO guid) 可命中
7. PostRestoreGameObject(*go):
   a. 仅当 component->GetOwner()==nullptr：SetOwner(go) 兜底
   b. 若 m_RootComponent 仍空：首个 SceneComponent 兜底
   c. SceneComponent：MarkRenderStateDirty；EoF update
8. 返回 restoredRuntimeId = go->GetID()
```

**禁止：** `CreateGameObject()` 在 Deserialize 前调用（会生成无关 GUID 并污染 `m_ObjectsByGuid`）。

**GUID 策略（§10.11 A）：** **保留快照 GUID**（payload 内 `m_Guid`）；根 GO 在步骤 4 注册；内联 Component 在 Deserialize 内联路径已注册。

**Runtime ID 策略（§10.11 B）：** **不保留** `sourceRuntimeId`；步骤 5 分配新 ID；`DeleteGameObjectCommand` Undo 后更新 `m_GameObjectId`。

#### 10.8.2 `RestoreComponentToGameObject`（Remove Undo）

与 GO 同构：**CreateDefaultInstance → Deserialize → RegisterObject → 插入场景图**。

```text
1. 定位 owner：优先 ownerGameObjectGuid → ObjectManager::FindObject；其次 ownerGameObjectId → scene.FindGameObjectById
2. componentClass->CreateDefaultInstance() → shared_ptr<Component>
3. comp->SetOuter(owner)
4. DeserializeObjectFromBuffer(..., skipUnknownField=false, allowObjectPtrSerialization=true)
   // m_Owner → GuidRef(owner)；Resolve 前 owner 须已按快照 GUID 注册
5. ResolvePendingObjectRefs
6. RegisterObject(component)   // 快照 component GUID
7. owner->InsertRestoredComponent(comp, componentIndexInOwner)  // 内含 SetOwner；索引与 Capture 一致
8. MarkRenderStateDirty / EoF update
9. 返回 restoredComponent*
```

**禁止** `ApplyAddComponentToSelectedGameObject(类型名)` 空壳路径。

### 10.9 Command 升级

#### 10.9.1 `DeleteGameObjectCommand`

| 字段（新） | 说明 |
|------------|------|
| `m_SnapshotEnvelope` | `CaptureGameObject` 结果 |
| `m_GameObjectId` | 执行时 ID；Undo 后更新为 restored ID |

```text
Execute:
  CaptureGameObject → envelope
  ApplyRemoveGameObjectFromScene(id, ...)   // 可弃用 outName/outTransform，或仅用于描述
Undo:
  RestoreGameObjectToScene → 更新 m_GameObjectId、SelectGameObject(restoredId)
```

**描述：** `Delete GameObject '{name}'`（从快照 metadata 或捕获前读取）。

#### 10.9.2 `RemoveComponentCommand`

| 字段（新） | 说明 |
|------------|------|
| `m_SnapshotEnvelope` | `CaptureComponent` |
| `m_OwnerGameObjectId` | 保留 |
| `m_ComponentTypeName` | 保留（描述 / 校验） |

```text
Execute:
  CaptureComponent → ApplyRemoveComponentFromGO
Undo:
  RestoreComponentToGameObject（禁止空壳 AddComponent）
```

#### 10.9.3 其它 Scene Command

| Command | E1.4 是否改动 |
|---------|----------------|
| Rename / Transform / Add GO / Add Component | **不改**（已满足或 Add 侧完整） |
| `SetObjectPropertyCommand` | **不改**；Inspector 扩展见 §10.10 |

### 10.10 Inspector：ObjectPtr / Array Undo（E1.4 同批）

**原则：** 仍用 **`SetObjectPropertyCommand`** + **单 property Binary blob**；**不** 为每个 ObjectPtr 单独做整对象 Snapshot（除非未来 E1.4+ 优化）。

| 步骤 | 改动 |
|------|------|
| `CanUndoInspectorProperty` | 返回 true：`Primitive` \| `Object` \| `ObjectPtr` \| `Array` |
| Capture / Commit | 已用 `SerializePropertyToBuffer` — 扩展后即可支持 GuidRef / 整数组 |
| `SerializerOptions` | Inspector Capture 使用默认 options；GuidRef 路径 **已** 在 `SerializeObjectPtr` 实现 |

**明确不做：** 嵌套 `Object` **内部**子字段逐字段 Undo（例如直接改 `Transform.Position` 字段行）——整块 `m_Transform` 作为 `Object` 属性可一条 blob Undo；细粒度位移仍优先 Gizmo `SetGameObjectTransformCommand`。

**验收：**

- StaticMesh 上换 Mesh/Material 资产 → Undo 恢复引用。
- 若存在可编辑 `Array` 字段 → 改长度/元素 → Undo 恢复整数组。

### 10.11 拍板结果（2026-05-24，用户确认）

| ID | 决策 | 结果 |
|----|------|------|
| **A** | 快照内 **GUID** | **保留原 GUID**（与 `.mescene`、组件互引一致） |
| **B** | **Runtime `m_ID`** | **新 ID** + Undo 后更新 `DeleteGameObjectCommand::m_GameObjectId` 与 Selection |
| **C** | **Component 插回** | **按 `componentIndexInOwner` 插回** |
| **D** | Inspector Undo 范围 | **Primitive + ObjectPtr + Array + Object 值类型**（嵌套 Object 子字段仍不做 per-field） |
| **E** | 栈深度 | **一条快照 = 一条 Undo**（未单独问询，沿用 E1.1；超大 blob 仅 Warn） |
| **F** | `m_AttachParent` 附件层级 | **E1.4 接受 Undo 后丢失**（不扩反射） |

### 10.12 栈体积与失败策略

- 单条 `DeleteGameObjectCommand` 体积 ≈ 该 GO 全子树 Binary 大小；**仍占 1 条** `MaxUndoStackDepth`。
- Capture / Restore 失败：`ME_CORE_ERROR` + **不** 执行删除（Execute  aborted）或 Undo no-op（与现 Command 一致）。
- Restore 后 `ResolvePendingObjectRefs` 若 **unresolved > 0**：打 **Warn**，Mesh 可能为空；不阻塞 Undo（与 Scene Load 一致）。

**软上限（可选，实现阶段）：** 若 `payload.size() > 4 MiB`，打 Warn；**不** 拒绝入栈（E1.5 Material 再评）。

### 10.13 PostRestore 与渲染

与 `SceneManager::LoadSceneByPath` 对齐：

```cpp
void SceneEditor::PostRestoreSceneObject(GameObject& go)
{
    for (Component* c : /* each component on go */) {
        if (auto* sc = dynamic_cast<SceneComponent*>(c)) {
            sc->MarkRenderStateDirty();
        }
        SceneManager::Get().MarkComponentForNeededEndOfFrameUpdate(c);
    }
    MarkSceneDirty();
}
```

**Editor 帧顺序** 保持 E1.3 修复：`SendAllEndOfFrameUpdates` 在 ImGui 之后，避免 proxy UAF。

### 10.14 测试与验收

| 层级 | 内容 |
|------|------|
| **Runtime** | `SerializationArchiveTest` 增加 `GameObject` round-trip（空 GO、带 StaticMeshComponent + GuidRef，需测试资产或 mock GUID） |
| **Editor CLI** | 可选 `--editor-snapshot-test`：Capture/Restore 不经过 CommandStack |
| **手动** | 删带 Mesh 的 GO → Undo → 视口仍显示；Remove StaticMeshComponent → Undo → Mesh 回来；改 Material 引用 → Undo |

**E1.4 完成勾选：**

- [x] `SerializeObjectToBuffer` / `DeserializeObjectFromBuffer`
- [x] `EditorObjectSnapshot` Capture/Restore（GO：CreateDefaultInstance + InsertRestoredGameObject）
- [x] `DeleteGameObjectCommand` 升级（Remove Component 代码已接，**待手测**）
- [x] Inspector ObjectPtr + struct Undo（§9.10 Transform / AssetRef 手测通过）
- [x] 文档 §12 验收表 E1.4 核心项（本文件 + 序列化 doc）

### 10.15 建议文件清单

| 文件 | 职责 |
|------|------|
| `Runtime/.../Serializer.{h,cpp}` | `SerializeObjectToBuffer` / `DeserializeObjectFromBuffer` |
| `Editor/.../EditorObjectSnapshot.{h,cpp}` | 结构体 + envelope 读写 + Capture/Restore |
| `Editor/.../SceneEditor.{h,cpp}` | `PostRestore*`、Restore 入口 |
| `Commands/Scene/DeleteGameObjectCommand.*` | 存 envelope |
| `Commands/Scene/RemoveComponentCommand.*` | 存 envelope |
| `SceneEditorInspectorSource.cpp` | §9.10 `PropertyUndoCaptureContext` + Capture/Commit |
| `SerializationArchiveTest.cpp` | GO round-trip |
| `SERIALIZATION_BINARY_AND_PROPERTY_API.md` | S3 勾选 |

### 10.16 与 E1.5 的衔接

- `MaterialEdGraph` Undo：Command 存 **`EditorObjectSnapshot`**（`rootClassName = MaterialEdGraph`，payload 为整图）。
- 栈体积与「是否拒绝超大图入栈」在 **E1.5 设计时** 复用 §10.12 经验。

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
- [x] Scene：Rename、Gizmo Transform、GO/Component 增删（Delete GO 真快照 Undo）
- [x] Global Ctrl+Z/Y（视口聚焦可用）

### 待做

- [x] **E1.3** Inspector 属性 Undo（Primitive）
- [x] **E1.3+** Inspector Transform / AssetRef property Undo（§9.10）
- [x] **E1.4** Snapshot + Delete GO Undo（§10）
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
| 2026-05-24 | v4.0：E1.4 Snapshot 详细设计（Binary、`EditorObjectSnapshot`、Command 升级、§10.11 拍板项） |
| 2026-05-25 | v4.1：E1.4 实现勾选；§9.10 Inspector Capture 现状（Transform/AssetRef 漏洞） |
| 2026-05-25 | v4.2：§9.10 PropertyUndoCaptureContext 实现；Transform/AssetRef/Primitive Inspector Undo 验收 |
