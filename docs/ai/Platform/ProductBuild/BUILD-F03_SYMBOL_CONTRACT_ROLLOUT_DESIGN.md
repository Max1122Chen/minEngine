# BUILD-F03 — 符号契约盘点与三刀 rollout · 把导出面收敛成真契约

## Meta
- **ID:** `BUILD-F03`
- **Type:** Refactor（ABI 契约 / 构建边界）+ 设计盘点
- **Complexity:** `L2`（三刀切片，可分轮实施）
- **Status:** `Done` · **v3**（刀1 + 刀2 批0–5 + 默认 `explicit` + F02 S06；刀3 → F04）
- **Owner:** project maintainer
- **Last updated:** `2026-10-22`（默认 `MINENGINE_EXPORT_MODE=explicit`）
- **Design input:** [BUILD-F02](BUILD-F02_EXPLICIT_EXPORT_MINENGINE_API_DESIGN.md)（三段式状态机，S00–S06 Done）、[BUILD-F02-S01](BUILD-F02_S01_EXPORT_WHITELIST_IMPLEMENTATION.md)、[BUILD-F01](BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)
- **Blocks / 边界:** [BUILD-F04](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md)（编辑器模块边界）—— imgui 由 F04 S04 迁出；本文不展开 F04 方案。
- **本文性质:** 设计 + 进度快照。v1/v2 数字多为迁移前实测；**v3「现状」以 2026-10-22 代码/PE 为准**。刀2 **批准前不改**大批头文件 / `.def` / codegen。

## 变更记录

| 版本 | 日期 | 变更 | 原因 |
|---|---|---|---|
| v1 | 2026-10-20 | 初版：四刀（刀1 glfw+glad 移出 / 刀2 imgui 契约化 / 刀3 引擎面标注 / 刀4 编辑器边界） | — |
| v2 | 2026-10-20 | 原刀2（imgui 留引擎）作废；重编号；imgui→F04；C2/棘轮等 | 主人裁决 + F04 边界 |
| **v3** | **2026-10-22** | **①进度：刀1 + F04 大部落地，剩余主线 = 刀2 烧 `.def`→`explicit`；②刀2 心智模型简化（见下节）；③Q4 InputKeys 改裁定为「类级 `MINENGINE_API` 导出常量表」（已落地，不再推 `inline static`）；④标注范围清单（附录 C）供审批；⑤实施时顺带清死 include / 按调用面收紧，不另开 Feature** | 主人确认范围 OK；会话评审：干净终态 = 只靠 `*_API`，静态身份跨 DLL 单份 |
| v3.1 | 2026-10-22 | 刀2 终：批0–5 + `.def` 空 + 默认 `explicit` + S06 清冗余 linker 旗标；Feature Status → Done | 实测绿；回退闸改为 `whitelist` |

## TL;DR

> **进度（2026-10-22）：** 刀1 Done；刀3/F04 大部 Done；刀2 批0–5 Done（`.def` **0**）+ 默认 **`explicit`**；冗余 `--exclude-libs` 仅留在 `auto`。DLL 导出约 **2720**（第三方桶 = 0）。`.def` 文件可作历史/LEGACY 注释保留，链接不再消费。
>
> **刀2 怎么理解（人话）：** 导出只认标注；定义只活在一个 DLL 里。普通类型 → 要不要给外部用 → 标宏；反射生成 → 跟归属模块注入 API；单例/常量表 → `.cpp` 一份定义 + 宏（勿 `inline` 复制）。先标后删 `.def` 行，删光再切模式。门禁/棘轮是护栏，不是这件工作的本质。
>
> **审批点：** 附录 C 标注范围；Q4（InputKeys 已按导出方案落地）；刀2 批序与 codegen `MODULE_API`。批准前不大批改头文件。

<details>
<summary>v2 TL;DR 原文（迁移前数字，折叠）</summary>

当前（v2 写作时）导出 1545；四环分类；刀序 = 刀1 → 刀2 标注烧 `.def` → 刀3/F04。imgui 不属引擎契约。

</details>

**仍成立的三条技术事实（实施时别忘）：**

1. **inline 摸私有静态会强制导出 DATA**（如 `InputSystem::HasInstance()` → `s_Instance`）——跨 DLL 还可能读到错副本；刀2 遇到就改 out-of-line 或明确导出单份。
2. **imgui / glfw / glad 已不在引擎导出表**（F04 S04 + 刀1）；C1/C2 `--forbid-buckets` 已接 `verify.ps1`。
3. **测试脚手架仍在产品 DLL**（`Test*` / `ForwardToMinEngineTestsExecutable`）——刀2 为烧 `.def` 可先标上；迁出属 Deferred（Q6/Q9），不挡 `explicit`。

---

## 刀2 · 干净终态（v3 简化叙事 · 待审批）

### 目标

| 要什么 | 不要什么 |
|--------|----------|
| 导出唯一真源 = `MINENGINE_API` / 模块 `*_API` | 长期靠 `.def` 白名单顶契约 |
| 单例、键常量表、反射注册表：**每个进程一份定义** | 头文件 `inline static` 在每个 DLL 各造一份 |
| `.def` 行数 → **0**，再切 `MINENGINE_EXPORT_MODE=explicit` | 半标注却先删 `.def`（会链接红） |

### 工作实质（两块 + 一类例外）

1. **普通类型（机械）**  
   附录 C 里「要暴露」的类型：头文件 `class/struct MINENGINE_API …`（或点级标 out-of-line 函数）。默认整类标；特别胖、STL 很重的类可改点级以免导出胀太多（F02 F6）。  
   **实施时可顺带：** 按真实调用面收紧、删死 `#include`——不另开 Feature，记在该批 Progress。

2. **反射生成（唯一非机械块）**  
   codegen 注入 `ME_REFLECTION_MODULE_API`（默认 `MINENGINE_API`）：`StaticClass` / `GetClassName` 跟随类型归属模块；`RegisterClass_Internal` 等汇点必须导出；`GReflectionClassRegister_*` **永不导出**。

3. **身份型静态（例外，规则固定）**  
   定义放 `.cpp`（编进引擎或所属模块）+ 声明带 API 宏。  
   **InputKeys（Q4 已裁定）：** `struct MINENGINE_API InputKeys` + `.cpp` 里 `static const` 定义（已落地）。**不要**改成 `inline static const`（`InputKey` 含 `std::string`，会每模块一份，指针/身份比较不稳）。

### 操作循环（每批）

```text
标宏（声明 = 定义同宏）→ 确认消费者仍能链 → 从 .def 删对应行 → 下一类
全部删光 → 独立目录 -DMINENGINE_EXPORT_MODE=explicit 验证 → 改默认
```

`.def` 与 `dllexport` **并存取并集**（F02 已证）：所以「先标后删」中间态始终可绿。  
**不要**在燃烧中途整表 `--emit-whitelist`（会把已删行补回来）。零消费符号若故意丢掉，再刷 `export_baseline`（删除须进 diff）。

### 批序（可调；批准后按调用面微调）

| 批 | 内容 | 备注 |
|----|------|------|
| 0 | InputKeys | **Done**（导出方案，非 inline 瘦身） |
| 1 | Log / CLI / Path / Serialization | **Done**（2026-10-22）：补 `PathRegistry`/`ApplicationCommandLine`；`.def` −98 |
| 2 | Math / Input（含 `s_Instance` out-of-line） | **Done**（2026-10-22）：`InputSystem`/`InputTrigger*`/`Quaternion`/`Color`/`LinearColor`/`AABB`/`Frustum`/`PointerConstraint*`；`HasInstance()` 改 `.cpp`；`.def` −118（约 781 余） |
| 3 | Reflection + **codegen MODULE_API** | **Done**（2026-10-22）：`ME_REFLECTION_MODULE_API` + CMake `MODULE_API`；`GameModuleStub`→`GAMEMODULESTUB_API`；烧 Reflection/`StaticClass`/汇点 off `.def`；误滤签名提及已补回；`.def` −127 → **654** |
| 4 | Object / Scene / Components / GameplayTag* | **Done**（2026-10-22）：`MEObject`/`ObjectManager`/`GameObject`/`Component*`/`Scene*`/`GameInstance`/`Subsystem*`/`GameplayTag*`/`Engine`/`ProjectManager`；`Engine`/`ProjectManager::HasInstance` 出线；`.def` −156 → **498** |
| 5 | Render / Resource / UI / Audio / Physics / Prefab / Animation | **Done**（2026-10-22）：附录 C-A/B/C 大部标宏；`WindowSystem::HasInstance` 出线；嵌套类型 `LetterboxMapping`/`AssetRegistryBroadcastBatchScope` 补标；`.def` → **0** |
| 终 | `.def` 空 → `explicit` → 清冗余 linker 旗标（F02 S05/S06） | **Done**（2026-10-22）：默认 `explicit`；`--exclude-libs`/`--exclude-modules-for-implib` 仅 `auto` 保留 |

测试夹具（附录 C-C）：本波允许先标以烧 `.def`；迁出产品 DLL 不挡终态。

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

**统计：12 类 / 涉及约 60 条导出符号。** 按处置分三类：**该删/该关（M1、M11）**、**该改造成 out-of-line 消除（M2、M5、M7）**、**该正式申报为契约（M3、M8、M9、M10、M12）**。~~原「InputKeys 改 inline 瘦身」已作废（v3 Q4）~~——常量表保留导出。

外仓用量（只读结论，作为"我们该提供什么"的输入）：外仓 180 条引擎导入中，**140 条在其源码里能找到同名标识符**（直接调用/派生），**40 条找不到**（forced：inline 强制 / vtable / codegen / `main.h` 传导）。那 40 条正是"我们改了就会打断外仓"的高危集合。

## 三刀切片表（原「四刀」；原刀2 已作废，原刀3 → 刀2，原刀4 → 刀3）

约定：**每片结束时 `cmake --build minEngine/build` 全绿 + `audit_dll_exports.py --fail` 通过**；"档位"指 F02 的工作量档（A=0.5d / B=1d / C=2–3d）。

> **编号变更与边界（v2）：** ①**原刀2（给 imgui 补导出宏、留在引擎 DLL）已作废**，理由见「变更记录」——imgui 迁进编辑器 DLL，由 [BUILD-F04](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md) 的 **S04** 执行；②原刀3 → **刀2**，原刀4 → **刀3**；③**imgui 家族的归属由 BUILD-F04 负责，两个 Feature 的边界不重叠**——本文对 imgui 只保留一条约束：引擎导出中不得出现 imgui 桶（C2）。

| 刀 | 做什么 | 结束时构建为何仍绿（论证） | 验收 oracle（可自动断言） | 回退方式 | 档位 |
|---|---|---|---|---|---|
| **刀1 · glfw+glad 移出导出表** | （设计原文略） | — | C1 等 | — | A · **Done**（含引擎自装载 GL 表；见 [Impl](BUILD-F03_KNIFE1_AND_REMOVAL_GUARD_IMPLEMENTATION.md)） |
| **刀2（原刀3）· 标注 + 烧 `.def` → `explicit`** | 见上文「刀2 · 干净终态」：批 0 InputKeys **Done（导出）**；批 1–5 机械标宏 + 批 3 codegen `MODULE_API`；顺带清死 include；`.def`→0 后切 `explicit`。**范围清单 = 附录 C（待主人确认）** | 注解 ∪ `.def` 并存；先标后删 | 每批：四类 target 链接 + `--verify-coverage`；终态 `explicit` 且 `.def` 不用 | 单批 revert；总闸改回 `whitelist` | C · **待审批后开工** |
| **刀3（原刀4）· 编辑器模块边界** | 见 BUILD-F04 | — | C2 imgui∅ | — | — · **大部 Done**（S01–S05）；余量不挡刀2 |

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
| ~~刀2 批0 InputKeys inline 瘦身 −46~~ | — | — | **v3 作废**（Q4 改导出；批0 不降条数） |
| 刀2 批1–批5 | 缓降或微增 | 每批按实测调；类级标注可能短时抬升 | 以「能绿 + `.def` 行↓」为主；棘轮勿误伤正当公开 API |
| 刀2 终态（`explicit`） | **≈1200–1400** | **≤1400 量级** | 类级标注约 +23% vtable/typeinfo（S02 实测）；以切 `explicit` 后实测为准重标定 |

**v3 注：** 预算棘轮仍有用，但**不是刀2 开工前提**；主人选择先把契约标干净。终态数字以烧完 `.def` 后一次 `--report` 重算。

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

1. 把 `minEngineTests` 从"链接 DLL 的 510 条内部契约"改成"静态链接引擎内部"（P2；另立 Feature）。刀2 **不**为此阻塞。
2. ~~InputKeys → inline static~~ — **Cancelled（v3 Q4）**；保留导出。
3. BUILD-F04 余量（若有）——不挡刀2。
4. 非 Windows `-fvisibility=hidden`（F02 Deferred）。
5. 稳定 ABI / `find_package` / install export set。
6. `Maximum.exe` 陈旧产物；外仓旧 imgui import 过渡例外自退役。
7. 注解覆盖率 / inline-forced 清单等机械门禁增强——有则更好；**不作为刀2 开工前提**（主人选择先把契约标干净）。

## 待主人审批的决策清单

| # | 决策 | 选项 | 推荐 | 一行理由 | 什么会推翻它 |
|---|---|---|---|---|---|
| **Q1** | glfw 落地方案（刀1） | ①引擎与测试都改链 `glfw3.dll` ②静态两份 ③保留 8 条导出 | **①** | 单份动态库消灭"两份 glfw 全局态"（F5），且导出表直接 −8 | 若打包不允许新增 `glfw3.dll`（Staging 需 +1 行复制） |
| **Q2** | glad 落地方案（刀1） | ①`glad.c` 编进 `minEngineTests` ②引擎暴露 `IsGL46()` 之类薄 API ③保留 5 条 | **①** | 7 个测试文件本就自行 `glfwInit+gladLoadGLLoader`，自持 glad 零新增语义 | 若存在依赖"引擎已加载的 GL 指针"的测试（**该前提已被证伪 2026-10-20**：测试调用的 gladLoadGLLoader 正是引擎导出的那一份，因此一直在隐式初始化引擎的 glad 函数表；测试自持 glad 后引擎表全 NULL，material-ir 等直接 SEGFAULT exit 139。刀1 因此额外要求引擎自装载 GL 表：OpenGLFunctionTable.h + 30 处首行调用 + scripts/audit_opengl_load_guard.py 静态门禁。详见 BUILD-F03_KNIFE1_AND_REMOVAL_GUARD_IMPLEMENTATION.md §3） |
| **Q3**（**已裁定**） | imgui 归属（原"契约机制"） | **已裁定：迁进编辑器 DLL**（原选项①"宏覆盖、不迁移"作废；②独立 imgui DLL、③引擎薄封装均不再讨论） | **迁进编辑器 DLL，由 BUILD-F04 S04 执行** | **裁决者：主人（project maintainer）· 日期：2026-10-20**；引擎不该替第三方发牌，且"留在引擎 DLL"与 BUILD-F04 S04 硬冲突 | 已裁定，不再重议 |
| **Q4**（**已裁定 · v3**） | `InputKeys` 常量表 | ①`inline static const`（−导出） ②类级 `MINENGINE_API` + `.cpp` 定义（跨 DLL 单份） | **②** | Key 身份要跨模块稳定；`InputKey` 含 `std::string`，inline 会每模块一份 | **裁决：主人认可导出方案 · 2026-10-22 已落地**（`InputKeys.h` + `audit_input_key_exports.py`）；①作废 |
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

## 附录 B · 测量瑕疵与未完成项（诚实登记）

1. 声明头按「文件名 == 类型名」解析不完整——实施时以调用关系 / `.def` 行为准，附录 C 可增删。
2. v1/v2 四环数字是迁移前快照；**v3 现状以 2026-10-22 `audit_dll_exports.py --report` 为准**（约 1400 导出、第三方桶 0、`.def` ~997）。
3. 外仓「源码命中 vs forced import」仍是启发式。
4. 刀1「结束仍绿」已实测；刀2 的「结束仍绿」仍是论证，批准后按批验证。

## 附录 C · 刀2 标注范围清单（2026-10-22 · 待主人审批）

来源：当前 `minEngine_exports.def` 消费面归类。主人已口头确认「范围没什么问题」；实施时再按调用关系微调，并可清死 include。

### C-A · 建议公开（游戏模块 / 外仓 / 编辑器都会碰到）

- 框架：`Engine` · `GameObject` · `Component` · `SceneComponent` · `PrimitiveComponent` · `MEObject` · `ObjectManager` · `ObjectCloneContext` · `Scene` · `SceneManager` · `SceneDuplicator` · `GameInstance` · `SubsystemCollectionBase` · `WorldSubsystem` · `ProjectManager` · `PathRegistry` · `ApplicationCommandLine`
- 组件：Camera / Light* / StaticMesh / SkeletalMesh / Sprite / Movement / Input / Lua / Audio* / Widget·Image·Text·Button·Canvas·Drag* / Collider*·RigidBody / `GameplayEventSystemComponent`
- Tag：`GameplayTag` · `GameplayTagContainer` · `GameplayTagManager` · `NativeGameplayTag`
- 资源：`AssetManager` · `AssetTypeRegistry` · `DataAsset` · `AudioClip` · `Texture2D`（及确需外露的 loader/importer）
- Prefab：`Prefab` · `PrefabUtility` · `PrefabOverrideUtility` · `PrefabPropertyPath`
- 输入：`InputSystem` · `InputTrigger*`（`InputKeys` **已标**）
- 系统：`WindowSystem` · `ParameterStore`/`Schema`/`Layout` · `AudioSystem` · `AudioMixer` · `PhysicsSystem` · `PhysicsWorld` · `CollisionChannelRegistry` · `RenderSystem`
- 材质/网格扩展面：`Material` · `MaterialInstance` · `MaterialInstanceDynamic` · `MaterialInterface` · `StaticMesh` · `Sprite`/`SpriteAtlas` · `MeshBatchCollector` · `PrimitiveSceneProxy` · `RHIBackendSelection` · `RHIClipSpace`
- 动画：`AnimationClip` · `AnimationGraph`/`Instance` · `AnimationPlayer` · `Skeleton` · `Pose`
- 数学：`Quaternion` · `Color`/`LinearColor`（若跨 DLL 传值需要）
- 反射运行时：`ReflectionSystem` + 属性/函数类型（多数已有宏）+ **codegen A/B/D**

### C-B · 工具 / 编辑器契约（可标；不冒充玩法 API）

- `MaterialEdGraph*` · `MaterialGraphNodeDef*` · `MaterialCompiler` · `ShaderCompiler`
- `SpriteAtlasPacker` · `FontGlyphAtlas` · `TextMeshBuilder` · `FileDialogService`
- `EditorFilesystemMutationPass` · `UISystem` · `UILayoutPass` · `ScreenUI*` · `SceneViewport`
- `OpenGLRHI` / `VulkanRHI` / `RenderGraph` / `RenderPass`（测试与编辑器重）

### C-C · 测试脚手架（先标以烧 `.def`，或本波末再处理）

- `Test*` fixtures · `TestRunner` · `ForwardToMinEngineTestsExecutable` —— 迁出产品 DLL = Deferred

### C-D · 已有类级/点级宏（燃烧时主要删 `.def`）

- Log / 部分 Serialization / 部分 Reflection / `InputKeys` / `LuaScriptSystem` / `LuaBindProbe` 等（以头文件为准）

---

## 待主人审批（刀2 开工闸）

请确认或改正下列项；**全部 OK 前不大批改头文件 / `.def` / codegen**：

| # | 项 | 设计默认 |
|---|-----|----------|
| 1 | 附录 **C-A** 整类/点级标 `MINENGINE_API` | 接受；实施按调用面可微调 |
| 2 | 附录 **C-B** 工具面一并标（烧 `.def`） | 接受；不冒充玩法教程 API |
| 3 | 附录 **C-C** 测试夹具本波先标 | 接受；迁出 Deferred |
| 4 | **Q4** InputKeys 保持导出（已落地） | 接受；禁止改回 inline |
| 5 | 批 3 做反射 **codegen `MODULE_API`**，再切 `explicit` | 必须 |
| 6 | 实施顺带清死 include / 按调用收紧 | 允许，记 Progress |
| 7 | 不升 Primary；与 ASSET-F10 并行、批前可停 | 是 |

**批准口令示例：** 「F03 刀2 按 v3 批」或逐条改正上表。
