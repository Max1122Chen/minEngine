# 按值传参审计（first-party）

> 生成方式：`node scripts/debug/scan_by_value_params.mjs`（可重跑）
> 目的：找出"大型结构体仍按值传参"的位置——既省拷贝，也消除与 BUG-EDITOR-002 同源的
> 出参栈拷贝隐患（`-O0 + -mavx2` 下 GCC 可能生成 32 字节对齐栈写，栈只保证 16 字节对齐）。

## 方法与局限

- 扫描范围：`minEngine/minEngine/src` + `minEngine/Editor/src`，跳过 `Generated/`、`Third-Party/`。
- 先掩码注释与字符串字面量；再收集 `class/struct` 体按成员类型估算字节数（`Matrix4`=64B、`Vector4`=16B、`std::string`=32B、`std::vector`/`std::function`=24B、指针=8B…）。
- 只报 **估计 ≥24B** 且出现在参数表里、且**没有** `*` / `&` 的形参。
- 已知盲区：模板别名类型（如 `TSubclassOf<T>`）、估计 <24B 的小结构、宏生成的签名；字节数为估算值，不是 `sizeof`。

## P1 · 大结构体按值（建议统一改 `const&`）

| 类型 | 估计大小 | 位置（声明 / 定义） | 调用频率 |
|---|---|---|---|
| `DebugCommandDescriptor` | ~100B（string+vector+function） | `Editor/src/DebugCommand/DebugCommandRegistry.h:29` / `.cpp:13` `Register(...)` | 注册期，低频 |
| `AutomationCapability` | ~100B | `Editor/src/Automation/CapabilityRegistry.h:36` / `.cpp:12` `Add(...)` | 注册期，低频 |
| `Selection` | ~100B+ | `Editor/src/UI/SmGraph/SmGraphWidget.h:99` / `.cpp:354` `SetSelection(Document&, Selection, …)` | UI 交互，中频 |
| `TypeInfo` | ~112B | `Runtime/Core/Reflection/Legacy/Reflection_Legacy.h:22` `RegisterType<T>(TypeInfo)`（模板） | 启动注册 |
| `EditorDocumentTypeInfo` | ~98B | `Editor/src/Shell/Document/EditorDocumentTypeRegistry.h:39` / `.cpp:5` | 启动注册 |
| `ClosedDocumentRecord` | ~96B | `Editor/src/Shell/Document/EditorDocumentHost.h:92` / `.cpp:519` `PushClosedHistory(...)` | 关文档时 |
| **`InputActionValue`** | ~92B | `Runtime/Function/Input/InputModifiers.h:21` + 3 个 override `virtual ModifyRaw(InputActionValue, float)` | **每帧 / 每输入（虚函数）** |
| `ArrayTypeInfo` | ~84B | `Reflection_Legacy.h:159` `RegisterArrayType(...)` | 启动注册 |
| `EnumInfo` | ~68B | `Reflection_Legacy.h:234` `RegisterEnum(...)` | 启动注册 |
| `CallableScriptFunction` | ~56B | `Runtime/Core/Delegates/DynamicMulticastDelegate.h:102` / `DynamicMulticastDelegateBase.h:17` `virtual AddScript(...)` | 脚本注册 |
| `ShadowGraphPermanentOutput` | ~53B | `Render/…/RenderPasses/ShadowGraphPass.h:34` / `.cpp:23` | 渲染配置变更 |
| `SceneContext` | ~52B | `Runtime/Function/Framework/Scene/SceneManager.h:45` / `.cpp:265` `SetEditorSceneContext(...)` | 场景切换 |
| `AnimGraphSelection` | ~48B | `Editor/src/SubEditor/AnimationGraph/AnimationGraphEditor.h:75`（inline，内部已 `std::move`） | 交互 |
| `InputKey` | ~40B | `InputSystem.h:102/103/106` + `.cpp:556/561/789`、`GLFWWindowSystem.h:123/124` + `.cpp:522/530`、`InputMappingContext.h:22` | 每键/鼠标事件 |
| `EditorCommandBinding` | ~39B | `Editor/src/Shell/EditorInputHub.h:39/42` / `.cpp:36/46` | 注册期 |

## P2 · 常见重型类型按值（数量）

| 类型 | 处数 | 判断 |
|---|---|---|
| `std::string` | 128 | 多数是 sink 形参（构造后 `std::move` 存成员），属正常 C++；只挑"接进来又拷贝使用"的改 |
| `std::filesystem::path` | 4 | `PathRegistry::SetEngineDefaultAssetsRootOverride`、`ShaderCompiler::SetGlslangValidatorPath` / `SetCacheDirectory`、`EditorImportDialog::Open` → 改 `const&` |
| `std::function<…>` | 3 | `AssetWorkflowModule::RunWithUnsavedCheck`（3 个参数）→ 改 `const&`（`std::function` 拷贝可能分配） |
| `Transform` | 4 | `EditorSetGameObjectTransformCommand` 构造（命令对象只建一次），收益低 |
| `Vector4` | 2 | `EditorSetSpriteSourceRectCommand` 构造（16B，GCC 用 SSE 对齐写，16B 对齐是有保证的，风险为零） |

## 为什么 P1 优先

1. **崩溃风险同源**：≥32B 结构体按值时，调用方要把它复制到栈上的出参区；`-O0 -mavx2` 下 GCC 可能用
   `vmovdqa ymm, -0xNN(%rbp)`（要求 32 字节对齐），而 Win64 只保证 16 字节 —— 正是
   [BUG-EDITOR-002](./bugs/BUG-EDITOR-002.md) 的成因。
2. **性能**：热路径（`InputModifier::ModifyRaw` 每帧每修饰器一次）最值得先改。
3. **一致性**：注册/配置类接口改 `const&` 后，调用点语义不变、无行为风险。

## 建议处理顺序

1. `InputActionValue`（虚函数 + 每帧） → 2. `SceneContext` / `ShadowGraphPermanentOutput` / `ClosedDocumentRecord` / `EditorDocumentTypeInfo` / `Selection` → 3. 其余注册期类型（纯一致性） → 4. `std::filesystem::path` / `std::function` → 5. `std::string` 只挑非 sink 的。
