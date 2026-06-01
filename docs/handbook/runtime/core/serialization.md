# Serialization

序列化在**反射元数据**驱动下，将 `MEObject` 及 `ME_STRUCT` 等实例写入存档或从存档恢复。

## 架构

| 组件 | 说明 |
|------|------|
| `Serializer` | 核心：按类名 + 对象指针遍历属性；处理 GUID 引用与延后绑定 |
| `WriterArchive` / `ReaderArchive` | 抽象接口（对象、字段、数组、标量） |
| `JsonArchive` | JSON 读写（底层 **[nlohmann/json](https://github.com/nlohmann/json)**，`Serialization/Json.h`） |
| `BinaryArchive` | 二进制格式 |
| `PrimitiveCodecRegistry` | 基本类型编解码 |

`Serialization/Legacy/` 为旧序列化路径，**新存档格式应使用当前 `Serializer` + Archive**。

## 对象引用

反序列化时，对象指针字段可能暂时无法解析。`Serializer` 收集 `PendingObjectRef`（GUID、期望类型、字段路径），在场景/材质等**加载单元完成**后调用：

`Serializer::ResolvePendingObjectRefs(unresolvedRefs)`

这与 UE 中先加载再 Fixup 引用的思路类似，实现独立。

## 典型用法

- `Serialize` / `Deserialize` + 任意 `WriterArchive`/`ReaderArchive` 实现
- `ToFile` / `FromFile` 封装文件路径
- 选项见 `SerializerOptions`（`SerializationTypes.h`）

序列化过程会配合 `ObjectManager` 查找 GUID 对应实例。

**入口：** `Serialization/Serializer.h`, `Serialization/Archive.h`
