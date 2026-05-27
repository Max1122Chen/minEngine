# ObjectPtrWidget — 设计案（M3.1 v3 定稿）

Last updated: 2026-05-25  
Status: **已定稿 — 已实施（待 commit）**  
父文档：[EDITOR_APPEARANCE.md](./EDITOR_APPEARANCE.md) §7  

---

## 0) 一句话

**`ObjectPtrWidget::Draw`** 唯一入口：`AllowedClasses`（`const MEClass*`）→ 内联收集（`IsA(Asset)` 分流）→ **`PropertyRefPicker`**（**始终含 None**）→ 写回 + hooks；Meta 第一版不解析。

---

## 1) 扁平结构（定稿）

```text
Inspector / Drawer
  → PropertyEditPolicy + PropertyEditSession
  → ObjectPtrWidget::Draw
       ├─ 默认 Allowed = { valueClass }
       ├─ CollectAssetCandidates / CollectObjectCandidates  // ObjectPtrWidget 内 private static
       ├─ PropertyRefPicker::DrawCombo（首项固定 None；候选可为空）
       └─ 写回 + ObjectPtrWidgetHooks（Scene）
```

| 文件 | 内容 |
|------|------|
| `PropertyRefTypes.h` | `PropertyRefCandidate`、`PropertyRefCandidateKind` |
| `PropertyRefPicker.h/.cpp` | Combo UI |
| `ObjectPtrWidget.h/.cpp` | 收集 + `Draw` + 写回 + hooks |
| `AssetTypeRegistry.*` | `m_AssetTypeIdByClass`；登记时用 **`T::StaticClass()`** |
| `AssetManager.*` | `FindAssetMetasByClass(const MEClass*)` |
| `ObjectManager.*` | `ForEachLiveObject`（Object 收集用） |

**无** `PropertyRefResolve` 独立模块。

---

## 2) `AllowedClasses`

```cpp
using AllowedClasses = std::vector<const Reflection::MEClass*>;
```

第一版：`{ valueClass }`。Meta 将来 `FindClass` → 指针。

---

## 3) 分流

```cpp
if (allowed->IsA(Asset::StaticClass()))
    ObjectPtrWidget::CollectAssetCandidates(allowed, out);
else
    ObjectPtrWidget::CollectObjectCandidates(allowed, out);
```

- Asset 子类 **只**列 Meta 桶，不列 ObjectManager 已加载实例。
- 不用 `HasAssetTypeForClass`；新 Asset 类型由 Runtime **自动注册 AssetType**（`MEClass*` ↔ `AssetTypeId` 一一对应）。

---

## 4) Runtime

### 4.1 `AssetTypeRegistry`

```cpp
// RegisterBuiltinTypes / RegisterType:
m_AssetTypeIdByClass[Texture2D::StaticClass()] = "Texture2D";
// 或 RegisterType 内: FindClass(descriptor.RuntimeClassName) → 指针

std::string_view GetAssetTypeIdForClass(const Reflection::MEClass* assetClass) const;
```

 Builtin 登记 **优先 `T::StaticClass()`**，与反射一致。

### 4.2 `AssetManager`

```cpp
std::vector<const AssetMeta*> FindAssetMetasByClass(const Reflection::MEClass* assetClass) const;
```

### 4.3 `ObjectManager`

```cpp
void ForEachLiveObject(const std::function<void(const std::shared_ptr<MEObject>&)>& visitor) const;
```

`CollectObjectCandidates`：`visitor` 内 `obj->GetClass()->IsA(allowedClass)`。

---

## 5) `PropertyRefPicker`

仅 ImGui Combo；**固定**在列表首项提供 **None**（清空 `shared_ptr`），无开关、无按 Editor 关闭 None 的路径。

---

## 6) `ObjectPtrWidget`

```cpp
struct ObjectPtrWidgetHooks { /* Undo / SetMesh / Dirty */ };

bool Draw(const Reflection::MEProperty& property,
          void* propertyPtr,
          const PropertyEditSession& session,
          const ObjectPtrWidgetHooks& hooks = {},
          float itemWidth = -1.0f);
```

**None 写回：** `selected == nullptr` → `valueClass->SetSharedPtr({})`（不经 `TryApplySelection`）。  
**非 None：** `AssetMeta` → `LoadAssetByPath`；`LiveObject` → `SetSharedPtr`；Scene 对 `m_Mesh` / `m_Material` 优先 `TryApplySelection`。

---

## 7) Meta（第一版）

`AllowedClasses`、`ReferenceKind` — **预留，不解析**。

---

## 8) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-25 | v3 定稿：`StaticClass` 登记；`Collect*` 并入 `ObjectPtrWidget`；去掉 `PropertyRefResolve` 文件 |
| 2026-05-25 | ObjectPtr **一律可 None**：移除 `allowNone` 参数；Scene / Material 调用方不再区分 |
