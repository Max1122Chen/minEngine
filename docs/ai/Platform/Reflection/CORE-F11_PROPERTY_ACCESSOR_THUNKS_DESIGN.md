# CORE-F11 — Property Getter/Setter Native Thunks & AssignProperty

## Meta
- **ID:** `CORE-F11`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/core`
- **Related:** [Implementation](./CORE-F11_PROPERTY_ACCESSOR_THUNKS_IMPLEMENTATION.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md) · [BUG-CORE-001](../../bugs/BUG-CORE-001.md) · [BUG-PHYS-002](../../bugs/BUG-PHYS-002.md) · [TD-026](../../TECH_DEBT.md) · [REFLECTION_FUNCTIONS_DESIGN](./REFLECTION_FUNCTIONS_DESIGN.md) · [UE_FUNCTION_REFLECTION_NOTES](./UE_FUNCTION_REFLECTION_NOTES.md)
- **Depends on:** Reflection Finalize + header tool；`ME_FUNCTION`/`InvokeFunction` **不**作为属性 Assign 热路径依赖
- **Decisions (2026-09-05):** codegen **属性专用 native thin thunk**（靠拢 UE UPROPERTY Getter/Setter）；`meta=(Getter/Setter)`；统一 `AssignProperty`；**语义层凡属性编辑皆为 PostEdit 事件**；实现层有 Setter 时由 Setter 承担传播（**不**再调虚函数 `PostEditChangeProperty`）；无 Setter 时直写 + 虚函数兜底；Serialize 有 Setter 则走 Assign；**thunk 体定义在 C++ 宏，codegen 只注入宏调用**；Inspector live temp→Assign（S06）

## TL;DR

反射写路径今天直捅成员，绕过手写 `SetXxx`（BUG-CORE-001）。本期用 header tool 生成 **Get/Set native thunk** 挂到 `MEProperty`，再经统一 **`AssignProperty` / `GetPropertyValue`** 供 Editor、Serialize、Resolve 使用；可选 **`PostEditChangeProperty`** 兜底无 Setter 字段。不把属性写默认走 `InvokeFunction`。

## Scope

### In
- `meta=(Getter = "Name", Setter = "Name")` 解析；标识符校验（tool）；**签名 Fail-closed 以 C++ 编译为准**（宏展开调用真实成员）
- `ReflectionMacros.h` 提供 `ME_REFLECTION_PROPERTY_{GETTER,SETTER}_THUNK`；codegen **只注入宏**；`MEProperty` 挂 `PropertyGetFn` / `PropertySetFn`
- 运行时 `AssignProperty` / `GetPropertyValue`（+ 选项：是否 notify PostEdit）
- Inspector / Undo（`ApplySetObjectProperty`）切入 Assign
- Serializer 叶子写入与关键 ObjectPtr resolve：有 Setter 则 Assign（含 **TD-026** `m_Owner` → `SetOwner` 路径）
- `MEObject::PostEditChangeProperty` 最小虚函数 API（Editor 兜底）
- 收缩/删除依赖硬编码列表的 `ApplyPhysicsEditorSideEffects`（在代表属性挂上 Setter 或 PostEdit 之后）
- 单测 + 文档；更新 BUG-CORE-001 / TD-026 状态

### Out
- 属性 Assign 默认走 `ME_FUNCTION` / `InvokeFunction`（脚本复用属 follow-up）
- 完整 UE 式 `FPropertyChangedEvent` / ChainProperty / PreEdit 全家桶
- 强制所有字段必须有 Setter
- Getter 类型转换（字段类型 ≠ 返回类型）
- BlueprintGetter/Setter、复制 Notify、完整 PostLoad 框架产品化（可留钩子）
- 一次迁完引擎内全部副作用字段（按代表例 + 文档迁移指南）

## Reader quick start
1. 本文件 — 机制与契约（尤其 §3）
2. [Implementation Plan](./CORE-F11_PROPERTY_ACCESSOR_THUNKS_IMPLEMENTATION.md)
3. 代码：`ReflectionMacros.h` · `MEProperties.h` · `minEngine_header_tool.py` · `Serializer` · `SceneEditor::ApplySetObjectProperty`

---

## 0) Pre-flight（2026-09-05）

| 项 | 结论 |
|----|------|
| 现状 | `ME_REFLECTION_ACCESSOR_FIELD` 仅取成员地址；Inspector/Serialize 直写；Physics 靠战术 `ApplyPhysicsEditorSideEffects` |
| 依赖 | header tool、Finalize、PropertyPath、`MEObject` — **sound** |
| 债风险 | **Medium** — Editor+Serialize 面广；用切片先 Editor 后 Serialize 控风险 |
| WIP | `feat/core` 序列化 F08–F10 **Done**；本 Feature 为其自然续作 |
| 哲学 | **机制**（如何权威写属性），非 gameplay 框架；对齐 UE 的 UHT+native 桥，不抄全套 UProperty 生态 |
| 建议 | **Go** — Design 定稿后按 S01→S05 开码 |

**True refactor 成分：** 引入统一写入契约并迁移调用点；删除战术旁路（Physics side-effects 列表）属成功判据之一。

---

## 1) 背景与目标

### Pain
- 手写 Setter 含副作用（物理、Owner、脏标记），反射/编辑器/反序列化不调用 → 行为分裂。
- 战术补丁按组件/属性名硬编码，不可扩展（BUG-PHYS-002）。

### Goals
- 可选地把属性读写绑到现有 C++ 成员函数（**codegen thin thunk**）。
- 所有权威写入走 **`AssignProperty`**（或明确 document 的例外）。
- 无 Setter 时仍可直写成员；Editor 可用 **PostEdit** 兜底。
- 加载路径对挂了 Setter 的字段走 Assign，根治 **TD-026** 类问题。

### Success
- 代表例（如 `SetSimulatePhysics` / `SetOwner`）经 Inspector 与（至少）相关反序列化路径触发同一 Setter。
- 未标注 Getter/Setter 的现有 `ME_PROPERTY` **零行为变化**。
- `ApplyPhysicsEditorSideEffects` 可删除或缩成空壳/少数遗留。

---

## 2) 现状

| 层 | 行为 |
|----|------|
| 注解 | Specifier 枚举 + 自由 `meta` 字符串；**无** Getter/Setter 约定 |
| Codegen | `ME_REFLECTION_ACCESSOR_FIELD` → GetConst/GetMutable 取地址 |
| 写入 | Inspector buffer deserialize、Serializer、部分 Resolve 直写 |
| 函数反射 | `ME_FUNCTION` + `InvokeFunction` 已可用；与属性写入未连接 |
| 战术 | `ApplyPhysicsEditorSideEffects` 在 `ApplySetObjectProperty` 之后 |

---

## 3) 方案

### 3.1 与 UE 的对齐关系

| UE | minEngine（本期） |
|----|-------------------|
| UHT 解析 `meta=(Getter/Setter)` | header tool 解析同名 meta |
| 生成 native 属性访问桥 | 生成 `Get_/Set_` thin thunk |
| Native 属性热路径不经完整 `ProcessEvent` | Assign **不经** `InvokeFunction` |
| `PostEditChangeProperty` | `MEObject::PostEditChangeProperty` 最小版 |
| `UFUNCTION` 管线 | 保留给脚本/Console；**非**属性 Assign 默认路径 |

结论：对齐 **UPROPERTY Native Getter/Setter + 可选 PostEdit**，不是 Blueprint/`ProcessEvent` 默认写属性。

### 3.2 注解契约

```cpp
ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSimulatePhysics", Getter = "IsSimulatePhysics"))
bool m_bSimulatePhysics = true;

void SetSimulatePhysics(bool enabled);
bool IsSimulatePhysics() const; // 或 GetSimulatePhysics
```

规则：

| 项 | 约定 |
|----|------|
| Key | `Getter` / `Setter`（大小写敏感，与现有 meta 一致） |
| 值 | 同 class（含继承查找策略见下）的 **成员函数名** 字符串 |
| Setter 签名 | `void Name(T)` 或 `void Name(const T&)`，`T` = 去除 cv/ref 后的字段类型 |
| Getter 签名 | `T Name() const` 或 `const T& Name() const`（一期允许值返回） |
| 找不到 / 签名不符 | **C++ 编译失败**（宏展开调用 `TYPE::METHOD`；方法不存在或参数不匹配即红） |
| meta 值非法 | header tool 报错（空串 / 非标识符） |
| 仅 Getter 或仅 Setter | 允许 |
| 继承 | 方法须在 **字段所属类型** 上可调用（含基类 public/protected）；与普通 C++ 成员调用规则一致，无需 tool 做 AST 查找 |

一期 **不要求** Setter 同时标 `ME_FUNCTION`。

### 3.3 Codegen 形态：C++ 宏 + 注入

**原则：** thunk **实现**写在 `ReflectionMacros.h`，便于以后改调用约定 / 加断言 / 调调试而不改 generator 逻辑；header tool **只输出宏调用**与注册指针。

```cpp
// ReflectionMacros.h（权威实现）
#define ME_REFLECTION_PROPERTY_SETTER_THUNK(TYPE, FIELD, METHOD) \
    static void PropertySet_##FIELD(void* object, const void* value) { \
        using FieldType = FieldType_##FIELD; \
        static_cast<TYPE*>(object)->METHOD(*static_cast<const FieldType*>(value)); \
    }

#define ME_REFLECTION_PROPERTY_GETTER_THUNK(TYPE, FIELD, METHOD) \
    static void PropertyGet_##FIELD(const void* object, void* outValue) { \
        using FieldType = FieldType_##FIELD; \
        *static_cast<FieldType*>(outValue) = static_cast<const TYPE*>(object)->METHOD(); \
    }
```

```cpp
// .gen.cpp — FieldAccessor 特化内（tool 注入）
ME_REFLECTION_ACCESSOR_FIELD(RigidBodyComponent, m_bSimulatePhysics)
ME_REFLECTION_PROPERTY_SETTER_THUNK(RigidBodyComponent, m_bSimulatePhysics, SetSimulatePhysics)
ME_REFLECTION_PROPERTY_GETTER_THUNK(RigidBodyComponent, m_bSimulatePhysics, GetSimulatePhysics)

// 注册（无 accessor 时 GET/SET 传 nullptr）
ME_REFLECTION_CLASS_ADD_FIELD_ACCESSORS(
    RigidBodyComponent, m_bSimulatePhysics, /*spec*/, /*meta*/,
    &FieldAccessor<RigidBodyComponent>::PropertyGet_m_bSimulatePhysics,
    &FieldAccessor<RigidBodyComponent>::PropertySet_m_bSimulatePhysics)
```

注册时：`property->SetPropertyValueAccessors(getFn, setFn)`；**保留**现有 FieldConst/Mutable 成员地址 accessor（Serialize 布局、无 Setter 写入、调试仍可用）。

### 3.4 运行时 API

```cpp
struct PropertyAssignOptions
{
    // Editor: true only when property has NO Setter (虚函数兜底).
    // Semantic PostEdit always happens for edits; see §3.5.
    bool notifyPostEdit = false;
};

bool AssignProperty(void* owner, const MEProperty& property, const void* valuePtr,
                    PropertyAssignOptions options = {});
bool GetPropertyValue(const void* owner, const MEProperty& property, void* outValuePtr);
```

算法：

```text
AssignProperty:
  if property.HasSetterThunk:
    call SetterThunk(owner, valuePtr)   // ← 语义 PostEdit 由 Setter 承担
  else:
    copy/assign into GetMutable(owner)
  if options.notifyPostEdit && owner is MEObject*:
    PostEditChangeProperty(event{propertyName})  // ← 仅无 Setter 时的实现兜底
  return ok

GetPropertyValue:
  if property.HasGetterThunk:
    call GetterThunk(owner, outValuePtr)
  else:
    copy from GetConst(owner)
```

**实现层：** 有 Setter 时 **不**再调虚函数 `PostEditChangeProperty`（避免双重副作用）。  
**语义层：** 有 Setter 的编辑 **仍然是** PostEdit 事件——传播写在 Setter 内。

Inspector live（S06）：`GetPropertyValue` → 编辑临时缓冲 → `AssignProperty`（`notifyPostEdit = !HasSetter`）。

### 3.5 PostEdit：语义 vs 实现

**语义（永远成立）：** 属性被权威编辑后，发生一次「属性变更 / PostEdit」——副作用、刷新、监听都从这条变更故事出来。

**实现（二选一落地，不双调）：**

| | 有 Setter | 无 Setter |
|--|-----------|-----------|
| 变更落地 | Setter thunk（经 `AssignProperty`） | 直写成员 |
| 传播入口 | **Setter 自身**（即语义 PostEdit） | 虚函数 `PostEditChangeProperty` |
| `notifyPostEdit` | **false**（实现层不做第二次 PostEdit） | Editor 路径 **true** |

```cpp
struct PropertyChangedEvent
{
    std::string_view propertyName;
};

class MEObject
{
public:
    virtual void PostEditChangeProperty(const PropertyChangedEvent& event) {}
};
```

| 路径 | 有 Setter | 无 Setter |
|------|-----------|-----------|
| Editor Assign（含 Inspector live） | 调 Setter；**实现层**不调虚 PostEdit | 直写 + `notifyPostEdit=true` |
| Serialize / Load | 调 Setter；不调虚 PostEdit | 直写；对象级 PostLoad/Finalize |
| 手写 C++ `obj->SetX()` | 不经 Assign；Setter 内自带传播 | — |

组件上按名分发的 `PostEditChangeProperty` 覆盖：**只保留无 Setter 字段**（如 `m_Transform`、`m_bActive`）；已挂 Setter 的字段副作用迁入 Setter，覆盖内删重复分支。

### 3.6 调用点迁移

| 调用方 | 动作 |
|--------|------|
| `SceneEditor::ApplySetObjectProperty` | 经 Assign；有 Setter 不调虚 PostEdit |
| Inspector live `DrawProperty`（S06） | temp → Assign；有 Setter 不调虚 PostEdit |
| `Serializer` 反序列化叶子 | 临时值后 Assign |
| Pending ObjectPtr / `m_Owner` | Assign → `SetOwner`（TD-026 Done） |
| PropertyPath set | 与 Editor 对齐走 Assign |
| Lua | **Out**；follow-up |

### 3.7 数据流

```text
ME_PROPERTY meta Getter/Setter
        │
        ▼
header_tool → .gen.cpp thunks + Register
        │
        ▼
MEProperty { field accessors, optional get/set thunks }
        │
        ├─ Editor live / Undo ──► temp? → AssignProperty ──► Setter(=语义PostEdit) or member+虚PostEdit
        └─ Serializer / Resolve ──► AssignProperty ──► Setter or member
```

### 3.8 与 `ME_FUNCTION` 的边界

| | 属性 thunk | `ME_FUNCTION` |
|--|------------|----------------|
| 目的 | 字段权威读写 | 通用可反射调用 |
| 入口 | `AssignProperty` / `GetPropertyValue` | `InvokeFunction` |
| 一期关系 | **独立** | 不依赖 |
| 二期 | 同一 `SetFoo` 可再标 `ME_FUNCTION` 供脚本 | 可选复用 |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| Assign 默认 `InvokeFunction` | **拒绝（一期）** — 过重；逼所有 Setter 进函数反射 |
| 仅 PostEdit、不绑 Setter | **拒绝作主方案** — 无法表达写入时校验；保留作无 Setter 兜底 |
| 强制全字段 Setter | **拒绝** — 迁移成本不可接受 |
| 取消成员、只留 Getter/Setter | **拒绝** — 与现有布局/序列化冲突 |
| 有 Setter 仍再调虚 PostEdit | **拒绝默认** — 双重副作用；语义已由 Setter 承担 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Tool 签名解析脆弱 | **不依赖 tool 解析签名**；宏 + C++ 类型检查 Fail-closed；tool 只校 meta 标识符 |
| Serialize 调 Setter 导致 load 副作用过重 | Setter 必须幂等/可从默认态赋值；坏 Setter 点名修 |
| 双重副作用（Setter + 虚 PostEdit） | 有 Setter 时实现层不调虚函数；组件 PostEdit 删已迁移分支 |
| Inspector 直写再 Assign early-out | S06：控件编辑 **临时缓冲**，再 Assign |
| 拖动帧每帧 RefreshPhysics | Setter 内 early-out 同值；后续可加 EndEdit（非 S06） |
| 大面积改 Serializer | S01–S05 已完成 |
| 继承查找不准 | 一期文档限制；不够再增强 tool |

---

## 6) 验收标准

- [x] Design 决策与实现一致（thunk ≠ InvokeFunction 热路径）
- [x] `meta=(Getter/Setter)` 生成宏注入；坏方法名/签名在 **编译期**失败；非法 meta 在 tool 失败
- [x] 无注解字段行为与改前一致
- [x] 代表组件：Serialize/`ApplySetObjectProperty` 走 Assign（`m_Owner`；物理 meta Setter）
- [x] `AssignProperty` 被 Editor/Serialize 使用；Physics 战术补丁删除
- [x] Serialize/Resolve 对 `SetOwner` 走 Assign；**TD-026 → Done**
- [x] PostEdit 虚函数在无 Setter + Editor notify 路径可测
- [x] **S06：** Inspector live Primitive（及 flat Object widget）经 temp→Assign；有 Setter 不调虚 PostEdit
- [x] **S06：** 物理组件 PostEdit 去掉与 Setter 重复的分支
- [x] `serialization-archive` / `scene-clone` / `reflection-function` / `physics-smoke` 绿
- [x] BUG-CORE-001 更新；Registry / ACTIVE_WORK / PROGRESS_LOG

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| What's next | 准备 commit；回 Primary `ANIM-F01` |
| Follow-up | Lua Assign；Setter 兼 `ME_FUNCTION`；ChainProperty；ObjectPtr live Assign；拖动 EndEdit |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | 正式 Design：native thunk、Assign、PostEdit 兜底、Serialize 走 Setter；登记 CORE-F11 |
| 2026-09-05 | §3.3：thunk 改为 C++ 宏权威实现，codegen 只注入宏；签名 Fail-closed 以编译为准 |
| 2026-09-05 | S01–S05 Done — 实现 + 删 PhysicsEditorSideEffects；TD-026/BUG-CORE-001 收口 |
| 2026-09-05 | §3.5：澄清「语义 PostEdit 必有 / 实现层有 Setter 不双调」；开 S06 Inspector live Assign |
| 2026-09-05 | **S06 Done** — live Assign + 物理 PostEdit 瘦身 |
