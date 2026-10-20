# BUILD-F03 — 符号契约盘点与三刀 rollout · 把导出面收敛成真契约

## Meta
- **ID:** `BUILD-F03`
- **Type:** Refactor（ABI 契约 / 构建边界）+ 设计盘点
- **Complexity:** `L2`（三刀切片，可分轮实施）
- **Status:** `Design — 待主人审批` · **v2**（2026-10-20 按 imgui 裁决修订，见「变更记录」）
- **Owner:** project maintainer
- **Last updated:** `2026-10-20`
- **Design input:** [BUILD-F02](BUILD-F02_EXPLICIT_EXPORT_MINENGINE_API_DESIGN.md)（三段式状态机，已批准）、[BUILD-F02-S01](BUILD-F02_S01_EXPORT_WHITELIST_IMPLEMENTATION.md)（`whitelist` 已落地）、[BUILD-F01](BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)
- **Blocks / 边界:** [BUILD-F04](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md)（编辑器模块边界）—— **imgui 家族的归属由 BUILD-F04 负责（S04 执行迁移）；本文不提供 imgui 的落地方案，两个 Feature 的边界不再重叠**。本文对刀3 只给接口说明与一条约束，不展开。
- **本文性质:** **只写设计，不改任何 `.h/.cpp/CMakeLists`**；全部数字为本轮只读实测（命令见附录 A）

## 变更记录

| 版本 | 日期 | 变更 | 原因 |
|---|---|---|---|
| v1 | 2026-10-20 | 初版：四刀（刀1 glfw+glad 移出 / 刀2 imgui 契约化 / 刀3 引擎面标注 / 刀4 编辑器边界） | — |
| **v2** | **2026-10-20** | **①原刀2（给 imgui 补导出宏、留在引擎 DLL）整刀删除；②重编号：原刀3 → 刀2、原刀4 → 刀3；③环3 计数改为"本仓白名单层面 = glfw 8 + glad 5 = **13**"，imgui 253 改为"随 BUILD-F04 S04 整体移出引擎导出表"，总导出预测 1545 → **约 1292**（叠加刀1 的 −13 ⇒ **1279**）；④判定准则 C2 由 `--require-annotated-bucket imgui` 改写为 `--forbid-buckets imgui`；⑤预算棘轮重算（1700→1650→1450→1400→1350）；⑥失效模式 F4 改为"imgui 只在编辑器 DLL 编译一次"；⑦决策 Q3 标注**已裁定**（迁进编辑器 DLL / 裁决者：主人 / 2026-10-20）；⑧纠正「刀3 接口说明」里"引擎 DLL 必须保住 imgui 253"的自相矛盾；⑨TL;DR、P3 宏覆盖表、附录 A ⑤ 的 imgui 宏覆盖证据降级为"迁移核对基线"** | **与 BUILD-F04 S04 硬冲突 + 主人 2026-10-20 架构裁决**：imgui 不留在引擎 DLL，迁进编辑器 DLL（F04 S04 执行）；通用编辑器 `MaximumEditor.exe` 重新定位为"资产-only 项目编辑器"、缺匹配游戏模块时硬报错（F04 S05 执行）；BUILD-F04 的 S01–S06 已一次性批准 |

## TL;DR

> 当前 `libminEngined.dll` 导出 **1545** 条，其中**只有 1260 条被任何消费者 import**，285 条零消费；被消费的 1260 条按"值得暴露给谁"分成四环：**引擎运行时 API 186 / in-repo 工具专用 778 / 第三方 266 / 内部实现泄漏 315**。三刀的顺序 = **先砍掉"第三方只在测试里用"的 glfw+glad 13 条（刀1）→ 把 994 条引擎符号逐批标注、把 `.def` 烧到 0、切 `explicit`（刀2）→ 编辑器自己的边界（含 imgui 家族整体迁出引擎 DLL）交给 BUILD-F04（刀3）**。刀1 约 0.5 人日，刀2 12–18 人日分 6 批，每批结束时构建都绿、都能单片回退。
>
> **imgui 归属（v2 裁决）：** imgui 家族**不再属于引擎契约**，其归属由 [BUILD-F04](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md) 的 **S04** 负责（整体迁进编辑器 DLL）。因此**本仓库白名单层面的环3 = glfw 8 + glad 5 = 13**（不是 266）；imgui 253 条随 F04 S04 移出引擎导出表，总导出预测 **1545 → 约 1292**（叠加刀1 的 −13 后为 **1279**）。原刀2（给 imgui 补导出宏、留在引擎 DLL）**已作废**。

**本轮最重要的三个发现（都不是"看着办"，都有 file:line 或 PE 证据）：**

1. **"内部符号"里最危险的一类不是被直接调用的，而是被 inline 函数强制引入的**：`InputSystem::s_Instance`（`Runtime/Function/Input/InputSystem.h:128`，被同文件 `:39` 的 inline `HasInstance()` 引用）因此进了**编辑器 + 外仓游戏模块**的导入表——这解释了 30 条"内部命名但被消费"的符号中的绝大多数。
2. **imgui 的 253 条不是"引擎契约"，而是"搬家对象"**：`imgui.h:87-92` 与 `ImGuizmo.h:110-124` 的宏单点覆盖形成无缺口覆盖——实测 253 = 224（直接注解）+ 20（`IMGUI_IMPL_API` 转手）+ 9（ImGuizmo）**无遗漏**。这条证据在 v2 里的用途已经反转：它证明的是"**整族迁出时不会漏掉任何一条**"，而不是"补宏就能把它们留成引擎契约"。**裁决（2026-10-20）：imgui 迁进编辑器 DLL，由 BUILD-F04 S04 执行；引擎导出中不得再出现 imgui 桶（C2 已改写为 `--forbid-buckets imgui`）。原刀2 已作废。**
3. **产品二进制里带着一整套测试脚手架**：`Runtime/Test/`（10 个文件，含 `TestRunner` / `TestSuiteRegistry`）编进引擎 DLL，`main.h:38` 的 `--mode=test` 分支让**每一个用 main.h 的宿主**（`SampleGame.exe`、外仓 `MinCraft.exe`/`libminCraft.dll`）都必须 import `ForwardToMinEngineTestsExecutable`；另有 38 条 `Test*` 类型符号（`SubsystemTestFixtures.h:59`）也在导出表里。

## Problem

### P1 · 导出表 1545 条，只有 1260 条有人要（实测）

```text
NumberOfFunctions 1545  (code 1381 / data 164)   maxNamedOrdinal 1545   headroom 63990
composition: minEngine 1278 | imgui 253 | glfw 8 | glad 5 | other(TAG_Channel_Default) 1
```

消费者实测（PE 导入表，`libminEngined.dll`）：

| 消费者 | 导入 | 备注 |
|---|---|---|
| `minEngineTests.exe` | **807** | 最大消费者，且是唯一使用 glad/glfw 的二进制 |
| `MaximumEditor.exe` | 579 | = `MaximumEditorLib`（STATIC）+ `Editor/CMakeLists.txt` |
| `SampleEditor.exe` | 579 | 同一 STATIC 库 + `GAME_MODULE` |
| `libGameModuleStubd.dll` | 26 | 外部 Game 模块最小样本 |
| `SampleGame.exe` | **7** | 只用了 `Engine::*` + `ApplicationCommandLine` + 1 个测试转发函数 |
| **in-repo 并集** | **1225** | |
| `libminCraft.dll`（外仓 ×3 产物） | 130 / 160 / 144 | import 表里写的是 `libminEngine.dll`（见 P7） |
| `MinCraft.exe`（外仓） | 7 | |
| **含外仓并集** | **1260** | ⇒ **285 条零消费** |

零消费不等于无害：它们是**没人验证过的契约**，且每一条都可能是"某次类级 `MINENGINE_API` 顺带导出的 vtable/typeinfo"。

### P2 · 最大的一块不是"编辑器专用"，而是"只有 in-repo 工具用"：778 条

按消费者拆解被消费的 1260 条引擎/第三方符号：

| 子集 | 条数 | 含义 |
|---|---|---|
| 只被 `MaximumEditor.exe`/`SampleEditor.exe` 消费 | **130** | 真·编辑器独占 |
| 只被 `minEngineTests.exe` 消费 | **510** | **测试把内部实现钉成了契约**（单元测试直连 DLL 的内部 API） |
| 编辑器与测试都用 | 138 | 同上，且编辑器也在用"非公开"面 |
| 被游戏侧（SampleGame/Stub/外仓）消费 | 186 | 这才是"真公开面" |

**结论：** 现在谈"哪些符号该暴露给编辑器"之前，必须先回答"测试为什么能 import 510 条只有它用的符号"。这不是导出表的问题，是**测试链接模型**的问题——但它是预算与棘轮绕不开的 41%（510/1260）。

### P3 · 第三方符号的消费画像：全是"in-repo 工具需要"，产品侧零依赖（实测）

| 库 | 导出 | 谁 import | 结论 |
|---|---|---|---|
| imgui + ImGuizmo | **253** | `MaximumEditor.exe` **253/253**、`SampleEditor.exe` **253/253**；测试 0；`SampleGame.exe` 0 | 100% 由编辑器消费，且是**全部** imgui 导出 |
| glfw | **8** | 只 `minEngineTests.exe` 8/8 | 引擎自己用 glfw（`WindowSystem`），但**没人需要从引擎导出** |
| glad | **5** | 只 `minEngineTests.exe` 5/5（`GLAD_GL_VERSION_4_6` 为 DATA） | 同上：`glad.c` 编进引擎 DLL（`minEngine/CMakeLists.txt:36`） |

glad 那 5 条是 `GLAD_GL_VERSION_4_6`、`gladLoadGLLoader`、`glad_glBindTexture`、`glad_glGetError`、`glad_glGetTexImage`——**测试在用自己的 GL 上下文**：`minEngine/Tests/Suites/` 下 7 个文件（`MaterialCompileCacheTest`、`MaterialIRTest`、`RenderGraphTest`、`ShaderCompilerTest`、`SpriteAtlasTest`、`SpriteAuthoringTest`、`SpriteRuntimeConsumeTest`）**每一个都自己 `glfwInit()` + `gladLoadGLLoader()`**（逐个实测计数 1/1）。这意味着"让测试自己持有 glfw/glad"不是新增负担，而是**把测试已经在做的事补完**。

**imgui 253 条的构成实测（原刀2 的证据 · 原刀2 已作废；现仅留作 BUILD-F04 S04 的迁移核对基线——迁移后引擎导出表必须一条 imgui 都不剩）：**

| 覆盖来源 | 条数 | 证据 |
|---|---|---|
| `IMGUI_API` 直接注解声明 | 224 | `imgui.h`/`imgui_internal.h` 中 `IMGUI_API <name>` 同行匹配 |
| `IMGUI_IMPL_API` 转发 | 20 | `imgui.h:90-92` `#ifndef IMGUI_IMPL_API / #define IMGUI_IMPL_API IMGUI_API`；`backends/imgui_impl_glfw.h:35-39` 等 |
| ImGuizmo | 9 | `ImGuizmo.h:110-111` `#ifndef IMGUI_API / #define IMGUI_API`，`:124-127` 使用它 |
| **合计** | **253** | 与导出面**逐一相等**，无缺口 |

### P4 · 内部实现正被产品侧依赖：12 类误用（详见"消费方误用清单"）

最刺眼的三条：

- `minEngine::ForwardToMinEngineTestsExecutable(int,char**)` —— 声明在 `Runtime/Test/TestExecutableForward.h:6`，由 `main.h:38`（`Mode == ApplicationMode::Test` 分支）强制引入。`SampleGame.exe` 与外仓 `MinCraft.exe`/`libminCraft.dll` 的导入表里都有它。
- `minEngine::SpriteAtlasPacker::DilateBorderRing(ImagePixels&,int,int,int,int,int)` —— 打包器内部步骤，外仓 `src/Render/BlockTextureAtlas.cpp:365` 直接调用。
- `minEngine::Reflection::ReflectionSystem::FinalizeReflection()` —— 引擎生命周期（引擎自己在 `Runtime/Engine.cpp:229` 调），外仓 `src/Content/Core/ContentRegistry.cpp:383` 与 `FragmentList.hpp:27` 也在调。

### P5 · inline 引用私有静态：把"内部数据"变成契约（最隐蔽的一类）

```cpp
// Runtime/Function/Input/InputSystem.h
:39   static bool HasInstance() { return s_Instance != nullptr; }   // inline，头文件内
:128  static InputSystem* s_Instance;                               // 私有静态
```

任何消费者只要调一次 `HasInstance()`，就必须 import `minEngine::InputSystem::s_Instance`（DATA）。实测该符号的导入方是 `MaximumEditor.exe` + `SampleEditor.exe` + **外仓 `libminCraft.dll`**。同型还有 `Engine::s_Instance`、`WindowSystem::s_Instance`。这类符号**不能靠"没人用就删"处理**：删了就链接失败，且失败点在外仓。

导出表里 `::s_` 静态共 **39** 条（30 条被消费、9 条零消费）。

### P6 · 测试脚手架随产品发布（实测）

| 事实 | 证据 |
|---|---|
| `Runtime/Test/` 编进引擎 DLL | `Runtime/Test/` 下 10 个文件：`ITestSuite.h`、`TestContext.*`、`TestExecutableForward.*`、`TestRunner.*`、`TestSuiteMetadata.h`、`TestSuiteRegistry.*` |
| 测试夹具在引擎源码树里 | `Runtime/Function/Framework/Subsystems/SubsystemTestFixtures.h:59`（`class TestWorldSubsystem` + `s_InitCount` 等） |
| 导出表里的 `Test*` 符号 | **38** 条，全部只被 `minEngineTests.exe` 消费 |
| 每个宿主都要背这个包袱 | `main.h:38` → 所有用 `main.h` 的 exe/dll 都 import `ForwardToMinEngineTestsExecutable` |

### P7 · 外仓契约基线目前**不能在本仓链接期验证**（新发现，必须在刀1 前处理）

外仓产物的 PE 导入表写的是 **`libminEngine.dll`**（无 `d`），本仓产物是 **`libminEngined.dll`**（Debug 命名）。也就是说：

- 外仓最近一次链接用的是**另一套命名的引擎产物**（可能是 Release，也可能是被复制过名字的旧 DLL）；
- `.def` 头注释里的 `libminCraftd.dll 153 names` 对应的文件**当前已不在 `minEngine/bin/` 里**（本轮实测该目录无此文件），只有 `minCraft*/bin/libminCraft.dll`（130/160/144）；
- 因此"外仓需要什么"这件事，**只能用 live 外仓 `bin/` 作 `--consumer-dir` 实时扫**，不能依赖白名单历史行。

## Scope

1. 给出**四环分类**（引擎运行时 / in-repo 工具专用 / 第三方 / 内部实现泄漏）的**可判定规则**与当前条数，覆盖全部 1545 条导出。
2. 给出**消费方误用清单**（6 个 in-repo/外部消费者），每条含"为什么内部 + 应提供什么替代 API"。
3. 给出**三刀 rollout**（原四刀；原刀2 已作废）：每刀的改动面、"结束仍绿"论证、可自动断言的验收 oracle、回退方式、工作量档位。
4. 给出**可机器检查的判定准则**（可落成 `scripts/audit_dll_exports.py` 的检查项 C1–C8）。
5. 给出**预算棘轮**序列（1700 → 1650 → 1450 → 1400 → 1350，终态 ≤1400）与数值依据。
6. 给出**反射 codegen 符号归属规则**（A/B/C/D 四类，含新发现的"注册汇点"类）。
7. 给出**刀3（编辑器边界）的接口说明**并显式指向 BUILD-F04；**imgui 家族的迁移方案在 F04 S04，本文不再给**。

## Non-goals

- **不实施**：本文不改任何 `.h/.cpp/CMakeLists`，不跑构建、不跑 `verify.ps1`、不动 `FEATURE_REGISTRY.md`、不动 `docs/ai/bugs/*`、不碰外仓 `minCraft*`。
- **不展开编辑器 DLL 化与 imgui 迁移**（刀3 / BUILD-F04 S04）——接口说明只到"边界在哪、谁依赖谁"，方案与排期在 BUILD-F04。
- **不承诺稳定 ABI**、不做版本化导出名、不做 `-fvisibility=hidden` 专项（沿用 F02 Non-goals）。
- **不改测试架构**（不把 `minEngineTests` 改成静态链接引擎的 510 条"内部契约"）——只在 P2/决策 Q9 里登记为可选后续。
- **不修外仓**：外仓符号只出清单 + "我们应提供什么"，不提改外仓的实现方案。

## 环定义与判定流程（对每条导出可判定）

四环是**互斥**的，判定顺序固定（顺序本身就是规则的一部分）：

```text
for each exported name N:
  1. bucket = classify(N)                        # 复用 audit 脚本的 mangled-name 规则
     if bucket ∉ {minEngine, other} → 环3（第三方）
  2. if N matches INTERNAL → 环4
     INTERNAL = "::Detail::" | "::detail::" | "::_Impl" | "(anonymous namespace)"
              | ^GReflection | ^_ZZ | "guard variable for" | "::s_<Name>"
  3. elif N ∈ imports(GAME)   → 环1               # SampleGame ∪ GameModuleStub ∪ 外仓 libminCraft/MinCraft
  4. elif N ∈ imports(EDITOR ∪ TESTS) → 环2
  5. else                     → 环4               # 零消费
```

**为什么这样定义（每条都可判定）：**

| 判据 | 机器可测 | 说明 |
|---|---|---|
| 是否第三方 | 是 | 名字规则 + 定义在 vendored TU（不是引擎 `src/` 头声明） |
| 是否内部 | 是 | 名字模式（`Detail::`/`_Impl`/`s_`/`GReflection`/`_ZZ`） |
| 谁在消费 | 是（唯一硬契约） | 6 个 in-repo 消费者 + 外仓 live 产物的 PE 导入表 |
| 声明头可达性 | 是 | 从消费者 TU 的 `#include` 出发对 `#include` 图做 BFS：实测可达 430（Editor）/ 361（Tests）/ 64（Examples）/ 43（外仓）个引擎头 |
| 声明头是否有 out-of-line 定义 | 是（按符号类型） | 导出符号本身即证明存在 out-of-line 定义（inline 符号不会进导出表） |

## 四环分类表（覆盖 1545 条）

| 环 | 条目 | 条数 | 判定依据（实测） |
|---|---|---|---|
| **环1 引擎运行时 API** | 游戏侧真的在用 | **186** | ∈ imports(`SampleGame.exe` 7 ∪ `libGameModuleStubd.dll` 26 ∪ 外仓 180) 的并集；典型：`Engine::Initialize/Run/Shutdown`、`SceneManager::Get`、`InputKeys::*`、组件 Setter、`Scene::SpawnGameObject` |
| **环2 in-repo 工具专用** | 编辑器/测试在用，产品不用 | **778** | 只被 `MaximumEditor.exe`/`SampleEditor.exe`/`minEngineTests.exe` import |
|  · 2a 编辑器独占 | | **130** | 只被两个编辑器 exe import |
|  · 2b 测试独占 | | **510** | 只被 `minEngineTests.exe` import |
|  · 2c 编辑器+测试 | | **138** | 两者都 import |
| **环3 第三方实现** | 引擎替第三方发的牌 | **266** | `classify(N)` ∈ {imgui, glfw, glad}：imgui 253 / glfw 8 / glad 5 |
| **环4 内部实现泄漏** | 没人要 / 不该外露 | **315** | 内部命名共 39 条（其中 **30** 条仍被 in-repo 工具消费 ⇒ 计入本环）+ 零消费 **285** 条（含上表 39 条里的 9 条）= 315 |
| **合计** | | **1545** | |
| 迁移后（BUILD-F04 S04 落地） | 环3 只剩 glfw 8 + glad 5 | **13** | imgui 253 随 F04 S04 整体移出引擎导出表；总导出 1545 → **约 1292**（再叠加刀1 的 −13 ⇒ **1279**） |

补充计量（同一批数据的不同切面，用于定工作量，不与上表相加）：

| 量 | 值 | 用途 |
|---|---|---|
| 引擎自有导出（`classify == minEngine` + `other`） | **1279** | 刀2 的总盘子 |
| 其中被消费 | **994** | 刀2 真正需要标注/保住的符号 |
| 其中零消费 | **285** | 刀2 之后应当消失（或登记豁免） |
| 被消费引擎符号覆盖的不同类型/命名空间根 | **185** | 与 F02 估的 182 类型吻合 |
| 被消费引擎符号可直接解析到的声明头（按"头文件名 == 类型名"规则） | **132** 个 | 其中仅 **14** 个已含 `MINENGINE_API` |
| 全树 `MINENGINE_API` 标注现状 | **95 处 / 33 个文件** | 刀2 的起点（F02 记 94/33，S02 后 +1） |
| 未解析到声明头的被消费符号 | **230** | 194 条 vtable/typeinfo（`vtable for X` 归 X）+ 36 条 std 返回类型的自由函数（需去掉返回类型再解析） |
| codegen 符号（`StaticClass()` / `GetClassName<T>`） | **53 / 36** | 消费 52 / 36；见"反射 codegen 归属规则" |
| 注册副作用变量 `GReflectionClassRegister_*` 的导出数 | **0** | 目前**没有**被导出——必须保持 0（刀2 不得把它们带进导出面） |
| 数据符号导出 | **164** | 其中 `InputKeys::*` **46** 条是 `static const InputKey`（`Runtime/Function/Input/InputKeys.h:42-144`） |
| `Test*` 类型符号（测试夹具） | **38** | 全部只被 `minEngineTests` 消费 |

**一句话读法：** 环1（186）是必须冻结并文档化的真契约；环2（778）是"in-repo 契约"（编辑器/测试的私有约定，不对外承诺）；环3 里的 imgui 253 **不在本仓契约里**——它随 BUILD-F04 S04 整体移出引擎导出表，本仓白名单层面只剩 glfw 8 + glad 5 = **13** 条，由刀1 换机制（不再由引擎导出表提供）；环4（315）是要裁掉或申报的。

## 消费方误用清单

扫描 6 类消费者（`MaximumEditor.exe` / `SampleEditor.exe` / `SampleGame.exe` / `libGameModuleStubd.dll` / `minEngineTests.exe` / 外仓 `libminCraft.dll`+`MinCraft.exe`），筛出"本意不该暴露却被使用"的符号。**外仓只读：只出清单与"我们应提供什么"，不修改、不提改外仓的实现。**

| # | 符号 | 所属模块/类 | 消费方 | 为什么判定为内部（证据） | 建议替代 |
|---|---|---|---|---|---|
| M1 | `minEngine::ForwardToMinEngineTestsExecutable(int,char**)` | `Runtime/Test/` | `SampleGame.exe`、外仓 `MinCraft.exe`、`libminCraft.dll` | 测试转发器；由 `main.h:38` 的 `Mode==Test` 分支强制引入，任何 `#include "main.h"` 的宿主都中招（外仓 `Source/Programs/GameMain.cpp:3`）。外仓源码 0 处直接引用 ⇒ 纯强制 | 把 `--mode=test` 分派移出 `main.h`（宿主 `main` 不再发 test 分支），或加 `MINENGINE_NO_TEST_FORWARD` 编译期开关；引擎不再导出该符号 |
| M2 | `minEngine::GameObject::AddComponent_Internal(std::shared_ptr<Component>)` | `GameObject` | 外仓 `libminCraft.dll` | 名字即内部；只服务于 inline `AddComponent<T>`（`GameObject.h:103` 调用，`:134` 声明）。外仓源码 0 处直接引用 ⇒ 强制导入 | 短期：保留导出但登记为"inline 强制"，不进公共教程；中期：`AddComponent<T>` 改调用 out-of-line 公开入口 `AddComponentByClass(MEClass*, shared_ptr<Component>)` |
| M3 | `ReflectionSystem::RegisterClass_Internal(MEClass*)`、`ReflectionSystem::AppendError(std::string)` | `Reflection` | `libGameModuleStubd.dll`、外仓 | 由 `Reflection.h:144` 的 inline `RegisterClass<T>()` 与各模块 `.gen.cpp` 调用 ⇒ **每个模块 DLL 都必然需要**。这不是误用，而是"未申报的 codegen ABI" | **正式提为 API**：声明处标注 `MINENGINE_API`，登记进 codegen 符号表（类 D），文档化为"codegen 专用，模块作者不得手调" |
| M4 | `ReflectionSystem::FinalizeReflection()` | `Reflection` | 外仓 `ContentRegistry.cpp:383`、`FragmentList.hpp:27` | 引擎生命周期：`Reflection.h:385` 声明，引擎自己已在 `Runtime/Engine.cpp:229` 调用；模块再调会二次 finalize | 改成**幂等惰性**：`GetClass/FindClass` 首次查询时自动 finalize；或提供唯一的 `ME_REFLECTION_FINALIZE_ONCE()` 宏 |
| M5 | `InputSystem::s_Instance`（DATA） | `InputSystem` | `MaximumEditor.exe`、`SampleEditor.exe`、外仓 | 私有静态（`InputSystem.h:128`）被同文件 `:39` 的 inline `HasInstance()` 引用 ⇒ 强制导入 | `HasInstance()` 改 out-of-line（`.cpp` 定义）⇒ 数据符号不再跨 DLL。同型：`Engine::s_Instance`、`WindowSystem::s_Instance` |
| M6 | `SpriteAtlasPacker::DilateBorderRing(ImagePixels&,int,int,int,int,int)` | `Sprite` | 外仓 `BlockTextureAtlas.cpp:365` | 打包器内部步骤（`SpriteAtlasPacker.h`），外仓**直接调用** | 提升为公开图像工具：`ImagePixels::DilateBorderRing(int)`；或提供 `ITextureAtlasBuilder` 公共入口 |
| M7 | `ObjectManager::Initialize()`、`ObjectManager::RegisterObject(std::shared_ptr<MEObject> const&)` | `ObjectManager` | 外仓 `ContentRegistry.cpp`、`FragmentList.hpp`（Initialize 直接调用；RegisterObject 无源码引用 ⇒ 强制） | 对象表生命周期属于引擎；测试侧已有更干净的 `Testing::TestAccess<ObjectManager>`（`minEngine/Tests/.../Access/ObjectManagerTestAccess.h`） | `ObjectManager::Get()` 惰性初始化；模块侧只保留 `RegisterObject` 且标注为"工厂内部使用" |
| M8 | `MeshBatchCollector::AddMesh(MeshBatch const&)` | `Render/DrawCommands` | 外仓 `ChunkMeshSceneProxy.cpp` | 通过 override `GetMeshBatches(MeshBatchCollector&)` 使用（`SceneProxy` 协议）——**合法扩展点但从未申报** | **正式提为 API**：`GetMeshBatches` + `MeshBatchCollector` 一并纳入环1 并文档化为"自定义 SceneProxy 扩展契约" |
| M9 | `PrimitiveSceneProxy` vtable + `GetBoundsLocalToWorld()`/`ResolveWorldBounds()` | `PrimitiveSceneProxy` | 外仓 `ChunkMeshComponent`/`ChunkMeshSceneProxy` | 派生需要 vtable/typeinfo（合法），但 bounds 查询是实现细节 | 冻结"可派生面"为显式 `ISceneProxy` 契约（virtual 方法集 + vtable 导出），内部成员不外露 |
| M10 | `SubsystemCollectionBase::GetSubsystem(MEClass const*)`、`~SubsystemCollectionBase()`、`WorldSubsystem::ShouldCreateSubsystem`、`WorldSubsystem::GetScene()` | `Subsystems` | `libGameModuleStubd.dll`、外仓 | 模块派生 Subsystem 的必需面（外仓 `MinCraftWorldSubsystem` 等）⇒ **合法派生面** | 归环1，标注 `MINENGINE_API` 并写进"模块作者可派生类型"清单（不是删除对象） |
| M11 | `TestWorldSubsystem::*`（15 个 `s_*` + vtable）、`TestEngineSubsystem*`、`TestGameInstanceSubsystem::*` 等 | `Subsystems/SubsystemTestFixtures.h:59` | 仅 `minEngineTests.exe` | 测试夹具编进**产品 DLL**；38 条 `Test*` 导出全在其中 | 夹具移进 `minEngine/Tests/`（或 `MINENGINE_BUILD_TEST_FIXTURES` 开关），产品 DLL 不再含 `Test*` 符号 |
| M12 | `PrimitiveComponent::SyncSceneProxy/DoEndOfFrameUpdate/ApplyActivationToSystems`、`Component::OnActivate/OnDeactivate`、`SceneViewport::GetBufferSize` | `Components`/`Render` | `libGameModuleStubd.dll`、外仓 | 派生 `PrimitiveComponent`/`Component` 的连带（ctor/dtor/vtable + 保护虚函数），属于"继承面"而非"调用面" | 与 M10 同处理：明确"可派生类型清单"，其余内部方法不导出 |

**统计：12 类 / 涉及约 60 条导出符号。** 按处置分三类：**该删/该关（M1、M11 各 1 组 + 46 条 `InputKeys` 数据瘦身）**、**该改造成 out-of-line 消除（M2、M5、M7）**、**该正式申报为契约（M3、M8、M9、M10、M12）**。

外仓用量（只读结论，作为"我们该提供什么"的输入）：外仓 180 条引擎导入中，**140 条在其源码里能找到同名标识符**（直接调用/派生），**40 条找不到**（forced：inline 强制 / vtable / codegen / `main.h` 传导）。那 40 条正是"我们改了就会打断外仓"的高危集合。

## 三刀切片表（原「四刀」；原刀2 已作废，原刀3 → 刀2，原刀4 → 刀3）

约定：**每片结束时 `cmake --build minEngine/build` 全绿 + `audit_dll_exports.py --fail` 通过**；"档位"指 F02 的工作量档（A=0.5d / B=1d / C=2–3d）。

> **编号变更与边界（v2）：** ①**原刀2（给 imgui 补导出宏、留在引擎 DLL）已作废**，理由见「变更记录」——imgui 迁进编辑器 DLL，由 [BUILD-F04](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md) 的 **S04** 执行；②原刀3 → **刀2**，原刀4 → **刀3**；③**imgui 家族的归属由 BUILD-F04 负责，两个 Feature 的边界不重叠**——本文对 imgui 只保留一条约束：引擎导出中不得出现 imgui 桶（C2）。

| 刀 | 做什么 | 结束时构建为何仍绿（论证） | 验收 oracle（可自动断言） | 回退方式 | 档位 |
|---|---|---|---|---|---|
| **刀1 · glfw+glad 移出导出表（−13）** | ① 引擎与 `minEngineTests` 都改链 `libglfw3dll.a`（`Third-Party/glfw/lib-mingw-w64/` 已同时有 `glfw3.dll` 与 `libglfw3dll.a`）；② `glad.c` 编进 `minEngineTests`（`Tests/CMakeLists.txt` +1 源文件）；③ `--emit-whitelist` 重生成 ⇒ `.def` −13 行 | 符号提供方从"引擎导出表"换成"glfw3.dll 导入库 / 测试自身 TU"，两者都是**链接期可证**的等价替代；测试本就自己 `glfwInit+gladLoadGLLoader`（7/7 实测），**不依赖引擎的 glad 状态**；且 8+5 条全部只被测试 import，编辑器/游戏侧零依赖（P3） | ① `--forbid-buckets glfw,glad`（C1）：导出集合 ∩ {glfw,glad} = ∅；② `--verify-coverage`：6 个消费者 missing = 0；③ `minEngineTests.exe test smoke` 退出码 0；④ 导出数 = 1532 | 还原 `Tests/CMakeLists.txt` 1 行 + 引擎 CMake 1 行 + 重生成 `.def`（3 文件，独立于刀2/刀3） | A |
| **刀2（原刀3）· 引擎公开面标注 + 删 `.def` → `explicit`** | 分 6 批：批0 数据瘦身（`InputKeys` 46 条 `static const` → `inline static const`，−46 数据导出）→ 批1 Log/CLI/Path → 批2 Math/Input → 批3 Reflection + codegen A/B/D → 批4 Object/Scene/Components → 批5 Render/Resource/UI/Audio/Physics/Serialization；每批把对应 `.def` 行删掉；全烧完后切 `MINENGINE_EXPORT_MODE=explicit` | 每批处于"注解 ∪ 白名单"并存态：**删行前注解已存在**，导出集合不变 ⇒ 绿；漏标注只会在**消费者链接期** loud 失败；最后一刀切 `explicit` 时白名单已空 ⇒ 导出集合等价（F02 S05 论证） | ① C6：被消费引擎符号中"声明头缺 `MINENGINE_API`"计数逐批单调降，终态 = 0；② C5：inline 强制导出的符号全部登记在清单里（无未登记项）；③ 每批：4 类 target 链接成功 + `--verify-coverage` 0 缺 + C7 棘轮满足；④ 终态：`MINENGINE_EXPORT_MODE=explicit` 下导出数 ≤ 1400 且缺失 0 | 单批 revert（头文件 + `.def` 行）；终态一行 `-DMINENGINE_EXPORT_MODE=whitelist` | C（12–18d） |
| **刀3（原刀4）· 编辑器模块边界（不在本文展开）** | 见"刀3 的接口说明"，方案与排期在 **BUILD-F04** | 边界文件（`MaximumEditorLib` STATIC → 编辑器 DLL）不改引擎导出面；引擎侧只减不增 | 交给 BUILD-F04；本文只声明一条约束：**刀3 不得增加引擎导出条目**（C7 棘轮保证），**且引擎导出中不得出现 imgui 桶**（C2） | — | — |

### 刀3（原刀4）的接口说明（只给事实与约束，方案在 BUILD-F04）

| 事实 | 证据 |
|---|---|
| 编辑器目前**没有 DLL 边界** | `Editor/CMakeLists.txt:17` `add_library(MaximumEditorLib STATIC ...)`；`MaximumEditor.exe`/`SampleEditor.exe` 是两个 exe 各自链这个 STATIC 库 |
| 编辑器自己编的第三方只有 imgui-node-editor + efsw | `Editor/CMakeLists.txt:9-27`（`IMGUI_NODE_EDITOR_SRC`、include 目录 `../minEngine/Third-Party/imgui`） |
| imgui 内核**今天**在引擎 DLL 里（**BUILD-F04 S04 会把这条事实反转**：imgui 家族源与 `IMGUI_ENABLE_FREETYPE` 迁到编辑器 target，引擎 DLL 不再编 imgui） | 引擎 target `glob` 了 `Third-Party/imgui/*.cpp` + `backends/imgui_impl_glfw.cpp`（`minEngine/CMakeLists.txt:24-32`），并 `PRIVATE IMGUI_ENABLE_FREETYPE`（`:159`） |
| 编辑器对 GL/GLFW **零函数调用** | `ImGui::` 调用点 **1359**；`gl[A-Z]*(` 调用点 **0**；glfw **引用** 2 处（`Editor.cpp:35`、`Platform/EditorImGuiBackend.cpp:4` 两个 include），**无 glfw 函数调用**——编辑器只把 `GLFWwindow*` 句柄交给 `imgui_impl_glfw`（该 backend 在 DLL 内） |
| 通用编辑器对游戏模块**零感知** | `Editor/CMakeLists.txt` 无 `GAME_MODULE`；`SampleEditor.exe` 由 `cmake/MinEngineTargets.cmake:93` `minengine_add_editor_executable(TARGET ... GAME_MODULE ... MAIN_SOURCE ...)` 产出 |
| **刀3 的边界含义（本文结论 · v2 已纠正）** | 引擎 DLL 必须保住的是**环1 的 186 条**；**imgui 253 不属于引擎契约**——它由 BUILD-F04 S04 整体迁进编辑器 DLL，引擎侧一条不留（C2 兜底）；编辑器独有的 130 条环2 是"编辑器 DLL 的第一批自有符号"，而 510 条"测试独占"是**测试链接模型的债**，不应被编辑器 DLL 继承 |

## 判定准则（可机器检查）

以下每条都能直接写成 `scripts/audit_dll_exports.py` 的检查项；括号内是建议的 CLI 与退出语义。**"看着办"不作为任何切片放行理由。**

| 编号 | 检查项 | 判定式 | 当前值 | 目标态 |
|---|---|---|---|---|
| **C0**（已有） | 预算与序号余量 | `NumberOfFunctions ≤ BUDGET` 且 `65535 − maxOrdinal ≥ 60000`（`--max-exports/--min-headroom --fail`） | 1545 / 63990 | 通过 |
| **C1** | 第三方 C 符号不得出现在引擎导出表 | `exports ∩ bucket(glfw, glad, stb_image, jolt, fmt/spdlog, nlohmann)` = ∅（`--forbid-buckets`） | glfw 8 + glad 5 → 刀1 后恒 ∅（实测 1283 = minEngine 1282 / other 1）。C1 现由两条共同守护：`--forbid-buckets` 防第三方混入，`--assert-no-removals-vs cmake/minEngine_export_baseline.json` 防契约符号静默消失 | 刀1 后恒 ∅ |
| **C2** | **引擎导出中不得出现 imgui 桶符号**（原"imgui 面必须由宏注解覆盖 + `--require-annotated-bucket imgui`"已作废） | `exports ∩ bucket(imgui) = ∅`（`--forbid-buckets imgui`） | 253（BUILD-F04 S04 前基线） | F04 S04 落地后恒 ∅ |
| **C3** | 分环报告与"第三方不得被产品侧消费" | 生成四环报告；断言 `imports(GAME) ∩ ring3 = ∅`（`--ring-report --assert-rings`） | ring3∩GAME = 0 | 恒真 |
| **C4** | 测试脚手架不得进产品导出面 | `exports ∩ (Test* 前缀 ∪ 声明头 ∈ Runtime/Test/** ∪ SubsystemTestFixtures.h)` = ∅，例外须登记 `--allow`（`--assert-no-test-scaffolding`） | 38 条 | 0 条 |
| **C5** | inline 强制导出必须显式登记 | 对每个 `kind == data` 且被消费的导出：若其声明头内存在 inline 函数引用该标识符，则必须出现在 `cmake/minEngine_inline_forced.txt`（`--assert-inline-forced-manifest`） | 无清单（39 条 `s_` 全部裸奔） | 清单覆盖 100% |
| **C6** | 注解覆盖率（刀2 燃烧指标） | 对被消费引擎符号，统计"声明头不含 `MINENGINE_API`"的条数并落盘（`--annotation-report --json`）；`--assert-annotation-coverage 100` | 994 条中仅约 80 条已覆盖 | 0 |
| **C7** | 预算棘轮单调不增 | 读 `cmake/minEngine_export_budget.txt` 的上次记录；本次 `BUDGET` 必须 ≤ 上次，且实测 ≤ BUDGET（`--assert-budget-ratchet`） | 无记录机制 | 每批提交都降 |
| **C8** | 外仓导入必须干净 | `--consumer-dir <外仓 bin>` 扫描：外仓导入 ⊆ 导出，且**导入不得命中内部模式**（`s_`/`_Internal`/`Detail::`/`*Impl*`/`GReflection*`），除非登记在 `cmake/minEngine_internal_allowlist.txt` 且带理由（`--assert-external-imports-clean`） | 外仓命中内部模式约 12 条（M1/M2/M3/M5/M7/M8） | 0 未登记命中 |

**分环判定本身也落成脚本**（`--ring-report`，输出 JSON：每条导出 → ring / consumers / 声明头 / kind），这样"四环表"每次构建都能复现，而不是本文档的一次性快照。

## 反射 codegen 符号归属规则（最易漏的一类）

`ME_CLASS`/`ME_STRUCT` 生成的符号分**四**类（F02 How-4 只列了三类，本轮补出第四类）：

| 类别 | 生成处 | 具体符号 | 导出规则 | 实测 |
|---|---|---|---|---|
| **A · 类成员 out-of-line 定义** | `ME_REFLECTION_CLASS_DEFINE_END` → `src/Generated/Reflection/*.gen.cpp` | `T::StaticClass()` | 跟随 T 的归属模块 | **53 条导出 / 52 条被消费** |
| **B · 模板特化自由函数** | `ME_REFLECTION_CLASS_DEFINE_BEGIN` | `minEngine::Reflection::GetClassName<T>()`（枚举侧 `GetEnumName<E>`） | 跟随 T 的归属模块 | **36 条导出 / 36 条被消费** |
| **C · 注册副作用变量** | `ME_REFLECTION_CLASS_DEFINE_BEGIN` | `const bool GReflectionClassRegister_<T>` | **永不导出**（TU 内部副作用） | **0 条导出**（必须保持 0；刀2 不得把它们带进导出面） |
| **D · 注册汇点（新发现）** | 不在 codegen 里，但**被 codegen 与 inline 模板调用** | `ReflectionSystem::RegisterClass_Internal`、`ReflectionSystem::AppendError` | **必须导出且必须标注/文档化**——任何模块 DLL 的 `.gen.cpp` 都调用它们（`Reflection.h:144` 的 inline `RegisterClass<T>`）。漏标 = 所有模块链接失败 | 3 条（含 `ReflectionSystem::Get`） |

**机制（沿用 F02 Q5 方向，细化到参数）：** 给 `minengine_add_reflection_codegen(<target>)`（`cmake/MinEngineReflection.cmake`）增加 `MODULE_API <macro>` 参数，写进生成文件头，并让 `ME_REFLECTION_CLASS_DEFINE_BEGIN/_END` 把 `ME_REFLECTION_MODULE_API`（默认 `MINENGINE_API`）加到 A/B 的定义前。**不变量：标注跟随归属模块，声明与定义同宏**（声明 `dllimport` + 定义 `dllexport` 在 GCC/MSVC 都是错误）。

**D 类的额外要求：** `RegisterClass_Internal` 必须在头文件里标注 `MINENGINE_API` 且加注释"codegen 专用"；否则刀2 切 `explicit` 的那一刻，所有模块（含外仓）会在链接期失败。

## 预算棘轮

`MINENGINE_EXPORT_BUDGET` 现在是 **3000**（`cmake/MinEngineExports.cmake:24`），实测 1545——即预算只用了 51%，起不到"早失败"作用。棘轮规则：**预算只降不升；每个切片提交必须同时下调到"实测 × 1.10 向上取整到 50"，且必须 < 上一档；任何上调都需要一次审批决策。**

**v2 重算（原序列作废一段）：** 原序列 3000→1700→1650→**1650**→1600→1400 里"imgui 契约化之后数量不变"那一档不再成立——imgui 253 迁出引擎导出表后，实测序列是 **1545 → 1532（刀1 −13）→ ≈1279（F04 S04 −253）→ 1233（刀2 批0 −46）→ 逐批燃烧 → ≈1220（`explicit` 终态）**，预算相应地 **1700 → 1650 → 1450 → 1400 → 逐批降 → 1350**（终态 ≤1400）。

| 阶段 | 预计实测 | 预算 | 依据 |
|---|---|---|---|
| 现状（`whitelist`+S02） | **1545** | **1700** | 1545 × 1.10 = 1699.5 → 1700 |
| 刀1 后 | **1532** | **1650** | 1532 × 1.077；C1 上线后 glad/glfw 无法回归，余量可收窄 |
| **imgui 迁出后**（BUILD-F04 S04，−253） | **≈1279** | **1450** | 1545 − 13 − 253 = 1279；1279 × 1.10 = 1406.9 → 1450。**原"刀2 之后数量不变"一档已作废**（imgui 不再是引擎契约，条数直接 −253）；F04 S04 单独口径（未含刀1）为 1292 |
| 刀2 批0（`InputKeys` 46 数据瘦身） | 1233 | **1400** | 1279 − 46 = 1233；1233 × 1.10 = 1356.3 → 1400（< 上一档 1450，棘轮咬合） |
| 刀2 批1–批5 | 缓降 | 每批 = 上批实测 × 1.10 向上取整到 50，且 < 上批预算 | 棘轮逐批咬合；单批下降幅度由该批标注面决定 |
| 刀2 终态（`explicit`） | **≈1220** | **1350** | 994 契约符号 × 1.23（S02 实测类级标注膨胀率 286/1259 = **22.7%**）→ 1222；1220 × 1.10 = 1342 → **1350**，同时满足"`explicit` 终态 ≤ 1400"的目标上限 |

**为什么终态不是 1000 而是 1350（上限 1400）：** 类级 `MINENGINE_API` 会连带导出 vtable/typeinfo（S01 V3 实测 GCC 不导出 inline 成员，但导出 vtable/typeinfo），S02 单次实测就 +286 条（+22.7%）。按 994 × 1.23 ≈ 1222（imgui 迁出后的终态实测预测 ≈1220），预算取"实测 × 1.10 向上取整"= **1350**，仍远低于 65535。

## Failure Model

| # | 失效模式 | 症状（可观测） | 观测点 | 处置/回滚 |
|---|---|---|---|---|
| F1 | 漏标注 | 消费者**链接期** `undefined reference to '_ZN9minEngine...'` | `cmake --build` | 补 1 行标注；或切回 `whitelist` |
| F2 | 数据符号缺 `dllimport` | **启动即失败**：`procedure entry point ... could not be located` / `0xC0000139`；构建期全绿（最危险，S01 V4 已踩过） | 启动 `MaximumEditor.exe` / `minEngineTests.exe` | 给数据符号补 `MINENGINE_API`；必要时 `.def` 加 `DATA` |
| F3 | **inline 引用私有静态** | 运行期读到**另一个 DLL 的副本**（值错误但不崩）或链接期缺符号；只在跨模块路径复现 | C5 清单 + 跨模块 smoke | 把 inline 访问器改成 out-of-line（M5 方案） |
| F4 | imgui 双份上下文 | 两套 `GImGui`：断言/崩溃，位置漂移 | 编辑器启动 + 帧率/布局异常 | **imgui 只在编辑器 DLL 编译一次**（BUILD-F04 S04 后引擎 target 不再 glob imgui 源）；**引擎 DLL 与游戏模块不得出现 imgui 符号**（C2 `--forbid-buckets imgui` 兜底） |
| F5 | glfw 双份全局态 | `glfwInit` 两次、窗口/上下文互相看不到 | 测试里创建隐藏窗口 + 引擎窗口并存 | **用 `glfw3.dll`（单份动态）**，不用两份静态 |
| F6 | 外仓契约漂移 | 本仓构建全绿，外仓升级才炸；且外仓 import 名是 `libminEngine.dll`（P7）⇒ 本仓无法在链接期发现 | `--consumer-dir` 扫 live 外仓产物（C8） | 外仓升级时跑 C8；缺符号 loud 报出 |
| F7 | vtable/typeinfo 双份 | `dynamic_cast` 返回 null、`catch` 不命中、`typeid` 名字不同 | 跨边界 typeinfo 取址断言 | 导出基类 vtable/typeinfo（F02 R4） |
| F8 | 移除测试脚手架断链 | `minEngineTests` 链接失败（`TestWorldSubsystem` 等） | 构建 | 夹具移进 `Tests/` 与移除同批提交 |
| F9 | 棘轮误伤 | 新增 1 个公开 API 就要改预算 → 决策成本 | C7 | 预算按"档位"调整（每批一次），不做逐符号审批 |

**回滚总则：** 刀1/刀2 的状态全部落在 5 个文件 —— `Runtime/Core/EngineAPI.h`、`minEngine/minEngine/CMakeLists.txt`、`cmake/MinEngineExports.cmake`、`cmake/minEngine_exports.def`、`Tests/CMakeLists.txt`；`MINENGINE_EXPORT_MODE` 是唯一总闸。**imgui 迁移（BUILD-F04 S04）不新增回退面**：它落在这 5 个文件中的 2 个（`minEngine/minEngine/CMakeLists.txt` 的源列表、`cmake/minEngine_exports.def` 的 253 行）与 1 个源文件的 3 行删改。

## Verification

### 可自动断言的 oracle

| # | Oracle | 命令 | 阈值 |
|---|---|---|---|
| O1 | 导出数 + 序号余量 | `python scripts/audit_dll_exports.py --max-exports 1700 --min-headroom 60000 --fail` | 退出 0；1545 ≤ 1700；headroom 63990 ≥ 60000 |
| O2 | 消费覆盖（含外仓） | `python scripts/audit_dll_exports.py --verify-coverage --consumer-dir D:/Dev/GitRepo/minCraft-ui/bin --consumer-dir D:/Dev/GitRepo/minCraft-gameplay/bin` | `coverage total missing = 0` |
| O3 | 分环 + 第三方不得被产品消费 | `--ring-report --assert-rings` | ring3 ∩ GAME = 0；四环计数 ≈ {186, 778, 266, 315}（±本批变化）；**imgui 迁出后环3 = 13**（glfw 8 + glad 5），总导出 ≈ **1292**（含刀1 则 1279） |
| O4 | 四类 target 链接 | `cmake --build minEngine/build` | `libminEngined.dll` + `MaximumEditor.exe` + `SampleEditor.exe` + `SampleGame.exe` + `libGameModuleStubd.dll` + `minEngineTests.exe` 全部产出 |
| O5 | 冒烟 | `minEngineTests.exe test smoke` | 退出码 0（`verify.ps1` 已有该门禁） |
| O6 | 注解覆盖（刀2 燃烧） | `--annotation-report --json build/annotation.json` | `missing_annotated = 0`（终态）；逐批必须单调降 |
| O7 | inline 强制清单 | `--assert-inline-forced-manifest` | 未登记项 = 0 |
| O8 | 预算棘轮 | `--assert-budget-ratchet` | 本次预算 ≤ 上次记录 |

### 每刀的最小自动验收集（可直接贴进 `verify.ps1`）

```text
刀1: O1 + O2 + O4 + O5 + C1
刀2: O1 + O2 + O4 + O5 + O6 + O7 + C7（每批）
刀3: 由 BUILD-F04 定义；本文只加约束 C7（棘轮）+ C2（imgui 桶归零）
```

## Deferred

1. 把 `minEngineTests` 从"链接 DLL 的 510 条内部契约"改成"静态链接引擎内部"（P2 的根因；属测试架构，另立 Feature）。
2. `InputKeys` 等 46 条数据符号改 `inline static const` 的**语义**评估（`std::string` 成员带来的每模块副本），本文只列为刀2-批0 的候选。
3. 编辑器 DLL 化、每项目编辑器 exe 的符号契约、以及 **imgui 家族搬迁（BUILD-F04 S04）**——本文不再覆盖。
4. 非 Windows 可见性（`-fvisibility=hidden`）—— F02 已 Deferred；注意 `EngineAPI.h` 的 GCC 分支目前是装饰性的（未开 hidden 时 `visibility("default")` 无约束力）。
5. 稳定 ABI / 版本化导出名 / `find_package` / install export set（F02 Deferred 4）。
6. `Maximum.exe`（陈旧 Playground）与外仓旧产物处置。

## 待主人审批的决策清单

| # | 决策 | 选项 | 推荐 | 一行理由 | 什么会推翻它 |
|---|---|---|---|---|---|
| **Q1** | glfw 落地方案（刀1） | ①引擎与测试都改链 `glfw3.dll` ②静态两份 ③保留 8 条导出 | **①** | 单份动态库消灭"两份 glfw 全局态"（F5），且导出表直接 −8 | 若打包不允许新增 `glfw3.dll`（Staging 需 +1 行复制） |
| **Q2** | glad 落地方案（刀1） | ①`glad.c` 编进 `minEngineTests` ②引擎暴露 `IsGL46()` 之类薄 API ③保留 5 条 | **①** | 7 个测试文件本就自行 `glfwInit+gladLoadGLLoader`，自持 glad 零新增语义 | 若存在依赖"引擎已加载的 GL 指针"的测试（**该前提已被证伪 2026-10-20**：测试调用的 gladLoadGLLoader 正是引擎导出的那一份，因此一直在隐式初始化引擎的 glad 函数表；测试自持 glad 后引擎表全 NULL，material-ir 等直接 SEGFAULT exit 139。刀1 因此额外要求引擎自装载 GL 表：OpenGLFunctionTable.h + 30 处首行调用 + scripts/audit_opengl_load_guard.py 静态门禁。详见 BUILD-F03_KNIFE1_AND_REMOVAL_GUARD_IMPLEMENTATION.md §3） |
| **Q3**（**已裁定**） | imgui 归属（原"契约机制"） | **已裁定：迁进编辑器 DLL**（原选项①"宏覆盖、不迁移"作废；②独立 imgui DLL、③引擎薄封装均不再讨论） | **迁进编辑器 DLL，由 BUILD-F04 S04 执行** | **裁决者：主人（project maintainer）· 日期：2026-10-20**；引擎不该替第三方发牌，且"留在引擎 DLL"与 BUILD-F04 S04 硬冲突 | 已裁定，不再重议 |
| **Q4** | 46 条 `InputKeys` 数据符号 | ①改 `inline static const`（−46 导出） ②保留 | **①** | 常量表无身份需求，改后消除一整类"数据符号缺失 → 启动失败"（F2） | 若需要二进制级 Key 身份稳定（跨模块指针比较） |
| **Q5** | inline 强制导出的治理 | ①显式清单 + C5 断言 ②逐案改 out-of-line ③不管 | **①**（②作为 M2/M5/M7 的定向改造并行） | 把"为什么必须导出"变成可审文件，成本最低且可度量 | 若 out-of-line 改造能在 1 周内清掉 30 条 → 直接②，清单只剩 0 |
| **Q6** | 测试脚手架去留 | ①移出产品（`Runtime/Test` + `SubsystemTestFixtures`） ②保留 | **①** | 38 条 `Test*` + 一个测试运行器不该出现在产品 DLL；且 M1 的转发函数随之消失 | 若产品确实需要内建 `--mode=test`（当前 `main.h:29-38` 支持）→ 保留但改编译期开关 |
| **Q7** | 预算棘轮序列 | ①1700→1650→**1450**→1400→逐批降→**1350**（终态 ≤1400） ②一步到 1400 ③保持 3000 | **①** | 每刀都咬合一次，越界在"下一刀开工前"就暴露，而不是等终态；imgui 迁出后台阶由 5 档变 6 档（原"刀2 数量不变"那档删除） | 若 `explicit` 实测 > 1400（膨胀超预期）→ Q7 重议一次 |
| **Q8** | 外仓契约基线怎么验 | ①`--consumer-dir` 扫 live 外仓 `bin/` ②等外仓重建后用默认扫描 ③忽略 | **①** | 外仓 import 名是 `libminEngine.dll`（P7），默认扫描扫不到；不扫等于放弃 34 条外仓独有契约 | 外仓重建并统一命名回 `libminEngined.dll` 后 → 改回默认 |
| **Q9**（可选） | 510 条"测试独占"契约的归宿 | ①保持现状（登记为 in-repo 私有契约） ②测试改静态链接引擎 ③测试改用 `Testing::TestAccess` 门面 | **①本次不动，登记为 Deferred-1** | 它占导出面 33%，但改造属测试架构，与三刀正交 | 若后续要把预算压到 1000 以下 → 必须做② |

## 附录 A · 可复现测量命令

```text
① 导出表（纯解析，不运行 DLL）
   python scripts/audit_dll_exports.py --report
   → NumberOfFunctions 1545 / maxNamedOrdinal 1545 / headroom 63990
   → composition: minEngine 1278 | imgui 253 | glfw 8 | glad 5 | other 1
   → imgui 253 条由 BUILD-F04 S04 整体移出引擎导出表；迁移后 composition 中不再有 imgui 桶

② 逐消费者导入表（PE32+ ILT thunk 步长 8 字节）
   python scripts/audit_dll_exports.py --verify-coverage \
       --consumer-dir D:/Dev/GitRepo/minCraft-ui/bin
   → minEngineTests 807 / MaximumEditor 579 / SampleEditor 579 / Stub 26 / SampleGame 7
   → 外仓 libminCraft.dll 130·160·144（三个产物）、MinCraft.exe 7（import 名 libminEngine.dll）

③ 名字还原与归属
   c++filt -n < exports.txt            # D:/Dev/mingw64/bin/c++filt

④ 四环判定（本文规则）：见"环定义与判定流程"伪代码
   未解析到声明头的情况：194 条 vtable/typeinfo（"vtable for X" → X）+ 36 条 std 返回类型自由函数

⑤ imgui 253 条构成核对（原刀2 证据 · 原刀2 已作废；现用于 BUILD-F04 S04 的迁移核对）
   grep -n 'define IMGUI_API\|define IMGUI_IMPL_API' <imgui>/imgui.h    # :87-92
   grep -n 'IMGUI_API' <imgui>/ImGuizmo/ImGuizmo.h                        # :110-124
   → 224 + 20 + 9 = 253 = 导出面

⑥ 编辑器调用点
   grep -rc 'ImGui::' minEngine/Editor/src    → 1359
   grep -rn 'gl[A-Z][A-Za-z0-9]*(' minEngine/Editor/src → 0
   grep -rn 'glfw' minEngine/Editor/src       → 2（均为 include，无函数调用）
```

## 附录 B · 本文的测量瑕疵与未完成项（诚实登记）

1. **声明头解析覆盖率不是 100%**：按"头文件名 == 类型名"规则解析到 132 个引擎头；剩余 230 条被消费符号需补两条规则（vtable/typeinfo → 类型的声明头；去掉返回类型后再解析名称）。这不影响四环计数（环由消费者决定），但影响刀2 的"逐头文件燃烧清单"精度。
2. **`.def` 的 124 条 DATA 与本文的"数据符号 164"是不同口径**：前者只含被消费且被写进白名单的数据符号，后者是当前 DLL 导出表的全部数据符号（含 `MINENGINE_API` 带出的）。
3. **外仓源码标识符匹配是启发式**：180 条外仓导入中 140 条能在其 `Source/src/Generated` 文本里找到同名标识符，40 条找不到 ⇒ 判定为"forced（inline/vtable/codegen/main.h 传导）"。文本匹配会漏掉宏/模板间接引用，故该数字是下界（错判方向：可能把 forced 判多）。
4. **未跑构建、未跑 `verify.ps1`**（按任务约束）。因此三刀的"结束仍绿"是**论证**而非实测；刀1/刀2 的实测门槛留给实施者按 O1–O5 验收。
5. **`MINENGINE_API` 95 处 / 33 文件**为本轮实测（F02 记 94/33，差 1 处应为 S02 后的增量），未逐文件核对增量来源。
6. **v2 的计数是算术推导，不是重新实测**：环3 = 13 与总导出 **1292 / 1279** 分别来自 "1545 − 253" 与 "1532 − 253"；1292 这个基准取自 BUILD-F04 S04 的实测口径。F04 S04 落地后必须用 `--ring-report` 复核一次。
