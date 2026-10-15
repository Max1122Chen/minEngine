# CORE-F37 Material IR Kind Cast Design

## Meta
- **ID:** `CORE-F37`（切片 `CORE-F37-S01`）
- **Type:** Refactor
- **Complexity:** `L1`
- **Status:** `Done`（`CORE-F37-S01` 已实现）
- **Owner:** project maintainer
- **Last updated:** 2026-10-15
- **Related:**
  - [CORE-F37 Survey](../../Platform/Core/CORE-F37_RTTI_HYGIENE_SURVEY.md) 簇 A2
  - UE：`Engine/Source/Runtime/Engine/Public/Materials/MaterialIR.h`（`FValue::As<T>()`）
  - 代码：`MaterialIR/MaterialIR.h` · `MIRDebug.cpp` · `MIREmitter.cpp` · `GLSLMaterialTranslatorImpl.cpp`
- **Campaign note:** 本篇只覆盖 **MIR Kind 分派**；不含 A1 pin 类型、B1 `Cast<>`、Editor/RHI。

## TL;DR

MIR 已有 `MIRValueKind` + `TMIRValue`/`TMIRInstruction::TypeKind`，但 Debug / Emitter / GLSL 仍用 C++ `dynamic_cast`。  
对齐 UE MaterialIR：在 `MIRValue` 上提供 `As<T>()`（`IsA(T::TypeKind)` + `static_cast`），消灭 MaterialIR 路径上的 `dynamic_cast`。  
**Status: Done — `As<T>()` 已落地；MaterialIR/GLSL 路径无 MIR `dynamic_cast`。**

## Problem

1. `MIRDebug.cpp` 用一长串 `dynamic_cast` 打印常量/指令属性。  
2. `MIREmitter.cpp` 匿名命名空间内「`AsConstant`」名对齐 UE，实现却是 `dynamic_cast`。  
3. `GLSLMaterialTranslatorImpl.cpp` 生产路径对 `MIRTextureObject` / `MIRExternalInput` 使用 `dynamic_cast`。  

已有范本：`AsDimensional` / `AsInstruction`（Kind 判断 + `static_cast`）。机制半套好了，调用习惯不统一。

## Requirements

- MaterialIR 相关 `.cpp`（见 Integration）中 **零** `dynamic_cast` / `dynamic_pointer_cast` 作用于 `MIRValue*` 树。
- 对外提供与 UE 同形的 `MIRValue::As<T>()` 及自由函数 `As<T>(MIRValue*)`。
- `AsInstruction` / `AsDimensional` 行为保持；可改为调用 `As<T>` 或保留薄包装（语义不变）。
- `material-ir`（及现有依赖 MIR dump/fold 的测试）通过。

## Constraints

- 不改 MIR 图语义、不改 Kind 枚举含义、不改磁盘格式。
- 不把 MIR 节点塞进 `MEObject` 反射。
- 不在本切片做 A1（NodeDef pin）或 Runtime `Cast<>` 机械波。
- 源码注释不写 Feature/Slice ID（WF-F05）。

## Preferences

- API 形状跟 UE：成员模板 `As<T>()`，而不是为每个 Kind 永久保留一堆手写 `AsFoo` 自由函数（兼容期可留别名）。
- Debug 与生产同一套转型，无「Debug 例外保留 RTTI」。

## Assumptions

- 每个具体 `TMIRValue` / `TMIRInstruction` 派生类的 `TypeKind` 与运行时 `Kind` 一致（现状已满足）。
- `MIRInstruction` 基类本身无单一 `TypeKind`；跨指令基类转型继续用 `AsInstruction`（Kind 区间），与 UE 一致。

## Non-Goals

- Material graph NodeDef pin 类型（A1 / 候选 MAT-F07）。
- MEObject `Cast<>` 机械替换（B1）。
- Editor 子模块 / RHI 后端 cast。
- 引入 visitor、第二次类型注册表、或关闭编译器 RTTI 全局开关。

## Current Architecture

| 已有 | 说明 |
|------|------|
| `MIRValueKind` / `IsA(Kind)` | 真源标签 |
| `TMIRValue` / `TMIRInstruction` | `static constexpr TypeKind` |
| `AsInstruction` / `AsDimensional` | Kind + `static_cast` 范本 |
| `MIRBuilder` | 已多用 `AsInstruction` |
| Debug / Emitter / GLSL | 仍混用 C++ RTTI |

## Proposed Architecture

对齐 UE `FValue::As<T>()`：

```cpp
// MaterialIR.h — on MIRValue
template <typename T>
T* As()
{
    return IsA(T::TypeKind) ? static_cast<T*>(this) : nullptr;
}

template <typename T>
const T* As() const
{
    return IsA(T::TypeKind) ? static_cast<const T*>(this) : nullptr;
}

// Free functions (null-safe), same as UE As<T>(FValue*)
template <typename T>
T* As(MIRValue* value)
{
    return value != nullptr ? value->As<T>() : nullptr;
}

template <typename T>
const T* As(const MIRValue* value)
{
    return value != nullptr ? value->As<T>() : nullptr;
}
```

调用约定：

| 场景 | API |
|------|-----|
| 具体派生（Constant、Operator、TextureObject、…） | `value->As<MIRConstant>()` 或 `As<MIRConstant>(value)` |
| 任意指令基类 | 继续 `AsInstruction(value)` |
| Dimensional | `As<MIRDimensional>(value)`；现有 `AsDimensional` 可改为转发或保留同义 |

**删除 / 替换清单**

1. `MIREmitter.cpp` 匿名命名空间假 `AsConstant`（`dynamic_cast`）→ 使用公共 `As<MIRConstant>`。  
2. `MIRDebug.cpp` 全部 `dynamic_cast` → `As<T>`。  
3. `GLSLMaterialTranslatorImpl.cpp` 两处 → `As<MIRTextureObject>` / `As<MIRExternalInput>`。  
4. Grep 确认 MaterialIR + 上述 Compiler 文件无残留 `dynamic_cast` 到 MIR 类型。

可选清理：`AsDimensional` 实现改为 `As<MIRDimensional>`，避免两套写法长期并存。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `MIRValue` / `MaterialIR.h` | Kind 守卫转型 API | 图构建、GLSL 文本 |
| `MIRDebug` / Emitter / GLSL translator | 使用 `As<T>` 消费节点 | 定义新 Kind 机制 |
| `TMIRValue` / `TMIRInstruction` | 提供 `TypeKind` | C++ RTTI |

## Boundaries

- Allowed: MaterialIR 模块内、GLSL translator 读 MIR 图时使用 `As<T>`。  
- Forbidden: 对本切片范围内的 `MIRValue*` 使用 `dynamic_cast`；用 `Cast<>`（MEObject）处理 MIR。

## Dependencies

- Depends on: 现有 Kind / `TypeKind`（无新模块）。  
- Depended on by: Debug dump、常量折叠、GLSL 纹理槽收集。  
- New dependency direction: none。

## Ownership & Lifetime

Not applicable because 不改变 MIR 节点所有权；仅改变向下转写法。

## Data Flow

Not applicable because 无新数据管道；编译/Debug 读写同一 MIR 图。

## Control Flow

调用方在需要具体字段时：`if (auto* c = value->As<MIRConstant>()) { ... }`。  
失败（Kind 不匹配）→ `nullptr`，与今日 `dynamic_cast` 失败语义一致。

## State & Invariants

- Invariant: `value->As<T>() != nullptr` ⇒ `value->Kind == T::TypeKind`。  
- Invariant: 具体节点构造时 `Kind` 已设为对应 `TypeKind`（现有 emitter/builder 负责）。

## API / Interface Semantics

**`MIRValue::As<T>()`**

- Responsibility: Kind 匹配则返回 `T*`，否则 `nullptr`。  
- Inputs: `T` 必须有 `static constexpr MIRValueKind TypeKind`（即 `TMIRValue` / `TMIRInstruction` 派生）。  
- Errors: 不匹配 → `nullptr`；不抛异常。  
- Thread-safety: 与现有 MIR 单线程编译假设一致。

**`AsInstruction`**

- Unchanged contract: instruction Kind 区间 → `MIRInstruction*`。

## Failure Model

- 错误 Kind：调用方得 `nullptr`，不得假设成功。  
- 误用无 `TypeKind` 的 `T`：编译失败（`T::TypeKind` 不存在）——可接受；不必 SFINAE 包装 unless 需要更清晰 static_assert。

## Alternatives & Trade-offs

### Option A (recommended) — UE 同形 `As<T>()` 模板

- Pros: 与参考引擎一致；一处 API；Debug/生产统一；扩展新 Kind 无需新自由函数。  
- Cons: 需确保派生类都有 `TypeKind`（已有）。

### Option B — 仅补齐手写 `AsConstant` / `AsOperator` / …

- Summary: 每个 Kind 一个自由函数。  
- Why not selected: 与 UE 不一致；Kind 增多时重复劳动；Emitter 已证明「假 AsConstant」容易写歪。

### Option C — 保留 Debug 路径 `dynamic_cast`

- Why not selected: 双路径腐化；UE Debug 也不走 RTTI。

## Integration Impact

| 文件 | 改动 |
|------|------|
| `MaterialIR.h` | 增加 `As<T>()` 成员 + 自由函数 |
| `MaterialIR.cpp` | 可选：`AsDimensional` 转发 |
| `MIRDebug.cpp` | 替换 cast |
| `MIREmitter.cpp` | 删假 AsConstant；替换调用 |
| `GLSLMaterialTranslatorImpl.cpp` | 两处替换 |

无 Editor / Asset / RHI 契约变化。

## Migration / Compatibility

- Forward-only：直接替换调用点；无兼容 shim。  
- 若外部（游戏仓）直接 `dynamic_cast` MIR（预期无）：自行改为 `As<T>`。

## Verification Strategy

- Build `minEngineTests`；跑 `minEngineTests.exe test material-ir`。  
- Grep：`MaterialIR/**` 与 `GLSLMaterialTranslatorImpl.cpp` 中无 `dynamic_cast` 作用于 MIR 类型。  
- 若有 MIR dump 金样/日志测试，确认输出不回归（行为应相同）。

## Open Questions

- 是否删除自由函数 `AsDimensional` 名称、仅保留 `As<MIRDimensional>`？—— **不 blocking**；推荐保留转发别名一版以减小 diff，后续可删。  
- `As<T>` 是否对缺少 `TypeKind` 的 `T` 加 `static_assert`？—— **不 blocking**；默认依赖编译错误即可。

## Design Review

- **Verdict:** `Ready`
- **Reviewer / date:** project maintainer / 2026-10-15（批准）
- **Agent self-check:** Ready with deferred items（A1/B1/C 不在本篇；Open Questions 非阻塞）

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected（含本机 UE MaterialIR）
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

**Ready for implementation?** `yes`

## Acceptance Checklist

- [x] 维护者批准（Status → In Progress）
- [x] 实现后：MaterialIR/GLSL 路径无 MIR `dynamic_cast`；`material-ir` PASS
- [x] Progress / Registry 切片状态同步
- [x] 源码注释无 Feature ID

## Implementation slices (traceability)

| Slice | Summary | Design decision | Verification | Status |
|-------|---------|-----------------|--------------|--------|
| `CORE-F37-S01` | `As<T>()` API + 替换 Debug/Emitter/GLSL cast | Option A | `material-ir` + grep | **Done** |

## Changelog

| Date | Note |
|------|------|
| 2026-10-15 | Draft/Review：对齐 UE `FValue::As<T>()`；待审批 |
| 2026-10-15 | 批准并实现：`MIRValue::As<T>()`；Debug/Emitter/GLSL 去 RTTI；`material-ir` PASS |
