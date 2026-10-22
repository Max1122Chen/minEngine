# BUILD-F02 — 显式导出契约 · 让 `MINENGINE_API` 真正生效 · 退役 `-Wl,--export-all-symbols`

## Meta
- **ID:** `BUILD-F02`
- **Type:** Refactor（构建 / ABI 契约）
- **Complexity:** `L2`（切片化后可分轮实施）
- **Status:** `Done` — **档 1（S00–S02）+ S05/S06** 由 [BUILD-F03 刀2](BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md) 落地（默认 `explicit`）
- **Owner:** project maintainer
- **Last updated:** `2026-10-22`（进度注记；问题数字仍为 2026-10-18 首测，见 Problem）
- **Related:**
  - 前序：[BUILD-F01](BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)
  - 续作 / 刀2：[BUILD-F03](BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md) · 编辑器边界 [BUILD-F04](BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md)
  - 模块链接：[ED-F21](../../Editor/ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_DESIGN.md)、[ED-F20](../../Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)
  - 反射 codegen：[CORE-F27](../Core/CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md)
  - UE：`ENGINE_API` / `<MODULE>_API`；从不自动导出全部符号
- **Depends on:** 无硬依赖
- **Blocks（历史）：** P1 序号炸弹 — **S01 已解除**；后续阻塞项见 F03 刀2

## 进度（2026-10-22）

| 片 | 状态 | 说明 |
|----|------|------|
| S00 审计脚本 | **Done** | `scripts/audit_dll_exports.py` |
| S01 whitelist 默认 | **Done** | `.def` 由消费者 import 生成；余量从 2 拉到六万级 |
| S02 宏转正 | **Done** | `MINENGINE_API` = dllexport/dllimport；测试不再误定义 `MINENGINE_BUILD` |
| S03 第三方 | **Superseded** | glfw/glad → F03 刀1；imgui → F04 S04（均已落地） |
| S04 分批标注 | **Done**（经 F03 刀2 批0–5） | 见 F03 附录 C |
| S05 切 `explicit` | **Done**（2026-10-22） | 默认 `MINENGINE_EXPORT_MODE=explicit` |
| S06 收尾 | **Done**（2026-10-22） | whitelist/explicit 仅 `--exclude-all-symbols`；`--exclude-libs` / `--exclude-modules-for-implib` 只留在 `auto` |

**回退注意：** `auto` 在 S02 后余量可到 0，**不可**当长期退路；排障用 `whitelist`。

## TL;DR

**一句话结论：** 走**显式 `MINENGINE_API`（`__declspec(dllexport)`）+ 三段式状态机 `auto → whitelist(.def) → explicit`**；**S01 只做一件事——把 `.def` 白名单从"消费者的 PE import 表"生成，而不是从 DLL dump**，一步解除序号炸弹。此后把白名单名字换成头文件标注并删 `.def` 行；行数归零后切 `explicit`（`--exclude-all-symbols`，只认标注）。

**终态已落地（2026-10-22）：** S00–S02 + S04–S06；默认 `explicit`。刀2 范围与批序见 **[BUILD-F03](BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md)**（本文保留状态机 / Failure / 反射规则原文）。

**必须先纠正一个前提（仍成立）：** 「先 dump 全表 `.def` 就能解炸弹」**不成立**——必须用消费者契约降条目数。

## Problem

### P1 · 每个符号都进导出表，已贴在 PE 上限上（实测）

- 引擎是 SHARED 库：`minEngine/minEngine/CMakeLists.txt:34`（`add_library(${ENGINE_NAME} SHARED ...)`）。
- 链接期靠自动导出：`minEngine/minEngine/CMakeLists.txt:150-157`，关键行 `:152` 的 `LINKER:--export-all-symbols`。
- 直接解析 `minEngine/bin/libminEngined.dll` 的 PE 导出目录（脚本见附录 A）**实测**：

| 量 | 实测值 |
|---|---|
| `OrdinalBase` | 1 |
| `NumberOfFunctions` | **65533** |
| `NumberOfNames` | 65533（全部按名字导出，无"仅序号"导出） |
| 最大具名序号 | **65533** |
| PE 硬上限 | **65535**（序号字段 16 位） |
| **剩余余量** | **2** |
| 导出目录字节数 | 7,473,708 |
| DLL 体积（Debug） | 483.7 MB |

早前一轮构建报过 `export ordinal too large: 65537` 并**删除了 `bin/libminEngined.dll`** —— 也就是说再新增 3 个导出符号，构建就会失败并销毁产物。这不是理论风险，而是已经发生过的静默破坏（失败模式是"产物消失"，最难定位）。

### P2 · 导出面里 98% 不是公开契约（实测构成）

对 65533 个导出名做 `c++filt` 还原后归因：

| 来源 | 条数 | 占比 |
|---|---|---|
| std / libstdc++（模板实例化、STL ABI） | 42,456 | 64.8% |
| fmt / spdlog / nlohmann / sol2 | 2,856 | 4.4% |
| imgui / ImGuizmo | 2,205 | 3.4% |
| glad（GL loader） | 699 | 1.1% |
| **minEngine 自有**（顶格 `_ZN9minEngine*`/`_ZNK9minEngine*` 计 7,729） | 5,792 | 8.8–11.8% |
| Jolt Physics | 360 | 0.5% |
| stb_image | 43 | 0.1% |
| 其它 / 未归类（含 vtable、typeinfo、Windows SDK 泄漏） | 11,121 | 17.0% |

结构分类：Itanium mangled 60,714（92.6%）、纯 C 名 4,819；其中 vtable 613、typeinfo 880、typeinfo name 880、函数内静态 `_ZZ` 1,770、guard variable `_ZGV` 347、thunk 38。

**最刺眼的一类证据：** 465 个导出是 Windows SDK 的 UI-Automation GUID 常量（`ALL_POWERSCHEMES_GUID`、`ARRAYID_PathProperties`、`AcceleratorKey_Property_GUID` …），它们只是某个引擎 TU 间接 include 了 Windows 头。自动导出把"这个 TU 里出现过的一切全局符号"都当成了契约。

### P3 · 真正被消费的只有 1216 个（含外仓 1250）——实测

解析全部消费者的 PE 导入目录：

| 消费者 | 从 `libminEngined.dll` 导入的符号数 |
|---|---|
| `minEngineTests.exe` | 802 |
| `MaximumEditor.exe`（= `MaximumEditorLib` STATIC，经 `Editor/CMakeLists.txt:30` 链 DLL） | 578 |
| `SampleEditor.exe` | 564 |
| `libGameModuleStubd.dll`（外部 Game 模块，`minEngine/CMakeLists.txt:75-77`） | 20 |
| `SampleGame.exe`（Game 宿主，`minEngine/CMakeLists.txt:83-85`） | 9 |
| **去重并集（in-repo 5 个消费者）** | **1216（占导出 1.9%）** |
| 外仓产品 `libminCraftd.dll`（旁证，正常消费者） | 153，其中 34 个不在 in-repo 并集内 → **含外仓合计 1250** |
| `Maximum.exe`（**陈旧产物**，已排除） | 519，其中 34 个在当前 DLL 中已不存在（引擎已删 `AssetManager::CreateAsset<T>`，见 `Runtime/Resource/AssetManager.h:250`） |

1250（含外仓）的构成（互斥归类）：**minEngine 自有 982（78.6%）**、imgui 家族 252、glad + glfw 13、其它 2（`GImGui`、`TAG_Channel_Default`）——**没有一条是「纯 std 符号」**，所有 std 名字都出现在引擎自有函数的签名里。其中 vtable 28、纯 C 名 15；覆盖 **182 个不同的引擎类型**；**反射 codegen 产出的 88 个（`StaticClass()` 52 + `GetClassName<T>()` 36 = 7.0%）**。

反向看：引擎命名空间下导出了 8,567 个符号，只有 982 个被消费 → **7,584 个（88.5%）是零消费的泄漏**。

### P4 · 宏已存在但没有 Windows 语义；`MINENGINE_BUILD` 被误用

- `Runtime/Core/EngineAPI.h:1-15`：Windows 分支两个 `__declspec` 都被注释掉（`:6`、`:9`），`#define MINENGINE_API` 展开为空（`:5`、`:8`）；只有 GCC/Clang 分支是 `visibility("default")`（`:12`）。
- 宏已在用：**94 处、33 个头文件**（Profiling 33、Reflection 系列、Serialization 系列、Noise、Log…）；只有 **28 个头文件** include 了 `EngineAPI.h`。
- 构建期宏已存在：`minEngine/minEngine/CMakeLists.txt:159` 的 `target_compile_definitions(${ENGINE_NAME} PRIVATE MINENGINE_BUILD IMGUI_ENABLE_FREETYPE)`。
- **缺陷：** `Tests/CMakeLists.txt:89` 也给 `minEngineTests` 定义了 `MINENGINE_BUILD`。宏一旦转正，测试二进制就会以 `dllexport` 视角编译"定义在 DLL 里"的引擎符号 → 链接期 "cannot export undefined symbol"。必须随 S02 一并修正。

### P5 · 第三方实现也在这条船上

导出面里 6,000+ 条来自第三方实现（imgui / fmt / spdlog / glad / Jolt / stb / glfw / eFsw / assimp…），它们现在同样靠自动导出跨过 DLL 边界：imgui 252、glad + glfw 13 被消费者真实 import。`IMGUI_API` 在 `Third-Party/imgui/imgui.h:87-88` 可被外部覆盖（`imconfig.h:27-29` 已留注释位），glad / glfw 没有等价的对外宏。所以"显式导出"不是引擎一家的事：必须同时给第三方定契约（How-3）。

## Scope

1. `MINENGINE_API` 在 Windows 上具备真实 `dllexport`/`dllimport` 语义；非 Windows 保持 `visibility("default")`。
2. 建立 `auto → whitelist → explicit` 三段式导出状态机（含 CMake 开关），每一步可回退。
3. 用"消费者 import 表"生成白名单 `.def`，**在第一个实施切片就解除 65535 序号炸弹**。
4. 逐模块把公开契约改成头文件标注，白名单燃烧归零，最终 `--exclude-all-symbols` fail-closed 终态。
5. 第三方（imgui / glad / glfw / stb / Jolt / lua）的显式导出契约。
6. 反射 codegen 产出符号（`StaticClass()` / `GetClassName<T>()` / 注册变量）的导出规则与"按归属模块注入"机制。
7. 可自动断言的验证 oracle（导出数、序号余量、消费覆盖、三类 target 链接）。

## Non-goals

- **不改总体架构**：引擎仍是 SHARED DLL；Editor / Tests / Game 仍是消费者；不静态化、不拆多 DLL。
- 不承诺稳定 ABI，不做版本化导出名。
- 不做跨平台符号可见性专项（`-fvisibility=hidden`、Linux/macOS .so 导出集）。
- 不引入 `find_package(minEngine)` / install export set。
- 不动反射 codegen 的语义（只动它输出的导出修饰）。
- 不修 `Maximum.exe` 陈旧产物；不重建外仓 `minCraft`。

## How

### How-1 · 所有权：宏由谁定义、在哪个视角展开

| 视角 | 目标 | `MINENGINE_BUILD` | Windows 展开 | GCC/Clang 非 Windows 展开 |
|---|---|---|---|---|
| 构建引擎本体 | `minEngine`（SHARED） | **定义**（`PRIVATE`，`minEngine/CMakeLists.txt:159` 已有） | `__declspec(dllexport)` | `__attribute__((visibility("default")))` |
| 消费引擎 | `MaximumEditorLib` / `Editor` / `minEngineTests` / `GameModuleStub` / `SampleGame` / `SampleEditor` / 外仓产品 | **不定义**（须删除 `Tests/CMakeLists.txt:89`） | `__declspec(dllimport)` | 同上（终态下应无实际作用） |
| 引擎侧 STATIC 辅助 | `minengine_lua` / `efsw-static` / `freetype` / `Jolt` | 不定义 | 不使用该宏 | — |

写进 CMake 注释的不变式：**`MINENGINE_BUILD` 只允许出现在"产出 `libminEngined.dll` 的那个 target"上**。其它 target 定义它都是 bug（当前已有 1 处）。

非 Windows 分支保持 `visibility("default")` 即可用；**不要求**同时开 `-fvisibility=hidden`（那是 ELF 语义，且一旦开启就要为"未标注但需导出"的符号全量补标注，属另一个 Feature 的工作量）。本 Feature 只保证宏在两个平台上"语义正确且可继续演进"。

### How-2 · 三段式状态机（CMake 开关）

`minEngine/CMakeLists.txt` 新增缓存变量：

> `set(MINENGINE_EXPORT_MODE "whitelist" CACHE STRING "auto | whitelist | explicit")`

三种模式对应的链接行为：

| 模式 | `--export-all-symbols` | `--exclude-all-symbols` | `.def` 白名单 | 是否用 `dllexport` | 导出数（预计） |
|---|---|---|---|---|---|
| `auto`（现状） | 是 | 否 | 否 | 冗余无害 | 65533 |
| `whitelist`（S01–Sn） | 否 | **是** | 是（1250 行，逐片删） | 增量并存 | 1250 → ~1000 |
| `explicit`（终态） | 否 | **是** | 否 | 唯一来源 | ~1000（预算 ≤ 3000） |

为什么用 `--exclude-all-symbols` 而不是"只删掉 `--export-all-symbols` "：ld 在"没有任何显式导出"时会**回落到自动导出全部**（PE 链接的默认行为）。`ld --help` 实测存在 `--exclude-all-symbols  Exclude all symbols from automatic export` —— 这是 fail-closed 开关，只有用它才能保证"将来某人漏标注"表现为**链接错误**，而不是"又悄悄导出 6.5 万个"。（该行为在 S05 前用 5 分钟实验确认，见 V0。）

`.def` 与 `dllexport` **可以并存**，ld 取并集——这是"逐步替换"的物理基础；两机制叠加产生的重复条目不影响正确性（`--warn-duplicate-exports` 默认关闭）。S02 前用最小实验验证（V1）。

### How-3 · 第三方库的导出契约

| 库 | 现状 | 目标机制 | 导出变化 |
|---|---|---|---|
| imgui | 2,205 导出，Editor 真实 import 252 | 覆盖 `IMGUI_API`：引擎 target `PRIVATE IMGUI_API=__declspec(dllexport)`，`INTERFACE IMGUI_API=__declspec(dllimport)`；无需改 `imconfig.h`（`imgui.h:87-88` 支持外部定义） | 2205 → ~250 |
| glad | 699 导出，消费 5（`GLAD_GL_VERSION_4_6`、`gladLoadGLLoader` + 3 个 `glad_gl*`） | 该 vendored 副本**无** `GLAD_API_CALL` 钩子（实测 grep 为空）→ 用**微型 `.def`（≤8 行、稳定）**，或给 vendored `glad.h` 加 `GLAD_API_CALL` 并记入 `VENDOR.md` | 699 → ≤8 |
| glfw | 引擎链的是静态 `libglfw3.a`，Editor 真实 import 7 个 `glfw*` | 见决策 Q4：改用 `libglfw3dll.a`（`Third-Party/glfw/lib-mingw-w64/glfw3.dll` 已存在），或保留微型 `.def` | 归零或 ≤12 |
| stb_image / Jolt / lua / miniaudio | 43 / 360 / 0 / 0 被消费 | 直接不导出（修正后靠标注/白名单天然排除，`--exclude-libs` 变冗余） | → 0 |
| fmt / spdlog / nlohmann / sol2 | 2,856，全部 header-only 实例化，消费 0 | 不导出；消费者自行实例化（ODR 正确，见 F5） | → 0 |

### How-4 · 反射 codegen 的导出契约（最易漏的一类）

`ME_CLASS` / `ME_STRUCT` 生成的符号分三类（`Runtime/Core/Reflection/ReflectionMacros.h`）：

| 类别 | 生成处 | 具体符号 | 是否导出 | 依据 |
|---|---|---|---|---|
| **A · 类成员 out-of-line 定义** | `ME_REFLECTION_CLASS_DEFINE_END`（`:138-146`）→ `src/Generated/Reflection/*.gen.cpp`（88 个文件） | `T::StaticClass()` | **引擎自有类型必须导出** | 实测 52 个被消费者 import |
| **B · 模板特化自由函数** | `ME_REFLECTION_CLASS_DEFINE_BEGIN`（`:65-70`）；枚举侧 `GetEnumName`（`:149-166`） | `minEngine::Reflection::GetClassName<T>()` / `GetEnumName<E>()` | **引擎自有类型必须导出** | 实测 36 个被 import |
| **C · 注册副作用变量** | `ME_REFLECTION_CLASS_DEFINE_BEGIN`（`:71`） | `const bool GReflectionClassRegister_*` | **禁止导出**（TU 内部副作用） | 实测消费者 import = 0；导出只浪费序号 |

为什么这是难点：A/B 的**声明在头文件**（`ReflectionAnnotations.h:22` 的 `static const MEClass* StaticClass();`），**定义在 `.gen.cpp`**。自动导出掩盖了这个割裂：谁编译 `.gen.cpp` 谁就持有定义，消费者只要能解析名字就能链上。改成显式后必须回答"**这份定义属于哪个模块，就用哪个模块的导出宏**"：

- **引擎自有类型**（`minEngine::` 命名空间）：`.gen.cpp` 编进引擎 DLL → 定义处需要 `MINENGINE_API`。
- **Editor 自有类型**：`.gen.cpp` 由 `MaximumEditorLib` 编译 → 需要 Editor 自己的宏（Editor 无 DLL 时可退化为空）。
- **Game 模块自有类型**：`.gen.cpp` 编进模块 DLL → 需要模块自己的宏（`Examples/GameModuleStub/GameModuleStub.cpp:12-18` 已是这个模式的现成模板）。

**推荐机制：codegen 按模块注入导出宏。** 给 `minengine_add_reflection_codegen(<target>)`（`cmake/MinEngineReflection.cmake`）增加 `MODULE_API` 参数，写进生成文件头部，并让 `ME_REFLECTION_CLASS_DEFINE_BEGIN` / `_END` 把 `ME_REFLECTION_MODULE_API` 加到 `GetClassName<T>` 特化与 `T::StaticClass()` 的定义前（默认值为 `MINENGINE_API`）。这样三类符号的处置从"人工判断"变成"规则"。

同时**必须**保证类上的 `MINENGINE_API` 与生成函数的标注**一致**：声明为 `dllimport` 而定义为 `dllexport`（或反之）在 GCC/MSVC 上都是错误。规则：*标注跟随归属模块，声明与定义同宏。*

## 导出契约（可判定规则）

判定流程（新符号一律走这个流程，不靠"看着办"）：

`符号 S 在任一消费者 import 表里？
├─ 是 → 必须导出（R1）：标注在"拥有定义"的头文件上
└─ 否 → S 是否有 out-of-line 定义（.cpp / .gen.cpp）？
         ├─ 是 → 是否为公共头文件里的公开类型/函数？
         │        ├─ 是 → 导出（R2，面向未来的契约面，需人工公告）
         │        └─ 否 → 不导出（N2，内部实现）
         └─ 否（模板 / inline / header-only 实例化）
                  ├─ 跨 DLL 身份必需（vtable / typeinfo / 单例取用点）→ 导出（R4/R5）
                  └─ 其余 → 不导出（N1，消费者自行实例化）`

**必须导出（R）**

| 规则 | 内容 | 实测规模 |
|---|---|---|
| R1 | 任一消费者 import 表出现的符号（机器可测，唯一硬契约） | 1250（in-repo 1216） |
| R2 | 公共头文件中对外 API 的类/函数（`Engine`/`SceneManager`/`AssetManager`/`Serializer` …） | 182 类型 / 982 符号 |
| R3 | 引擎自有反射类型的 codegen A/B 类符号（How-4） | 88 |
| R4 | 消费者派生 / 捕获 / `dynamic_cast` 用到的基类 vtable 与 typeinfo | 28 个 vtable |
| R5 | 单例与全局取用点（`ReflectionSystem::Get()`、`LogApp`、`GenerateGUID`） | 含在 982 内 |
| R6 | `extern` 数据符号（`GImGui`、`GLAD_*`、glfw 函数指针）——数据符号必须 `dllimport` 或 `--enable-auto-import` | 15 个纯 C 名 |

**必须不导出（N）**

| 规则 | 内容 | 实测可裁 |
|---|---|---|
| N1 | std/fmt/spdlog/nlohmann/sol2 与自研模板实例化、inline 定义 | 45,312 |
| N2 | `static` / 匿名命名空间 / 内部 helper（`*_Impl`、`Detail::`） | 含在 7,584 泄漏内 |
| N3 | 反射注册副作用变量 `GReflectionClassRegister_*` | 88（每个 `.gen.cpp` 一条） |
| N4 | 第三方实现内部（imgui 非 `IMGUI_API` 面、glad 非消费项、Jolt、stb、assimp、eFsw） | ≥ 4,000 |
| N5 | 系统头泄漏（`*_GUID` 等） | 465 |
| N6 | 引擎内部自用、零消费的符号 | 7,584 |

## 切片与"始终可绿"论证

| 片 | 做什么 | 结束时构建为何仍绿 | 可回退 | 导出数 | 炸弹 |
|---|---|---|---|---|---|
| **S00** 观测与护栏（0.5d） | 新增 `scripts/audit_dll_exports.py`（PE 导出 + 消费者 import + 报告/断言/白名单生成）+ CMake target `export-audit` | 纯新增文件，不改任何 target 行为 | 删文件 | 65533 | 仍武装，但**已可见** |
| **S01** 状态机 + 白名单（0.5–1d） | `MINENGINE_EXPORT_MODE`；从消费者 import 表生成 `cmake/minEngine_exports.def`（1250 行 = in-repo 1216 ∪ 外仓 153）；`--exclude-all-symbols` + `.def`；默认切 `whitelist` | `.def` **由构造保证** ⊇ 全部消费者 import → 三类 target 的链上符号一个不缺（缺口只可能来自外仓消费者，且是 loud 链接错误） | `-DMINENGINE_EXPORT_MODE=auto` 一行 | **65533 → ~1250** | **✅ 解除** |
| **S02** 宏转正（0.25d） | 打开 `EngineAPI.h` 的 `dllexport`/`dllimport`；删除 `Tests/CMakeLists.txt:89` 的 `MINENGINE_BUILD`；补 5 处 include | `whitelist` 模式下导出面由 `.def` 决定，`dllexport` 只是冗余叠加（V1 先验证 ld 合并） | revert 1 文件 | ~1250 | 已解除 |
| **S03** 第三方契约（0.5–1d） | `IMGUI_API` / glad 微型 `.def` / glfw 决策（Q4）落地；从白名单删 imgui 252 + glad/glfw 13 行 | 第三方符号改由等价机制导出，导出面等价 | revert CMake + 白名单行 | ~1000 | 已解除 |
| **S04.x** 引擎契约分批（每批 0.5–1d） | 按模块标注：`Log` → `Serialization` → `Reflection`（含 codegen A/B）→ `Object/MEObject` → `Scene` → `Resource/AssetManager` → `Render` → `Input` → `Physics` → `Audio` → `UI` → `Scripting` → `Platform`；每批删对应白名单行 | `.def` 与标注**并存取并集**：删行前标注已存在 → 导出集合不变；少标注只会在消费者链接期 loud 失败 | 单片 revert（头文件 + 白名单行） | 缓降 | 已解除 |
| **S05** 切 `explicit`（0.5d，含收敛循环） | 删 `.def`，只留 `--exclude-all-symbols`；先在 `build-explicit` 独立目录验证，再改默认 | 白名单已燃烧至 0 → 导出集合与上一片等价；剩余 link error 逐个补标（每处 1 行） | 改回 `whitelist` | ~1000 | 已解除 |
| **S06** 收尾（0.5d） | 删冗余 `--exclude-libs`/`--exclude-modules-for-implib`；`verify.ps1` 接审计断言；文档/进度更新 | 只删对显式模式已无作用的选项 | 逐行 revert | 不变 | 结构成因消除 |

**"始终可绿"的三个支点**

1. **两机制并存**：`.def` 白名单与 `dllexport` 取并集，因此任何"半标注"状态都是合法可链接的中间态——这正是能切片的原因。
2. **切换点唯一且可证**：只有 S01（auto→whitelist）与 S05（whitelist→explicit）两次切换。S01 的安全性来自"白名单由消费者 import 表构造"（等价性可证明，不是估计）；S05 的安全性来自"白名单已空"（导出集合等价）。
3. **双构建目录验证**：切默认值前先在 `minEngine/build-explicit`（`-DMINENGINE_EXPORT_MODE=explicit`）跑通三类 target + 测试，主 `build/` 始终可用。

## 状态机与导出契约（不变量）

```
auto ──(S01)──▶ whitelist ──(S05)──▶ explicit
 │                  │                  │
 │ .def 未开发       │ .def 行数单调↓    │ 唯一真源 = MINENGINE_API
 │ 导出=全部符号      │ 导出=.def ∪ 标注   │ 导出=标注
 └── 回退兜底 ◀───────┴──────────────────┘  （MINENGINE_EXPORT_MODE 单项开关）
```

不变量（任何时刻都必须成立，S00 起由脚本断言）：
- **I1** 导出条目数 ≤ 3000。
- **I2** `65535 - maxNamedOrdinal ≥ 60000`。
- **I3** 导出集合 ⊇ 全部消费者 import 并集（O3）。
- **I4** `MINENGINE_BUILD` 只被「产出引擎 DLL 的 target」定义，其它 target 一律不得定义。

## Failure Model

| # | 失效模式 | 典型症状（可观测） | 观测点 | 修复/回滚 |
|---|---|---|---|---|
| F1 | 漏标注（函数/类方法） | 消费者**链接期** `undefined reference to '_ZN9minEngine...'` | `cmake --build` 日志 | 补 1 行标注；切回 `whitelist` 即恢复 |
| F2 | 数据符号缺 `dllimport`/auto-import | **启动即失败**：`The procedure entry point ... could not be located` 或 `0xC0000139`；构建期全绿（最危险） | 跑 `minEngineTests.exe test smoke` / 启动 `MaximumEditor.exe` | 给 `extern` 数据补 `MINENGINE_API`；必要时 `-Wl,--enable-auto-import` |
| F3 | RTTI / typeinfo 跨 DLL 不一致 | `catch` 不命中、`dynamic_cast` 返回 null、`typeid` 名字不同；只在跨边界路径复现 | 跨边界断言 `&typeid(EngineEx)` 取址一致（新增 1 条 smoke） | 导出基类 vtable/typeinfo（R4） |
| F4 | 单例跨 DLL 重复实例 | 两套 `LogSystem`、两个 imgui 上下文（`GImGui` 断言/崩溃） | Editor 启动路径 + 反射冒烟（`Tests/CMakeLists.txt:100-108` 已链 `GameModuleStub`） | 修正 R5 取用点为导出而非各自定义 |
| F5 | inline / 模板 ODR | 同一 inline 的静态局部变量地址不同 → 注册表"查不到"；难复现 | 跨 DLL 反射 smoke：模块注册类型在引擎侧 `FindClass` 命中 | 保证 N1 类型单一实例化源；必要时改 out-of-line |
| F6 | **`dllexport` 类导致导出膨胀**（本路线特有） | 含大量 STL 成员的类一旦类级标注，会连带导出内联成员、成员类型实例化、`vtable`/`typeinfo` → 导出数**反弹** | 每片跑 `audit_dll_exports.py`，超预算即拒绝该片 | 改点级导出（只标 out-of-line 函数） |
| F7 | 序号再次越界 | `export ordinal too large: 65537` + **产物被删除** | S00 起的构建末尾断言 | 断言在 3000 处先失败，不再撞墙 |
| F8 | 白名单过期（外仓消费者） | 外仓产品链接期缺符号（loud） | 升级外仓时 `audit_dll_exports.py --consumer <exe>` | 补白名单行，或直接补标注 |

**回滚总则**：全部状态落在三个文件 `EngineAPI.h` / `minEngine/CMakeLists.txt` / `cmake/minEngine_exports.def`；`MINENGINE_EXPORT_MODE` 是唯一总闸。

## Verification

### 可观察 oracle（全部可自动断言）

| # | Oracle | 断言 | 当前 | 目标态 |
|---|---|---|---|---|
| O1 | 导出条目数 | `NumberOfFunctions ≤ 3000` | 65533 | 通过（S01 后 ~1250） |
| O2 | 序号余量 | `65535 - maxNamedOrdinal ≥ 60000` 且**打印进构建日志** | 2 | 通过 |
| O3 | 消费覆盖 | `exported ⊇ consumed`（5 个 in-repo 消费者 + 外仓 minCraft 逐一核对；当前全部为 0 缺） | 通过 | 通过 |
| O4 | 三类 target 链接 | `minEngineTests` + `Editor` + `GameModuleStub`/`SampleGame`/`SampleEditor` 全部链接成功 | 通过 | 通过 |
| O5 | `verify.ps1` 全绿 | `test smoke` 退出码 0 | 通过 | 通过 |
| O6 | 泄漏白名单化 | `exported − consumed` 每条都能追到 R2/R3 的公告面（脚本出 diff 报告） | 无报告 | 报告为空或仅含已公告项 |

**"再也不会撞 65535"的证明方式**（三条同时成立）：

1. **结构性**：导出集合 = 显式标注集合，新增符号默认**不导出** → 代码增长不再推动导出数增长（对比现状：任何新 inline 函数都可能增加一条导出）。
2. **前置断言**：O1/O2 在构建末尾执行（S00 起），越界在 3000（上限的 4.6%）处失败，而不是在 99.97% 处删产物。
3. **增长率可审计**：`export-audit` 报告记录每次构建的导出数与余量，diff 可见。

### 最小自动化路径

```text
scripts/audit_dll_exports.py            # 一份脚本三用途
  ├─ --report                        → JSON：导出数 / 序号 / 分类 / headroom
  ├─ --max-exports 3000 --fail        → O1/O2 断言（verify.ps1 与 CI 用）
  └─ --emit-whitelist <out.def>       → 从消费者 import 表生成白名单（S01 / 刷新）

scripts/verify.ps1（追加 2 行）
  python scripts/audit_dll_exports.py --max-exports 3000 --fail

minEngine/CMakeLists.txt（追加）
  add_custom_target(export-audit COMMAND ${Python3_EXECUTABLE} .../audit_dll_exports.py --report ...)
```

### 实施前必须先跑的 3 个 5 分钟实验

- **V0**：确认 ld 在"无任何导出"时回落到自动导出，且 `--exclude-all-symbols` 能关掉它（决定 S05 写法）。
- **V1**：确认 `.def` 与 `dllexport` 并存不报错、取并集（决定 S02 是否安全）。
- **V2**：确认 `<file>.def` 作为链接输入在 CMake 中的正确写法（`LINKER:` / `SHELL:` 包裹，避免逗号被拆）。

## Alternatives

| 方案 | 机制 | 解炸弹时点 | 回退成本 | 维护成本 | 结论 |
|---|---|---|---|---|---|
| **A 显式宏（推荐）** | `MINENGINE_API` → `dllexport`；`.def` 白名单作过渡 | **S01（~1 人日，65533 → ~1250）** | 一行开关 | 每新增公开 API 补 1 行标注（本应做的文档化） | **推荐** |
| B `.def` 长期化 | 白名单由消费者 import 表生成、长期维护 | S01 同样可解 | 一行 | 高：mangled 名 churn、每次增 API 需"先 auto 构建再 dump 再生成"两趟流程、外仓消费者易过期 | 只作 A 的 S01–Sn 过渡件 |
| B′ `.def` 来自 DLL dump | 把现 65533 行导出写进 `.def` | **不能解炸弹**（条目数不变） | 一行 | 高 | 只作**审计产物**（`--output-def`），不作导出机制 |
| C `--exclude-symbols` 负名单 / `--retain-symbols-file` | 保留自动导出、反向排除 6 万条 | — | — | — | **不可行**：`ld --help` 实测 `--exclude-symbols sym,sym,...` 只吃逗号串（无文件、无正则），6 万条命令行不现实；`--retain-symbols-file` 属 ELF/archive 语义，对 PE 导出表无效 |
| D 全静态化 | 引擎改 STATIC，各 target 自行编译链接 | 立刻（无导出表 → 无序号） | 很大（等于重建架构） | 低 | 不推荐：链接时间/内存暴涨（现 DLL 已 483MB Debug）；**破坏 Game 模块插件模型**（`GameModuleStub` 作为 SHARED 必须能 import 引擎）；外仓产物形态改变 |
| E 拆多 DLL（core/render/editor） | 真正的架构解 | 可分阶段 | 很大 | 中 | Deferred：本 Feature 的宏机制**正是**拆库前置（`<MODULE>_API` 已按模块语义设计） |

**推荐 = A**；B 只作过渡、B′ 只作审计、C 排除、D/E 明确出局。

## 工作量档位

| 档位 | 内容 | 人日 | 交付价值 |
|---|---|---|---|
| **档 1 · 去炸弹（必做）** | S00 + S01（+ 可选 S02） | **1–1.5 人日** | 导出 65533 → ~1250；序号余量 2 → 64285；构建末尾有断言；完全可回退。风险面最小、收益最大 |
| **档 2 · 契约成形** | + S03（第三方）+ 3–4 个高价值模块（Log/Serialization/Reflection/Object） | **5–8 人日** | 反射与序列化这两个最易漏的模块转正；imgui 只留 ~250 导出 |
| **档 3 · 全部标完** | + 其余 S04 + S05 + S06 | **12–18 人日** | `--export-all-symbols` 彻底退役；导出面 = 公开契约（~1000–1500）；`verify.ps1` 强制预算 |

**标注面（实测）**：182 个引擎类型 / 982 个被消费符号；这些类型名解析到 **≤186 个头文件**（按名解析的上界，其中 78 个存在同名候选需人工确认，实际约 **130–160 个**）。其中 **28 个头文件已 include `EngineAPI.h`、33 个文件已有 94 处 `MINENGINE_API`**，因此真正"从零加 include + 标注"的约 **100–130 个文件**；再加 codegen 2 处宏（`ReflectionMacros.h`）、CMake 1 处（`MinEngineReflection.cmake`）、第三方 2–3 处。

## Deferred

1. 拆多 DLL（core / render / editor）与 `<MODULE>_API` 全谱系。
2. 非 Windows 符号可见性专项（`-fvisibility=hidden -fvisibility-inlines-hidden`、`.so` 版本脚本）。
3. 稳定 ABI / 版本化导出名 / ABI 演进策略。
4. `find_package(minEngine)` / install export set / 头文件安装布局。
5. 跨 DLL `new`/`delete` 与 CRT 归并（MinGW 共享 msvcrt，当前不是问题）。
6. 导出表剥离、PDB/GDBN 分离、Debug 产物瘦身（现状 483MB）。
7. `Maximum.exe`（Playground）陈旧产物处置。
8. MSVC 分支实测（`WINDOWS_EXPORT_ALL_SYMBOLS` / `/EXPORT:` 差异）；本仓当前只有 MinGW 验证路径。

## 待主人审批的决策清单

| # | 决策 | 选项 | 推荐 | 一行理由 | 什么会推翻它 |
|---|---|---|---|---|---|
| **Q1** | 总体路线 | ①A 显式宏 + `.def` 过渡 ②B 长期 `.def` ③D 全静态化 | **①A** | 只有 A 让导出面与"公开契约"同源，且 S01 就先解炸弹 | 若决定引擎与 Editor 合并为单一 exe 交付（则 D 更省事） |
| **Q2** | S01 是否立刻切默认 `whitelist` | ①是（默认 whitelist）②否（先并行验证、默认仍 auto） | **①是** | 余量只剩 2，任何一次正常开发都可能再次删产物；`.def` 由消费者 import 表构造，等价性可证 | 若外仓 `minCraft` 等产品无法同期用新 import lib 重建（用 O3 脚本 1 小时可核对） |
| **Q3** | imgui 导出策略 | （原推荐留引擎 + `IMGUI_API`） | **已作废** → 迁进编辑器 DLL（[BUILD-F03](BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md) Q3 / [BUILD-F04](BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md) S04，2026-10-20） | — | — |
| **Q4** | glfw 策略 | ①静态 + 微型 `.def` ②`glfw3.dll` | **② 已落地**（F03 刀1 / F04） | 单份动态库 | — |
| **Q5** | 反射 codegen 导出机制 | ① `ME_REFLECTION_MODULE_API` ②点级 ③单独 `.def` | **①（刀2 批3 仍待做）** | 标注跟随归属模块 | 膨胀不可控 → 退② |
| **Q6** | 导出预算阈值 | ①3000 … | **①**（verify 现用 3000） | 早失败 | 终态稳定后可收紧 |
| **Q7** | 外仓契约 | ① live `--consumer-dir` ②等重建 | **① 已接 verify** | 外仓常 import `libminEngine.dll` | — |
| **Q8** | 做到哪一档 | ①档1 ②档2 ③档3 | **档1 Done；档3 = F03 刀2（待批）** | 去炸弹已完成 | — |

## 附录 A · 可复现测量命令

本设计的全部数字由以下只读测量得出：

```text
① 导出表（PE Export Directory，纯解析，不运行 DLL）
   OptionalHeader(PE32+) → DataDirectory[0] = Export
     OrdinalBase        = dir + 16
     NumberOfFunctions  = dir + 20
     NumberOfNames      = dir + 24
     maxNamedOrdinal    = OrdinalBase + max(AddressOfNameOrdinals[i])

② 消费者 import 表（PE32+ 的 ILT thunk 步长是 8 字节，这是最容易写错的一处）
   objdump -p minEngine/bin/MaximumEditor.exe | sed -n '/DLL Name: libminEngined.dll/,/^$/p'

③ 名字还原与归类
   c++filt -n < exports.txt > exports.demangled.txt

④ 本轮实测结果（可直接对照复核）
   libminEngined.dll : OrdinalBase 1, NumberOfFunctions 65533, NumberOfNames 65533, maxNamedOrdinal 65533
   MaximumEditor.exe 578 | minEngineTests.exe 802 | SampleEditor.exe 564
   libGameModuleStubd.dll 20 | SampleGame.exe 9 | (陈旧) Maximum.exe 519（34 个已不存在）
   libminCraftd.dll 153（外仓；其中 34 个不在 in-repo 并集内）
   in-repo 5 消费者并集 = 1216；含外仓 = 1250；引擎命名空间导出 8,567，其中被消费 982
```

**注**：`ld --help`（Binutils 2.45）实测存在 `--exclude-all-symbols`、`--exclude-symbols`、`--exclude-libs`、`--exclude-modules-for-implib`、`--export-all-symbols`、`--output-def`、`--retain-symbols-file`；其中 `--retain-symbols-file` 与 `--export-dynamic-symbol-list` 属 ELF 语义，**不能**用于 PE 导出裁剪——这是 Alternatives-C 被判死的直接依据。