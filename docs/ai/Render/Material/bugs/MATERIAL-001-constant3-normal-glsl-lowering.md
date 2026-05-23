# MATERIAL-001 — Constant3 → Normal 触发 Debug 崩溃

Status: **Fixed** (2026-05-23)  
Severity: **High**（BlinnPhong 材质编辑阻塞）  
Found: 2026-05-23  
Affects: Editor Material 模式、`MaterialCompiler` GLSL 后端（Debug 构建）

---

## 症状

- **BlinnPhong** 材质下，将 `Constant3` 的 **RGB（output 0）** 连到 MaterialOutput **Normal**。
- Editor **静默退出**（无 Compile Diagnostics、无 shader 错误日志）。
- **Scalar → AO**、Unlit、Masked 裁剪、新节点均正常。
- 退出码 Windows：`0x80000003`（`STATUS_BREAKPOINT` / `__debugbreak`）。

---

## 根因（minEngine）

### 1. Constant3 的 MIR 共享 vec3，引用计数 > 1

`MaterialGraphNodeDef_Constant3::BuildIR` 创建一个 `MIRDimensional`（vec3），并**无条件**挂到 4 个 output：

```cpp
MIRValue* vector = emitter.ConstantFloat3(R, G, B);
emitter.Output(0, vector);                              // RGB
emitter.Output(1, emitter.SubscriptChannel(vector, 0)); // R
emitter.Output(2, emitter.SubscriptChannel(vector, 1)); // G
emitter.Output(3, emitter.SubscriptChannel(vector, 2)); // B
```

即使用户只连 RGB → Normal，该 vec3 的 `NumUsers[Stage_Fragment]` 仍 ≥ 3（三个 Subscript 各算一次 use）。

### 2. GLSL 翻译器对「多 use 且无 local」直接 assert

`GLSLMaterialTranslatorImpl::LowerValue`：

- `NumUsers <= 1` 且 `IsFoldable` → 内联 `LowerInstruction`（正常）。
- 否则若不在 `m_LocalIdentifier` → `ME_ASSERT(false, "MIR lowering referenced an instruction without a pre-declared local.")`。

`SetMaterialOutput(Normal)` 在 **引用** 该 vec3 时，它往往**不在** `LowerBlock` 已遍历的 linked 指令链上（链从 output 往回拉，但 Subscript 等 sibling 可能未进同一 block 的 emit 序），故未预声明 `_N` local。

### 3. Debug assert = 硬断点

`ME_ASSERT` 调用 `__debugbreak()`；未挂调试器时表现为进程直接退出。

---

## UE 对照（新管线：`Translate_New` → MIR → HLSL）

> **更正：** 上一版误用旧 **CodeChunk / `CallExpression` / `GetParameterCode`** 路径（`FHLSLMaterialTranslator::Translate()`）。  
> 与 minEngine 架构可比的是 UE **Material IR 新管线**（UE 5.x，`IsUsingNewHLSLGenerator` / `FMaterialIRToHLSLTranslation`）。

### 两条管线（UE）

| | **旧（Legacy）** | **新（Material IR）** |
|---|------------------|------------------------|
| 入口 | `FHLSLMaterialTranslator::Translate()` | `FHLSLMaterialTranslator::Translate_New()`（名称以源码为准） |
| 图 → IR | 表达式 `Compile()` → **CodeChunk index** | `UMaterialExpression::Build(MIR::FEmitter&)` → **MIR::FValue\*** |
| IR 容器 | `CurrentScopeChunks` 等 | **`FMaterialIRModule`**（`Values`、`PropertyValues`、`EntryPoints[]`） |
| IR → HLSL | `GetMaterialShaderCode()` / `GetParameterCode` | **`FMaterialIRToHLSLTranslation::Run()`**（`MaterialIRToHLSLTranslator.h`） |
| 错误 | `Errorf` / compile sink | **`FMaterialIRModule::AddError`**、`FEmitter::Error`（返回 `FPoison`，不 hard-crash） |

### 新管线阶段（与 minEngine 一一对应）

```text
UE (Translate_New)                          minEngine
────────────────────────────────────────────────────────────
FMaterialIRModuleBuilderImpl                MIRBuilder
  MIR::FEmitter                               MIREmitter
  Expression::Build()                         NodeDef::BuildIR()
  SetMaterialOutput / SetPropertyValue        SetMaterialOutput / FlowValuesIntoMaterialOutputs
  Link + Analyze (FBlock, NumUsers)           Step_LinkInstructions / Step_AnalyzeIRGraph
FMaterialIRModule                           MIRGraph
FMaterialIRToHLSLTranslation::Run           GLSLMaterialTranslatorImpl::Translate
MaterialTemplate.ush 填参                   GLSLMaterialShellAssemblerImpl
```

**关键 API（UE 5.7 文档）：**

- [`MIR::FEmitter`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FEmitter) — 由 `FMaterialIRModuleBuilder` 创建，传给各 `Build()`；含 `ConstantFloat3`、`Subscript`、`SetMaterialOutput`、`Error`。
- [`FMaterialIRModule`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FMaterialIRModule) — backend 无关 MIR；`FEntryPoint` = 每 stage 的 `RootBlock` + `Outputs`。
- [`FMaterialIRToHLSLTranslation`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FMaterialIRToHLSLTranslation) — **MIR module → HLSL 字符串 / template 参数**（这才是新 HLSL 生成器）。

### Constant3 / 多引用：新管线里 UE 怎么做

1. **Emitter 阶段（`Build`）**  
   - Constant3 类表达式：`FEmitter::ConstantFloat3` 得到 **一个** `FValueRef`。  
   - R/G/B 分量通过 `FEmitter::Subscript(Value, Index)` **按需**产生；`Output(ExpressionOutput, Value)` 只登记**该 expression output pin** 的值。  
   - 未参与编译图的 output **不会**走 `Build` 链，因此不会为「未连接的 R/G/B」白建 subscript 指令。

2. **Link / Block 阶段**  
   - 指令挂到 `MIR::FBlock`，`FBlock::FindCommonParentWith` 处理跨 block 的 use（与 minEngine `MIRBuilder::Step_LinkInstructions` 同类问题）。  
   - `NumUsers` 用于决定指令是否必须在 block 序中 **materialize**。

3. **IR → HLSL 阶段（`FMaterialIRToHLSLTranslation`）**  
   - 遍历 `FEntryPoint.RootBlock` 的 linked 指令序，生成 HLSL body。  
   - 当 **`LowerValue` 引用一个 MIR 指令**且该值尚未有 local 名时：按策略 **inline（纯/单次 use）** 或 **emit 临时变量（多 use / 有副作用）** —— **不会 `check()` 杀进程**。  
   - 失败走 `Module.AddError`，编译结果 `IsValid() == false`。

### 与 minEngine 的差距（修正后结论）

| 点 | UE 新管线 | minEngine 现状 |
|----|-----------|----------------|
| 架构分层 | Builder + Emitter + **独立** IR→HLSL | 同构 ✓ |
| Constant3 | Subscript **按需**；未用 output 不 Build | **无条件** emit 4 路 output → 虚假 `NumUsers` |
| IR→HLSL 遇 multi-use | Materialize local 或 safe inline | `LowerValue` **assert**（缺口在 **`GLSLMaterialTranslatorImpl`**，不是 CodeChunk） |
| 编译失败 | Diagnostic / Poison | Debug 下 `ME_ASSERT` → 静默退出 |

**结论：** minEngine 的 **MIR 层设计方向与 UE 新管线一致**；本 bug 是 **IR→HLSL lowering 未完成「按需 materialize」**（`FMaterialIRToHLSLTranslation` 职责），再被 Constant3 eager subscript 放大。

---

## 修复思路（推荐顺序）

### P0 — IR→HLSL lowering（对齐 `FMaterialIRToHLSLTranslation`，解除崩溃）

在 `GLSLMaterialTranslatorImpl`（= UE 新管线 HLSL 生成器）中：

1. **已有 local** → 用 local 名（保持）。
2. **`NumUsers <= 1` 且 `IsFoldable`** → inline（保持）。
3. **新增：`EnsureMaterialized(instr)`**（UE 在 IR→HLSL 阶段对 multi-use / 非 foldable 的处理）  
   - 若 `m_LocalIdentifier` 无条目：emit `Type _k = <LowerInstruction>;`，登记后再引用。  
   - **纯表达式 multi-use**（Constant / Dimensional / Subscript）：可 inline duplicate **或** 共用同一 local（推荐 local，与 UE materialize 一致）。  
4. **无法 lowering** → `MaterialCompileDiagnostic::Error` + `Succeeded = false`；**禁止** `ME_ASSERT` 终止 Editor。

`LowerBlock` 的「先序声明 local」可保留，但 **`LowerValue` 必须能独立 materialize**（因 `SetMaterialOutput.Arg` 常引用不在当前 block 链上的子图，与 UE `PropertyValues` 引用 MIR 指令同构）。

### P1 — Emitter 侧（对齐 `FEmitter` / `Expression::Build`，减虚假 use）

`MaterialGraphNodeDef_Constant3::BuildIR`：

- 仅对已连接（或 preview 需要）的 scalar output 调用 `SubscriptChannel`；RGB 仍 `Output(0, vector)`。  
- 或改为 lazy：`GetOutputValue` 时再 subscript（更接近 UE `Output(ExpressionOutput, …)` 语义）。

可选：`Step_AnalyzeIRGraph` 只统计 **live** use（到达 `SetMaterialOutput` 的 def-use 链），与 UE link 阶段 live range 对齐。

### P2 — 回归测试

`MaterialIRTest` / `--material-ir-test`：

- BlinnPhong + `Constant3(0,1,0)` → `MP_Normal`：`CompileForDiagnostics` 成功 + GPU link 成功。
- 断言 fragment body 含 `FragmentMaterialInputs.Normal = vec3(0.…, 1.…, 0.…)`（或 local 名）。
- 可选：`MakeFloat3` → Normal、`Lerp` → Albedo 等多 use 纯表达式 smoke。

### P3 — assert 策略

- 编译器路径改用 **Diagnostic**，不用 `ME_ASSERT` 终止（`ME_ASSERT` 已改为打印 message，见 `Assert.h`）。
- 保留 assert 仅用于「绝不应发生的内部 invariant」。

---

## 验收

- [x] `--material-ir-test` 含 `VerifyConstant3ToNormalBlinnPhong`（compile + GPU link）。
- [x] GLSL lowering 失败走 **MaterialCompileDiagnostic**，不再 `ME_ASSERT` 杀进程。
- [ ] Editor 手测：BlinnPhong + Constant3 → Normal，Preview 法线/高光视觉正确。

---

## 相关文件

| 文件 | 说明 |
|------|------|
| `MaterialGraphNodeDefs/MaterialGraphNodeNefs.cpp` | Constant3 BuildIR |
| `MaterialCompiler/GLSL/GLSLMaterialTranslatorImpl.cpp` | IR→HLSL（≈ `FMaterialIRToHLSLTranslation`） |
| `MaterialIR/MIRBuilder.cpp` | IR 构建 / link（≈ `FMaterialIRModuleBuilderImpl`） |
| `MaterialIR/MIREmitter.cpp` | ≈ `MIR::FEmitter` |
| `Runtime/Core/Assert/Assert.h` | ME_ASSERT 输出 message |
