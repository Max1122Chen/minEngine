# Reflection

反射系统描述**运行时类型信息**：类层次、属性、枚举、函数签名。编辑器检视、序列化、部分测试与脚本扩展都依赖这一套元数据。

## 概念来源

整体设计**参考 Unreal Engine 的反射与 `UCLASS` / `UPROPERTY` 工作流**（注册期收集、生成辅助代码、按类遍历属性）。宏命名与数据模型（`MEClass`、`MEProperty`、`MEEnum`、`MEFunction`）为 minEngine 自有实现，构建时生成 `*.gen.h`，**不**使用 UE 头文件或模块。

旧版实现保留在 `Reflection/Legacy/`（手动类型表、与 UE 差异较大），**新代码应只使用当前 `ReflectionSystem`**。

## 核心类型

| 类型 | 作用 |
|------|------|
| `ReflectionSystem` | 单例；注册类/枚举、Finalize、按名/按 `type_index` 查找 |
| `MEClass` | 类元数据：父类、`IsA`、属性列表、工厂函数、函数表 |
| `MEProperty` / `MEObjectProperty` 等 | 属性描述与访问器 |
| `MEEnum` | 枚举反射 |
| `MEFunction` | 函数签名、`Invoke`、签名哈希（重载区分） |
| `MEStruct` | 可作为属性的结构体类型 |

注册阶段有 `PendingSuperClassRef` 等结构，在 **Finalize** 时解析前向依赖。

## 宏与代码生成

开发者侧主要使用 `ReflectionMacros.h`：

- `ME_CLASS()`、`ME_STRUCT()`、`ME_ENUM()`
- `ME_PROPERTY()`、`ME_FUNCTION()`
- `ME_GENERATED_BODY(TypeName)` → 展开到 `*.gen.h` 中的注册逻辑

`FieldAccessor<T>` 模板在编译期绑定字段指针，供通用序列化/编辑器读写。

**入口：** `Reflection/Reflection.h`, `Reflection/MEClass.h`, `Reflection/ReflectionMacros.h`

## 与序列化、对象

- `Serializer` 按 `MEProperty` 类型分派读写；对象指针写 GUID。
- `MEObject::GetClass()` 提供序列化根类型名与属性遍历起点。
- `Math` 中的 `Vector2/3/4` 等在反射层常按**基本类型**处理（与 UE 将 `FVector` 当结构体的做法类似，属简化策略）。

## 函数反射（进行中能力）

`MEFunction`、`MEFunctionFrame`、Native Thunk 模板支持按名/按签名调用；示例见 `ReflectionSample.*`。能力边界以代码与测试为准，手册不展开 API 列表。
