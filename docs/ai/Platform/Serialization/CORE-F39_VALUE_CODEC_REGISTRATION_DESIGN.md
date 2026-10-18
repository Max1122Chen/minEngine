# CORE-F39 Value Codec Registration — Design Spec

## Meta
- **ID:** `CORE-F39`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Cancelled`
- **Owner:** project maintainer
- **Last updated:** 2026-10-18
- **Related:** [Feature Registry](../../FEATURE_REGISTRY.md) · 替代路径 [GP-F03](../../Gameplay/GP-F03_GAMEPLAY_TAG_MESTRUCT_DESIGN.md) · 历史动机见下文（勿再当 backlog）

## Cancellation

**2026-10-18：** 维护者确认根因是 `GameplayTag` 句柄形状偏离 UE（只存 Manager* + index，名字不在 Tag 上），不是缺少可扩展 value-codec。纠正路径为 `GP-F03`（`ME_STRUCT` + `TagName`）。本 Feature **Cancelled**，不实现。将来若出现「真·非 struct 值类型」再重新开 Feature，勿复活本文勾选。

## TL;DR（历史）

反射把「非 `ME_STRUCT` / `ME_CLASS` 的值类型」认成未注册 class；曾规划用特征 + 运行期表统一 builtin 与外部 codec。**已取消**——见 Cancellation。

## Problem

`ME_PROPERTY` 的类型分类在 `ReflectionSystem::CreatePropertyByType` 里是一条封闭链：`vector`、指针、`kIsPrimitiveLike`、动态多播、最后才是 class。`kIsPrimitiveLike` 只承认算术类型、`std::string`、`Vector*`、`Matrix*` 和枚举。其余 class 会变成 `MEObjectProperty`，收口时按 `ME_CLASS` 解析。

因此一个普通值类型（有默认构造、按值存放、不该展开成对象字段）无法作为属性进出序列化。失败点在反射收口：`Unresolved property type`，还没走到读写。

序列化其实已经有按类型名查找的 `PrimitiveCodec`（`write` / `read` 函数对）。但它只被 `RegisterDefaultCodecs` 填 builtin，而且类型名字符串在反射里又抄了一遍。外部模块无法声明「这个类型走 codec」，也无法把 codec 注册进来，除非改 Core 的那两张表。

## Requirements

- 任意完整类型可以在**自己的头文件**里声明自己是值 codec 类型，并给出稳定类型名。Core 的属性分类代码不出现该类型的名字。
- 该类型的读写函数由**拥有它的模块**在序列化之前注册到同一张表。Core 不包含该模块的头文件。
- 现有 builtin（`GetPrimitiveName` 已特化的算术类型、`std::string`、`Vector2/3/4`、`Matrix3/4`）改为这条路径的客户。`CreatePropertyByType` 不再为它们保留按类型分支的名字链。
- `vector<T>` 的元素若是值 codec 类型，数组元素走同一个 codec。不另写数组专用注册。
- 已有资产的 JSON 字段形态不变：类型名字符串与 codec 写出的 JSON 形状都与今天一致（`bool`、`int32`、`Vector3` 数组等）。
- 缺 codec、读写返回失败、重复注册，都有确定的失败行为（见 Failure Model）。

## Constraints

- 分类发生在 `ME_PROPERTY` 实例化时，是编译期决定。运行期注册表只能提供读写，不能事后把一个已经建成的 `MEObjectProperty` 改成值类型。
- 稳定类型名就是磁盘和 `MEPrimitiveProperty::primitiveTypeName` 上的键。改名等于改存档格式，本期禁止改已有名字。
- 进程今天是单线程序列化。注册表不加锁。
- 前向开发：不为旧 API 留长期双轨。`PrimitiveCodecRegistry` 改名为 `ValueCodecRegistry`，调用点一起改。

## Preferences

- builtin 的注册仍集中在「第一次取注册表时」完成，避免每个数学类型一个静态初始化器。
- 编辑器里按类型名选 Widget 的那张表（`PropertyPath`）本期不动。

## Assumptions

- 游戏或 Gameplay 模块的反射生成单元会包含该值类型的头，因此特征特化在 `CreatePropertyByType<T>` 实例化前可见。这与现有动态多播特征的可见性规则相同。
- `int` 与 `int32_t` 在本仓库目标平台上是同一类型；wire 名继续是 `int32`。若某平台二者不同，两个特征都返回 `"int32"`，codec 只注册一次。

## Non-Goals

- `GameplayTag` / `GameplayTagContainer` 的特征、codec、词表解析。那是 `GP-F03`。
- 把值类型做成 `ME_STRUCT` / `ME_CLASS`，或把 `m_Index` 一类运行时句柄写进存档。
- 指针、`shared_ptr`、`unique_ptr` 指向值 codec 类型。它们仍走对象指针分类。
- 枚举改走用户注册。枚举继续由 `ME_ENUM` + `SetCodecForEnums` 按宽度生成 codec。
- Inspector / 属性面板为未知值类型补 Widget。未知类型名在编辑器里维持今天的 unsupported。
- 允许后注册的 codec 覆盖 builtin。
- 改 Binary / JSON 的对象包络、`$schemaVersion`、或某个 builtin 的数组长度与元素类型。

## Current Architecture

分类（编译期），`Reflection.h` 的 `CreatePropertyByType`：

1. `std::vector` → `MEArrayProperty`，元素递归进同一函数。
2. 指针 / `shared_ptr` → `MEObjectPtrProperty`，收口时解析 pointee 的 `ME_CLASS`。
3. `kIsPrimitiveLike` → `MEPrimitiveProperty`。类型名来自一串 `if constexpr (is_same_v<T, ...>)`，枚举先记 `UnresolvedEnum`，收口后改成枚举名。
4. 动态多播特征 → 委托属性。
5. `std::is_class` → `MEObjectProperty`，收口时必须找到 `ME_CLASS`。

`kIsPrimitiveLike`（`TypeTraits.h`）是第二张封闭表。`GetPrimitiveName<T>`（`ReflectionUtils.h`）是第三张，字符串必须和 codec 注册用的别名一致。`MEFunction` 的签名 token 再问一次 `kIsPrimitiveLike` + `GetPrimitiveName`。

读写（运行期）：`Serializer` 对 `MEPropertyCategory::Primitive` 只做 `ValueCodec` 查找，键是 `primitiveTypeName`。builtin 在 `RegisterDefaultCodecs` 里写死。枚举在反射 `Finalize` 时由 `SetCodecForEnums` 按 `MEEnum` 宽度注册。查找只用稳定名字；`typeid(T).name()` 别名有写入、没有读者。

模块方向：反射收口已经调用序列化注册表（枚举 codec）。序列化不包含 Gameplay。

## Proposed Architecture

两段式，中间只共享一个稳定类型名。

**编译期特征** `ValueCodecTraits<T>`（轻量头，不包含 Archive）：

- 主模板 `kIsValue = false`。
- 特化提供 `kIsValue = true` 和 `TypeName()`，返回非空字符串字面量。
- 宏 `ME_DECLARE_VALUE_CODEC(Type, "Name")` 展开该特化，写在类型头里、类型完整之后、`namespace minEngine` 内。外部类型用这个宏；Core 不改宏实现就能接纳它。

**分类**只问特征，不再点名 builtin：

1. `vector`（不变，元素递归）。
2. 指针（不变）。
3. 枚举（不变：`UnresolvedEnum`，稍后绑定 `MEEnum`）。
4. `ValueCodecTraits<T>::kIsValue` → `MEPrimitiveProperty`，`primitiveTypeName = TypeName()`。
5. 动态多播（不变）。
6. class → 对象属性（不变）。
7. 其余：算术类型 `static_assert`（今天未列名的算术类型本来就会在名字链里失败）；其它类型记反射错误。

`kIsPrimitiveLike` 改为 `ValueCodecTraits<T>::kIsValue || is_enum`，并改名为 `kIsValueCodecLike`。`GetTypeName` 与 `MEFunction` 签名只认这个特征，避免第四张表。`GetPrimitiveName<T>` 对值 codec 类型改为返回 `TypeName()`，编辑器里已有的字符串比较继续用同一份字面量。

**运行期表** `ValueCodecRegistry`（由 `PrimitiveCodecRegistry` 改名，职责不变：按类型名保存函数对）：

```text
template<typename T>
bool RegisterValueCodec(ValueCodec codec);
```

`static_assert` 要求 `kIsValue`。键是 `TypeName()`，调用方不另传字符串，避免特征和注册各写一遍。`ValueCodec` 仍是 `{ write, read }`，签名与今天的 `PrimitiveCodec` 相同。

builtin 特化放在 Core 的特征头里（这些类型本来属于 Core）。它们的 `write` / `read` 仍在注册表的 `RegisterDefaultCodecs` 中，通过 `RegisterValueCodec<T>` 登记。函数体与今天的 JSON 形状一致，只是去掉手写别名列表。

外部类型：头文件里 `ME_DECLARE_VALUE_CODEC`；拥有者在自己的 `.cpp` 或模块初始化里调用 `RegisterValueCodec<T>`。Core 的 `.cpp` 不包含该头。

枚举不走 `ValueCodecTraits`。`SetCodecForEnums` 继续按枚举名注册，查找路径仍是 Primitive 类别的同一张表。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `ValueCodecTraits<T>` | 声明 T 是否为值 codec，以及稳定类型名 | 读写、词表、对象图 |
| `CreatePropertyByType` | 按特征建成 `MEPrimitiveProperty` 或沿用对象/指针/数组/枚举路径 | 调用具体类型的构造或解析 |
| `ValueCodecRegistry` | 保存并按类型名查找 `write`/`read`；首次获取时登记 builtin | 决定某个 class 字段该不该走 codec |
| builtin codec | 维持现有 JSON 形状 | 认识 Core 以外的类型 |
| 类型拥有者（Gameplay 包或游戏模块） | 在自己的头里声明特征，在序列化前注册 codec | 修改 `CreatePropertyByType` 的分支 |

## Boundaries

- Allowed：反射头包含轻量特征头；序列化注册表头包含特征头；类型拥有者包含这两头；`Serializer` 只按 `primitiveTypeName` 查找。
- Forbidden：`CreatePropertyByType`、`TypeTraits.h`、`RegisterDefaultCodecs` 包含或点名 Core 以外的值类型。特征头包含 `Archive.h` 或具体 codec。同一类型既 `ME_CLASS`/`ME_STRUCT` 又 `ME_DECLARE_VALUE_CODEC`（值路径在 class 路径之前，对象字段会被跳过）。

## Dependencies

- Depends on：现有 `Serializer` 的 Primitive 分支、`MEPrimitiveProperty::primitiveTypeName`、枚举 codec 注册。
- Depended on by：下一步 `GP-F03`。本期没有新的产品客户。
- New dependency direction justified because：类型拥有者单向依赖 Core 的特征与注册表。Core 不反向依赖拥有者。这是今天 builtin 表做不到的方向。

## Ownership & Lifetime

- 注册表是进程内单例，条目在进程内一直有效。没有按 Scene 或按资产的生命周期。
- builtin 在第一次 `Get()` 时写入。外部条目在拥有者调用 `RegisterValueCodec` 时写入。
- codec 函数不持有堆对象。调用方保证 `void*` 指向活着的 `T`，大小与属性上的 `sizeof(T)` 一致。
- 关引擎不要求逐个反注册。进程退出即丢弃。

## Data Flow

声明：类型头特化特征 → 反射实例化时把 `TypeName()` 写进 `MEPrimitiveProperty`。

写出：`Serializer` 用该名字取 codec → `write(archive, &field)`。数组则对每个元素重复这一步。

读入：`read(archive, &field)` 就地覆盖字段存储。失败则本字段反序列化失败，不留下「半解析」的约定之外的值；codec 自己若已写入一半，视为 codec 的 bug，调用方不再回滚单个 primitive。

## Control Flow

- 反射生成代码调用 `CreatePropertyByType<Field>`。没有回调。
- 模块初始化或 `.cpp` 静态初始化调用 `RegisterValueCodec<T>`。该调用会先 `Get()`，从而先完成 builtin 登记。
- `Serializer` 同步调用函数指针。没有队列。
- 枚举仍在 `FinalizeReflection` 末尾由 `SetCodecForEnums` 登记。

## State & Invariants

- 键一旦登记，进程内不再更换函数指针。
- 每个 `kIsValue` 类型的 `TypeName()` 非空，且与注册键相同。
- 已有 builtin 的 `TypeName()` 与今天的 `GetPrimitiveName` 逐字相同。
- 值 codec 类型不会同时是待解析的 `ME_CLASS`。
- 非法组合：两个不同类型共用一个新类型名（后登记失败，先登记的 codec 留下）。builtin 之间今天已经共享的别名（仅 `int` / `int32_t` 的 `"int32"`）允许，因为是同一键、同一 codec。

## API / Interface Semantics

`ME_DECLARE_VALUE_CODEC(Type, NameLiteral)`

- Responsibility：在 `minEngine` 内特化 `ValueCodecTraits<Type>`。
- Inputs：完整类型；非空字符串字面量。
- Errors：空名字在特化上 `static_assert`。
- Sync：编译期。

`ValueCodecRegistry::RegisterValueCodec<T>(ValueCodec)`

- Responsibility：把 `T` 的读写登记到 `TypeName()`。
- Inputs：两个非空函数指针。`write` 读 `const T*`，`read` 写 `T*`。返回 `bool` 表示是否写入成功。
- Ownership：注册表复制函数指针，不拥有 `T`。
- Errors：`write` 或 `read` 为空则拒绝且不写入。键已存在则拒绝并保留原条目。
- Sync：同步。线程：只允许在开始序列化之前、单线程调用。

`Serializer` 行为不变：找到 codec 后以其返回值决定该字段成败。

## Failure Model

| 情况 | 行为 |
|------|------|
| 特征未特化，类型是 class | 仍是对象属性；没有 `ME_CLASS` 则收口失败（与今天相同） |
| 特征已特化，尚未 `RegisterValueCodec` | 反射成功。序列化或反序列化该字段失败，错误含类型名，与今天 “codec not found” 相同 |
| `write` / `read` 返回 false | 该字段失败，路径带上属性路径 |
| 第二次登记同一名字 | 返回 false，保留第一次。不覆盖 builtin |
| 枚举宽度仍不被 `MakeSizedEnumCodec` 支持 | 与今天一样在 `SetCodecForEnums` 记错误，不归本注册 API |

不在本期做：注册表在 `FinalizeReflection` 时检查每个值属性是否已有 codec。codec 可以在反射收口之后、第一次存档之前才注册（游戏模块晚于引擎反射初始化）。

## Alternatives & Trade-offs

### Option A（选定）— 编译期特征 + 运行期同一张表

- Summary：分类只读特征；读写只查表；builtin 是表的首批条目。
- Pros：`CreatePropertyByType` 不再随类型增长。Core 不包含业务类型。数组免费复用。存档键仍是今天的类型名。
- Cons：忘记特化时，class 仍会走对象路径并在收口失败，错误信息不会自动写成「你忘了声明 value codec」。忘记注册时，错误推迟到第一次序列化。

### Option B — 只在 `RegisterDefaultCodecs` 里为每个新类型加分支

- Summary：继续扩展现有两张封闭表。
- Why not selected：Gameplay 或游戏类型会重新写进 Core。primitive 与后来的值类型保持两条演化路径，正是本期要收掉的分叉。

### Option C — 运行期才决定分类

- Summary：属性先建成占位，序列化前再查表决定是对象还是值。
- Why not selected：对象路径在收口时就要 `ME_CLASS`。占位会把「未注册 class」和「值类型」混成同一种失败，并且现有属性类别被序列化、脚本签名和编辑器同时消费，不能拖到存档时再改。

## Integration Impact

- `Reflection.h`、`TypeTraits.h`、`ReflectionUtils.h`、`MEFunction.h`：分类与类型名改为特征。
- `PrimitiveCodecRegistry` 改名为 `ValueCodecRegistry`；`Serializer` 与 `SetCodecForEnums` 的调用点跟随。`typeid` 别名不再写入。
- builtin codec 函数体搬迁到 `RegisterValueCodec<T>` 调用，JSON 形状保持。
- 编辑器 `PropertyPath` 仍用 `GetPrimitiveName<T>()` 字符串，这些字符串保持不变，故 Widget 选择不变。
- 游戏模块本期无编译义务。未声明特征的类型行为与今天相同。

## Migration / Compatibility

- Compatibility promises：已有 `.meta` / 场景 / 资产 JSON 中 builtin 字段的类型名和值形状不因本次改动而变。不引入新的磁盘版本。
- Migration / rollout：单次替换分类链与注册入口。没有「旧 primitive 路径 + 新 value 路径」并存。
- Transitional code and exit condition：`typeid(T).name()` 别名在改名时删除。没有读者，故没有兼容期。

## Verification Strategy

- Core behavior checks：
  - 现有会写出 `Vector3`、`std::string`、枚举、整数的套件保持通过（至少 `ParameterStore` 与一条 Scene/Prefab 序列化套件）。对比点是行为而不是重新录一份 golden 文件，除非现有测试已经锁定 JSON 片段。
  - 扩展证明放在 `GameModuleStub`（站在游戏模块一侧，不放进 Core）：一个**非** `ME_STRUCT` 的小结构，头文件里 `ME_DECLARE_VALUE_CODEC`，测试在序列化前 `RegisterValueCodec`。某个已有或新增的 `ME_STRUCT` 上放一个该字段和 `vector<该类型>`。JSON roundtrip 后字段相等。Core 的属性分类源文件里搜不到这个类型的名字。
- Failure-path checks：同一测试里，未注册时序列化失败且错误含类型名；对同一 `TypeName` 第二次 `RegisterValueCodec` 返回 false，随后 roundtrip 仍用第一次的 codec。
- Integration / compatibility：不新增 builtin 的磁盘形状测试。枚举套件若已有则照跑，确认 `SetCodecForEnums` 在改名后仍登记成功。

## Open Questions

- 无阻塞项。Inspector 对未知值类型的展示留到真正有客户（`GP-F03`）时再决定要不要做 Widget。

## Design Review

- **Verdict:** `Pending`
- **Reviewer / date:**
- **Link or summary:**

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `no` — L2，Status 仍为 Draft，需要 design review 把 Verdict 落到 Ready 之后再写实现计划与代码。

## Acceptance Checklist

- [ ] Design review complete when required by complexity/profile
- [ ] Implementation plan traces to design decisions (L2+)
- [ ] Progress log updated when status changes
- [ ] Feature registry status synced
