# UE 方法反射学习笔记（阶段 1：只做机制观察）

Last updated: 2026-05-27  
Status: **学习笔记（不含 minEngine 设计结论）**

---

## 1) 先看到了什么

本轮只聚焦 UE 的“方法反射与调用链”核心文件：

- `Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h`
- `Engine/Source/Runtime/CoreUObject/Private/UObject/ScriptCore.cpp`
- `Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h`
- `Engine/Source/Programs/Shared/EpicGames.UHT/Parsers/UhtFunctionParser.cs`
- `Engine/Source/Programs/Shared/EpicGames.UHT/Types/UhtFunction.cs`

---

## 2) UE 的总体思路（观察结论）

### 2.1 数据层：`UFunction` 本质是一个 `UStruct`

在 `Class.h` 中，`UFunction` 继承自 `UStruct`，并且直接持有函数调用相关元数据：

- `EFunctionFlags FunctionFlags`
- `uint8 NumParms`
- `uint16 ParmsSize`
- `uint16 ReturnValueOffset`
- `FNativeFuncPtr Func`
- `void Invoke(UObject* Obj, FFrame& Stack, RESULT_DECL)`

这说明 UE 把“函数”也纳入了统一反射对象体系（和属性一样可以被运行时查询），不是单纯 C++ 指针表。

### 2.2 代码生成层：UHT 先解析 `UFUNCTION` / `UDELEGATE`

从 `UhtFunctionParser.cs` 和 `UhtFunction.cs` 能看到：

- UHT 明确有 `UFUNCTIONKeyword`、`UDELEGATEKeyword` 入口；
- `UhtFunction` 里除了引擎 `EFunctionFlags`，还有额外的导出/代码生成 flag（`UhtFunctionExportFlags`）；
- 动态委托（`DECLARE_DYNAMIC...`）在 UHT 里也是按“函数签名对象”来解析，含返回值/参数/多播标记等。

可见 UE 的“函数反射”与“动态委托”在工具链阶段已经强绑定了同一套函数签名语义。

### 2.3 调用层：统一入口是 `UObject::ProcessEvent`

`ScriptCore.cpp` 里 `UObject::ProcessEvent(UFunction*, void* Parms)` 是总入口：

- 先做安全与上下文检查（可达性、线程、调试态等）；
- 判断 `FunctionFlags`（native / script / net）；
- 构造 `FFrame`（脚本栈帧）；
- 把传入的 `Parms` 按 `ParmsSize` 拷入帧内存；
- 处理 out 参数链；
- 最终走 `Function->Invoke(...)`，并在结束后做析构/回写。

这里的关键是：不管 native 还是蓝图脚本，都会被收敛到“`UFunction + FFrame` 语义”的执行框架。

### 2.4 参数语义层：`FFrame` 是解释器栈帧 + 参数读取器

`Stack.h` 中 `FFrame` 包含：

- `Code`（字节码指针）
- `Locals`（当前帧局部/参数内存）
- `MostRecentProperty` / `MostRecentPropertyAddress`
- 多种 `Read*` / `Step` 方法

这让 UE 的调用链能同时支持：

- 编译后的 script 字节码执行（`Step` 驱动）
- native 函数参数封送（`ParmsSize` + 属性迭代）
- out 参数与返回值统一处理

### 2.5 委托在 VM 层也是 opcode 级的一等公民

在 `ScriptCore.cpp` 可看到多条委托相关 VM 指令：

- `execLetDelegate`
- `execLetMulticastDelegate`
- `execCallMulticastDelegate`
- `execAddMulticastDelegate`
- `execRemoveMulticastDelegate`
- `execClearMulticastDelegate`
- `execBindDelegate`

这说明 UE 不把委托当作“外部工具层功能”，而是直接纳入脚本执行模型。

---

## 3) 这套做法的核心抽象（仅总结 UE）

可以把 UE 的方法反射理解为四层协作：

1. **声明层**：`UFUNCTION` / `UDELEGATE` 宏标注语义  
2. **生成层**：UHT 产出函数元数据 + thunk glue  
3. **元数据层**：运行时 `UFunction`（flags、参数布局、native 指针）  
4. **执行层**：`ProcessEvent` + `FFrame` 统一调度 native/script/net/delegate

---

## 4) 下一轮阅读建议（仍然只做 UE 笔记）

如果下一轮继续做 UE 侧学习，建议补看：

- `UObject::ProcessInternal` / `ProcessLocalFunction` 细节分支（native vs script 热路径）
- UHT 代码生成中函数 thunk 的实际输出位置（`UhtHeaderCodeGenerator*.cs`）
- `EFunctionFlags` 的定义与常用组合（权限、RPC、Blueprint 可见性）
- Delegate 属性类型（`UhtDelegateProperty` / `UhtMulticastDelegateProperty`）到运行时字段的映射

本笔记保持“观察事实”，不进入 minEngine 方案设计。

