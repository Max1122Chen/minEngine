# BUILD-F03 · 刀 1 实现记录 —— glad 移出导出表 + 「删符号即失败」删除守卫

## Meta
- **ID:** `BUILD-F03-KNIFE1`（刀 1 收口）+ `BUILD-F03-REMOVAL-GUARD`（删符号守卫）
- **类型:** 构建 / ABI 契约（导出表构成）+ 引擎 GL 装载契约 + 审计脚本
- **状态:** 已落地，`verify.ps1` 退出码 0
- **设计输入:** [BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md](BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md) 刀 1（glfw+glad 移出导出表）、Q2、C1
- **实施范围:** `Tests/CMakeLists.txt`、`minEngine_exports.def` 5 行、OpenGL 后端 2 个实现文件 + 1 个新装载点、2 个审计脚本、`verify.ps1`、基线文件；**零业务语义改动**

---

## 0 · 结论摘要

| 指标 | 改前 | 改后 | 结论 |
|---|---|---|---|
| 引擎导出 `NumberOfFunctions` | **1288** | **1283** | glfw 0 / glad 0 / imgui 0 |
| 桶构成 | minEngine 1282 + glad 5 + other 1 | **minEngine 1282 + other 1** | 见 §1.3 |
| `.def` 条目 | 1003（998 契约 + 5 legacy） | **998（993 + 5 legacy）** | legacy 区块逐字未动 |
| `--forbid-buckets glfw,glad` | — | **PASS** | 恒 ∅（C1） |
| `minEngineTests.exe test smoke` | 绿 | **绿（退出码 0）** | 测试自带 glad TU |
| 7 个 GL 套件（各自建 context） | 绿 | **绿（7/7 退出码 0）** | 见 §2 |
| 删除守卫 `--assert-no-removals-vs` | 不存在 | **上线 + 基线 1283 条** | verify.ps1 已接 |
| `verify.ps1` | 绿 | **绿（exit 0）** | 见 §5 |

**一句话：** glad 的 5 条导出被移除，代价是「引擎必须自己装载 glad 表」——这条代价在原设计里被记为"测试不依赖引擎已加载的 GL 指针（实测 0 处）"，实测**是错的**，见 §2；同时补上"删符号即失败"的机械守卫，使外仓下一次链接失败不再可能从 verify 里溜过去。

---

## 1 · 任务 A：glad 5 条移出引擎导出表

### 1.1 安全闸（动手前）

`--report` + `classify()` 逐消费者统计 glad 桶 import：

```text
engine glad-bucket exports: 5
   GLAD_GL_VERSION_4_6
   gladLoadGLLoader
   glad_glBindTexture
   glad_glGetError
   glad_glGetTexImage
consumers scanned: 16 over ['minEngine\bin', '..\minCraft\bin', '..\minCraft-ui\bin', '..\minCraft-gameplay\bin']
  clean         minEngine/bin/MaximumEditor.exe                0 glad imports (imports 3)
  clean         minEngine/bin/SampleEditor.exe                 0 glad imports (imports 3)
  clean         minEngine/bin/SampleGame.exe                   0 glad imports (imports 7)
  clean         minEngine/bin/libGameModuleStubd.dll           0 glad imports (imports 26)
  clean         minEngine/bin/libMaximumEditord.dll            0 glad imports (imports 324)
  clean         minEngine/bin/libminCraftd.dll                 0 glad imports (imports 153)
  GLAD-CONSUMER minEngine/bin/minEngineTests.exe               5 glad imports: GLAD_GL_VERSION_4_6, gladLoadGLLoader, glad_glBindTexture, glad_glGetError, glad_glGetTexImage
  clean         ../minCraft/bin/MinCraftEditor.exe             0 glad imports (imports 573)
  clean         ../minCraft/bin/VoxelWorldTests.exe            0 glad imports (imports 24)
  clean         ../minCraft/bin/libminCraft.dll                0 glad imports (imports 130)
  clean         ../minCraft-ui/bin/MaximumEditor.exe           0 glad imports (imports 578)
  clean         ../minCraft-ui/bin/MinCraft.exe                0 glad imports (imports 7)
  clean         ../minCraft-ui/bin/MinCraftEditor.exe          0 glad imports (imports 578)
  clean         ../minCraft-ui/bin/VoxelWorldTests.exe         0 glad imports (imports 24)
  clean         ../minCraft-ui/bin/libminCraft.dll             0 glad imports (imports 144)
  clean         ../minCraft-gameplay/bin/libminCraft.dll       0 glad imports (imports 160)
consumers importing glad names: 1
```

**唯一消费者 = `minEngineTests.exe`**；编辑器 DLL 自身（`libMaximumEditord.dll`，imgui 走 imgl3w 而非 glad）与三个外仓均为 0。安全闸通过。

### 1.2 改动

1. `minEngine/Tests/CMakeLists.txt`：`minEngineTests` 自己编译 `Third-Party/glad/src/glad.c`（`MINENGINE_TEST_GLAD_SOURCE`）。7 个套件本来就各自 `glfwInit()+gladLoadGLLoader()`（`MaterialCompileCacheTest`/`MaterialIRTest`/`RenderGraphTest`/`ShaderCompilerTest`/`SpriteAtlasTest`/`SpriteAuthoringTest`/`SpriteRuntimeConsumeTest`），因此"测试自带 glad"不是新增语义。
2. `minEngine/cmake/minEngine_exports.def`：删 `GLAD_GL_VERSION_4_6 DATA`、`gladLoadGLLoader`、`glad_glBindTexture DATA`、`glad_glGetError DATA`、`glad_glGetTexImage DATA` 共 5 行；`LEGACY-EXTERNAL-DEBT` 区块逐字保留（5 条仍在）；头部条目计数注释同步改成 998 / 1283 / 120 DATA。
3. 引擎**仍然编译** `glad.c`（`GLFWWindowSystem.cpp` 用它的表装载 GL），只是不再**导出**——这两件事在刀 1 里必须分开看，见 §2。

### 1.3 实测

```text
NumberOfFunctions ... 1283   (NumberOfNames 1283)
maxNamedOrdinal ... 1283   PE limit 65535   headroom 64252
composition:
  minEngine          1282
  other                 1

PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000
PASS: export table contains no entry of bucket(s) glad,glfw
```

`minEngineTests.exe` 的 glad import 从 5 → **0**（现在 import 794 条，全部来自引擎契约）。

---

## 2 · 实施中发现的硬约束（V1）：glad 的函数指针**每个模块一份**

### 2.1 原设计假设

BUILD-F03 设计文档 Q2 选方案①（`glad.c` 编进 `minEngineTests`），风险栏写的是：

> 若存在依赖"引擎已加载的 GL 指针"的测试（**实测 0 处**）

**这个"实测 0 处"是错的。** 错的不是数字算错，而是**测量对象选错**：测试自己调用 `gladLoadGLLoader` 确实是 7/7，但在刀 1 之前，那次调用 `import` 的是**引擎 DLL 的** `gladLoadGLLoader`，所以它顺手把**引擎那份表**也装好了。测试自身"调了 loader"与"引擎表被装好"在刀 1 前是同一件事，无法用"测试有没有调 loader"区分。

### 2.2 错误假设的实际行为（本仓实测）

刀 1 只做 §1.2 的两步、不给引擎任何装载点时：

```text
$ ./minEngineTests.exe test smoke
Segmentation fault            (exit 139)   <-- 崩在 material-ir
```

gdb 栈（关键帧）：

```text
#0  0x0000000000000000 in ?? ()                       <-- glad 表里的空指针
#3  minEngine::(anonymous namespace)::VerifySmokeGpuCompile (...) at MaterialIRTest.cpp:228
#2  minEngine::EngineShaderUtils::TryCompileSourcesOnGpu (...) at EngineShaderUtils.cpp:277
```

`TryCompileSourcesOnGpu` 里 `OpenGLRHIShader` 的构造走 `glCreateShader` → 引擎表全 NULL → 调 0 地址。

**这就是"隐式跨模块初始化依赖"**：模块 A（引擎 DLL）的全局状态被模块 B（宿主/测试）的一次调用顺带初始化。它与 BUILD-F04 S04 的 `GImGui` 双实例是同一类风险（imgui 的全局状态每模块一份，谁使用谁必须自己初始化），处理原则一致：**谁使用，谁负责初始化自己那份**。

### 2.3 修法：引擎自己装载自己的表（不上新 DLL）

新增 `Runtime/Function/Render/OpenGL/OpenGLFunctionTable.{h,cpp}`：

```cpp
bool EnsureOpenGLFunctionTableLoaded();   // 幂等；无当前 context 时 no-op(false)；主线程 only
```

头文件即契约：**引擎侧任何使用 glad 全局表（任何 `gl*` 调用）的代码，必须先经过它**。

**装载点怎么定（本仓实证，不靠推测）：**

先按最小假设只放 3 处（`OpenGLRHI::Initialize()` + 两个 `OpenGLRHIShader` 构造），实测 7 个 GL 套件：

```text
material-compile-cache EXIT=139     render-graph EXIT=139     sprite-atlas EXIT=139
sprite-authoring EXIT=139           sprite-runtime-consume EXIT=139
material-ir EXIT=0                  shader-compiler EXIT=0
```

gdb 栈证明这些套件**根本不经过 `Initialize()`**，它们直接构造 RHI 对象/资源：

```text
#0  0x0000000000000000 in ?? ()
#1  minEngine::(anonymous namespace)::UploadTexture2D (...) at OpenGLRHIResources.cpp:160
#2  minEngine::OpenGLRHITexture::OpenGLRHITexture (...) at OpenGLRHIResources.cpp:554
#10 minEngine::OpenGLRHI::RHICreateTexture2D (...) at OpenGLRHI.cpp:139
#11 minEngine::RenderGraph::SetupAttachments (...) at RenderGraph.cpp:543
#12 DOCTEST_ANON_FUNC_2 () at RenderGraphTest.cpp:314          <-- OpenGLRHI rhi; 直接 RHICreateTexture2D
```

也就是说：**"RHI 结构性入口"不存在**。`OpenGLRHI` 的每个 GL 命令方法、每个 GL 资源构造，都可以在没有 `Initialize()` 的情况下成为该进程里第一次触 GL 的入口（测试就是这么用的，而且这是合法的）。

**因此装载点 = OpenGL 后端里所有直接触 GL 的函数**（在首个 `gl*` 调用之前），共 30 个函数 / 2 个文件：`OpenGLRHI.cpp` 12 处（含 `Initialize`），`OpenGLRHIResources.cpp` 18 处。逐个撒是因为**没有更粗的结构性入口**，而不是偷懒：上面两条实测就是为了证明这一点，而不是把 30 处当成默认答案。

### 2.4 把它变成机械不变量（而不是"靠纪律"）

30 处注解的坏处是：以后谁加一条 `gl*` 调用忘了注解，就是只在特定路径上崩的空指针。所以同批上线静态审计 `scripts/audit_opengl_load_guard.py`：

```text
RULE 1  引擎 OpenGL 后端的每个函数：若含 gl* 调用，则首个 gl* 之前必须有 EnsureOpenGLFunctionTableLoaded()
RULE 2  引擎源码里除白名单外不得出现 gl* 调用（白名单只有 GLFWWindowSystem.cpp，因为它建窗时自己 gladLoadGLLoader）
RULE 3  扫到 0 个文件 / 0 个函数 = 失败（空扫描不算通过）
```

实测：

```text
$ python scripts/audit_opengl_load_guard.py
PASS: 30 function(s) in 2 GL backend file(s) call EnsureOpenGLFunctionTableLoaded() before using glad
PASS: no engine source outside the allowlist calls a gl* function (3 GL file(s) total)
```

去掉其中一个注解（`LinkProgram`）后立刻变红：

```text
$ python scripts/audit_opengl_load_guard.py
FAIL: engine GL function-table load invariant violated (1 finding(s)):
  minEngine/minEngine/src/Runtime/Function/Render/OpenGL/OpenGLRHIResources.cpp:724: OpenGLRHIShader::LinkProgram calls glCreateProgram without EnsureOpenGLFunctionTableLoaded() - a null function pointer on the paths that reach it first
```

已接入 `verify.ps1`（§5），所以"以后新增 GL 调用"这条不再是人的记忆问题，而是门禁问题。

### 2.5 反向验证（摘掉装载点 → 必须重新崩）

把 `EnsureOpenGLFunctionTableLoaded()` 的装载体摘掉（调用点全部保留，只让函数直接 `return false`），重建后：

```text
material-ir       EXIT=139
sprite-authoring  EXIT=139
[18:16:26] [Test] MaterialIR pin type connection checks PASSED.      <-- 崩在同一行之后
```

恢复装载体、重建后：

```text
material-ir       EXIT=0
sprite-authoring  EXIT=0
smoke             EXIT=0
```

即"红-绿"由装载点本身控制，不依赖任何其它改动。

### 2.6 被否掉的方案（记录，避免下一轮重走）

| 方案 | 否掉原因 |
|---|---|
| 新增随行 DLL `libglad.dll`（glad 编成共享库，引擎 import 它） | 每个加载 `libminEngine.dll` 的宿主（含三个外仓产品 bin）都必须额外分发一个新 DLL，缺了就是"引擎 DLL 装载失败"，引入新的部署失败模式；而 glad 只需一份表这一收益，用 30 行注解 + 静态门禁即可得到 |
| 由测试 fixture 预先创建引擎窗口（引擎表顺带装好） | 改变全部套件的 context 生命周期语义；任一套件 `glfwTerminate()` 就会让后续套件失效；且测试自己的表仍未被装载 |
| 让引擎继续导出 `gladLoadGLLoader` 一条 | 违反 C1（第三方 C 符号不得出现在导出表），且仍然是"消费者的调用初始化引擎状态"这一隐式依赖 |

---

## 3 · 任务 B：「删符号即失败」的基线守卫

### 3.1 问题

刀 1 之前的断言只保证**不超预算、不出现第三方桶**——它们对"某个符号**消失**"完全无感。符号消失正是外仓下一次链接失败的原因，而这恰恰是让外仓能稳定依赖引擎的关键机制。

### 3.2 新增两个命令（`scripts/audit_dll_exports.py`）

```text
--write-export-baseline PATH     把当前 DLL 的导出符号集合写成 JSON（schema / contract /
                                 engine_version / 生成时间 / export_count / data_count / symbols[]）
--assert-no-removals-vs PATH     读基线：基线里有、当前 DLL 没有的符号 → 全部列出并退出码 1
```

**有意删除的路径只有一条：重新生成基线。** 删除因此出现在 commit diff 里（评审可见）。**没有**"允许删除清单"这类旁路参数——旁路参数会把守卫变成装饰。

**防假绿**：基线缺失 / 基线不可读 / 基线 `symbols` 为空 / 基线属于另一个 contract / DLL 缺失 → 一律失败（退出码 2，"无法验证"≠"通过"）。

### 3.3 基线文件

`minEngine/cmake/minEngine_export_baseline.json`（任务 A 完成后的 DLL 上生成）：

```json
{
  "schema": 1,
  "contract": "libminengine",
  "engine_version": "0.0.9",
  "generated_at": "2026-10-20T10:14:31Z",
  "export_count": 1283,
  "data_count": 159,
  "symbols": [ ... 1283 条，排序 ... ]
}
```

```text
$ python scripts/audit_dll_exports.py --assert-no-removals-vs minEngine/cmake/minEngine_export_baseline.json
baseline ....... minEngine/cmake/minEngine_export_baseline.json
  frozen ....... 1283 symbols (engine 0.0.9, written 2026-10-20T10:14:31Z, schema 1)
  current DLL .. ...\minEngine\bin\libminEngined.dll (1283 exports)
PASS: all 1283 baseline symbols are still exported by libminEngined.dll
```

### 3.4 反向验证（假符号 → 必红）

基线里塞一个不存在于 DLL 的符号 `_ZN9minEngine16FakeRemovedGuardEv`：

```text
baseline ....... minEngine/cmake/minEngine_export_baseline.json
  frozen ....... 1284 symbols (engine 0.0.9, written 2026-10-20T10:14:31Z, schema 1)
  current DLL .. ...\minEngine\bin\libminEngined.dll (1283 exports)
FAIL: 1 symbol(s) recorded in the baseline are no longer exported by ...\libminEngined.dll:
  removed: _ZN9minEngine16FakeRemovedGuardEv
      If the removal is intended, refresh the baseline with --write-export-baseline minEngine/cmake/minEngine_export_baseline.json: the deletion then appears in the commit diff, which is the review step. There is no allow-removal flag.
rc=1
```

还原后转绿（`PASS: all 1283 baseline symbols ...`，rc=0）。

### 3.5 防假绿三条（逐条实测）

```text
B1 基线文件缺失
$ --assert-no-removals-vs minEngine/cmake/does_not_exist_baseline.json
FAIL: export baseline not found: minEngine/cmake/does_not_exist_baseline.json
      nothing was compared; regenerate it with --write-export-baseline ...
rc=2

B2 DLL 缺失
$ --dll minEngine/bin/no_such_engine.dll --assert-no-removals-vs <baseline>
error: DLL not found: minEngine/bin/no_such_engine.dll
rc=2

B3 基线为空
$ --assert-no-removals-vs minEngine/cmake/_tmp_empty_baseline.json
FAIL: export baseline ... holds 0 symbols - an empty baseline passes every removal check by vacuity
rc=2
```

附赠一条（contract 错配，防止拿编辑器 DLL 的基线去量引擎）：

```text
FAIL: export baseline ... is for contract 'libmaximumeditor', but the DLL belongs to 'libminengine'
rc=2
```

---

## 4 · `verify.ps1` 接入

顺序（新增两段在两个 `--verify-coverage` 之外）：

1. 预算 + `--forbid-buckets imgui,glfw,glad`（本轮把 `glad` 加进禁令）
2. **`--assert-no-removals-vs minEngine/cmake/minEngine_export_baseline.json`**（预算断言之后）
3. in-repo 覆盖（0 容错）
4. **`audit_opengl_load_guard.py`**（引擎 GL 装载不变量）
5. 编辑器/模块边界、module gate matrix、外仓覆盖（imgui 过渡例外不变）
6. `minEngineTests.exe test smoke`

---

## 5 · 验证证据

```text
$ powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1
PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000
PASS: export table contains no entry of bucket(s) glad,glfw,imgui
baseline ....... ...\minEngine\cmake\minEngine_export_baseline.json
  frozen ....... 1283 symbols (engine 0.0.9, written 2026-10-20T10:14:31Z, schema 1)
PASS: all 1283 baseline symbols are still exported by libminEngined.dll
coverage total missing = 0
coverage in-repo missing = 0 (never tolerated)
PASS: 30 function(s) in 2 GL backend file(s) call EnsureOpenGLFunctionTableLoaded() before using glad
PASS: editor/module boundary holds
coverage total missing = 0            (libmaximumeditord.dll)
PASS identity    both hosts import the identity setter
coverage total missing = 759
coverage unexpected missing = 0 (anything outside the tolerated bucket(s))
WARN TRANSITIONAL EXCEPTION: 759 missing symbols tolerated in bucket 'imgui' - external editor hosts are pre-S04 artifacts and must be rebuilt against a post-S04 engine
verify: minEngineTests.exe test smoke ...
[doctest] Status: SUCCESS!
[18:15:23] [Test] TestRunner: suite 'material-ir' PASSED.
VERIFY_EXIT=0
```

外仓覆盖与改前一致：759 条 imgui 过渡例外（自退役设计不变），**unexpected missing = 0**。

7 个 GL 套件（全量守卫之后）：

```text
material-compile-cache EXIT=0   material-ir EXIT=0   render-graph EXIT=0   shader-compiler EXIT=0
sprite-atlas EXIT=0             sprite-authoring EXIT=0                   sprite-runtime-consume EXIT=0
```

---

## 6 · 偏差 / 未完成项

1. **偏差（已由主控裁定继续）**：刀 1 的"glad"半边在原设计里只需要"测试自带 glad.c"；实测证明还必须给引擎加自装载点 + 静态门禁（§2）。这是设计 Q2 的前提错误导致的必要增量，不是范围膨胀。
2. **未做（明确不做）**：不新增 `libglad.dll`；不改 `EngineShaderUtils.*`/`ShaderCompiler.*`/`RenderSystem.cpp`/`RHI.h` 等禁改文件；不动 `LEGACY-EXTERNAL-DEBT` 区块；不动 `docs/ai/FEATURE_REGISTRY.md`、`bugs/*`、`BUILD-F04_*`、`CORE-F38_*`。
3. **留给设计文档的修正（主控负责）**：`BUILD-F03` Q2 风险栏"实测 0 处"需改写为本 §2 的结论；C1 的判定现在同时由 `--forbid-buckets`（不得出现）与 `--assert-no-removals-vs`（不得消失）两条断言共同守护。
4. **待办（后续刀）**：`OpenGLRHIResources.cpp`/`OpenGLRHI.cpp` 里 30 处"函数首行注解"是同构重复，若未来该后端改成"统一 GL 调用包装层"，可收敛为 1 处；当下选择"30 处 + 机器门禁"是因为它能被静态检查证明，而包装层需要一次真实重构。

---

## 7 · 文件清单

| 文件 | 变更 |
|---|---|
| `minEngine/Tests/CMakeLists.txt` | + glad.c 源文件（测试自带 glad） |
| `minEngine/cmake/minEngine_exports.def` | −5 条 glad；头部计数注释同步（legacy 区块逐字保留） |
| `minEngine/cmake/minEngine_export_baseline.json` | 新增（1283 条符号基线） |
| `minEngine/minEngine/src/Runtime/Function/Render/OpenGL/OpenGLFunctionTable.h` | 新增（装载契约） |
| `minEngine/minEngine/src/Runtime/Function/Render/OpenGL/OpenGLFunctionTable.cpp` | 新增（幂等装载） |
| `minEngine/minEngine/src/Runtime/Function/Render/OpenGL/OpenGLRHI.cpp` | +12 处装载点 |
| `minEngine/minEngine/src/Runtime/Function/Render/OpenGL/OpenGLRHIResources.cpp` | +18 处装载点 |
| `scripts/audit_dll_exports.py` | + `--write-export-baseline` / `--assert-no-removals-vs` |
| `scripts/audit_opengl_load_guard.py` | 新增（GL 装载不变量门禁） |
| `scripts/verify.ps1` | + glad 禁令 / + 基线守卫 / + 装载门禁 |
