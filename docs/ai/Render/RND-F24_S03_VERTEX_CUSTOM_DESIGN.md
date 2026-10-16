# RND-F24-S03 Vertex Custom Attributes Design

## Meta
- **ID:** `RND-F24`（切片 `RND-F24-S03`）
- **Type:** Feature（slice）
- **Complexity:** `L1`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** 2026-10-16（实现 + material-ir 金样）
- **Related:**
  - 父 Design：[RND-F24_MESH_VERTEX_ATTRIBUTES_DESIGN.md](./RND-F24_MESH_VERTEX_ATTRIBUTES_DESIGN.md)（S01–S02 **Done**）
  - 语义表：`VertexAttributeSemantics.h`（Custom0@5 / Custom1@6 **已有**）
  - 范本：S02 `MaterialGraphNodeDef_VertexColor` + `EI_VertexColor` + shell `a_Color@4` / `v_Color`
- **Trust:** Tier A while Review / In Progress

## TL;DR

S01 已固定 Custom location；S02 已打通「外部顶点属性 → MIR ExternalInput → Uses* → shell 按 canonical location 声明」整条链。  
**S03** 镜像该链，交付 **Custom0 / Custom1** 的材质消费：单节点 `VertexCustom`（属性 `CustomIndex`）、`EI_Custom0/1`、`UsesCustom0/1`、shell `a_CustomN` @5/6 + dense varying `v_CustomN`。  
Custom2/3、打包格式、工厂糖衣 **不在本切片**。  
**Status: Done** — Custom0/1 材质消费已落地；S04 工厂糖衣不在本切片。

## Problem

程序化网格可在 layout 写入 `a_Custom0`（VertexAttr 已定义），但材质图 / shell **不能读**：无节点、无 MIR 外部输入、无 compile flag、无 GLSL IO。  
外仓只能再 fork shader；与 S02 前的 Color 缺口同类。

## Requirements

- **S03-R1** 图节点 `MaterialGraphNodeDef_VertexCustom`：属性 `CustomIndex`（MVP：**0 或 1**）；输出与 VertexColor 对齐——`RGBA`（Float4）+ `RGB`（Float3，Cast）。
- **S03-R2** `BuildIR` → `ExternalInput(EI_Custom0 | EI_Custom1)`，类型 Float4。
- **S03-R3** Translator 将 `EI_CustomN` 降为 `v_CustomN`，并置位 `UsesCustomN`。
- **S03-R4** Shell：
  - VS **in**：`layout(location = VertexAttr::Location(CustomN)) in vec4 a_CustomN;`（仅当 UsesCustomN）
  - VS **out** / FS **in**：dense pack 的 `v_CustomN`（见 §Varying 序）
  - Setup：`v_CustomN = a_CustomN;`
- **S03-R5** 未使用 Custom 的材质：shell **不声明** Custom；现有路径零回归。
- **S03-R6** Editor 调色板注册 `VertexCustom`。
- **S03-R7** `material-ir` 增加 Custom0（及建议 Custom1）编译金样，镜像 `VerifyVertexColorUnlit`。

## Constraints

- **不改** VertexAttr location 表（Custom0=5、Custom1=6、Custom2/3=7/8、Bone=9/10 保持）。
- 不改 S01/S02 已交付的 Color / 骨骼契约。
- 网格格式首期仍以 **Float4** 上传为主（与 DefaultType 一致）；节点侧不强制支持 Float2 专用 pin（可用 ComponentMask）。
- 源码注释无 Feature/Slice ID。

## Preferences

- **一个节点 + CustomIndex**，不要 `VertexCustom0` / `VertexCustom1` 两个类（少注册、少反射）。
- Compile flags 用 **`UsesCustom0` / `UsesCustom1` 两个 bool**（与 `UsesVertexColor` 同形，易 grep）；不引入动态数组 API。
- MIR 用 **分立 `EI_Custom0` / `EI_Custom1`**（对齐 `EI_VertexColor`，不用「带参 ExternalInput」）。
- Invalid `CustomIndex`（≥2 或 &lt;0）：`BuildIR` 产出 Poison 或 Constant 零 + Log Warn；**不**默默夹到 0。

## Assumptions

- S02 路径稳定：`ExternalInput` → Translator → `MaterialCompileResult` → `GLSLMaterialShellAssemblerImpl` 参数线程可照抄。
- Vulkan/GL 已按 `RHIVertexElement::Location` 绑定；shell 写出 `location = 5/6` 即可被 layout 对上。
- 首期不要求「UsesCustom 但 mesh 缺语义」的 skip-draw  hardening（父 Design Failure 仍适用；本切片以编译金样为主）。

## Non-Goals

- Custom2 / Custom3 节点可用（表占位即可；index≥2 拒绝）。
- `UByte4Norm` / 半浮点打包。
- S04 工厂糖衣与外仓手册长文。
- Per-instance CustomData。
- 画面级渐变金样（可后置；S02 Color 亦允许后置）。

## Current Architecture（S02 范本）

```text
VertexColor::BuildIR
  → MIREmitter::ExternalInput(EI_VertexColor)   // Float4
  → GLSLMaterialTranslatorImpl::LowerExternalInput
       m_UsesVertexColor = true; emit "v_Color"
  → MaterialCompileResult.UsesVertexColor
  → Shell: a_Color @4 (canonical in) + v_Color (dense varying)
```

| 层 | Color 现状 | Custom 现状 |
|----|------------|-------------|
| VertexAttr | Color@4 | Custom0@5 / Custom1@6 **已有** |
| Node | VertexColor | **无** |
| MIR | EI_VertexColor | **无** |
| CompileResult | UsesVertexColor | **无** |
| Shell | a_Color / v_Color | **无** |

## Proposed Architecture

### 端到端

```text
VertexCustom(CustomIndex=i)::BuildIR
  → ExternalInput(i==0 ? EI_Custom0 : EI_Custom1)
  → Translator: UsesCustomN=true; emit "v_CustomN"
  → Shell:
       in  a_CustomN @ Location(CustomN)
       out/in v_CustomN in dense varying stream
       v_CustomN = a_CustomN
```

### Varying 序（dense，必须稳定）

```text
v_MaterialTexCoord*  →  v_Color?  →  v_Custom0?  →  v_Custom1?  →  lighting varyings…
```

- **Attribute in** 仍用 VertexAttr **稀疏** location（Color 永远 @4，Custom0 永远 @5…）。  
- **Varying** 只按「本材质实际 Uses*」稠密递增，避免 FS `layout(location=N)` 空洞。  
- Lighting varying 起点 = `numTexCoords + UsesVertexColor + UsesCustom0 + UsesCustom1`（bool 当 0/1）。

### 节点

```cpp
ME_CLASS()
class MaterialGraphNodeDef_VertexCustom : public MaterialGraphNodeDef
{
    // CustomIndex: 0 or 1 (MVP). ME_PROPERTY.
    // Outputs: "RGBA", "RGB" — same pin types as VertexColor.
};
```

- `GetOutputPinType`：0→Float4，1→Float3。  
- Editor 显示名：`VertexCustom`（Registry strip 前缀）。

### MIR / Compiler

| 符号 | 说明 |
|------|------|
| `EI_Custom0` / `EI_Custom1` | `MIRExternalInputId` |
| `UsesCustom0` / `UsesCustom1` | `MaterialCompileResult` + Translator 成员 |
| Lower | `EI_CustomN` → append `v_CustomN` |

### Shell API 线程

扩展现有 `BuildVertexIoBlock` / `BuildVertexTexCoordSetup` / `BuildFragmentInTexCoords` / `BuildFragmentLightingVaryings` / `Assemble` 的参数（或打包一个 `VertexAttributeUsage` 小结构——**可选**；布尔扩参亦可，避免过度抽象）。

`needsVertexAttributeSetup`：`numTexCoords > 0 || UsesVertexColor || UsesCustom0 || UsesCustom1`。

### 网格侧

本切片 **不强制**改 StaticMeshLoader；作者用 `VertexAttr::Make(Custom0)` 填 layout。测试可程序化建 layout + 最小图（与 Color 测法一致：先断言 shell 字符串 / Uses*，GPU smoke 可选）。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `VertexCustom` NodeDef | index → EI_*；pin 类型 | shell 文本 |
| MIREmitter | ExternalInput 类型 Float4 | Uses* 标志 |
| GLSL Translator | EI → `v_CustomN` + Uses* | location 数字 |
| Shell Assembler | a_@canonical + v_ dense 序 | 图节点 |
| Editor Registry | 可创建列表 | IR |
| material-ir | 金样断言 | 外仓网格 |

## Boundaries

- Allowed：镜像 S02 全栈最小扩展。  
- Forbidden：改 Bone location；为 Custom 重编号 Color；中央 Util RTTI 梯子；S03 启用 Custom2/3。

## Dependencies

- Depends on：S01 VertexAttr；S02 Color 路径（范本）。  
- Depended on by：外仓程序化网格（S03 后可读 Custom）；S04 工厂可选。

## Ownership & Lifetime

Not applicable beyond existing Material / MIR 图所有权。

## Data Flow / Control Flow

同 S02；同步编译路径；无异步。

## State & Invariants

- `CustomIndex ∈ {0,1}` 才产生合法 EI；否则 Poison/Warn。  
- UsesCustomN false ⇒ shell 无 `a_CustomN` / `v_CustomN`。  
- Attribute location(CustomN) 恒等于 VertexAttr 表。

## API / Interface Semantics

### `MaterialGraphNodeDef_VertexCustom`

- **CustomIndex：** int，序列化；MVP 合法域 {0,1}。  
- **Outputs：** RGBA / RGB；语义同 VertexColor。  
- **Errors：** 非法 index → IR Poison + Warn（实现选定一种，须可测）。

### `MaterialCompileResult`

- 新增 bool，默认 false；Translate 写入；Assemble 只读。

## Failure Model

| Failure | Behavior |
|---------|----------|
| CustomIndex 非法 | Poison + Warn；不夹紧 |
| UsesCustom 但 mesh 无该语义 | 沿用父 Design（Warn / skip batch）；本切片不新增强制逻辑 |
| 重复声明 | 不可能（按 flag 一次） |

## Alternatives & Trade-offs

### Option A (recommended) — 单节点 + EI_Custom0/1 + UsesCustom0/1

- Pros: 与 S02 一致；改动面清晰；index 可扩展到 2/3 而不改类。  
- Cons: 两个 bool / 两个 EI（可接受）。

### Option B — 两个节点 VertexCustom0 / VertexCustom1

- Why not: 双倍注册与重复 BuildIR。

### Option C — 一个 EI_Custom + payload index

- Why not now: 打破 ExternalInputId 简单枚举；Translator 分支变复杂；收益低。

## Integration Impact

| Area | Change |
|------|--------|
| `MaterialIR.h` / `MIREmitter.cpp` | EI_Custom0/1 |
| `MaterialCompileTypes.h` | UsesCustom0/1 |
| `GLSLMaterialTranslatorImpl.*` | Lower + flags |
| `GLSLMaterialShellAssemblerImpl.*` | IO / setup / lighting base |
| `MaterialGraphNodeDef.*` / Nefs / gen | VertexCustom |
| `MaterialGraphNodeRegistry.cpp` | 调色板 |
| `MaterialIRTest.cpp` | 金样 |
| VertexAttr | **无** |

## Migration / Compatibility

- 无磁盘格式破坏（新节点仅新图使用）。  
- 旧材质无 VertexCustom ⇒ 行为不变。

## Verification Strategy

- `material-ir`：Custom0 → Albedo（Unlit）路径；断言 `UsesCustom0`、`layout(location = 5) in vec4 a_Custom0`、`v_Custom0 = a_Custom0`、FS `v_Custom0`。  
- 建议同等覆盖 Custom1 @ location 6（短测即可）。  
- 回归：现有 VertexColor / smoke / 无 Custom 材质仍过。  
- Grep：shell 在 UsesCustom 全 false 时无 `a_Custom`。

## Open Questions

| Q | Blocking? | 建议 |
|---|-----------|------|
| S03 是否必须测 Custom1？ | no | **建议测**；工作量小 |
| Invalid index：Poison vs 编译失败？ | no | **Poison + Warn**（与图容错一致） |
| 是否抽 `VertexAttributeUsage` 结构体？ | no | **可选**；布尔扩参优先，Assembler 参数爆炸再抽 |

## Design Review

- **Verdict:** `Approved`
- **Reviewer / date:** maintainer / 2026-10-16
- **Link or summary:** 批准单节点+CustomIndex、UsesCustom0/1、dense varying 序、非法 index Poison+Warn。

## Implementation Readiness

- [x] Requirements / Constraints / 范本路径已对照代码
- [x] 责任边界与 varying 序已锁定
- [x] 备选方案已比较
- [x] 验证策略可执行
- [x] 开放问题非 blocking

**Ready for implementation?** `yes`（已实现）

## Acceptance Checklist

- [x] Design Review / 批准
- [x] S03 代码落地 + `material-ir` 金样（Custom0 + Custom1）
- [x] 父 Design 切片表 S03 → Done；Progress / Registry / ACTIVE_WORK 同步

## Changelog

| Date | Note |
|------|------|
| 2026-10-16 | 初稿 Review：镜像 S02；Custom0/1 only；单 VertexCustom 节点 |
| 2026-10-16 | Done：实现 + `material-ir` VertexCustom0/1 金样；Assembler 用私有 `VertexAttributeUsage` |
