# Object

对象系统是 Core 的枢纽：**一切可被引擎跟踪、反射、序列化的实例** 通常继承 `MEObject`。

## 概念来源

`MEObject`、`Outer`、`GetClass()` / `IsA()`、按名查找与函数调用等用法，**参考了 Unreal Engine 的 `UObject` 模型**。minEngine 使用 `std::shared_ptr` 管理生命周期，并由 `ObjectManager` 统一登记，而非 UE 的 GC 对象表。

## MEObject

- **身份：** `GUID`、`Name`、`Outer`（外层对象，类似 UE Outer）。
- **类型：** 运行时指向 `Reflection::MEClass`（由 `GetClass()` / `SetClass()` 维护）。
- **调用：** `InvokeFunction` / `InvokeFunctionTyped` 通过 `MEFunction` + `MEFunctionFrame` 派发（依赖反射元数据）。

友元包括 `ReflectionSystem`、`Serializer`、`AssetManager`、`Editor` 等，以便核心系统访问内部状态而不暴露公共 API。

**入口：** `Runtime/Core/Object/MEObject.h`

## ObjectManager

单例（`ObjectManager::Get()`），职责：

| 能力 | 说明 |
|------|------|
| 注册 / 查找 | 按 `GUID` 或名称保存 `weak_ptr<MEObject>` |
| 创建 | `NewObject<T>()` 设置 Class、Name、Guid、Outer 并注册 |
| 移除 | `RemoveObject` / `UnregisterObject` |
| GC | `CollectGarbage`：清理已失效 weak 项；`CollectGarbageWithEngineRoots` 结合引擎根（资产缓存、场景等）做可达性审计 |

全局便捷函数：`NewObject`、`FindObject`、`FindObjectAs`（定义于 `ObjectManager.h` 末尾）。

**入口：** `Runtime/Core/Object/ObjectManager.h`

## 与其它 Core 模块

- **Reflection：** 每个 `MEObject` 对应一个 `MEClass`；属性迭代、编辑器检视依赖反射。
- **Serialization：** 序列化对象图时记录 GUID 引用；反序列化后由 `Serializer::ResolvePendingObjectRefs` 绑定实例。
- **GUID：** 新对象默认 `GenerateGUID()`（见 [GUID](guid.md)）。
