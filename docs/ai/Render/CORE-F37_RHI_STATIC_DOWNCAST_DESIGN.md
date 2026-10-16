# CORE-F37 RHI Static Downcast Design

## Meta
- **ID:** `CORE-F37`（切片 `CORE-F37-S02`）
- **Type:** Refactor
- **Complexity:** `L1`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** 2026-10-16
- **Related:**
  - [CORE-F37 Survey](../Platform/Core/CORE-F37_RTTI_HYGIENE_SURVEY.md) 簇 C1
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md)
  - 代码：`OpenGLRHI.cpp` · `VulkanRHI.cpp` · `VulkanRHIResources.cpp` · `RHICommandList.cpp`
  - 后端选择：`RHIBackend.h`（`RHIBackendType` / `RHIBackendSelection`）
- **Campaign note:** 消灭 RHI 路径上「已知后端仍用 `dynamic_cast`」；**不**做 typed handle 大改；**不**抄 MIR `As<T>()`；**不**在 Debug 用 `dynamic_cast` 认亲。

## TL;DR

进程一次只选一个 RHI 后端；Create 后擦成抽象、再 `dynamic_cast` 认亲，是用 RTTI 重复确认已知不变量。  
本切片：后端私有（及今日 Fallback 派生 PSO 的共享）路径改为 **纯 `static_cast`**——Debug / Release 均不再为认亲使用 `dynamic_cast`。  
长期 typed handle → TD-033，不在本切片。  
**Status: Done — 范围文件已改为纯 `static_cast`；`test smoke` 见 Progress。**

## Problem

1. `OpenGLRHI` / `VulkanRHI`（及 Resources）在 Cmd/Apply 对 `RHI*` 做 `dynamic_cast` → 具体后端类型。
2. 调用点已在对应后端内；`RHIBackendSelection` 保证单后端——类型在 Create 已知。
3. `dynamic_cast` 失败常变 `nullptr` + skip，把不变量违例收成静默失败。
4. Debug 再套一层 `dynamic_cast` assert 仍依赖 RTTI，且与「信任不变量」重复；本修订改为完全不依赖。

## Requirements

- 范围文件中，`RHI*` → 本后端具体类型（及 `RHIGraphicsPSOStateFallback*`）**不得**再出现 `dynamic_cast` / `dynamic_pointer_cast`（含 Debug-only assert）。
- 使用 `static_cast`（可直接写，或可选薄包装，见 Preferences）。
- 合法路径行为与今日成功 cast 一致；`nullptr` 入参仍得 `nullptr`。
- 非法混用后端资源 → UB（违例）；产品禁止该路径，不靠 RTTI 发现。
- 共享 `RHICommandList` 的 Fallback 查询：今日契约下（含 Vulkan PSO : Fallback）同样 `static_cast`。
- Grep 范围文件：作用于 RHI/OpenGL/Vulkan/Fallback 的 `dynamic_cast` → **0**。
- smoke（或约定套件）不回归。

## Constraints

- 不改对外 `RHI` Create API（仍返回抽象 `shared_ptr`）。
- 不把 RHI 塞进 `MEObject` / 反射 `Cast<>`。
- 不引入 MIR `As<T>()` Kind 表。
- 不在本切片做 typed handle。
- 源码注释不写 Feature/Slice/Bug/ADR ID。
- **排除** `RenderGraph.cpp` 的 `RDGTextureResource*` cast。

## Preferences

- **直接 `static_cast<T*>(ptr)`**，不必强上辅助模板。
- 可选：仅处理可读性的 `RHIStaticCast<T>(ptr)` = null 透传 + `static_cast`（**无** `dynamic_cast`）——非必须。
- 机械替换；`if (auto* x = dynamic_cast<T*>(p))` → `auto* x = static_cast<T*>(p); if (x)`（空指针语义保留）。
- 不顺手改 bind 业务语义。

## Assumptions

- **进程不变量：** 当前后端 Create 的资源，在该后端执行路径上即为对应具体类型。
- **不跨后端持有资源。**
- **PSO：** 当前 graphics PSO 均派生自 `RHIGraphicsPSOStateFallback`（含 Vulkan）；若未来引入非 Fallback PSO，先改虚接口。
- 违例不靠 RTTI 诊断；靠创建纪律与（可选）更高层测试。

## Non-Goals

- Debug / Release 任一侧的 `dynamic_cast` 认亲或 assert。
- Typed handle；MIR `As`；关全局 RTTI。
- RDG / Editor / B1-b。
- 改变 `GetNativeHandle()` 产品语义。

## Current Architecture

```text
RHIBackendSelection (OpenGL | Vulkan)
        │
   RHI::RHICreate* → shared_ptr<抽象>
        │
   PSO / Binding / Pass 存抽象指针
        │
   OpenGLRHI / VulkanRHI Cmd·Apply
        └── dynamic_cast → 具体类型 → 原生句柄
```

| 区域 | 约计 | 说明 |
|------|------|------|
| `OpenGLRHI.cpp` | ~8 | |
| `VulkanRHI.cpp` | ~8 | |
| `VulkanRHIResources.cpp` | ~10 | |
| `RHICommandList.cpp` | 1 | Fallback |
| `RenderGraph.cpp` | 1 | **排除** |

## Proposed Architecture

### 不变量（成文）

> 在给定后端的翻译单元内，凡由本进程当前 `RHI` Create 并传入该后端 Cmd/Apply 的抽象 RHI 指针，其动态类型为本后端对应具体类（或文档约定的 Fallback 派生）。

本切片用 **`static_cast` 承认该不变量**；不再用 RTTI 抽查。

### 替换模式

```cpp
// before
if (auto* glTexture = dynamic_cast<OpenGLRHITexture*>(srvDesc.Texture))
{
    target = glTexture->GetTextureTarget();
}

// after
if (OpenGLRHITexture* glTexture = static_cast<OpenGLRHITexture*>(srvDesc.Texture))
{
    target = glTexture->GetTextureTarget();
}
```

`static_cast` 对空指针仍得空指针；`if` 只区分空/非空，**不**区分「错类型」。

Fallback：

```cpp
auto* fallback = static_cast<RHIGraphicsPSOStateFallback*>(pipelineState);
```

### 可选薄包装（非必须）

```cpp
template <typename TTo, typename TFrom>
TTo* RHIStaticCast(TFrom* ptr)
{
    return static_cast<TTo*>(ptr);
}
```

若实现选择直接写 `static_cast`、不加此头，**验收同样通过**。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| OpenGL/Vulkan/CommandList | `dynamic_cast` → `static_cast` | 运行时类型探测 |
| `RHIBackendSelection` | 维持单后端（已有） | cast |
| （可选）`RHIStaticCast` | 语法糖 | Debug RTTI |

## Boundaries

- Allowed：范围文件内 `static_cast` 到本后端具体类型 / Fallback。
- Forbidden：任何为认亲服务的 `dynamic_cast`（含 assert）；MIR `As` / `MEObject::Cast` 处理 RHI；改 RDG cast。

## Dependencies

- Depends on：现有类层次与单后端启动。
- 不依赖：编译期开启 RTTI（本切片认亲路径）。

## Ownership & Lifetime

Not applicable；cast 不改变所有权。

## Data Flow / Control Flow

数据流不变。控制流：空指针仍可跳过；错类型不再变成「当空跳过」。

## State & Invariants

- 单后端；资源不跨后端。
- Graphics PSO 为 Fallback 派生（现状）。
- Illegal 混用 → UB。

## API / Interface Semantics

无新公共产品 API。若有 `RHIStaticCast`，语义 ≡ `static_cast`，含 null 透传。

## Failure Model

| Failure | Behavior |
|---------|----------|
| 空指针 | `static_cast` → null；调用方 skip |
| 错具体类型 | UB（违例） |
| 未来非 Fallback PSO | 须先改设计 |

## Alternatives & Trade-offs

### Option A (recommended) — 纯 `static_cast`

- Pros: 最简；零 RTTI 认亲；对齐单后端事实。
- Cons: 错类型无运行时保险。

### Option B — `static_cast` + Debug `dynamic_cast` assert（初稿）

- Why not selected: 维护者认为多余；仍绑 RTTI；与信任不变量重复。

### Option C — MIR `As` / Backend 字段

- Why rejected / deferred: 过重；typed handle 见 TD-033。

### Option D — 仅 TD、零改码

- Superseded by S02。

## Integration Impact

| Area | Change |
|------|--------|
| `OpenGLRHI.cpp`、`VulkanRHI*.cpp`、`RHICommandList.cpp` | `dynamic_cast` → `static_cast` |
| 可选新头 | 非必须 |
| 对外 API / 资产 | 无 |

## Migration / Compatibility

- 无格式变更。
- 今日「cast 失败当 null 继续」若曾掩盖坏指针：改为 UB——视为去掉错误吞没，不视为功能回归。

## Verification Strategy

- Grep 范围文件：无目标 `dynamic_cast`。
- Build + `test smoke`。

## Open Questions

- 是否加 `RHIStaticCast` 糖衣——实现自选；**不 blocking**。

## Design Review

- **Verdict:** `Ready`
- **Reviewer / date:** maintainer / 2026-10-16（批准纯 static_cast、无包装）
- **Link or summary:** 已实现。

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
- [x] Unresolved questions deferred/accepted

**Ready for implementation?** `yes` — 已实现。

## Acceptance Checklist

- [x] Design 批准
- [x] 范围文件无 RHI 认亲 `dynamic_cast`（含 Debug）
- [x] smoke PASS
- [x] Progress / Registry / Survey 同步
- [x] TD-033 仍 Open（typed handle）

## Slices

| Slice | Scope | Exit |
|-------|--------|------|
| **S02** | OpenGL/Vulkan/CommandList：`dynamic_cast` → `static_cast` | grep + build + smoke |

## Changelog

| Date | Note |
|------|------|
| 2026-10-16 | 初稿：static_cast + Debug dynamic_cast assert |
| 2026-10-16 | **修订：** 纯 `static_cast`；Debug 亦不用 `dynamic_cast` 认亲 |
| 2026-10-16 | S02 Done：直接 `static_cast`，无包装；smoke PASS |
