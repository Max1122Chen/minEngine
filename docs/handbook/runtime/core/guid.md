# GUID

`GUID` 是引擎内常用的 **128 位标识**（`High` / `Low` 两个 `uint64_t`），用于对象的唯一标识以及 `MEObject`、资产引用与序列化中的对象指针还原。

## 反射

`GUID` 本身为 `ME_STRUCT()`，字段带 `ME_PROPERTY()`，可参与序列化与编辑器展示。生成代码见 `GUID.gen.h`（构建期）。

## 运算与容器

- `ToString()` / 比较运算符、 `std::hash<GUID>`（`GUID::Hash`）供 `unordered_map` 使用（如 `ObjectManager::m_ObjectsByGuid`）。
- `GenerateGUID()` 用于创建新对象时的默认 ID。

`Runtime/Core/Hash/Hash.h` 提供通用的 `HashCombine` 模板（Boost 风格种子混合），供其它哈希场景使用，**与 GUID 无强绑定**。

**入口：** `Runtime/Core/GUID/GUID.h`, `Runtime/Core/Hash/Hash.h`