# 函数反射 — 现状说明（P4）

Last updated: 2026-05-27  
Status: **仅记录当前实现现状，详细设计待讨论**  
父文档：[Platform 路线图](../PLATFORM_ROADMAP.md) §2 P4、§11  
关联：[内存管理](../MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md)、[序列化扩展](../Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md)

---

## 0) 范围说明

- 本文 **只描述当前反射系统的结构与能力**，尤其是与「未来函数反射」相关的部分。  
- **不** 给出 `MEFunction` / 委托 / Lua 的详细设计，也 **不** 规定阶段切分；这些内容将在与你讨论之后另外补充。  
- 委托与 Lua 会放在各自文档中占位，仅在路线图中作为后续阶段出现。

---

## 1) 总体结构（与 UE 的类比）

当前反射分三层：

- **标记层（Annotations）**：头文件使用 `ME_CLASS`、`ME_STRUCT`、`ME_ENUM`、`ME_PROPERTY`、`ME_GENERATED_BODY` 作为「纯标记」宏（定义见 `ReflectionAnnotations.h`），只为 Python header tool 提供锚点，宏本身在编译时几乎为空。  
- **生成层（Header Tool）**：`scripts/minEngine_header_tool.py` 扫描包含这些标记的 `.h`：
  - 生成 `*.gen.h`：为每个类型专门化 `FieldAccessor<T>`，其中包含每个字段的 `GetConst_FIELD` / `GetMutable_FIELD` 静态函数。
  - 生成 `*.gen.cpp`：在匿名静态 lambda 中使用 `ME_REFLECTION_CLASS_DEFINE_BEGIN/END` 宏，为每个类型调用 `ReflectionSystem::CreateClass`、`AddFieldByType` 等完成注册。
- **运行时层（ReflectionSystem）**：`Runtime/Core/Reflection/Reflection.{h,cpp}` 中的单例 `ReflectionSystem` 维护所有 `MEClass` / `MEProperty` / `MEEnum` 对象，并在 `FinalizeReflection()` 里解析继承、属性类型和枚举绑定。

和 UE 类比：

| UE 概念 | 当前 minEngine 概念 |
|---------|----------------------|
| UHT + *.generated.h/.cpp | `minEngine_header_tool.py` + `*.gen.h/.gen.cpp` |
| `UClass` / `UStruct` | `MEClass` / `MEStruct` |
| `UProperty` | `MEProperty`（Primitive/Object/ObjectPtr/Array） |
| 反射初始化 / 注册 | `ReflectionSystem::FinalizeReflection()` |

当前 **没有** 等价的 `UFunction` 或 `ProcessEvent`。

---

## 2) 标记与生成文件形态

### 2.1 标记宏（`ReflectionAnnotations.h`）

- `ME_CLASS(...)`、`ME_STRUCT(...)`：用于标记一个类型需要反射信息。宏本身为空，但会被 header tool 识别。  
- `ME_PROPERTY(...)`：紧贴在某个字段前，表示该字段需要生成反射 `MEProperty`；参数中可以包含如 `Invisible`、`EditAnywhere` 等 specifier。  
- `ME_GENERATED_BODY(Type)`：插入 `StaticClass()` 声明，并把 `FieldAccessor<Type>` 设为友元，方便通过 accessor 函数访问私有字段。

`MEObject` 的头文件中可以看到典型用法（`ME_CLASS()`、`ME_PROPERTY(Invisible)`、`ME_GENERATED_BODY(MEObject)`）。

### 2.2 Header Tool 行为（`minEngine_header_tool.py`）

- 通过正则识别 class/struct/enum 声明和 `ME_PROPERTY` 行，构建 `ClassMeta` / `PropertyMeta` / `EnumMeta`。  
- 为每个类生成：
  - `*.gen.h`：`ME_REFLECTION_ACCESSOR_BEGIN/END` 包裹的 `FieldAccessor<T>` 专门化，其中每个属性使用 `ME_REFLECTION_ACCESSOR_FIELD` 生成 `GetConst_FIELD` / `GetMutable_FIELD`。  
  - `*.gen.cpp`：`ME_REFLECTION_CLASS_DEFINE_BEGIN/END` 包裹的静态注册 lambda，内部调用：
    - `ME_REFLECTION_CLASS_SUPER` 注册父类（通过 `AddPendingSuperClass` 记录类型索引）。
    - `ME_REFLECTION_CLASS_ADD_FIELD` 对每个 `ME_PROPERTY` 字段添加反射属性（最终通过 `ReflectionSystem::AddFieldByType` 创建 `MEProperty`）。
- enum 使用对应的 `ME_REFLECTION_ENUM_*` 宏和工具逻辑生成 `MEEnum` 反射数据，并在 `RegisterEnum<TEnum>` 时写入枚举大小和序列化所需信息。

当前 tool **只处理属性和枚举**，并没有扫描或生成任何「函数反射宏」相关的代码。

---

## 3) 运行时类型系统：`MEClass` 与 `MEProperty`

### 3.1 `MEClass`（`MEClass.h`）

- 继承自 `MEStruct`，基础字段只有一个名字 `m_Name`。  
- 提供：
  - 继承关系：`GetSuperClass()`、`IsA(const MEClass*)`、`IsA<T>()` 和 `GetDirectDerivedClasses()`。  
  - 工厂和类型擦除：`CreateDefaultInstance()`（通过 `MEClassFactoryFn`）、`CastObject(void*)`、`SetSharedPtr`（用于通过 `shared_ptr<void>` 赋值到特定类型）。  
  - 注解：`ClassSpecifier`（`Abstract`、`Transient`、`EditorOnly` 等）、`ClassMetadata`（字符串键值对）。
- 内部维护：
  - `std::vector<MEProperty*> m_Properties`：该类直接声明的属性列表。  
  - `std::vector<MEClass*> m_DirectDerivedClasses`：Finalize 后填充的直接子类列表。

重要的是：**当前 `MEClass` 不包含任何「函数列表」或与成员函数相关的元数据**。

### 3.2 `MEProperty`（`MEProperties.h`）

- 抽象基类，提供：
  - 名字、访问器（`FieldConstAccessorFn` / `FieldMutableAccessorFn`）  
  - Specifier（`EditAnywhere`、`Invisible`、`Instanced` 等）和 Metadata（键值对）  
  - `GetCategory()`（Primitive / Object / ObjectPtr / Array）
- 各子类：
  - `MEPrimitiveProperty`：保存 `primitiveTypeName`，在 enum 绑定后可通过 `GetEnum()`、`GetSize()` 获取枚举信息。  
  - `MEObjectProperty`：保存 value class 指针（`MEClass*`），用于内嵌 struct / object。  
  - `MEObjectPtrProperty`：在 `MEObjectProperty` 基础上多了指针类别（Raw / Shared）以及指向数据的 accessor。  
  - `MEArrayProperty`：包装 `std::vector<T>`，保存对内部元素属性的引用以及数组访问函数。

`ReflectionSystem::CreatePropertyByType` 根据字段 C++ 类型（含 `std::vector`、指针、`shared_ptr`、enum、math 向量等）自动推导出对应的 `MEProperty` 子类和枚举绑定。

---

## 4) `ReflectionSystem` 生命周期与使用方式

### 4.1 注册期 → Finalize

- 启动时，各 `*.gen.cpp` 的静态 lambda 在 C++ 静态初始化期运行，调用：
  - `ReflectionSystem::CreateClass` / `CreateEnum` 创建类型对象。  
  - `AddPendingSuperClass`、`AddPendingPropertyClass`、`AddPendingEnumProperty` 记录需要在 Finalize 时解析的依赖。  
  - `RegisterClass<T>` / `RegisterEnum<TEnum>` 完成类型登记。
- `Engine::Initialize` 以及测试入口（如 `ObjectManagerTest`、`SerializationArchiveTest`、`MaterialIRTest` 等）会调用 `ReflectionSystem::FinalizeReflection()`：
  - 解析所有 PendingSuperClass，填充 `MEClass::m_SuperClass`。  
  - 校验继承图无环（`ValidateInheritanceGraph`）。  
  - 解析所有 PendingPropertyClass，给 Object/ObjectPtr/Array 属性绑定 `MEClass`。  
  - 解析枚举属性绑定（`ResolvePendingEnumPropertyRefs`）并为 enum 设置序列化 codec。  
  - 构建派生类链表（`BuildDerivedClassLinks`）。

Finalize 成功后，`GetState() == Ready`，可以通过：

- `FindClass(name)` / `FindClass<T>()` / `GetAllClasses()` / `GetDerivedClasses(...)`  
- `ForEachPropertyInHierarchy(...)` 遍历某个基类及其子类中的所有属性。

### 4.2 消费者（当前）

当前使用反射数据的子系统主要包括：

- **序列化**：`Serializer` / `JsonArchive` / `BinaryArchive` 通过属性列表读写对象字段、处理 ObjectPtr/GUID 引用与 Instanced 子对象。  
- **Inspector 与 Undo**：Editor 侧使用 Property 信息驱动属性面板、记录快照、生成 Undo 命令。  
- **Asset 系统**：部分资源类型（如 Material graph）通过反射字段进行存储和编辑器集成。

这些路径都只依赖 `MEClass` + `MEProperty`，**完全不知道函数的存在**。

---

## 5) `MEObject` 与反射的关系

- `MEObject` 头文件中使用 `ME_CLASS()` 与 `ME_GENERATED_BODY(MEObject)`；生成的 `MEObject.gen.cpp` 注册了名字和两个属性（`m_Name`、`m_Guid`）。  
- 运行时每个 `MEObject` 实例持有一个 `const MEClass* m_Class`，用于：
  - `GetClass()` / `IsA(const MEClass*)` 查询类型信息。  
  - 序列化与 Inspector 获取该对象的 Property 列表。  
- `MEObject` 还持有 `MEObject* m_Outer`，用于表达所有权结构，与内存管理与序列化设计文档保持一致。

**当前不存在**：

- `MEObject::ProcessEvent` 或等价的「按名字查找 + 调用函数」接口。  
- 任何与「成员函数」相关的反射元数据结构。

---

## 6) 与未来函数反射/委托/Lua 的接口面

本节只是从当前实现角度，标出未来可以挂接的「接口点」，**不做具体设计**：

- `MEClass`：可以自然地新增「函数列表」或查找 API（类似属性列表），不影响现有字段反射。  
- `ReflectionSystem`：已有 Collecting → Finalizing → Ready 的注册模式；未来函数元数据也可以复用同一生命周期，在 Finalize 时完成解析与校验。  
- Header Tool：目前只扫描 `ME_CLASS` / `ME_PROPERTY`；未来若引入 `ME_FUNCTION` 之类的标记，可以复用现有 `.gen.h/.gen.cpp` 生成路径。  
- `MEObject`：目前只持有 `m_Class`；未来如果增加 `ProcessEvent`，可以通过 `MEClass` 中「函数元数据」进行分发，而不破坏现有属性反射使用者。

详细的 `MEFunction` / 委托 / Lua 设计将基于上述现状，在单独的讨论后按阶段补充到对应文档中。

---

## 7) 后续文档（占位）

- **函数反射详细设计（本文件后续章节）：** 讨论并拍板后，补充「阶段目标」「调用约定」「与 Lua 的关系」「测试策略」等内容。  
- **委托设计：** 独立文档 `REFLECTION_DELEGATES_DESIGN.md`（占位中），在函数反射落地后再展开。  
- **Lua 脚本设计：** 独立文档 `LUA_SCRIPTING_DESIGN.md`（仅占位概述），依赖函数反射完成之后再细化。

