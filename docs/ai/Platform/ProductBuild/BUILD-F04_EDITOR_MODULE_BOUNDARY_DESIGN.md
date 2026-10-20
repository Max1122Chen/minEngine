# BUILD-F04 — 编辑器模块边界 · 引擎内核 DLL + 编辑器 DLL + 每项目编辑器 exe

## Meta
- **ID:** `BUILD-F04`
- **Type:** Architecture（模块边界 / 产品形态）
- **Complexity:** `L2`（切片后每片一个语义变更，可独立回退）
- **Status:** `Design — 待主人审批`
- **Owner:** project maintainer
- **Last updated:** `2026-11-30`（首版；全部数字为本轮只读实测，命令见附录 A）
- **Related:**
  - 导出契约：[BUILD-F02](BUILD-F02_EXPLICIT_EXPORT_MINENGINE_API_DESIGN.md)（本 Feature 让 F02 的 **Q3「imgui 留在 DLL 内」** 失去前提）
  - 符号契约四刀：[BUILD-F03](BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md)（**其第 2 刀「imgui 契约化」与本设计 S04 正面互斥，其刀 4 又把「编辑器自己的边界」交给本 Feature —— 与本文的冲突点见 §9）
  - 产品构建链：[BUILD-F01](BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)（`Editor 包装形态（OQ2 — Accepted）` 的下一步）
  - 模块链接契约：[ED-F21](../../Editor/ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_DESIGN.md)、[ED-F20](../../Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)
  - UE 对照：`TargetType.Editor` · `<Project>Editor.Target.cs` · `UnrealEd.Build.cs` 的 `bCompileAgainstEditor` 硬门
- **Depends on:** BUILD-F02 S01+S02（已落地：`MINENGINE_API` 已是真 `dllexport/dllimport`，`MINENGINE_EXPORT_MODE=whitelist` 已生效）
- **Blocks:** 编辑器插件/扩展 ABI、编辑器增量交付、任何"编辑器代码只编一次"的收益

---

## TL;DR

**一句话结论：主人的行业判断成立，且本仓已经走到 60–70% —— 缺的不是"每项目编辑器 exe"（`MinCraftEditor.exe` 在外部仓库 bin 里已经存在），缺的是两件事：（1）编辑器没有 DLL 边界（`MaximumEditorLib` 是 STATIC，每个项目编辑器 exe 各自复制约 188 MB 编辑器代码与调试信息）；（2）`MaximumEditor.exe` 对游戏模块零感知却不报错，产品语义模糊。**

三个核心判断：

1. **"通用编辑器打开任意项目"只在"项目里没有要被编辑器认识的 C++ 代码"时成立。** 这不是实现缺陷，是 PE 加载模型的硬边界：游戏模块是**链接期**绑定的（全仓 `LoadLibrary`/`dlopen` 出现次数 = **0**），没有链接步骤就没有 `ME_CLASS` 静态注册，编辑器就看不到项目的组件类型。
2. **imgui 应当整体搬到编辑器 DLL，而不是给引擎 DLL 补 `IMGUI_API`。** 引擎侧对 imgui 的依赖经 grep 实测为 **0 个 API 调用、0 个 `GImGui` 引用、0 个 `imgui_internal.h`**，只有 `RenderSystem.cpp:3-5` 三行**死 include**；而 253 条 imgui 导出**只被三个编辑器 exe 消费**（MaximumEditor 253 / SampleEditor 253 / 外仓 MinCraftEditor 253），**所有游戏模块对 imgui 的导入都是 0**。把 imgui 从引擎 DLL 拿掉是"删掉一个没人用的契约"，不是"搬家"。
3. **这条路线顺手修掉一个潜伏的 MSVC 断点。** imgui 今天能跨 DLL 使用，靠的是 MinGW 的 `.def` 白名单自动导出；而 `MinEngineExports.cmake:36-40` 对 MSVC 直接 `return()`（F02 Deferred D8）。imgui 移到编辑器 DLL 后**不再跨 DLL**，`IMGUI_API` 可以永远保持空定义（`imgui.h:88`）。

---

## Problem

### P1 · 编辑器没有模块边界：`MaximumEditorLib` 是 STATIC

`minEngine/Editor/CMakeLists.txt:17-20`

```cmake
add_library(MaximumEditorLib STATIC
    ${EDITOR_LIB_SRC}
    ${IMGUI_NODE_EDITOR_SRC}
)
```

后果是**每个编辑器宿主 exe 都静态复制一整份编辑器**（实测 `minEngine/bin`）：

| 产物 | 字节 | 说明 |
|---|---|---|
| `MaximumEditor.exe` | 188,033,796 | 薄 main + 整份 STATIC 编辑器库 |
| `SampleEditor.exe` | 188,034,182 | 同上（仅多链 GameModuleStub） |
| `libminEngined.dll` | 531,407,389 | 引擎内核 |

两个编辑器 exe 体积差 **386 字节**、差 0 个引擎导入（各 579，见 P4）——即"项目编辑器"与"通用编辑器"目前是**同一份代码编译两遍**。项目数 = N 时，编辑器代码与调试信息被复制 N 份；外仓 `D:\Dev\GitRepo\minCraft\bin\MinCraftEditor.exe` 实测 **184,905,322 字节**，同一形态。

### P2 · imgui 编在引擎 DLL 里，引擎自己不用

`minEngine/minEngine/CMakeLists.txt`：

- `:24-27` imgui 源 glob（含 `backends/imgui_impl_glfw.cpp`、`imgui_impl_opengl3.cpp`）
- `:29` 排除 `imgui_demo.cpp`（注释直说：为了压 PE 序号）
- `:30-31` `imgui_freetype.cpp`
- `:33` ImGuizmo 源 glob
- `:39-41` 三者一起进入 `add_library(minEngine SHARED ...)`（`:35`）
- `:159` `target_compile_definitions(... PRIVATE MINENGINE_BUILD IMGUI_ENABLE_FREETYPE)` —— **定义在引擎 target 上**
- `:214` 找到 Vulkan 时把 `imgui_impl_vulkan.cpp` 也编进引擎 DLL

**引擎侧对 imgui 的真实依赖（grep 实测，见附录 A4）：**

| 检查项 | 结果 |
|---|---|
| `minEngine/minEngine/src` 里 include imgui 头文件的文件数 | **1**（`Render/RenderSystem.cpp:3-5`） |
| `RenderSystem.cpp` 里的 `ImGui::` 调用 | **0** |
| 引擎 src 中 `ImGui::` 调用总数 | **0** |
| 引擎 src 中 `GImGui` 引用 | **0** |
| 引擎 src 中 `imgui_internal.h` include | **0** |
| 引擎 src 中 `ImGuizmo` 引用 | **1**，且只是注释（`Framework/Components/SceneComponent.h:78`） |

**结论：`RenderSystem.cpp:3-5` 三行是死 include。引擎 DLL 编译 imgui 的唯一目的就是把它导出给编辑器。**

### P3 · 253 条 imgui 导出，消费者只有编辑器

`python scripts/audit_dll_exports.py --dll minEngine/bin/libminEngined.dll --report` 实测：

| 量 | 值 |
|---|---|
| 引擎 DLL 导出条目 | **1545**（序号余量 **63990**） |
| 构成 | minEngine **1278** / imgui **253** / glfw **8** / glad **5** / other **1** |
| `cmake/minEngine_exports.def` 条目 | **1259**（minEngine 992 / imgui **253** / glfw 8 / glad 5 / other 1；124 条标 `DATA`） |

按消费者拆解导入面（互斥归类；`MaximumEditor.exe` 与 `SampleEditor.exe` 均 579 条 = imgui **253** + 引擎 326）：

| 消费者 | 导入引擎 DLL | 其中 imgui | 其中 glfw / glad | 其中引擎自有 |
|---|---|---|---|---|
| `MaximumEditor.exe` | 579 | **253** | 0 / 0 | 326 |
| `SampleEditor.exe` | 579 | **253** | 0 / 0 | 326 |
| 外仓 `MinCraftEditor.exe` | 573 | **253** | 0 / 0 | 320 |
| `minEngineTests.exe` | 807 | **0** | 8 / 5 | 794 |
| `SampleGame.exe` | 7 | **0** | 0 / 0 | 7 |
| `libGameModuleStubd.dll` | 26 | **0** | 0 / 0 | 26 |
| 外仓 `libminCraft.dll`（游戏模块） | 130 | **0** | 0 / 0 | 130 |

**这是本设计最硬的一条事实：imgui 导出面的消费者集合 = {编辑器 exe}，游戏模块对 imgui 的依赖恒为 0。** 因此把 imgui 移进编辑器 DLL **不改变游戏模块插件契约**。

imgui 家族 253 条的内部构成：imgui core **224** + imgui backends **20** + ImGuizmo **9**；其中唯一的 `DATA` 符号是 `GImGui`（`cmake/minEngine_exports.def:14`）。

### P4 · 通用编辑器对游戏模块零感知，且静默降级

- `minEngine/Editor/src` 全目录 grep `gamemodule`（大小写不敏感）：**0 命中**。
- `minEngine/Editor/CMakeLists.txt:43-46`：通用编辑器不接 `GAME_MODULE`。

```cmake
minengine_add_editor_executable(Editor
    MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/EditorMain.cpp"
    OUTPUT_NAME MaximumEditor
)
```

- `minEngine/cmake/MinEngineTargets.cmake:93-130` 的 `minengine_add_editor_executable(... GAME_MODULE ...)` 已支持游戏模块，`Examples/SampleGameHost/CMakeLists.txt:29-35` 用它产出了 `SampleEditor.exe`。
- 但通用编辑器**没有任何代码路径**去检查"我被要求打开的这个项目是不是需要 C++ 模块"。`Editor.h:137-140` 的 `OpenProject(projectPath)` 只吃一个路径；`Editor.h:201-202` 的 `ResolveProjectDescriptorPath` 只解析描述符位置。

即：**用 `MaximumEditor.exe` 打开一个带 C++ 组件的项目 → 项目类型在 Inspector/层级里查不到 → 不报错，表现为"我的组件丢了"。** 这是最坏的一类失败：无信号。

### P5 · 模块绑定是链接期的，全仓没有动态加载

`LoadLibrary` / `GetProcAddress` / `dlopen` 在 `minEngine/Editor/src` 与 `minEngine/minEngine/src` 中：**0 命中**（唯一相邻命中是 `GLFWWindowSystem.cpp:177` 的 `gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`，属 GL 函数装载，与游戏模块无关）。

游戏模块靠 `-u`/`/INCLUDE` 强制把 DLL 拉进 PE 导入表，让静态注册副作用在 `DllMain` 阶段跑（`MinEngineTargets.cmake:9-10, 21-36`；`MinEngineGameModuleEntry.h:12-18`）。

**推论：** "通用编辑器能打开任意项目"这句话，在本仓当前的模块模型下，其充分必要条件就是 §1 里的先决条件 C1。

### P6 · 现有"项目编辑器"已经跑通，因此本 Feature 不是从零造

`D:\Dev\GitRepo\minCraft\CMakeLists.txt:169-172`（`minCraft-gameplay`、`minCraft-ui` 同构）：

```cmake
minengine_add_editor_executable(MinCraftEditor
    ...
    OUTPUT_NAME MinCraftEditor
)
```

产物 `minCraft/bin/MinCraftEditor.exe`（184 MB，导入 `libminengine.dll` 573 条）。**"每项目编辑器 exe"已经是生产现实。** 本 Feature 要补的是它下面那一层缺失的边界。

---

## Scope / Non-goals

### Scope（本 Feature 内）
1. `MaximumEditorLib`：`STATIC` → `SHARED`，并给它一套与引擎同构的导出状态机。
2. imgui 家族（imgui core + backends + `imgui_freetype.cpp` + ImGuizmo）从引擎 DLL 迁入编辑器 DLL；引擎 DLL 导出减少 **253** 条。
3. `EDITOR_API` 宏与编辑器 DLL 的入口契约（1 个符号）。
4. `MaximumEditor.exe` 的产品语义：重新定位为"资产-only 项目编辑器"，并对"项目声明了 C++ 模块但宿主未链接任何模块"**显式报错**。
5. 编辑器产物打包挂点（`package-editor`），与 `minengine_add_package_game` 同构。

### Non-goals（明确不做，见 §11 Deferred）
1. **不做编辑器插件系统**（不定义"游戏模块向编辑器注册面板"的 ABI）。
2. **不改 PIE 架构**（`PlayInEditorSession` 保持进程内场景克隆，不引入独立 PIE 进程）。
3. **不改资产管线**（资产导入/cook/序列化格式不动）。
4. **不做编辑器热重载**（不改 DLL 后热替换）。
5. **不动 glfw / glad 归属**（归 BUILD-F03 第 1 刀，见 §5）。
6. **不改 `FEATURE_REGISTRY.md`**、不动外仓 `minCraft*`。
7. **不做 MSVC 实测**（沿用 F02 D8 口径；本设计只是让 imgui 不再跨 DLL，降低该路径风险）。

---

## 1. 行业对照（设计地基）

三方对照，**UE / Godot 两行为本地源码实测，Unity 行为公开文档事实（本仓无 Unity 源码，故标注"未实测"）**：

| 维度 | Unreal Engine | Godot 4 | Unity |
|---|---|---|---|
| 引擎内核 | 模块化 DLL 集（`UnrealEditor-Core.dll`、`UnrealEditor-Engine.dll`…） | 单进程序；`modules/` 在**构建期**编进可执行体 | 单进程序 + 原生 runtime（Mono/IL2CPP） |
| 编辑器 | **独立模块/目标**；`TargetType.Editor` = *"Uncooked modular editor executable and DLLs (UnrealEditor.exe, UnrealEditor*.dll, GameName*.dll)"*（`Engine/Source/Programs/UnrealBuildTool/Configuration/TargetRules.cs:27-30`） | 与运行时**同一个二进制**，编辑器是运行模式：`static bool editor = false;`（`main/main.cpp:209`），`-e/--editor` 置真（`main/main.cpp:1537-1539`） | **单一通用编辑器**，不随项目重编 |
| 编辑器代码如何被排除出游戏包 | 模块级硬门：`UnrealEd.Build.cs:9-12` 在 `!Target.bCompileAgainstEditor` 时直接 `throw new BuildException("Unable to instantiate UnrealEd module for non-editor targets.")` | 构建开关：`SConstruct:541` `env.Append(CPPDEFINES=["TOOLS_ENABLED"])`；`editor/SCsub:11` `if env.editor_build:` | 编辑器本身不分发 |
| 每项目编辑器 exe | **是**：`Templates/TP_Blank/Source/TP_BlankEditor.Target.cs:10` `Type = TargetType.Editor;`，`:13` `ExtraModuleNames.Add("TP_Blank")`（项目游戏模块被**编进/链进项目编辑器目标**） | **否**：一个 `godot` 二进制 + 项目目录参数 | **否**：一个 Editor + `.unity` 工程 |
| 项目的 C++ 代码怎么进编辑器 | 项目模块在 `<Project>Editor` 目标里编译/链接 | 在构建期编进定制 editor build（"custom editor build"）；或用 GDExtension | **不进**：项目代码是托管程序集，编辑器运行时加载 |
| 通用编辑器为何可行 | 不可行（编辑器必须与项目模块一起构建） | 有条件可行（有定制构建 / GDExtension） | 可行 |
| 关键机制 | C++ 模块 → 静态/链接期 | C ABI（GDExtension，`editor/editor_node.cpp:1004` 运行时加载进编辑器）+ 动态脚本 | 托管程序集 + 稳定 CLR/IL2CPP 边界 |

### "通用编辑器打开任意项目"成立的三个先决条件

> **C1 · 项目只有资产（数据），没有需要被编辑器认识的本地代码。**
> **C2 · 或项目的可执行扩展以稳定 ABI 运行时加载**（C ABI / GDExtension / COM-like），而非 C++ 链接期符号。
> **C3 · 或项目代码由动态脚本 / 托管运行时承载**（GDScript、C#、Blueprint 级脚本）。

三者同时不满足时，**通用编辑器不可能正确编辑该项目**——它连项目的类型都看不见。C++ 组件类要在编辑器里出现，必须有一份"把该项目 C++ 编/链进编辑器进程"的构建步骤，即每项目编辑器 exe。

**Unity 是例外，不是反例**：它满足 C3（托管程序集 + 稳定运行时边界），且为此付出了"不能自由使用 C++ 组件"的代价。Unity 的 editor 之所以能通用，代价是**项目侧没有 C++ 链接期依赖**。

**对本仓的推论：** 本仓组件的扩展方式正是 `ME_CLASS` + 反射 codegen + 链接期注册（`MinEngineGameModuleEntry.h`），即"C++ 链接期"路线 —— 这条路线**只能**走 UE 形态（每项目编辑器 exe），不满足 C1/C2/C3 任何一条。主人的判断与本地源码证据一致。


---

## 2. 现状测绘

### 已具备 60–70% 的七条依据

| # | 已有能力 | 证据（file:line） |
|---|---|---|
| E1 | 编辑器已是一个**独立 CMake target**（边界接缝已存在，只差链接类型） | `minEngine/Editor/CMakeLists.txt:17-20` |
| E2 | "编辑器宿主 exe"工厂已存在且已带 `GAME_MODULE` 参数 | `minEngine/cmake/MinEngineTargets.cmake:93-130`（`:120-129` 链接 + 拷贝游戏模块） |
| E3 | 游戏模块契约已成型（固定 C 入口 + 强制链接 + 反射 codegen） | `MinEngineTargets.cmake:9-10, 13-42`；`Runtime/Core/Module/MinEngineGameModuleEntry.h:12-18`；`Examples/GameModuleStub/CMakeLists.txt:16-42` |
| E4 | "每项目编辑器 exe"已在小规模生产环境跑通（不只在 in-tree 样例里） | `Examples/SampleGameHost/CMakeLists.txt:29-35` → `SampleEditor.exe`；外仓 `minCraft/CMakeLists.txt:169-172` → `MinCraftEditor.exe` |
| E5 | 引擎的模块导出机制已转正并验证过（`<MODULE>_API` 这套做法可直接复用到编辑器 DLL） | `Runtime/Core/EngineAPI.h:7-17`；`cmake/MinEngineExports.cmake:32-100`；`MINENGINE_EXPORT_MODE=whitelist` 已生效（导出 1545，余量 63990） |
| E6 | 编辑器源码已与渲染后端解耦到"可整包搬走"的程度 | 编辑器 src **152** cpp / **185** h，`ImGui::` 调用 **1359** 处，裸 GL 调用 **0** 处，接触 GLFW 仅 **2** 处（`Editor/src/Editor.cpp:336` 取窗口句柄、`Editor/src/Platform/EditorImGuiBackend.cpp:41` 接收 `GLFWwindow*`）；无任何直接 `glfw*()` 调用 |
| E7 | PIE 是进程内实现，不依赖进程边界，搬到 DLL 不改语义 | `Editor/src/PlayMode/PlayInEditorSession.h:16-41`（`SceneContext` + `PlayObjectMapping` + `SceneCloneContext`），成员内联于 `Editor::m_PlayInEditorSession`（`Editor.h:190`） |

### 缺的是什么（三条，正好是本 Feature 的范围）

| # | 缺口 | 证据 |
|---|---|---|
| M1 | 编辑器**不是 DLL**：无编辑器 ABI、无共享代码、每项目复制 188 MB | P1 |
| M2 | imgui 家族挂在**引擎** DLL 上，253 条导出服务零个非编辑器消费者；且 MSVC 路径靠不住 | P2 / P3；`MinEngineExports.cmake:36-40` |
| M3 | 通用编辑器的**产品语义不明**：不检查项目是否需要 C++ 模块，静默降级 | P4 |

**判定：模式已具备约 60–70%。** 剩余 30–40% 全部是"链接类型 + 归属 + 语义"，**没有一条需要重写编辑器功能**。

---

## 3. 目标形态与模块职责表

### 链接图（目标态）

```text
                    ┌────────────────────────────┐
                    │  libminEngined.dll（内核）   │
                    │  导出 = MINENGINE_API       │
                    │  不含 imgui（−253）         │
                    └───────┬────────────┬───────┘
                            │            │
        ┌───────────────────┴──┐      ┌──┴───────────────────────┐
        │ libMaximumEditord.dll│      │ <Project>Game.dll        │
        │ （编辑器, SHARED）    │      │ （游戏模块, SHARED）      │
        │ OWN: imgui 家族      │      │ ME_CLASS 静态注册         │
        │      node-editor     │      └──┬───────────────────────┘
        │      efsw            │         │
        │      freetype(私有)  │         │
        │ 导出 = EDITOR_API    │         │
        └───────┬──────────────┘         │
                │                        │
    ┌───────────┴──────────┬─────────────┴────────┐
    │                      │                      │
┌───┴──────────────┐  ┌────┴──────────────┐  ┌────┴─────────────┐
│MaximumEditor.exe │  │<Project>Editor.exe│  │ <Project>.exe    │
│ 资产-only 产品    │  │ 项目编辑器（产品） │  │ Game 宿主（无编辑器）│
│ → 引擎+编辑器 DLL │  │ → 引擎+编辑器+Game │  │ → 引擎+Game      │
└──────────────────┘  └───────────────────┘  └──────────────────┘
```

> 注意：`MaximumEditor.exe` 仍然链接编辑器 DLL（它需要资产编辑器 UI），它与 `<Project>Editor.exe` 的差别**不是**"有没有编辑器"，而是"**有没有游戏模块**"。这正是产品语义要钉死的点（§12 Q1）。

### 模块职责表

| 资产 / 职责 | 现状归属 | 目标归属 | 证据 / 理由 | 迁移片 |
|---|---|---|---|---|
| **引擎运行时**（反射、序列化、资产管线、RHI/渲染、物理、音频、脚本、窗口） | `libminEngined.dll` | 不变 | `minEngine/minEngine/CMakeLists.txt:35` | — |
| **imgui core（224 导出）** | 引擎 DLL（`:24-29`） | **编辑器 DLL** | 引擎侧 0 调用（P2）；消费者仅编辑器（P3） | S04 |
| **imgui backends（20 导出：glfw/opengl3/vulkan）** | 引擎 DLL（`:25-26, 214`） | **编辑器 DLL** | 唯一使用者 `Editor/src/Platform/EditorImGuiBackend.cpp:4,52-59,173,219` | S04 |
| **`imgui_freetype.cpp`** | 引擎 DLL（`:31,40`） | **编辑器 DLL** | 只被 `imgui.cpp:22085,22145,23323` / `imgui_draw.cpp:39,4271` 在 `IMGUI_ENABLE_FREETYPE` 下引用；必须与 imgui core 同 DLL | S04 |
| **FreeType** | 引擎 DLL，PRIVATE 静态（`:89-93`） | **两边各一份** | 引擎自用：`Runtime/Function/UI/Font/FontGlyphAtlas.cpp:11-12,38-49`；编辑器侧 imgui_freetype 也需要 | S04 |
| **ImGuizmo（9 导出）** | 引擎 DLL（`:33,41`） | **编辑器 DLL** | 引擎侧仅一条注释（`SceneComponent.h:78`）；编辑器 3 个文件使用 | S04 |
| **imgui-node-editor** | 编辑器 STATIC 库内（`Editor/CMakeLists.txt:9-15,19`） | **编辑器 DLL 内**（同 DLL，符号不外泄） | 4 个编辑器文件 include；内部调用 `ImGui::` | S02 |
| **efsw（文件监视）** | 编辑器 STATIC 库内（`Editor/CMakeLists.txt:32-40`） | **编辑器 DLL 内**（同 DLL） | `Services/AssetWatch/ProjectAssetWatcher.h:6,17,74` | S02 |
| **资产编辑器服务**（AssetWorkflow / ContentBrowser / Inspector / MaterialGraph / AnimGraph / Sprite / SpriteAtlas / Automation / DebugCommand / ContextMenu） | 编辑器 STATIC 库 | **编辑器 DLL** | `Editor/src/Editor.h:6-35,159-199`（全部为 `Editor` 的成员或模块类） | S02 |
| **PIE**（`PlayInEditorSession`） | 编辑器 STATIC 库，进程内 | **编辑器 DLL 内，进程内不变** | `Editor.h:190`；`PlayInEditorSession.h:16-41` | S02（随库搬动，无需改逻辑） |
| **游戏模块加载** | **链接期**，由 exe 承担（`MinEngineTargets.cmake:9-42`） | **不变**（仍链接期），但**归属到 `<Project>Editor.exe`** 的链接行 | P5 | S05 |
| **编辑器入口符号** | `minEngine::CreateApplication`（`Editor.cpp:656`，经 `main.h:7` 声明） | `EDITOR_API` 标注的同一符号，从编辑器 DLL 导入 | §4.3 | S02 |
| **编辑器自动化管道**（`AutomationHost` / `AutomationPipeServer`） | 编辑器 STATIC 库 | 编辑器 DLL（**导出面之外**） | `Editor.h:142-146,199`；管道名经 CLI 传入 | S02 |

**命名建议（最小改动）：** 保留 target 名 `MaximumEditorLib`（`MinEngineTargets.cmake:95` 的 `if(NOT TARGET MaximumEditorLib)` 与 `:119` 的 `target_link_libraries` 无需改动），只改 `add_library` 的类型并加 `OUTPUT_NAME MaximumEditor` → 产物 `libMaximumEditord.dll`（与 `libminEngined.dll` 命名族一致）。

---

## 4. imgui 归属迁移：完整影响分析

### 4.1 导出面变化

| 量 | 迁移前 | 迁移后 |
|---|---|---|
| 引擎 DLL 导出条目 | 1545 | **1292**（−253） |
| 引擎 `.def` 条目 | 1259 | **1006**（−253；占白名单 **20.1%**） |
| 引擎 DLL 中 `GImGui` | 存在（`minEngine_exports.def:14`，`DATA`） | **不存在** |
| 编辑器 DLL 导出条目 | 无此产物 | `EDITOR_API` 集合（今天 **1** 个：编辑器入口） |
| 编辑器 exe 从引擎导入 | 579 | **326**（−253） |
| 编辑器 exe 从编辑器 DLL 导入 | — | **1**（入口符号） |
| 游戏模块从引擎导入 | 26 / 130（imgui 0） | **不变** |

**253 条 imgui 导出不应出现在编辑器 DLL 的导出表里。** 迁移后 imgui 只被同一 DLL 内的 node-editor 与编辑器 TU 使用（DLL 内部引用），无需导出。这与 §6 的 explicit 模式一致：编辑器 DLL 走 `--exclude-all-symbols` + `EDITOR_API`，imgui 符号自然不外泄。

### 4.2 `GImGui` 数据单例跨 DLL 唯一性 —— 本设计的第一号风险

`GImGui` 是 `imgui.cpp` 里的全局 `ImGuiContext*`，`ImGui::GetCurrentContext()` 的全部语义都建立在它之上。**若进程里存在两份 `GImGui`，就是两个独立上下文** → 症状：`IM_ASSERT` 命中、窗口 ID 冲突、字体图集分裂、渲染错乱或直接崩。

本设计**用结构消除它，而不是用约定约束它**：

> **不变量 I-gimgui：进程内 `GImGui` 的定义体（即 imgui 的编译单元）只能存在于一个 target 中。**

落实手段：
1. imgui 源文件从 `minEngine/minEngine/CMakeLists.txt:24-33,39-41,214` **移出**，引擎 target 不再编译任何 imgui TU；
2. 编辑器 target 编译 imgui 全家（core + backends + freetype + ImGuizmo）；
3. **可自动断言**：解析产出二进制，要求 `GImGui` 在 `libminEngined.dll` 导出表中**不存在**，且在编辑器 DLL 中存在（内部符号即可，不必导出——用 `objdump -p`/`-t` 或 `audit_dll_exports.py --emit-export-dump` 交叉核对）。

今天这条不变量**尚未被任何机制保证**：imgui 只在引擎 target 编译，是"恰好单份"。S04 之后必须由 CMake 结构保证。

### 4.3 `IMGUI_API` 与 `MINENGINE_API` 的关系

| | `IMGUI_API` | `MINENGINE_API` |
|---|---|---|
| 定义位置 | `Third-Party/imgui/imgui.h:88` → `#define IMGUI_API`（**空**）；模板在 `imconfig.h:27-29`（**全部注释掉**） | `Runtime/Core/EngineAPI.h:7-17`：`MINENGINE_BUILD` → `dllexport`，否则 `dllimport` |
| 语义 | 第三方库自定义钩子，**只影响 imgui 自己的声明** | 引擎第一方契约，`MINENGINE_BUILD` 只能定义在产出引擎 DLL 的 target |
| 今天为何能用 | imgui 符号靠 MinGW `.def` 白名单自动导出（253 条），`IMGUI_API` 空着不影响 | 显式标注 + 白名单取并集 |
| 迁移后 | **保持空**（imgui 已不跨 DLL） | 编辑器 DLL 用**同构但独立**的 `EDITOR_API` |

**关键结论：** `IMGUI_API` **不需要**被定义。BUILD-F02 的 **Q3（推荐 ①`IMGUI_API` dllexport/dllimport，imgui 留在 DLL 内）** 的前提是"imgui 留在引擎 DLL"；本设计改变了该前提，因此 **F02 Q3 应被本设计取代并留空**，F02/F03 计划里"imgui 契约化"一条应删除（见 §9）。

新增的 `EDITOR_API`（建议 `Editor/src/EditorAPI.h`）：

```cpp
#if defined(_WIN32)
    #ifdef MAXIMUMEDITOR_BUILD
        #define EDITOR_API __declspec(dllexport)
    #else
        #define EDITOR_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define EDITOR_API __attribute__((visibility("default")))
#else
    #define EDITOR_API
#endif
```

**不变量 I-buildexact：** `MAXIMUMEDITOR_BUILD` 只能定义在产出编辑器 DLL 的 target（与 `EngineAPI.h:3-6` 为 `MINENGINE_BUILD` 写的铁律完全同构 —— F02 的 P4 记录过这条被误用的代价）。

### 4.4 引擎侧是否还依赖 imgui —— grep 结论

**结论：不依赖。** 唯一接触点是 `RenderSystem.cpp:3-5` 三行 include，且该文件零 `ImGui::` 调用、零 `ImDrawData` 使用、零 `GImGui` 引用（附录 A4 可复现）。

因此 S04 的引擎侧改动 = **删 3 行死 include + 把 imgui 源从 target 移出**，无任何逻辑改动。

其余 `ImGui` 字样在引擎 src 中全部是**注释**（8 处）：

| 文件:行 | 性质 |
|---|---|
| `Framework/Components/SceneComponent.h:78` | 注释（"e.g. ImGuizmo world result"） |
| `Render/GLFWWindowSystem.cpp:96, 282, 569` | 注释（说明会与 imgui 的 GLFW backend 链式处理事件） |
| `Render/RenderGraph/RenderGraph.cpp:448`、`RenderPass.cpp:45`、`Vulkan/VulkanRHI.cpp:1286`、`Vulkan/VulkanRHI.h:90` | 注释（提到 Editor/ImGui viewport 的纹理生命周期约束） |

**注意 `GLFWWindowSystem.cpp:96,569` 的两条注释是真实约束**：imgui 的 GLFW backend 会安装/链式接管 `glfwSet*Callback`。迁移后 backend 在编辑器 DLL 内，**只要仍通过 `WindowSystem` 的公开接口取窗口与回调链**，这条约束不变；但若编辑器 backend 与引擎窗口系统各自安装回调而顺序变化，会丢失输入事件 —— 列入 §7 Failure Model。

### 4.5 FreeType：迁移的隐性代价（必须记录下来）

`imgui_freetype.cpp` 依赖 FreeType；FreeType 今天 `PRIVATE` 静态链进引擎（`minEngine/minEngine/CMakeLists.txt:89-93`），且 `MinEngineExports.cmake:51` 用 `--exclude-libs=...:libfreetype.a:libfreetyped.a` **明确禁止**它进导出表。而引擎自身**确实**需要 FreeType（`Runtime/Function/UI/Font/FontGlyphAtlas.cpp:11-12,38-49` 调 `FT_Init_FreeType` / `FT_New_Memory_Face`）。所以 FreeType 必须留在引擎 DLL，不能被 imgui 带走。

| 选项 | 后果 | 结论 |
|---|---|---|
| **① 编辑器 DLL 再静态链一份 FreeType** | 进程内两份 FreeType 代码，各自独立的 `FT_Library` 实例；FreeType 无跨实例全局契约 → 语义安全，代价是 ~1–2 MB 代码重复与两份初始化 | **推荐**（最小、最可预测） |
| ② 让引擎导出 FreeType | 要破开 `--exclude-libs` 并导出数百条 FreeType 符号，重新污染导出面（F02 刚清干净） | 否 |
| ③ 把 FreeType 改成第三方 DLL（`libfreetype-6.dll`，参照 assimp 预编译模式，`minEngine/CMakeLists.txt:64-68`） | 一份实现、两方共享；但 Staging 多一个 DLL，且 freetype 目前无预编译产物 | **Deferred**（若 ① 的重复被证明有实际代价再评估） |

### 4.6 编译配置漂移（现状已存在，迁移顺手修掉）

`IMGUI_ENABLE_FREETYPE` 只定义在引擎 target（`minEngine/minEngine/CMakeLists.txt:159`）。因此**今天**：

- imgui 自己的 TU：定义了 `IMGUI_ENABLE_FREETYPE` → `imgui_internal.h:126-130` 不会 `#define IMGUI_ENABLE_STB_TRUETYPE`；
- 编辑器 target 的 node-editor TU：**没有**定义 → 它们看到的 `imgui_internal.h` 会打开 `IMGUI_ENABLE_STB_TRUETYPE`。

已核实这**不改变结构体布局**：该 define 只影响 `imgui_draw.cpp:39,4271` 的 stb_truetype 代码是否编译，以及 `imgui_internal.h:126-130` 的宏默认值；`imgui.h:3929` 的 `ImFontLoader* FontLoader` 成员与 define 无关。所以今天是"侥幸正确"。

迁移后在编辑器 target 上把 imgui 编译选项设为 `PUBLIC`（`IMGUI_ENABLE_FREETYPE`），编辑器所有 TU 与 node-editor 看到同一个配置 → **漂移消失**。这是 S04 的附带收益，必须写进验收。

### 4.7 Vulkan 后端随迁的附带要求

`imgui_impl_vulkan.cpp` 现在挂在引擎 target 上（`minEngine/minEngine/CMakeLists.txt:209-217`）。迁到编辑器后，编辑器 target 需要 Vulkan 头 + 库。引擎已经把 Vulkan include 目录设为 `PUBLIC`（`:211`）、`MINENGINE_HAS_VULKAN=1` 设为 `PUBLIC`（`:213`），因此编辑器**自动继承**；只需在编辑器 CMake 里 `target_link_libraries(... PRIVATE Vulkan::Vulkan)`。这一点必须在 S04 的验收里明写，否则 Vulkan 构建路径（`--rhi vulkan`）会静默失去 imgui 后端。

---

## 5. 静态依赖处理（与 BUILD-F03 第 2 刀口径对齐）

`BUILD-F03` 在登记表里的四刀是「glfw/glad 出表 · **imgui 契约化** · 全面标注 · 删 `.def`」（`docs/ai/FEATURE_REGISTRY.md:72`）。本设计与之的交界：

| F03 刀 | 与本设计的关系 |
|---|---|
| 第 1 刀 glfw/glad 出表 | **不重叠**。glfw 8 / glad 5 的消费者只有 `minEngineTests.exe`（实测），它们从引擎 DLL 拿走是因为测试直接调 GL；与编辑器 DLL 无关。留给 F03。 |
| **第 2 刀 imgui 契约化** | **正面互斥**。F03 的机制是 `target_compile_definitions(minEngine PRIVATE IMGUI_API=__declspec(dllexport))` + `INTERFACE IMGUI_API=__declspec(dllimport)`，从 `.def` 删 253 行（`BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md:223`），即 **imgui 继续留在引擎 DLL**。本设计是把它**整族搬进编辑器 DLL**，引擎 DLL 里根本没有 imgui。见 §9 与 §12 Q2。 |
| 第 3 刀全面标注 | 不重叠（`MINENGINE_API` 面）。本设计只新增 `EDITOR_API`，规则同构，可作为第 3 刀的"第二个模块"样板。 |
| 第 4 刀删 `.def` | **可复用同一状态机**。编辑器 DLL 复用 `minengine_apply_export_mode`（`cmake/MinEngineExports.cmake:32`），因此 `auto → whitelist → explicit` 与 `-DMINENGINE_EXPORT_MODE=auto` 一行回退**对编辑器同样成立**。 |

### S02 之后三类静态依赖的策略

| 依赖 | 现状 | SHARED 后策略 | 理由 |
|---|---|---|---|
| **imgui-node-editor**（4 个 cpp） | 编进 `MaximumEditorLib`（`Editor/CMakeLists.txt:9-15,19`） | 继续直接编进编辑器 DLL 源列表 | 它只是编辑器的一个功能部件，不是独立可复用的库；放进去后它的 `ImGui::` 引用变成**同 DLL 内部引用**，天然解决 |
| **efsw**（`efsw-static`） | `BUILD_SHARED_LIBS OFF` + 静态链（`Editor/CMakeLists.txt:32-40`） | 保持静态链进编辑器 DLL；`--exclude-all-symbols` 模式下无外泄风险，`--exclude-libs=libefsw-static.a` 变成可选加固项 | efsw 无跨 DLL 全局契约，仅 `ProjectAssetWatcher` 一个消费者（`Services/AssetWatch/ProjectAssetWatcher.h:74`） |
| **FreeType** | 静态进引擎（`:89-93`） | **引擎留一份；编辑器另链一份**（§4.5 选项①） | 双方真正都需要 |

### 新增不变量（必须由构建断言）

> **I-onedll：一个第三方静态库的编译单元只能进入一个 DLL。**
> 逐项检查：imgui / ImGuizmo / node-editor / efsw → 编辑器 DLL；freetype → 引擎 DLL **和** 编辑器 DLL（**唯一被批准的例外，理由 = 无跨实例全局契约**）；glad / glfw / stb / Jolt / assimp / lua / nfd → 引擎 DLL。

`BUILD_SHARED_LIBS OFF` 写在 `Editor/CMakeLists.txt:34`（`CACHE ... FORCE`）需要在 S02 复核：它是**全局 cache 变量**，若将来在 `add_subdirectory(Editor)` 之后还有别的 `add_subdirectory` 会受牵连。当前 `minEngine/CMakeLists.txt:79-89` 的次序里，其后只有 `Examples/SampleGameHost` 与 `Tests`，两者都不依赖 `BUILD_SHARED_LIBS` → 安全，但**改成局部变量 `set(EFSW_BUILD_SHARED ...)` 更稳**，列入 S02 的清理项。


---

## 6. 迁移切片

切片原则：**每片只动一条轴**（① 编辑器获得 DLL 边界；② imgui 归属；③ 产品语义），且每片结束时构建与测试仍绿、回退是"改一个文件/一行开关"。

| 片 | 做什么 | 结束时构建为何仍绿 | 回退方式 | 改变发布产物？ | 人日 |
|---|---|---|---|---|---|
| **S01** 观测与护栏（不改行为） | ①审计脚本扩一条 `--consumer` 对编辑器 DLL 的断言；②新增 CMake target `boundary-audit`：记录三个编辑器 exe 的引擎导入基线（579/579/573）、引擎导出基线 1545、`.def` 1259；③基线写入 `docs/ai/Platform/ProductBuild/BUILD-F04_S01_BASELINE.md` | 纯新增文件 + 一个不参与默认构建的 custom target | 删文件 | **否** | 0.5 |
| **S02** `MaximumEditorLib` `STATIC` → `SHARED` | ①`Editor/CMakeLists.txt:17` 改 `SHARED` + `OUTPUT_NAME MaximumEditor`；②新增 `Editor/src/EditorAPI.h`（`EDITOR_API`）；③`MAXIMUMEDITOR_BUILD` 只定义在编辑器 DLL target；④`Editor.cpp:656` 的 `CreateApplication` 标注 `EDITOR_API`；⑤`main.h:7` 增加 `MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL` 分支（见 §12 Q3）；⑥编辑器 DLL 套用 `minengine_apply_export_mode`（**必须**，否则 MinGW 默认导出全部编辑器符号 → 新的序号炸弹）；⑦编辑器 `.def` 白名单由编辑器 exe 的 import 表生成（今天仅 1 条）；⑧`BUILD_SHARED_LIBS` 换成局部变量 | 编辑器 exe 只需要 `CreateApplication` 1 个符号；白名单由消费者 import 表**构造**，⊇ 全部需求（等价性可证，同 F02 S01 的论证）；三类 target 依旧链接成功 | `STATIC` 一行 + 去掉 1 处标注 | **是**：`bin` 新增 `libMaximumEditord.dll`；`Staging/SampleGame` **不变**（游戏包不含编辑器）；`MaximumEditor.exe`/`SampleEditor.exe` 从 188 MB 降到"薄 exe" | 1.5–2 |
| **S03** 编辑器导出收敛到 explicit | 燃烧编辑器 `.def` 到 0，切 `--exclude-all-symbols`（复用同一 `MINENGINE_EXPORT_MODE` 开关；先在独立构建目录验证） | 白名单已空 → 导出集合等价；编辑器 DLL 的导出面此刻 = `EDITOR_API` 标注集合 | 切回 `whitelist`（一行） | **否**（产物名不变，只有导出表变小） | 0.5 |
| **S04** imgui 家族迁入编辑器 DLL | ①从 `minEngine/minEngine/CMakeLists.txt:24-33,39-41,159,214` 移出 imgui/ImGuizmo/backends/freetype，落到编辑器 target；②删 `RenderSystem.cpp:3-5` 三行死 include；③引擎 `.def` 删 253 行（imgui 家族归零）；④编辑器 target 上 `IMGUI_ENABLE_FREETYPE` 设为 `PUBLIC`；⑤编辑器链 `freetype` + `Vulkan::Vulkan` | 三个编辑器 exe 原本就从引擎 DLL 拿 imgui，迁移后从编辑器 DLL 拿（同进程、同符号名、同布局，因 `IMGUI_API` 仍为空、`IMGUI_ENABLE_FREETYPE` 反而是**唯一**配置）；**游戏模块对 imgui 的依赖实测为 0** → 插件契约不变；引擎侧零调用 → 引擎不会因缺 imgui 而链接失败 | 把 imgui 源行搬回引擎 target + 恢复 `.def` 253 行 + 撤销 3 行删改（两个 CMake 文件 + 一个源文件） | **否**（产物名不变）；引擎 DLL 导出 1545 → 1292，`.def` 1259 → 1006 | 2–3 |
| **S05** 产品语义：资产-only + 缺模块显式报错 | ①`Editor` target 定义 `MINENGINE_EDITOR_ASSET_ONLY=1`（仅 `MaximumEditor`）；②`Editor::Initialize` 里解析项目描述符的模块声明，若项目声明了 C++ 模块而本宿主未链接任何游戏模块 → 错误日志 + 非零退出码 + 恢复指引（"请运行 `<Project>Editor.exe`"）；③`minengine_add_editor_executable` 在 `GAME_MODULE` 存在时定义 `MINENGINE_EDITOR_HAS_GAME_MODULE=1` | 只增一条**启动期检查**与两个编译期宏；资产-only 项目保持原行为；`SampleEditor`/`MinCraftEditor` 走"已链接模块"分支，行为不变 | 删宏 + 删检查（2 处） | **否** | 1 |
| **S06** 项目编辑器产品化 + 打包挂点 | ①新增 `minengine_add_package_editor`（与 `minengine_add_package_game` 同构，`MinEngineTargets.cmake:138-180`）；②`Examples/SampleGameHost/CMakeLists.txt` 增加 `package-SampleEditor`；③BUILD-F01 模板的 Editor 目标成为默认形态，`MaximumEditor` 明确登记为"资产编辑样例" | 纯新增 custom target，不改变默认 `all` 的语义 | 删 custom target | **是（新增）**：`Staging/<Project>/` 内含 `<Project>Editor.exe` + `libMaximumEditord.dll` + `libminEngined.dll` + `<Project>Game.dll` | 1 |

**"每片仍绿"的三个支点**（与 F02 同构，可直接复用其论证）：
1. **两机制并存**：`.def` 白名单与 `EDITOR_API`/`MINENGINE_API` 标注取并集 → 任何"半标注"状态都合法可链接。
2. **切换点唯一**：只有 S02（STATIC→SHARED）、S03（whitelist→explicit）、S04（imgui 换宿主）三个语义切换点，每个都有独立断言。
3. **双构建目录**：像 F02 的 `build-explicit` 那样，先在不影响主 `build/` 的目录里跑通再改默认。

---

## 7. Failure Model

| # | 失效模式 | 典型症状（可观测） | 观测手段 | 回滚 |
|---|---|---|---|---|
| **F1** | **双份 `GImGui`（两个 imgui 上下文）** | `IM_ASSERT` 命中 / 窗口点击错位 / 字体图集分裂 / 启动即崩 | ①`audit_dll_exports.py --dll libminEngined.dll` 断言 `GImGui` **不在**引擎导出表（自动化 I-gimgui）；②ASan 构建（`-DMINENGINE_ENABLE_ASAN=ON`，`build-asan/` 已存在）跑编辑器启动 | 撤销 S04（imgui 源搬回引擎） |
| **F2** | 静态初始化顺序：游戏模块的 `ME_CLASS` 静态注册晚于编辑器 DLL 的静态初始化 | 项目组件在层级/Inspector 里"偶尔"或"总是"缺失；反射冒烟通过但编辑器看不到类型 | 反射冒烟（`Tests/CMakeLists.txt:102-109` 已链 `GameModuleStub`）+ 编辑器启动后断言"游戏模块声明的类型在 `ReflectionSystem` 中可查"（O7） | 撤销 S02（回到 STATIC：编辑器代码与 exe 同镜像，静态初始化顺序回到 exe-first） |
| **F3** | **不变量被破坏**：编辑器在静态初始化阶段枚举反射 | 编辑器启动极早期崩（注册表半初始化） | 现状已满足：`Editor.cpp:424` 的 `RegisterModules()` 在 `Initialize()` 内；反射枚举均为函数作用域（`TypeQueryServices.cpp:14`、`DebugCommandSceneUtils.cpp:509`）。S02 后加断言：`Editor.cpp` 不得出现命名空间作用域静态对象 | 移除该静态对象 |
| **F4** | 跨 DLL `new`/`delete` 与 RTTI/vtable | `delete app`（`main.h:41-45`）在 exe 内执行、对象在编辑器 DLL 内分配 → CRT 不一致时崩；`dynamic_cast`/`catch` 跨边界不命中 | ASan 构建 + `bin-asan` 跑完整退出路径；跨边界 `&typeid(Editor)` 取址一致断言 | 撤销 S02 |
| **F5** | 静态库进 DLL 的符号重复 | 同一 inline 的静态局部变量在两份副本里地址不同 → 注册表/单例"查不到"；链接期无错 | `objdump -p` 对比两 DLL 的导出符号交集（应只剩引擎契约）；I-onedll 的脚本化检查 | 按 §5 表格把该静态库收回单一 DLL |
| **F6** | 编辑器 DLL 的序号炸弹（重演 F02 P1） | `export ordinal too large` + **产物被删除**（最坏失败模式，已在引擎上发生过） | 编辑器 target 复用 `MinEngineExports.cmake:90-97` 的 POST_BUILD 断言（`--max-exports 3000 --min-headroom 60000`）→ 在 3000 处 fail，不再撞 65535 | `-DMINENGINE_EXPORT_MODE=auto` 一行（会重新武装炸弹，仅作临时） |
| **F7** | imgui backend 与引擎窗口系统的回调链顺序变化 | 键盘/鼠标事件丢失、输入焦点错乱（引擎 `GLFWWindowSystem.cpp:96,569` 的注释约束被破坏） | 编辑器启动后自动化冒烟：注入一次按键/鼠标，断言编辑器收到（`AutomationHost` 已具备管道能力，`Editor.h:142-146`） | 撤销 S04 |
| **F8** | 双份 FreeType 的语义/内存问题 | 字体渲染差异、内存增长（两份 `FT_Library`）；FreeType 无跨实例契约，风险低但需确认 | 编辑器启动后字体渲染回归比对；`FT_Init_FreeType` 失败日志（`FontGlyphAtlas.cpp:38-41` 已有 `ME_LOG(LogUI, Error, ...)`） | 改 §4.5 选项③（FreeType 转第三方 DLL） |
| **F9** | 编辑器热重载（改 DLL 后热替换） | 崩溃 / 类型系统撕裂 | **本设计不承诺热重载**；若有人尝试，症状即在上表 | 不做（Deferred） |
| **F10** | MSVC / 非 MinGW 工具链 | imgui 跨 DLL 无导出 → 链接期 `undefined reference`（**loud**，好事） | `MinEngineExports.cmake:36-40` 已有 MSVC 早退分支；S04 之后该风险面**消失**（imgui 不跨 DLL） | — |

**回滚总则：** 全部状态落在 **5 个文件** —— `minEngine/minEngine/CMakeLists.txt`、`minEngine/Editor/CMakeLists.txt`、`minEngine/cmake/MinEngineExports.cmake`（若扩参数）、`Editor/src/EditorAPI.h`（新增）、`cmake/minEngine_exports.def`。总闸是 `MINENGINE_EXPORT_MODE` 与 `add_library(... STATIC|SHARED)` 两处。

---

## 8. Verification

### 可观察 oracle（全部可自动断言）

| # | Oracle | 断言 | 当前 | 目标 |
|---|---|---|---|---|
| **O1** | 引擎导出条目 | `NumberOfFunctions ≤ 3000` | 1545 ✅ | ≤ 1292 |
| **O2** | 序号余量 | `65535 - maxNamedOrdinal ≥ 60000` | 63990 ✅ | 63990+ |
| **O3** | 消费覆盖 | `exported ⊇ consumed`（6 个消费者：MaximumEditor/SampleEditor/SampleGame/GameModuleStub/minEngineTests + 外仓 minCraft；当前全部 missing 0） | ✅ | ✅（且编辑器 exe 从引擎的导入 579 → 326） |
| **O4** | **I-gimgui** | `GImGui` 只出现在一个二进制里；**引擎导出表中不存在** | 在引擎导出表（`.def:14`） | 只在编辑器 DLL |
| **O5** | **imgui 零残留** | 引擎 DLL 导出集中 imgui 归类条数 = **0**；`.def` 中 = 0 | 253 / 253 | 0 / 0 |
| **O6** | **编辑器 DLL 导出面 = 契约** | 编辑器 DLL 导出条目 = `EDITOR_API` 标注集合（explicit 模式）；条数 ≤ 8 | 无此产物 | ✅ |
| **O7** | **模块可达性** | 用 `<Project>Editor.exe` 打开项目后，`ReflectionSystem::Get().GetAllClasses()` 中能查到游戏模块声明的每个 `ME_CLASS`（如 `GameModuleStub` 的 `StubGamePrimitiveComponent`） | 反射冒烟已覆盖（`Tests/CMakeLists.txt:102-109`） | 扩到"编辑器进程内" |
| **O8** | **缺模块必须报错** | `MaximumEditor.exe <带 C++ 模块的 .meproject>` → 退出码 ≠ 0 且日志含明确指引；同一项目用 `<Project>Editor.exe` → 退出码 0 | 静默降级（无信号） | ✅ |
| **O9** | 三类 target 全链接 | `minEngineTests` + `MaximumEditor` + `SampleEditor` + `SampleGame` + `GameModuleStub` 全部链接成功 | ✅ | ✅ |
| **O10** | `verify.ps1` 全绿 | `test smoke` 退出码 0（`scripts/verify.ps1:15,21`） | ✅ | ✅ |

### 最小自动化路径

```text
scripts/audit_dll_exports.py                    # 已存在，三用途
  ├─ --report                                   → 引擎导出数/组成/headroom（O1/O2/O5）
  ├─ --max-exports 3000 --min-headroom 60000 --fail   → 构建期断言
  └─ --consumer <exe> --verify-coverage         → 逐消费者覆盖（O3）

新增（S01）：
  scripts/audit_module_boundaries.py            # 只读；或复用上面脚本的 --emit-export-dump
    ├─ 断言 GImGui 只在一个二进制（O4）
    ├─ 断言引擎导出集 imgui 归类 = 0（O5）
    ├─ 断言编辑器 DLL 导出集 ⊆ EDITOR_API 允许名单（O6）
    └─ 断言两个 DLL 的导出符号交集 = 引擎契约（F5）

scripts/verify.ps1（追加 2 行）
  python scripts/audit_dll_exports.py --max-exports 3000 --fail
  python scripts/audit_module_boundaries.py --fail
```

### 实施前必须先跑的 4 个 5 分钟实验

- **V0 · STATIC→SHARED 可行性**：临时把 `add_library(MaximumEditorLib SHARED)` + `--exclude-all-symbols`，只构建 `Editor` target。**要验证的是：编辑器 152 个 TU 在 SHARED 下是否存在"需要 `dllimport` 才能链接"的符号**（MinGW 上通常不需要；MSVC 上必然需要 → 这就是为什么 S02 要用 `EDITOR_API` 而不是裸 SHARED）。观察：链接错误数量与形态。
- **V1 · `GImGui` 唯一性可断言**：把 imgui 源同时放进两个 target（**故意制造双份**），确认 `audit_module_boundaries.py` 的断言能**抓到**这件事。观测手段本身必须先被证明有效，否则 O4 是空话。
- **V2 · imgui 移出后引擎是否真的不依赖**：在引擎 target 上**不**编译 imgui，看引擎 DLL 是否链接成功。预期成功（§4.4 结论）；若失败，失败点就是 §4.4 漏掉的真实依赖，必须先修正结论再切片。
- **V3 · 静态初始化顺序**：在编辑器 DLL 里加一个临时 `static` 对象打印 `GetAllClasses().size()`，对比 exe-first 与 DLL 两种情况的值。用于量化 F2 的真实风险。

---

## 9. 冲突与排序（跨 Feature）

| 冲突 | 事实 | 建议 |
|---|---|---|
| **BUILD-F03 第 2 刀「imgui 契约化」vs 本设计 S04** | 两者都处理 imgui 的 253 条导出，机制互斥。F03 侧证据：刀 2 机制 `BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md:223`；刀 4 的表把 **imgui 253 条明确划入「引擎 DLL 必须保住」**（`:236`）；失效模式 F4 要求「保证 imgui 只在一处编译（**引擎 DLL**）」（`:293`）；Q3 推荐 ①「留在 DLL」（`:341`）。但 F03 同时把「编辑器自己的边界」交给 F04（`:227` 刀4 接口说明），其 `:279` 也已承认「若选 imgui 独立 DLL 则直接降到 1300」——与本设计 §4.1 的 **1292** 吻合 | **先裁 §12 Q2**。若选 ①（本设计 S04），则 F03 的**刀 2 整刀应删除**、`:236`/`:293`/`:341` 三处表述需同步修订，刀 1/刀 3 不受影响。**不要两边都做**（会先补 `IMGUI_API` 再删，白做一遍） |
| **BUILD-F02 Q3「imgui 留在 DLL 内」vs 本设计 §4.3** | F02 Q3 的前提是"不动架构"；F04 改变了架构 | F02 Q3 标注为"被 F04 取代"，`IMGUI_API` 保持空定义 |
| **BUILD-F02 档 1 已落地 vs 本设计 S02 的导出状态机** | `MINENGINE_EXPORT_MODE` 是单值 cache 变量，作用于调用它的 target | 编辑器 DLL 复用同一函数，但需要独立白名单文件（`cmake/MaximumEditor_exports.def`）与独立预算；`MinEngineExports.cmake` 需加参数（S02 的一处扩参） |
| **外仓 `minCraft*`（只读，禁止修改）** | 三个仓库的 `CMakeLists.txt` 都用 `minengine_add_editor_executable`（`:169-172` / `:166-169` / `:181-184`），S02 之后它们会**自动**改用编辑器 DLL 的新形态 | 本设计**不修改外仓**；S02 必须保证 `minengine_add_editor_executable` 的**调用签名不变**（只改内部实现）→ 外仓零改动即可受益。这是 S02 的硬验收项 |

---

## 10. Alternatives

| 方案 | 机制 | 编辑器 DLL | imgui 归属 | 引擎导出 | 编辑器 relink 成本 | 改动面 | 人日 | 结论 |
|---|---|---|---|---|---|---|---|---|
| **A′ 最小方案：继续单 DLL + 只做完导出契约** | 编辑器保持 STATIC；imgui 留在引擎 DLL，按 F02 Q3 补 `IMGUI_API`；`.def` 燃烧到 explicit | ❌ 无 | 引擎 DLL（253 条保留，只是换机制） | 1545 → ~1292（**仅引擎自有符号变显式，imgui 253 仍在表内**） | 每项目全量重编/复制 ~188 MB | 2 文件（`imconfig.h` + `.def`） | ~1 | **不推荐单独做**：拿不到 DLL 边界收益，MSVC 隐患保留 |
| **B 本设计：编辑器 DLL + imgui 迁出（推荐）** | S01–S06 | ✅ SHARED | **编辑器 DLL**（引擎导出 −253） | 1545 → **1292** | 一次性 ~188 MB，之后每项目 exe 只有几 MB | 5 文件（+1 新增 `EditorAPI.h`） | **5–8** | **推荐** |
| C 参考 UE 全模块化（core/render/editor/… 多 DLL） | 把引擎也拆成多 DLL | ✅ | 编辑器 DLL | 总量大幅下降 | 同上且更快（增量） | 很大（等于重建构建图） | 30+ | **Deferred**（F02 Alternatives-E 已同判） |
| D 参考 Unity：编辑器运行时加载游戏的托管/脚本层 | 项目逻辑改脚本，编辑器不链接 C++ | ✅ 通用编辑器成立 | 引擎 DLL | 不变 | 无需 relink | 极大（等于换脚本架构） | 不可估 | **出局**（违背本仓 C++ 组件模型） |

### A′ 与本设计 B 的代价对比（给主人决策用）

| 维度 | A′（最小） | B（本设计） |
|---|---|---|
| 解决主人原始疑问（编辑器 DLL + 每项目编辑器） | ❌ 只解决"导出契约" | ✅ 结构上成立 |
| 磁盘：N 个项目编辑器合计 | N × ~188 MB | 188 MB + N × 几 MB |
| 全量构建耗时 | 每项目独立重编编辑器 | 编辑器 DLL 编一次 |
| 引擎 DLL 里"零消费者的 253 条 imgui 导出" | 保留 | 消除 |
| MSVC 路径 imgui 缺失风险（F02 D8） | 保留 | 消除 |
| 新引入的风险 | 无 | F1/F2/F4/F5/F7/F8（六条，全部有观测手段） |
| 可回退性 | 一行 | 每片独立可回退（S02/S04 各自两个文件） |

**推荐 B，但分两批批准**：S01–S03（拿到 DLL 边界，风险最低、收益最大）→ 停下来复核 → 再批 S04（imgui 迁移，唯一会动引擎侧的一片）。

---

## 11. Deferred

1. **编辑器插件系统 / 游戏模块向编辑器注册面板的 ABI**。今天游戏模块对 imgui 的依赖 = 0（实测），因此**不需要**任何 imgui 跨 DLL 契约；将来若要让项目写自定义编辑器面板，那才是 `EDITOR_API` 的扩张时机（届时 imgui 必须随 `EDITOR_API` 一起导出，或提供受控的包装层）。
2. **引擎全模块化**（core / render / editor / asset 多 DLL）——同 BUILD-F02 Alternatives-E / 本文 C。
3. **编辑器热重载**（改 DLL 热替换、状态迁移）。
4. **FreeType 转第三方 DLL**（§4.5 选项③）。
5. **`-fvisibility=hidden` + `-fvisibility-inlines-hidden`**（非 Windows 可见性；注意 `EngineAPI.h:14` 的 GCC 分支 `visibility("default")` 今天因未设 hidden 而是**装饰性**的）。
6. **MSVC 路径实测**（F02 D8）。
7. **编辑器产物的稳定 ABI / 版本化导出名**。
8. **`Maximum.exe`（Playground，100 MB，陈旧）处置**。

---

## 12. 待主人审批的决策清单

| # | 决策 | 选项 | 推荐 | 一行理由 | 什么会推翻它 |
|---|---|---|---|---|---|
| **Q1** | **`MaximumEditor.exe` 是否重新定位为"资产-only 项目编辑器"** | ①**重新定位**：显式登记为资产编辑器（样例/工具），项目编辑器（`<Project>Editor.exe`）才是正式产品形态；②保留"通用编辑器"说法，仅在文档里注明限制；③删除 `MaximumEditor.exe`，只保留每项目编辑器 | **①** | 本仓的组件扩展是链接期 C++（P5），通用编辑器对带 C++ 模块的项目**原理上不可用**；继续叫"通用"会持续制造"我的组件丢了"的静默失败（P4）。改名是零成本的诚实 | 若决定将来引入编辑器插件 ABI（Deferred-1）并让通用编辑器通过 C ABI 加载项目模块 —— 则通用编辑器的语义才真正成立，②才有意义 |
| **Q2** | **imgui 归属：迁进编辑器 DLL，还是留在引擎 DLL 补 `IMGUI_API`**（同时裁定 BUILD-F03 第 2 刀去留） | ①**迁进编辑器 DLL**（本设计 S04，引擎导出 −253）；②留在引擎 DLL + `IMGUI_API`（F02 Q3 / F03 第 2 刀原计划） | **①** | 引擎侧对 imgui 的依赖实测为 **0 调用 / 0 `GImGui` / 0 `imgui_internal.h`**，253 条导出只服务 3 个编辑器 exe，游戏模块依赖恒 0 —— 这是"删除零消费契约"，不是"搬架构" | 若**将来**游戏模块需要写自定义编辑器面板（Deferred-1），则 imgui 必须能被第三方消费；届时要么把 imgui 留在引擎 DLL（②），要么为编辑器 DLL 设计受控的 imgui 导出面。**先做 Q2=①，将来再补导出面，成本远低于反向** |
| **Q3** | **编辑器 DLL 入口符号形态** | ①保留 `minEngine::CreateApplication`，在 `main.h` 加 `MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL` 分支把声明切到 `EDITOR_API`（约 4 行）；②新增独立 C 符号 `MinEngine_CreateEditorApplication()`，每个宿主 main 自己调；③不做 `dllimport`，靠 MinGW auto-import | **①** | 复用已有的 `main.h` CLI 装配（25 行），`EditorMain.cpp` 仍只有 2 行、外仓 main 零改动；②会把 CLI 装配复制到每个宿主；③在 MSVC 上必崩 | 若外仓宿主的 main 已改写为不走 `main.h`（需核对 minCraft 的 EditorMain）→ 改用② |
| **Q4** | **缺匹配游戏模块时是否硬报错** | ①**硬报错**：日志 + 非零退出码 + 恢复指引；②仅警告后继续；③保持今天静默 | **①** | 静默降级的代价是"用户以为工具坏了"（P4 最坏失败类型）；非零退出码让自动化与 CI 也能发现 | 若"用通用编辑器快速看一眼资产"是高频且合理的用法 → 改为②（警告 + 明显的 UI 横幅），但仍不能③ |
| **Q5** | **target 名：`MaximumEditorLib` 保留还是改名** | ①保留 target 名，只改类型 + `OUTPUT_NAME MaximumEditor`；②重命名为 `MaximumEditor` 并让 helper 适配 | **①** | `MinEngineTargets.cmake:95,119` 与三个外仓都按这个名字写；改名是纯 churn，且外仓禁止修改 | 若借机统一 `<MODULE>Lib` → `<MODULE>` 命名族（另一轮） |
| **Q6** | **批准范围：一次全做，还是 S01–S03 先做** | ①**S01–S03 先批** → 复核 → 再批 S04 → S05–S06；②S01–S06 一次批准；③只做 S01（只观测） | **①** | S02/S03 的收益（N×188 MB → 1×188 MB）不依赖 imgui 迁移；S04 是唯一动引擎侧的一片，单独设一道门能把回滚面缩到最小 | 若希望一次收掉 imgui 的 253 条导出（例如紧接着要交付导出面预算）→ ②，但要把 §8 的 V0–V3 实验前置 |
| **Q7** | **FreeType 重复（两份实例）是否接受** | ①**接受**（编辑器另链一份，§4.5 选项①）；②把 FreeType 转第三方 DLL（选项③）；③让引擎导出 FreeType（选项②） | **①** | FreeType 无跨实例全局契约（每个 `FT_Library` 独立），重复 1–2 MB 代码换零导出面污染；③会重新污染刚清干净的导出面 | 若实测两份 FreeType 造成可测的内存/性能问题 → ②（参照 assimp 预编译模式）；③永不推荐 |

---

## 附录 A · 可复现测量命令

全部为**只读**测量（不构建、不运行 DLL/EXE；仅 PE 解析与 grep）。本轮实测环境：MinGW `D:/Dev/mingw64/bin/g++.exe`，`CMAKE_BUILD_TYPE=Debug`，`MINENGINE_EXPORT_MODE=whitelist`。

**A1 · 引擎导出表与组成**
```text
python scripts/audit_dll_exports.py --dll minEngine/bin/libminEngined.dll --report
→ NumberOfFunctions 1545 / NumberOfNames 1545 / maxNamedOrdinal 1545 / headroom 63990
  export dir bytes 113712
→ composition: minEngine 1278, imgui 253, glad 5, glfw 8, other 1
→ consumers(6): MaximumEditor 579 | SampleEditor 579 | SampleGame 7 |
                libGameModuleStubd 26 | libminCraftd 153 | minEngineTests 807
→ consumed union 1259, imported-but-not-exported 0
```

**A2 · 逐消费者第三方构成（本轮新测，用于 P3 表）**
用同一脚本的 `ExportTable`/`ImportTable`/`classify` 做互斥归类：`MaximumEditor.exe` 与 `SampleEditor.exe` 各 579（imgui 253 + 引擎 326）；`minEngineTests.exe` 807（glad 5 + glfw 8 + 引擎 794，imgui **0**）；`SampleGame.exe` 7；`libGameModuleStubd.dll` 26 —— 后三者 imgui 均为 **0**。

**A3 · 外仓消费者（旁证，只读）**
`D:\Dev\GitRepo\minCraft\bin\MinCraftEditor.exe` → `libminengine.dll` **573**（imgui **253** + 引擎 320）；`libminCraft.dll`（游戏模块）→ 引擎 **130**，imgui **0**。体积 184,905,322 / 93,789,075 / 505,240,851 字节。

**A4 · 引擎侧 imgui 依赖（grep 结论的可复现命令）**
```text
grep -rl -E "^\s*#\s*include\s*[<\"](imgui)" minEngine/minEngine/src   → 1 个文件（Render/RenderSystem.cpp）
grep -ro "ImGui::[A-Za-z_]*(" minEngine/minEngine/src | wc -l             → 0
grep -rn "\bGImGui\b"  minEngine/minEngine/src | wc -l                    → 0
grep -rl "imgui_internal.h" minEngine/minEngine/src                       → （空）
grep -rnE "\bImGui[A-Za-z_]*|\bImGuizmo\b" minEngine/minEngine/src | wc -l → 8（全部为注释）
```

**A5 · 编辑器源码规模与耦合面**
```text
find minEngine/Editor/src -name "*.cpp" | wc -l              → 152
find minEngine/Editor/src -name "*.h"   | wc -l              → 185
grep -ro "ImGui::[A-Za-z_]*(" minEngine/Editor/src | wc -l   → 1359
grep -rnE "(^|[^A-Za-z_])gl[A-Z][A-Za-z_]*\(" minEngine/Editor/src | wc -l → 0
grep -rn -i "glfw" minEngine/Editor/src  → 2 处接触（Editor.cpp:336 句柄；Platform/EditorImGuiBackend.cpp:41 形参）
grep -rn -i "gamemodule" minEngine/Editor/src  → 0
grep -rn "LoadLibrary|GetProcAddress|dlopen" minEngine/{Editor,minEngine}/src → 0
  （唯一邻近命中 GLFWWindowSystem.cpp:177 的 gladLoadGLLoader）
```

**A6 · 白名单与产物**
```text
minEngine/cmake/minEngine_exports.def → 1259 条目（minEngine 992 / imgui 253 / glfw 8 / glad 5 / other 1）；124 条标 DATA
minEngine/bin: MaximumEditor.exe 188,033,796 | SampleEditor.exe 188,034,182 | libminEngined.dll 531,407,389
```

**A7 · 行业对照的本地源码锚点**
```text
UE   D:/Dev/GitRepo/UnrealEngine
  Engine/Source/Programs/UnrealBuildTool/Configuration/TargetRules.cs:20-40  （TargetType；:30 Editor 的文档注释）
  Templates/TP_Blank/Source/TP_BlankEditor.Target.cs:10,13                   （Type = TargetType.Editor；ExtraModuleNames.Add("TP_Blank")）
  Engine/Source/Editor/UnrealEd/UnrealEd.Build.cs:9-12                       （!bCompileAgainstEditor → throw）

Godot D:/Dev/GitRepo/godot
  main/main.cpp:209,1537-1539  （static bool editor；-e/--editor）
  SConstruct:541               （TOOLS_ENABLED）
  editor/SCsub:11              （if env.editor_build）
  editor/editor_node.cpp:1004  （GDExtensionManager 运行时加载进编辑器）

Unity 未实测（本仓无 Unity 源码）：通用编辑器 + 托管程序集，属公开文档事实
```

**A8 · 本轮实测与既有文档数字的差异（须知）**

| 量 | BUILD-F02 文档（S01 前） | 本轮实测（S02 后，`whitelist`） |
|---|---|---|
| 引擎导出条目 | 65533 | **1545** |
| 序号余量 | 2 | **63990** |
| `MaximumEditor.exe` 导入 | 578 | **579** |
| `SampleEditor.exe` 导入 | 564 | **579**（与 MaximumEditor 相同——两者链同一份 STATIC 库，差异仅为游戏模块） |
| `libGameModuleStubd.dll` | 20 | **26** |
| `SampleGame.exe` | 9 | **7** |
| `minEngineTests.exe` | 802 | **807** |
| in-repo 消费者并集 | 1216 | **1259** |

**含义：** `SampleEditor.exe` 与 `MaximumEditor.exe` 的引擎导入面**完全一致**（579 = 579），是"同一份编辑器代码编译两遍"的直接证据（P1），也是 S02 之后两者会共享同一份编辑器 DLL 的直接理由。

