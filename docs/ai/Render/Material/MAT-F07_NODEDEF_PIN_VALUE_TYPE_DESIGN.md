# MAT-F07 NodeDef Pin Value Type Design

## Meta
- **ID:** `MAT-F07`
- **Type:** Refactor
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** 2026-10-16
- **Related:**
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)
  - [CORE-F37 Survey](../../Platform/Core/CORE-F37_RTTI_HYGIENE_SURVEY.md) 簇 A1
  - UE：`Engine/.../Materials/MaterialExpression.h` — `GetInputValueType` / `GetOutputValueType`
  - 代码：`MaterialGraphNodeDef.h` · `MaterialValueType.cpp` · `MaterialEdGraph.cpp` · `MaterialPropertyUtil.*`
- **Campaign note:** 承接 CORE-F37 A1；消灭中央 NodeDef `dynamic_cast` 梯子。不含 MIR Kind（S01 Done）、B1-a（Done）、RHI、Editor 子模块。

## TL;DR

`MaterialValueTypeUtil::GetNode*PinType` 用 C++ RTTI 闭世界认识所有 `MaterialGraphNodeDef_*`——设计错，不是换 `Cast<>`。  
对齐 UE：**仅**在 `MaterialGraphNodeDef` 上提供虚 `GetInputPinType` / `GetOutputPinType`；固定类型节点 override 直接返回类型；默认 `nullptr`（宽/推断）。  
**删除** Util 上的 `GetNode*PinType`（无薄转发层）。**不**在 pin 结构上挂类型字段，**不**要 `RefreshPinTypes`。  
**不抄** UE Mask / MCT。  
**Status: Done — S01+S02 已实现；`material-ir` PASS。**

## Problem

1. `MaterialValueType.cpp` 中 `GetNodeOutputPinType` / `GetNodeInputPinType` 对每个具体 NodeDef 做 `dynamic_cast`（约 40+ 处）。
2. 新节点必须改中央 Util → 闭世界、易漏。
3. `Material.cpp` / `MaterialTestGraph.cpp` 有同类闭世界探测，应收敛，禁止再堆中央表。

## Requirements

- 查询 pin 类型的路径**只**调用 `nodeDef->Get*PinType(i)`（含 Validate 内部）。
- 删除 `MaterialValueTypeUtil::GetNodeOutputPinType` / `GetNodeInputPinType`；调用点改虚接口。
- `MaterialValueType.cpp` 内对 `MaterialGraphNodeDef_*` 的 `dynamic_cast` → **0**。
- `nullptr` 语义不变：`AreConnectable` 仍把未知/推断当宽类型。
- 固定类型节点行为与今日梯子对齐（见 §Pin Type Inventory）。
- 反序列化后类型仍正确——因类型在虚函数中按类/index/Name 计算，**与磁盘无关**。
- `material-ir` 与现有连线校验路径通过。

## Constraints

- 不改磁盘 `.memtl` / 图 JSON 格式。
- pin 结构**不**增加类型字段；不引入序列化类型指针问题。
- 不引入 UE Mask 通道编码 / `EMaterialValueType` 位旗。
- 继续 `MaterialValueType = const MIRValueType*`。
- 不改 MIR 图语义、BuildIR、Load API。
- 源码注释不写 Feature/Slice/Bug/ADR ID。
- 本 Feature 不碰 RHI / Editor 子模块 / CORE-F37 C1。

## Preferences

- API 形状跟 UE：虚 `Get*PinType`，调用方持有 `MaterialGraphNodeDef*` 时直接问节点。
- Util 只保留跨节点策略：`AreConnectable` / `AreCompatible` / `GetDisplayName` / `ValidateGraphPinConnections` / `GetMaterialPropertyValueType`。
- 今日无实例依赖 pin 类型（TextureObject 写死 Texture2D）；不为「将来自定义节点」预留 pin 字段缓存。

## Assumptions

- 现有 NodeDef pin 类型均为 **类级固定**（或 MaterialOutput：按 input Name 查属性表）；无「同一 index 因实例状态改类型」。
- `GetMaterialPropertyInputDescription` 仍是 MaterialOutput 各 shading 输入 ExpectedType 的真源。
- 调用点很少（今日主要是 `MaterialEdGraph` + `ValidateGraphPinConnections`），删除 Util 类型 API 的迁移成本可接受。

## Non-Goals

- pin 上 `DeclaredType` / `ExpectedType` 字段与 `RefreshPinTypes`。
- Util 薄转发层。
- 复制 UE Mask / MCT。
- visitor / 反射类型注册表 / 中央 switch 换皮。
- 改变 `AreConnectable` 宽类型策略。
- CORE-F37 C1；B1-b Editor/Tests LazyRTTI。

## Current Architecture

| 已有 | 说明 |
|------|------|
| `MaterialGraphNodeDefInput` / `Output` | 仅 Name + 连接；无类型 |
| `MaterialValueTypeUtil::GetNode*PinType` | 中央 `dynamic_cast` 梯子 |
| `MaterialEdGraph` 连线 | 调 Util → `AreConnectable` |
| `ValidateGraphPinConnections` | 内部再调 Util GetNode* |
| `GetMaterialPropertyInputDescription` | MaterialOutput 输入类型真源 |

## Proposed Architecture

### 责任对齐 UE（比初稿更贴）

```text
Caller (EdGraph / Validate / …)
    → nodeDef->GetOutputPinType(i) / GetInputPinType(i)   // 虚，唯一真源
         基类默认 → nullptr
         固定类型子类 → override 返回 MIR 类型
         MaterialOutput → 按 input Name 查 GetMaterialPropertyInputDescription
```

UE 锚点：`UMaterialExpression::GetInputValueType` / `GetOutputValueType`（`MaterialExpression.h` ~355–367）。  
默认 Mask→FloatN（`MaterialExpressions.cpp` ~1831）→ **不采用**。

### 基类 API

```cpp
class MaterialGraphNodeDef {
public:
    /** Declared output type; nullptr = polymorphic / unknown (wide). */
    virtual MaterialValueType GetOutputPinType(int32_t outputIndex) const
    {
        (void)outputIndex;
        return nullptr;
    }

    /** Expected input type; nullptr = polymorphic / unknown (wide). */
    virtual MaterialValueType GetInputPinType(int32_t inputIndex) const
    {
        (void)inputIndex;
        return nullptr;
    }
};
```

可选（非必须）：基类先做越界检查再 `return nullptr`，子类 override 时仍应自行校验或依赖调用方已合法 index。推荐基类：

```cpp
virtual MaterialValueType GetOutputPinType(int32_t outputIndex) const
{
    if (outputIndex < 0 || outputIndex >= GetOutputCount())
        return nullptr;
    return nullptr; // wide by default; subclasses override
}
```

子类 override 内：越界 → `nullptr`；合法 → 固定类型或按 Name 分支。

### 子类示例

```cpp
MaterialValueType MaterialGraphNodeDef_Constant::GetOutputPinType(int32_t outputIndex) const
{
    if (outputIndex != 0)
        return nullptr;
    return MIRPrimitiveType::GetFloat();
}

MaterialValueType MaterialGraphNodeDef_TextureSample::GetInputPinType(int32_t inputIndex) const
{
    const Input* input = GetInput(inputIndex);
    if (input == nullptr)
        return nullptr;
    if (input->Name == "Texture")
        return MIRObjectType::GetTexture2D();
    if (input->Name == "UV")
        return MIRPrimitiveType::GetFloat2();
    return nullptr;
}
```

多态节点（Add / Multiply / Select / …）：**不 override**，吃基类 `nullptr`。

### MaterialOutput

```cpp
MaterialValueType MaterialGraphNodeDef_MaterialOutput::GetInputPinType(int32_t inputIndex) const
{
    const Input* input = GetInput(inputIndex);
    if (input == nullptr)
        return nullptr;
    for (/* each MaterialProperty */) {
        MaterialPropertyInputDescription description;
        if (!GetMaterialPropertyInputDescription(property, description))
            continue;
        if (input->Name == description.InputName)
            return description.ExpectedType;
    }
    return nullptr;
}
```

输入集合仍由现有 Ensure/Resolve 维护；类型查询不再经 Util RTTI。

### 删除 Util 类型 API

- 从 `MaterialValueType.h` / `.cpp` **移除** `GetNodeOutputPinType` / `GetNodeInputPinType`。
- `MaterialEdGraph.cpp`、`ValidateGraphPinConnections` 改为直接调虚函数。
- Util **保留**：`AreConnectable`、`AreCompatible`、`GetDisplayName`、`ValidateGraphPinConnections`、`GetMaterialPropertyValueType`。

### 无需反序列化钩子

类型不存在 pin 字段上；加载后只要 NodeDef 动态类型正确，虚函数即正确。

### 连带（同 Feature）

| 位点 | 改法 |
|------|------|
| `Material.cpp` TextureObject / ScalarParameter / TextureSample 探测 | 单点 `Cast<具体 NodeDef>` 或业务所需的 pin 类型查询；禁止新中央梯子 |
| `MaterialTestGraph.cpp` | 同上 |

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `MaterialGraphNodeDef` | 虚 API 默认 `nullptr`；越界约定 | 具体节点类型表 |
| 各 `MaterialGraphNodeDef_*` | override 声明本类固定 pin 类型 | 连线策略、MIR 编译 |
| `MaterialValueTypeUtil` | 兼容性、显示名、图级 Validate | 任何 NodeDef 具体类型 / GetNode*PinType |
| `MaterialPropertyUtil` | MaterialOutput 属性 ExpectedType 真源 | NodeDef RTTI |
| `MaterialEdGraph` | 连线时问节点虚接口 | 拥有类型知识 |

## Boundaries

- Allowed：持有 NodeDef → `Get*PinType`；单点 `Cast<具体 NodeDef>` 做业务查询。
- Forbidden：Util（或任何中央文件）再出现全 NodeDef `dynamic_cast` 表；pin 类型字段；Mask/MCT；Util 薄转发 `GetNode*PinType`。

## Dependencies

- Depends on：`MIRPrimitiveType` / `MIRObjectType`；`MaterialPropertyUtil`（MaterialOutput）。
- Depended on by：`MaterialEdGraph`、Validate、Editor 连线。
- Direction：类型知识只在 NodeDef 继承树内；Util 不再依赖具体子类头文件（实现后可收敛 include）。

## Ownership & Lifetime

- 返回的 `MaterialValueType` 为静态 MIR 描述符指针；无 pin 级类型状态。
- `Not applicable` beyond existing NodeDef / 图所有权。

## Data Flow

```text
Caller → nodeDef->Get*PinType(i) → MaterialValueType (or nullptr)
       → AreConnectable(from, to)
```

## Control Flow

- Sync；无加载后 Refresh。
- Editor 连线：`CanConnect` → 两端 `Get*PinType` → `AreConnectable`。
- Validate：遍历边，同上。

## State & Invariants

- 固定类型子类：合法 index 的 `Get*PinType` 与 §Inventory 一致。
- 多态节点：保持基类 `nullptr`。
- Illegal：恢复 Util `GetNode*PinType`；中央梯子。
- 谁维护：各 NodeDef 子类。

## API / Interface Semantics

### `GetOutputPinType` / `GetInputPinType`

- **Responsibility:** 声明/期望类型；未知/推断 → `nullptr`。
- **Inputs:** index；越界 → `nullptr`。
- **Ownership:** 静态指针，不释放。
- **Errors:** 无异常。
- **Thread-safety:** 与现有 Material 图一致（默认主线程）。

### 已删除 API

- `MaterialValueTypeUtil::GetNodeOutputPinType`
- `MaterialValueTypeUtil::GetNodeInputPinType`

## Failure Model

| Failure | Behavior |
|---------|----------|
| 子类漏 override | 该 pin 变宽（`nullptr`）→ 可能漏拒错误边；靠 Inventory + 测试防回归 |
| 越界 index | `nullptr` |
| MaterialOutput Name 与属性表不一致 | `nullptr`（宽）— 与今日查不到时一致 |

## Alternatives & Trade-offs

### Option A (recommended) — 纯虚函数 + 删 Util 类型 API

- Summary: 上文。
- Pros: 最贴 UE；无双真源；无 Refresh/反序列化钩子；无无转发层。
- Cons: 每个固定类型子类写 override（节点数可控）。

### Option B — 虚接口 + pin transient 字段 + Refresh（初稿）

- Why not selected: 今日无实例改类型；Refresh 与 ctor 双写；加载钩子额外成本。

### Option C — Util 薄转发保留

- Why not selected: 零行为多一层；易再被塞回闭世界逻辑；调用点少，迁移便宜。

### Option D — Mask + MCT

- Why rejected: 与多输出 pin + `MIRValueType*` 重复；Prefer Simplicity。

## Integration Impact

| Area | Impact |
|------|--------|
| `MaterialGraphNodeDef.h` + 各子类 `.cpp` | 加虚函数 + overrides |
| `MaterialValueType.h/.cpp` | 删 GetNode*；Validate 改调虚接口；删梯子 |
| `MaterialEdGraph.cpp` | 改调用点 |
| `Material.cpp` / `MaterialTestGraph.cpp` | S02 收敛探测 |
| 磁盘 / 加载路径 | **无变更**（无需 Refresh） |

## Migration / Compatibility

- 资产格式不变。
- 行为：与今日梯子语义对齐（§Inventory）。
- Rollout：S01 虚接口 + 删 Util 类型 API + 删梯子；S02 连带探测。

## Verification Strategy

- `minEngineTests.exe test material-ir` PASS。
- Grep：`GetNodeOutputPinType` / `GetNodeInputPinType` 符号消失；`MaterialValueType.cpp` 无 `dynamic_cast`。
- 连线：不兼容边仍拒；多态边仍允许（现有 pin 测试）。
- Manual（可选）：Editor 错误类型连线仍报错。

## Open Questions

- `ConstantInt` 等今日梯子未覆盖：保持基类 `nullptr` 直至有产品需求；**不 blocking**。

## Design Review

- **Verdict:** `Ready`
- **Reviewer / date:** maintainer / 2026-10-16（批准实现）
- **Link or summary:** 纯虚 Get*PinType + 删 Util GetNode*；已落地。

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

**Ready for implementation?** `yes` — 已实现。

## Acceptance Checklist

- [x] Design review / 批准完成
- [x] Progress / Registry 状态同步
- [x] `GetNode*PinType` 已删除；调用点改虚接口
- [x] `MaterialValueType.cpp` NodeDef `dynamic_cast` = 0
- [x] `material-ir` PASS
- [x] 无 pin 类型字段、无 Refresh、无 Mask/MCT

## Slices

| Slice | Scope | Exit |
|-------|--------|------|
| **S01** | 基类虚 API + 全固定类型子类 override + 删 Util GetNode* + 改 EdGraph/Validate + 删梯子 | grep + `material-ir` |
| **S02** | `Material.cpp` / `MaterialTestGraph` 闭世界探测收敛 | 无新梯子；相关测试过 |

## Pin Type Inventory（迁移对照 · 与今日梯子对齐）

`nullptr` = 推断/宽（不 override 或显式返回 null）。

### Outputs

| NodeDef | Pin (index / Name) | Type |
|---------|-------------------|------|
| Constant | 0 Value | Float |
| Constant3 | 0 Value | Float3 |
| Constant3 | 1..3 R/G/B | Float |
| MakeFloat2 | 0 Value | Float2 |
| MakeFloat3 | 0 Value | Float3 |
| ScalarParameter | 0 Value | Float |
| TextureCoordinate | 0 UV | Float2 |
| VertexColor | 0 / 1 | Float4 / Float3 |
| TextureObject | 0 | Texture2D |
| TextureSample | 0/1/2 RGBA/RGB/A | Float4 / Float3 / Float |
| NormalUnpack | 0 | Float3 |
| ComponentMask | 0 | Float |
| MaterialOutput | （若有输出） | Float（与今日一致） |
| Add/Multiply/Subtract/Divide/Max/Min/Negative/Lerp/Not/Select/IfThenElse | * | nullptr（基类） |

### Inputs

| NodeDef | Pin | Type |
|---------|-----|------|
| MakeFloat2 / MakeFloat3 | 各分量 | Float |
| NormalUnpack | 0 | Float3 |
| TextureSample | Texture / UV | Texture2D / Float2 |
| MaterialOutput | 各属性 InputName | `GetMaterialPropertyInputDescription` |
| Not | 0 | Bool |
| Lerp | Alpha | Float |
| Lerp | A/B | nullptr |
| ComponentMask | 输入 | nullptr |
| Add/Multiply/…/Select/IfThenElse | * | nullptr（基类） |

## Changelog

| Date | Note |
|------|------|
| 2026-10-16 | 初稿：虚接口 + pin transient 字段 + Util 薄转发 + Refresh |
| 2026-10-16 | **修订：** 废弃 Util `GetNode*PinType`；纯虚函数 override；取消 pin 字段与 Refresh |
| 2026-10-16 | S01+S02 Done；`material-ir` PASS；Status → Done |
