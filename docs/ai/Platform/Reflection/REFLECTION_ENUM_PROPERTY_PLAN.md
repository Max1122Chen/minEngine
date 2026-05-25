# Reflection — Enum 属性存储信息补全

Last updated: 2026-05-25  
Status: **已实施（master）**  
关联：[Editor Appearance — M3-Enum 后置](../Editor/EDITOR_APPEARANCE.md)、`MEEnum` / `MEPrimitiveProperty`、`PropertyEnumWidget`（Editor scaffold）

---

## 1) 背景

`MEPrimitiveProperty` 上的 enum 字段在 `ResolvePendingEnumPropertyRefs` 之后仅有 `primitiveTypeName = MEEnum::GetName()`，**没有**绑定的 `MEEnum*` 与 C++ 字段 **内存大小**。

Editor `PropertyEnumWidget` 因此只能用枚举名字启发式猜 `sizeof`，不安全；已在 appearance 分支 **scaffold、未接线**，待本计划落地后再启用。

---

## 2) 目标与非目标

### 2.1 目标

| 项 | 说明 |
|----|------|
| `MEEnum` | 注册时记录该枚举类型的 **`Size`**（`sizeof(TEnum)`） |
| `MEPrimitiveProperty` | Finalize 后绑定 **`const MEEnum*`**，可通过属性查询 **`Size`** |
| Editor | merge 后 `PropertyEnumWidget` 读反射 **`Size`**，去掉名字启发式 |

### 2.2 非目标

- 不改 JSON/Binary 序列化 enum 路径（另开任务）
- 不在 master 上接线 `PropertyEnumWidget`
- 不暴露单独的 `underlying_type` 元数据（`sizeof(TEnum)` 已足够读写字段）

---

## 3) 拍板（2026-05-25）

| 项 | 决定 |
|----|------|
| 命名 | 接口与字段统一用 **`Size`** / **`SetSize`** / **`GetSize`**（不用 `StorageSize` 等长名） |
| Header tool | **不需要** 为 enum 改 codegen 捕获「继承关系」 |
| 注册点 | 在 **`ReflectionSystem::RegisterEnum<TEnum>`** 模板内 `SetSize(sizeof(TEnum))` |

---

## 4) 为何不用改 Header Tool

- Enum 属性在 **编译期** 通过 `AddPendingEnumProperty<RawFieldType>` 登记，Finalize 时用 `type_index` 解析到已注册的 `MEEnum`。
- **`Size` 来自 `sizeof(TEnum)`**，在 `RegisterEnum<TEnum>` 实例化时固定，已包含：
  - `enum class : uint8_t`
  - 非 scoped enum（通常按 `int` 宽度）
  - 各枚举类型彼此独立，**不存在 class 式的 super 链**，也无需反射「enum 继承」。
- 现有 `ME_REFLECTION_ENUM_DEFINE_*` 宏最终调用 `RegisterEnum<ENUM_TYPE>(meEnum)`；在 **Runtime 模板** 里补 `SetSize` 即可，**不必**改 `minEngine_header_tool` 生成逻辑。

若未来有「属性类型 → enum」的代码生成扩展，仍应走同一 `RegisterEnum<T>` 路径，而不是在 tool 里重复算 size。

---

## 5) Runtime 改动清单

### 5.1 `MEEnum`（`MEEnum.h`）

```text
size_t m_Size = 0;
void SetSize(size_t size);
size_t GetSize() const;
```

### 5.2 `ReflectionSystem::RegisterEnum<TEnum>`（`Reflection.h`）

注册成功后：

```text
enumInfo->SetSize(sizeof(TEnum));
```

### 5.3 `MEPrimitiveProperty`（`MEProperties.h`）

```text
const MEEnum* boundEnum = nullptr;

bool IsEnum() const;
const MEEnum* GetEnum() const;
size_t GetSize() const;   // enum 属性：boundEnum->GetSize()；非 enum：0
```

### 5.4 `ResolvePendingEnumPropertyRefs`（`Reflection.cpp`）

解析成功时：

```text
property->primitiveTypeName = resolvedEnum->GetName();
property->boundEnum = resolvedEnum;
```

### 5.5 Finalize 校验（可选但建议）

- `IsEnum()` 且 `GetSize() == 0` → 记反射错误，避免静默坏数据。

---

## 6) Editor 后续（`feat/editor-appearance`，依赖 master）

1. `git merge master`（或 cherry-pick 反射 commit）
2. `PropertyEnumWidget`：`GetSize()` 改用 `primitiveProperty.GetSize()` / `GetEnum()->GetSize()`
3. `PropertyValueWidget` 恢复 enum 分支（在 primitive 分发最前）
4. 更新 `EDITOR_APPEARANCE.md`：M3-Enum 依赖项勾选

---

## 7) 验收

| 检查 | 期望 |
|------|------|
| `MaterialShadingModel` 字段 | `IsEnum()==true`，`GetSize()==1` |
| `MaterialBlendMode` 字段 | `GetSize()==1` |
| 非 scoped 大枚举（如 `InputKeyAction`） | `GetSize()==sizeof(int)`（典型为 4） |
| `ReflectionSystem::Finalize` | 无 enum size 相关 error |
| Editor（接线后） | Combo 改值不踩邻接内存 |

**临时冒烟（master）：**

```text
libminEngine.dll / minEngine.exe --reflection-enum-property-test
```

（具体可执行名以工程输出为准；日志应打印 `ReflectionEnumPropertyTest: all tests passed.`）

---

## 8) Commit 建议

**分支：** `master`  
**Subject：** `feat(reflection): bind MEEnum and size on enum properties`

**范围：** `MEEnum.h`、`MEProperties.h`、`Reflection.h`、`Reflection.cpp`（仅 Runtime 反射）

---

## 9) 与并行分支关系

```text
master          → 本计划（enum Size）
master          → merge feat/editor-asset-workflow（Asset P1）
feat/editor-appearance → merge master → M3-Enum 接线 + M3.1 ObjectRef
```

---

## 10) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-25 | 初稿；拍板 Size 命名、不改 header tool |
| 2026-05-25 | Runtime 实现（`MEEnum`/`MEPrimitiveProperty` Size + boundEnum） |
