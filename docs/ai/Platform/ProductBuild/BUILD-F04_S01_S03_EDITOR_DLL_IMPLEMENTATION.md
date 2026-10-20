# BUILD-F04 · S01–S03 实现记录 —— 编辑器 DLL 边界

## Meta
- **ID:** `BUILD-F04-S01/S02/S03`
- **类型:** 构建 / 模块边界（编辑器 DLL 边界，不改任何编辑器功能）
- **状态:** 已落地。S02 = `MaximumEditorLib` SHARED；S03 = 编辑器导出 `explicit`；S01 的边界断言已接入 `verify.ps1`
- **设计输入:** [BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md](BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md)（§3 目标形态 · §6 切片 S01–S03 · §8 O1–O10 · §12 决策 Q1–Q7）
- **本轮主人裁定:** Q1 编辑器定位为"资产-only 项目编辑器"（S05 执行）· Q2 imgui 迁进编辑器 DLL（S04 执行）· Q5 target 名保留 `MaximumEditorLib` · Q6 只批 S01–S03
- **实施范围:** 构建配置 + 2 个导出宏 + 1 个入口标注 + 2 个脚本 + 1 个基线清单；**零编辑器功能改动**
- **明确不做:** S04（imgui 迁移）· S05（资产-only 语义）· S06（产品化 / 打包挂点）

---

## 0 · 结论摘要

| 指标 | 改前 | 改后 | 结论 |
|---|---|---|---|
| `MaximumEditor.exe` | 188,033,796 B | **323,536 B** | ↓ 99.83% |
| `SampleEditor.exe` | 188,034,182 B | **323,922 B** | ↓ 99.83% |
| 编辑器代码副本 | 2 份（每 exe 一份 STATIC） | **1 份** `libMaximumEditord.dll` 187,973,485 B | `N` 个项目 = 1 份 DLL + N × 0.32 MB |
| 两个编辑器合计占用 | 376,067,978 B | **188,620,943 B** | ↓ 49.84%（N=2；N 越大越赚） |
| 编辑器 exe 的引擎导入 | 579（imgui **253**） | **3**（imgui **0**） | exe 不再直接碰 imgui |
| 编辑器 exe 从编辑器 DLL 导入 | — | **1**（`CreateApplication`） | 边界只有一个入口符号 |
| 编辑器 DLL 导出条目 | 无此产物 | **1**（`explicit`；whitelist 也是 1） | O6 ✅（预算 8） |
| 编辑器 DLL 序号余量 | — | 65,534 | 无炸弹风险 |
| 引擎 DLL 导出条目 | **1549** | **1549** | **未增加**（硬约束） |
| 引擎 DLL imgui 导出 | 253 | **253** | 未恶化（S04 才归零，F03 C2 现状保持） |
| 引擎序号余量 | 63,986 | 63,986 | 不变 |
| 消费覆盖（in-repo 并集） | 1259，0 缺 | 1264，0 缺 | 多出的 5 条来自编辑器 DLL 自身 |
| 外仓 10 个消费者 | 0 缺 | **0 缺** | 外仓零改动即受益 |
| `verify.ps1` | 绿 | **绿（退出码 0）** | 新增 3 段断言 |

**一句话：** 编辑器从"每个 exe 复制 188 MB"变成"一份 188 MB 的 `libMaximumEditord.dll` + 每个 exe 0.32 MB"，编辑器对外契约收敛到 **1 个符号**，引擎导出表**一个条目都没动**。

---

## 1 · S01 · 观测与护栏（纯新增，不改产物）

### 1.1 基线落盘

基线文件 `docs/ai/Platform/ProductBuild/BUILD-F04_S01_BASELINE.json`，由脚本自身记录（不是手抄）：

```text
python scripts/audit_module_boundaries.py --record-baseline --stage static-editor   # S01 时点
python scripts/audit_module_boundaries.py --record-baseline --stage editor-dll      # S02/S03 之后
```

S01 时点（切换前）记录的原始值：

| 量 | 值 |
|---|---|
| `libminEngined.dll` 导出 | 1549（maxNamedOrdinal 1549 / headroom 63986） |
| 引擎 imgui 导出 | 253 |
| `MaximumEditor.exe` | 188,033,796 B，引擎导入 579（imgui 253），编辑器 DLL 导入 0 |
| `SampleEditor.exe` | 188,034,182 B，引擎导入 579（imgui 253），编辑器 DLL 导入 0 |
| `libMaximumEditord.dll` | **不存在** |

S02/S03 之后同一文件改写为 `stage=editor-dll`，并保留 `bytes_before` / `engine_imports_before` / `imgui_imports_before` 三个"改前"字段——断言由这些字段驱动，而不是由人记忆驱动。

### 1.2 边界断言（新增脚本）

`scripts/audit_module_boundaries.py`（PE 解析复用 `scripts/audit_dll_exports.py` 的 `ExportTable`/`ImportTable`/`classify`，不另造一套）：

| 断言 | 失败形态 |
|---|---|
| 引擎 DLL 存在 | 缺 → FAIL |
| 引擎导出条目 ≤ 基线（1549） | 增 → FAIL |
| 引擎 imgui 导出 ≤ 基线（253） | 增 → FAIL |
| **编辑器 DLL 存在**（`stage=editor-dll`） | 缺 → FAIL |
| 编辑器 DLL 导出 ≤ 8，含 `CreateApplication`，imgui 导出 = 0 | 违反 → FAIL |
| 两个编辑器 exe **存在** | 缺 → FAIL（不跳过） |
| exe 体积 ≤ 16 MiB（`max_bytes`） | 回退成 188 MB → FAIL |
| exe 引擎导入 ≤ 3、imgui 导入 = 0、编辑器 DLL 导入 ≥ 1 | 违反 → FAIL |
| exe mtime ≥ 编辑器 DLL mtime | 陈旧 exe → FAIL |
| 基线必须覆盖两个编辑器 exe | 少写一个 → FAIL |

**防假绿的两条证据（本轮实测）：**

1. 把同一份基线的 `stage` 翻成 `editor-dll`（当时 DLL 还不存在）：

```text
FAIL: 1 boundary assertion(s) violated
  - editor DLL exists (S02 artifact): MISSING
exit=1
```

2. 真的把 `add_library ... STATIC` 回退后（§6.1）：

```text
FAIL SampleEditor.exe size within cap               188034182 <= 16777216 B
FAIL SampleEditor.exe engine imports did not increase 579 <= 3
FAIL SampleEditor.exe imports no imgui directly     253
FAIL SampleEditor.exe imports the editor DLL        0 symbol(s)
exit=1
```

即：断言在"目标不存在""目标退回旧形态"两种情况下都会红，不存在静默跳过的分支。

### 1.3 接入 `verify.ps1`

```powershell
# 构建行追加 SampleEditor：S01 断言覆盖两个编辑器 exe，陈旧 exe 不许静默通过
cmake --build $BuildDir --target minEngineTests Editor SampleEditor

# BUILD-F04 S01/S02/S03：边界断言（缺失即失败）
python ...\scripts\audit_module_boundaries.py --assert --fail
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# BUILD-F04 S02：两个编辑器 exe 的每条导入都必须能从编辑器 DLL 导出解析
python ...\scripts\audit_dll_exports.py --dll ...\bin\libMaximumEditord.dll --verify-coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

第二条复用已有的 `--verify-coverage`（它本身就有"扫到 0 个消费者 = 失败"的防假绿规则）。

---

## 2 · S02 · `MaximumEditorLib` STATIC → SHARED

### 2.1 关键 diff

**`minEngine/Editor/CMakeLists.txt`** —— 链接类型 + 产物名 + 目录 + 导出状态机：

```cmake
add_library(MaximumEditorLib SHARED      # 原 STATIC
    ${EDITOR_LIB_SRC}
    ${IMGUI_NODE_EDITOR_SRC}
)

set_target_properties(MaximumEditorLib PROPERTIES
    OUTPUT_NAME MaximumEditor                                    # -> libMaximumEditord.dll
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
    ARCHIVE_OUTPUT_DIRECTORY ${EXECUTABLE_PATH}
)

target_compile_definitions(MaximumEditorLib PRIVATE MINENGINE_EDITOR_BUILD)

get_target_property(_editor_lib_type MaximumEditorLib TYPE)
if(_editor_lib_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(MaximumEditorLib PRIVATE MINENGINE_EDITOR_DLL)
    minengine_apply_export_mode(MaximumEditorLib
        MODE "${MINENGINE_EDITOR_EXPORT_MODE}"
        DEF "${MINENGINE_EDITOR_EXPORTS_DEF}"
        BUDGET "${MINENGINE_EDITOR_EXPORT_BUDGET}"
        MIN_HEADROOM "${MINENGINE_EDITOR_EXPORT_MIN_HEADROOM}"
        LABEL "MaximumEditorLib")
endif()
```

**`minEngine/Editor/src/EditorAPI.h`**（新增，与 `EngineAPI.h` 同构）：

```cpp
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifdef MINENGINE_EDITOR_BUILD          // 只有产出编辑器库的 target 定义
        #ifdef MINENGINE_EDITOR_DLL        // 只有 SHARED 几何下才存在 DLL 边界
            #define EDITOR_API __declspec(dllexport)
        #else
            #define EDITOR_API
        #endif
    #elif defined(MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL)
        #define EDITOR_API __declspec(dllimport)
    #else
        #define EDITOR_API
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define EDITOR_API __attribute__ ((visibility ("default")))
#else
    #define EDITOR_API
#endif
```

**`minEngine/minEngine/src/main.h`**（Q3：外仓 main 零改动）：

```cpp
#if defined(MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL)
#include "EditorAPI.h"
extern EDITOR_API minEngine::Application* minEngine::CreateApplication();
#else
extern minEngine::Application* minEngine::CreateApplication();
#endif
```

**`minEngine/Editor/src/Editor.cpp`**（唯一入口符号）：

```cpp
// BUILD-F04 S02: the single editor DLL entry symbol (O6: editor export surface = 1).
EDITOR_API Application* CreateApplication()
```

**`minEngine/cmake/MinEngineTargets.cmake`**（`minengine_add_editor_executable` 内部，**签名不变** → 外仓零改动）：

```cmake
get_target_property(_editor_lib_type MaximumEditorLib TYPE)
if(_editor_lib_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(${TARGET_NAME} PRIVATE MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL)
endif()
```

**`minEngine/cmake/MinEngineExports.cmake`** —— 复用同一状态机，只加可选参数与独立 cache 变量：

```cmake
function(minengine_apply_export_mode target)
    cmake_parse_arguments(EXPORT "" "MODE;DEF;BUDGET;MIN_HEADROOM;LABEL" "" ${ARGN})
    # 缺省值仍是 MINENGINE_EXPORT_MODE / MINENGINE_EXPORTS_DEF / ... → 引擎侧行为逐字不变
```

新增 cache 变量：`MINENGINE_EDITOR_EXPORT_MODE`（默认 `explicit`）· `MINENGINE_EDITOR_EXPORTS_DEF` · `MINENGINE_EDITOR_EXPORT_BUDGET=8` · `MINENGINE_EDITOR_EXPORT_MIN_HEADROOM=60000`。

### 2.2 编辑器白名单：由消费者 import 表构造

**没有** dump 编辑器 DLL 的导出表（设计 §9 / BUILD-F02 P1 的坑）。生成命令与结果：

```text
python scripts/audit_dll_exports.py --dll minEngine/bin/libMaximumEditord.dll \
    --emit-whitelist minEngine/cmake/MaximumEditor_exports.def

wrote minEngine/cmake/MaximumEditor_exports.def: 1 entries (consumed union 1, legacy block 0)
  ;   minEngine/bin/MaximumEditor.exe                1 names
  ;   minEngine/bin/SampleEditor.exe                 1 names
  Entries: 1 (consumed union 1, DLL exports 18834; 0 marked DATA; 0 in legacy block)
EXPORTS
_ZN9minEngine17CreateApplicationEv
```

**"编辑器 exe 需要的符号数" = 1**（两 exe 各 1，并集 1）：`_ZN9minEngine17CreateApplicationEv`（`minEngine::CreateApplication()`）。

生成顺序（避开鸡生蛋）：

1. `-DMINENGINE_EDITOR_EXPORT_MODE=auto` 先构出 DLL，两个编辑器 exe 链接成功；
   auto 模式实测导出 **18834** 条 / 余量 46701 —— 这就是 F6 描述的炸弹形态，也正是"必须套用导出状态机"的现场证据。
2. `--emit-whitelist` 从两个 exe 的 import 表生成 1 条白名单。
3. 切回默认 `whitelist` 重新链接：导出 **1** 条，两个 exe 依旧链接通过。

### 2.3 体积对比（字节 + 百分比）

| 产物 | 改前 | 改后 | Δ | Δ% |
|---|---|---|---|---|
| `MaximumEditor.exe` | 188,033,796 | **323,536** | −187,710,260 | **−99.828%** |
| `SampleEditor.exe` | 188,034,182 | **323,922** | −187,710,260 | **−99.828%** |
| `libMaximumEditord.dll` | — | **187,973,485** | 新增 | — |
| 两个编辑器合计 | 376,067,978 | **188,620,943** | −187,447,035 | **−49.84%** |
| `libminEngined.dll` | 531,407,389 | 531,407,389 | 0 | 0% |

两个 exe 现在差 **386 B**（323,922 − 323,536），与改前差值（386 B）**一模一样**——差的仍然只是游戏模块那点东西，编辑器代码已只剩一份。

### 2.4 构建期断言（F6 护栏）

编辑器 target 走 `minengine_apply_export_mode` 的 POST_BUILD 一段，链接完成即断言：

```text
实测输出：
MaximumEditorLib export budget (explicit): assert entry count and ordinal headroom
PASS: NumberOfFunctions=1 <= 8, ordinal headroom=65534 >= 60000
```

---

## 3 · S03 · 编辑器导出收敛到 `explicit`

1. `minEngine/cmake/MaximumEditor_exports.def` **燃烧到 0 条目**（保留文件 + 生成来源与回退步骤的注释）。
2. 默认 `MINENGINE_EDITOR_EXPORT_MODE=explicit`：导出面只剩 `EDITOR_API` 标注集合。
3. 强制重链（删掉 DLL 再构）验证真值：

```text
rm -f minEngine/bin/libMaximumEditord.dll
cmake --build minEngine/build --target MaximumEditorLib
[100%] Linking CXX shared library ...\bin\libMaximumEditord.dll
MaximumEditorLib export budget (explicit): assert entry count and ordinal headroom
PASS: NumberOfFunctions=1 <= 8, ordinal headroom=65534 >= 60000
```

| 量 | S02（whitelist）基线 | S03（explicit） | 断言 |
|---|---|---|---|
| 编辑器 DLL 导出条目 | 1 | **1** | 不增加 ✅ |
| 序号余量 | 65,534 | 65,534 | ✅ |
| 两个编辑器 exe | 链接通过 | 链接通过 | ✅ |
| 游戏模块 `libGameModuleStubd.dll` | 26 条引擎导入 | 26 条 | 不变 ✅ |
| DLL 体积 | 187,973,485 | 187,973,485 | 逐字节相同（导出表同为 1 条） |

**`whitelist` 回退实测（两步，按 .def 注释里的顺序执行）：**

```text
python scripts/audit_dll_exports.py --dll minEngine/bin/libMaximumEditord.dll \
    --emit-whitelist minEngine/cmake/MaximumEditor_exports.def      -> 1 entries
cmake ... -DMINENGINE_EDITOR_EXPORT_MODE=whitelist
-- MaximumEditorLib exports: mode=whitelist, 1 .def entries
[100%] Linking CXX shared library ... libMaximumEditord.dll
PASS: NumberOfFunctions=1 <= 8, ordinal headroom=65534 >= 60000
NumberOfFunctions ... 1 / consumers: MaximumEditor.exe 1, SampleEditor.exe 1, missing 0
（随后恢复 explicit + 燃烧版 .def，边界断言重新 PASS）
```

---

## 4 · 引擎侧：一个条目都没动

| 量 | 改前 | 改后 |
|---|---|---|
| 引擎 DLL 导出条目 | 1549 | **1549** |
| maxNamedOrdinal / 余量 | 1549 / 63986 | 1549 / 63986 |
| 引擎 imgui 导出 | 253 | 253 |
| `minEngine_exports.def` | — | **未修改**（mtime 早于本轮，逐字保留 `LEGACY-EXTERNAL-DEBT` 区块） |

为什么不需要增加：编辑器 exe 原先 579 条引擎导入 = 编辑器 TU 的并集；搬到 DLL 后这份并集变成**编辑器 DLL 的导入表**（实测 577 条，其中 imgui 253），仍是引擎 1549 条导出表的子集，`--verify-coverage` 实测 `missing 0`。

> 备注：577 = 579 − 3（main.h 自身的 3 条，留在 exe 里）+ 1（多出的一条是原先 exe 里由链接器从其他 TU 间接带入的符号）。

---

## 5 · Verification 对照（设计 §8）

| # | Oracle | 结果 | 证据 |
|---|---|---|---|
| O1 | 引擎导出 ≤ 3000 | ✅ 1549 | `PASS: NumberOfFunctions=1549 <= 3000, ordinal headroom=63986 >= 60000` |
| O2 | 序号余量 ≥ 60000 | ✅ 63986 | 同上 |
| O3 | 消费覆盖 `exported ⊇ consumed` | ✅ in-repo 1264 / 外仓 10 个消费者全部 missing 0 | `coverage total missing = 0`（两段） |
| O4 | I-gimgui | 本轮**不适用**（imgui 仍在引擎 DLL，S04 才迁） | — |
| O5 | imgui 零残留 | 本轮**不适用**（S04） | 断言已锁定"不得增加"：253 → 253 |
| O6 | 编辑器 DLL 导出面 = 契约 | ✅ 1 条 = `EDITOR_API` 集合 | 见 §3 |
| O9 | 五类 target 全链接 | ✅ | 见下 |
| O10 | `verify.ps1` 全绿 | ✅ 退出码 0 | 见下 |

**五个 target 链接结果：**

```text
cmake --build minEngine/build --target minEngineTests Editor SampleEditor SampleGame GameModuleStub
[100%] Built target minEngineTests        (137,419,254 B)
[100%] Built target MaximumEditor        -> bin/MaximumEditor.exe    323,536 B
[100%] Built target SampleEditor         -> bin/SampleEditor.exe     323,922 B
[100%] Built target SampleGame           -> bin/SampleGame.exe       341,235 B
[100%] Built target GameModuleStub       -> bin/libGameModuleStubd.dll 10,075,465 B
[100%] Built target MaximumEditorLib     -> bin/libMaximumEditord.dll 187,973,485 B
```

**`verify.ps1`（本轮实跑）：**

```text
verify: cmake --build (minEngineTests, Editor, SampleEditor) ...
PASS: NumberOfFunctions=1549 <= 3000, ordinal headroom=63986 >= 60000
BUILD-F04 boundary assertions (baseline docs/ai/Platform/ProductBuild/BUILD-F04_S01_BASELINE.json, stage=editor-dll):
  ...
PASS: editor/module boundary holds
  minEngine/bin/MaximumEditor.exe                      imported     1  missing 0
  minEngine/bin/SampleEditor.exe                       imported     1  missing 0
coverage total missing = 0
  ../minCraft/bin/MinCraftEditor.exe                   imported   573  missing 0
  ../minCraft-ui/bin/MaximumEditor.exe                 imported   578  missing 0
  ... (10 个外仓消费者，全部 missing 0)
coverage total missing = 0
verify: minEngineTests.exe test smoke ...
[doctest] test cases:  4 |  4 passed | 0 failed | 229 skipped
[doctest] assertions: 77 | 77 passed | 0 failed |
[doctest] Status: SUCCESS!
exit code 0
```

---

## 6 · 回退

### 6.1 回退到 STATIC（**一个词**，已实测）

`minEngine/Editor/CMakeLists.txt:22`：`add_library(MaximumEditorLib SHARED` → `STATIC`。

之所以真的一行就够：DLL 专属的三件事（`MINENGINE_EDITOR_DLL` 宏、`minengine_apply_export_mode`、host 的 `MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL`）都从 `get_target_property(... TYPE)` 派生，STATIC 时它们自动全部消失；`EDITOR_API` 同时塌陷为空，`main.h` 走回原来的裸 `extern`。

**实测记录：**

```text
sed -i 's/^add_library(MaximumEditorLib SHARED$/add_library(MaximumEditorLib STATIC/' minEngine/Editor/CMakeLists.txt
cmake -S minEngine -B minEngine/build
[100%] Linking CXX static library ...\bin\libMaximumEditord.a
[100%] Linking CXX executable ...\bin\MaximumEditor.exe
[100%] Linking CXX executable ...\bin\SampleEditor.exe

MaximumEditor.exe  188,033,796 B   engine imports 579  imgui 253
SampleEditor.exe   188,034,182 B   engine imports 579  imgui 253
```

体积与导入数与 S01 基线**逐字节一致**（188,033,796 / 188,034,182），说明回退是真正的"回到改前"而不是"近似"。
同一状态下 `audit_module_boundaries.py --assert` **红**（exit 1，见 §1.2）——护栏与回退互为验证。
恢复：`STATIC` → `SHARED`，删除 `libMaximumEditord.a`，重新 configure + build，边界断言回到 PASS。

### 6.2 回退到 `whitelist`（两步，已实测）

见 §3 末尾。顺序不能反：先按消费者 import 表重新生成 `.def`，再切 `-DMINENGINE_EDITOR_EXPORT_MODE=whitelist`（空 `.def` + whitelist = 0 导出 = 两个 exe 链接失败，这是**故意保留的响亮失败**）。

### 6.3 回退面清单

| 语义切换点 | 一个动作 | 影响 |
|---|---|---|
| STATIC ↔ SHARED | `Editor/CMakeLists.txt` 一个词 | 编辑器边界 |
| whitelist ↔ explicit | `-DMINENGINE_EDITOR_EXPORT_MODE` | 编辑器导出机制 |
| 引擎导出机制 | `-DMINENGINE_EXPORT_MODE` | 不受本轮影响 |

---

## 7 · 偏差与发现

| # | 事项 | 说明 |
|---|---|---|
| **D1** | 宏名用 `MINENGINE_EDITOR_BUILD`（设计 §4.3 写的是 `MAXIMUMEDITOR_BUILD`） | 按任务书口径取 `MINENGINE_EDITOR_BUILD`，与 `MINENGINE_BUILD` / `MINENGINE_API` 命名族一致 |
| **D2** | 多了 `MINENGINE_EDITOR_DLL` 一个宏 | 让"一个词回退到 STATIC"成立：没有它，STATIC 几何下 `EDITOR_API` 会是 `dllimport`，而 `CreateApplication` 的定义方也拿到 `dllimport` → 必然链接失败。它只在 SHARED 时定义 |
| **D3** | **发现既有假绿**：`MinEngineExports.cmake` 的 POST_BUILD 断言用 `Python3_EXECUTABLE`，而它是 `minEngine/minEngine/CMakeLists.txt` 目录作用域的普通变量，兄弟目录 `Editor/` 根本不继承 → 编辑器 target 一条预算断言都没有，configure 日志却毫无异常 | 已修为 `MINENGINE_PYTHON_EXECUTABLE`（cache），引擎与编辑器两边都真正跑起来：`MaximumEditorLib export budget (explicit) ... PASS` |
| **D4** | `audit_dll_exports.py` 的 `--emit-whitelist` 表头原先硬编码"引擎"与引擎 `.def` 路径 | 改为按目标模块派生（否则编辑器白名单会被标成引擎的）。引擎 `minEngine_exports.def` **未重新生成**，不受影响 |
| **D5** | 构建期预算用 8（O6 目标 ≤ 8），不是 3000 | 编辑器 DLL 一旦像 auto 模式那样涨到 18834 就会在链接后立刻红；这是 F6 的正面护栏 |
| **D6** | 基线落 `.json`（设计 S01 原文写的是 `.md`） | 断言脚本要机器可读；人读部分在本文件 §1.1 |
| **D7** | `minEngineTests` 直接编译部分 `Editor/src` 源（`Tests/CMakeLists.txt:9-20`） | 这些 TU 不 `#include "EditorAPI.h"`，也不定义 `MINENGINE_EDITOR_BUILD` → 不受影响，实测 807 条引擎导入 / imgui 0 与改前一致 |
| **D8** | 编辑器 DLL 仍从引擎导入 imgui 253 条 | 本轮**不迁 imgui**（S04）。因此 F03 的 C2"引擎导出中不得出现 imgui 桶"前提未变，也**没有被恶化**（253 → 253，断言锁定） |
| **D9** | 构建目录的 `MINENGINE_EDITOR_EXPORT_MODE` cache 值被显式设成 `explicit` | 与 CMakeLists 里的默认值相同，不构成魔法状态 |
| **D10** | `Build/Staging` 与打包挂点未动 | S06 范围 |
| **D11** | 未做 ASan 验证（F4 跨 DLL `new`/`delete`） | 本轮 exe 只 `delete app`（对象在编辑器 DLL 内分配）；MinGW 两侧同一 `libstdc++`/`msvcrt`，风险低。按设计 §11 Deferred 留待后续（需要 `build-asan` 单独构建） |

---

## 8 · 未完成 / 下一轮

1. **S04**（imgui 家族迁入编辑器 DLL、引擎导出 1549 → 1292）：本轮未做。做完后 §5 的 O4/O5 才适用，且编辑器 DLL 的引擎导入会从 577 掉到约 324。
2. **S05**（资产-only 语义 + 缺模块显式报错）：本轮只按"编辑器将来可独立"的形态铺好边界，未写语义代码。
3. **S06**（`package-editor` 打包挂点）。
4. 外仓 `minCraft*` **未修改**：`minCraft-ui/bin/MaximumEditor.exe` 仍是旧的 188 MB 形态（只读扫描可见），下次在其仓库构建时自动受益；本轮只读扫描并断言其引擎导入 0 缺。
5. `libmaximumenteditord.dll` 的 release/debug 两个拼写尚未进 `DLL_CONTRACT_ALIASES`（引擎有此机制）。当前 debug 构型下自洽；若将来出现"release 引擎 + debug 编辑器"的混搭产物，需要在脚本里补别名。

---

## 9 · 本轮碰过的文件

| 文件 | 动作 |
|---|---|
| `minEngine/Editor/CMakeLists.txt` | 改：SHARED + OUTPUT_NAME + 输出目录 + `MINENGINE_EDITOR_BUILD`/`MINENGINE_EDITOR_DLL` + 导出状态机 + efsw 局部变量 |
| `minEngine/Editor/src/EditorAPI.h` | **新增**：`EDITOR_API` |
| `minEngine/Editor/src/Editor.cpp` | 改：`#include "EditorAPI.h"` + `EDITOR_API Application* CreateApplication()` |
| `minEngine/minEngine/src/main.h` | 改：`MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL` 分支 |
| `minEngine/cmake/MinEngineExports.cmake` | 改：可选参数 `MODE/DEF/BUDGET/MIN_HEADROOM/LABEL` + 编辑器独立 cache 变量 + `MINENGINE_PYTHON_EXECUTABLE` 修正 |
| `minEngine/cmake/MinEngineTargets.cmake` | 改：`minengine_add_editor_executable` 里按 TYPE 定义 host 宏（**签名不变**） |
| `minEngine/cmake/MaximumEditor_exports.def` | **新增**：由消费者 import 表生成 1 条，S03 燃烧到 0 |
| `scripts/audit_module_boundaries.py` | **新增**：基线记录 + 边界断言 |
| `scripts/audit_dll_exports.py` | 改：`--emit-whitelist` 表头按模块派生（D4） |
| `scripts/verify.ps1` | 改：构建行加 `SampleEditor` + 两段新断言 |
| `docs/ai/Platform/ProductBuild/BUILD-F04_S01_BASELINE.json` | **新增**：机器可读基线 |
| `docs/ai/Platform/ProductBuild/BUILD-F04_S01_S03_EDITOR_DLL_IMPLEMENTATION.md` | **新增**：本文件 |

**未触碰（硬性约束）：** `ScreenUICoords.*` · `SpriteQuadMesh.*` · `PointerConstraintState.*` · `GLFWWindowSystem.*` · `WindowSystem.h` · `Render/Material.*` · `ShaderCompiler.*` · `SpriteMaterialFactory.*` · `ScreenUIMaterialFactory.*` · `EngineShaderUtils.*` · `RenderSystem.cpp` · `RHI/RHI.h` · `docs/ai/FEATURE_REGISTRY.md` · `docs/ai/bugs/*` · `BUILD-F03_*` · `BUILD-F04_*`（设计冻结，本文件是另一份实现记录）· `CORE-F38_*` · `D:\Dev\GitRepo\minCraft*` · `minEngine/cmake/minEngine_exports.def`（含 `LEGACY-EXTERNAL-DEBT` 区块）。

---

## 附录 A · 可复现命令

```text
# S01 基线（切换前 / 切换后各跑一次）
python scripts/audit_module_boundaries.py --record-baseline --stage static-editor
python scripts/audit_module_boundaries.py --record-baseline --stage editor-dll

# 边界断言
python scripts/audit_module_boundaries.py --assert --fail

# 编辑器白名单（必须从消费者 import 表生成）
python scripts/audit_dll_exports.py --dll minEngine/bin/libMaximumEditord.dll \
    --emit-whitelist minEngine/cmake/MaximumEditor_exports.def

# 编辑器导出面 / 覆盖率
python scripts/audit_dll_exports.py --dll minEngine/bin/libMaximumEditord.dll --report
python scripts/audit_dll_exports.py --dll minEngine/bin/libMaximumEditord.dll --verify-coverage

# 引擎导出面（未增加）
python scripts/audit_dll_exports.py --report
python scripts/audit_dll_exports.py --max-exports 3000 --fail

# 全量验证
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1
```
