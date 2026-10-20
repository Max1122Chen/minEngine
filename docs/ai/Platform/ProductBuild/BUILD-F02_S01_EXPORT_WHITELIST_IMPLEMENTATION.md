# BUILD-F02 · S01 实现记录 —— 档 1「去炸弹」：PE 导出白名单上线

## Meta
- **ID:** `BUILD-F02-S01`
- **类型:** 构建 / ABI 契约（档 1：S00 + S01 + S02）
- **状态:** 已落地，`whitelist` 为默认；`auto` 一行可回退
- **设计输入:** [BUILD-F02_EXPLICIT_EXPORT_MINENGINE_API_DESIGN.md](BUILD-F02_EXPLICIT_EXPORT_MINENGINE_API_DESIGN.md)
- **实施范围:** 只动构建配置、导出宏、审计脚本与 8 处最小 include；**零业务逻辑改动**

---

## 0 · 结论摘要

| 指标 | 改前 | 改后（`whitelist`+`S02`） | 结论 |
|---|---|---|---|
| `NumberOfFunctions` | **65533** | **1545** | ↓ 97.6% |
| `maxNamedOrdinal` | 65533 | 1545 | — |
| 序号余量 `65535-65533` | **2** | **63990** | 炸弹解除 |
| 导出目录字节 | 7,473,708 | 113,712 | ↓ 98.5% |
| 消费覆盖（6 个二进制） | 1250，0 缺 | 1259，0 缺 | 保持 |
| `verify.ps1` | 绿 | **绿（退出码 0）** | 保持 |

三段式状态机已落地，默认 `whitelist`；`-DMINENGINE_EXPORT_MODE=auto` 一行回到今天的行为。

**实施中发现设计未覆盖的一个硬约束（V4）：** 把**数据符号**（vtable/typeinfo/单例）写进 `.def` 会让 GNU ld 在 import lib 里生成 `.text` 代码 thunk（`T _ZTV...`），与消费者自己 COMDAT 生成的同名 vtable 冲突——`multiple definition of 'vtable for ...'`。必须用 `.def` 的 `DATA` 关键字声明。这一条决定 S02 能否与 S01 共存，已修进生成器。

---

## 1 · 实施前的 3 个实验（V0 / V1 / V2）+ 2 个补充实验

实验目录在仓库外（`D:\Dev\f02exp`），全部用 MinGW GCC 15.2.0 / GNU ld 2.45 实测。

### V0 · ld 在「无任何显式导出」时是否回落自动导出 —— **是，且 `--exclude-all-symbols` 能关掉**

```text
# a.cpp: extern "C" { int v0_alpha(int); int v0_beta(int); } double v0_gamma = 1.0;

$ g++ -shared -o v0_auto.dll a.cpp            # 未加 --export-all-symbols，无 dllexport
Export Address Table          00000003
[   0] +base[   1] 00001310 Export RVA
[   1] +base[   2] 0000131f Export RVA
[   2] +base[   3] 00003010 Export RVA
=> 3 个导出（v0_alpha / v0_beta / v0_gamma）：确认"什么都不写"也会自动导出全部

$ g++ -shared -Wl,--exclude-all-symbols -o v0_excl.dll a.cpp
=> objdump 完全没有 Export Address Table 段落 = 0 导出
```

**结论：** 只删 `--export-all-symbols` 不足以关掉自动导出，必须显式用 fail-closed 的 `--exclude-all-symbols`。与设计 How-2 假设一致，S01/S05 的写法成立。

### V1 · `.def` 与 `__declspec(dllexport)` 是否取并集 —— **是**

```text
# b.cpp: __declspec(dllexport) int v1_decl(int);  int v1_deff(int);   v1.def: EXPORTS / v1_deff

$ g++ -shared -Wl,--exclude-all-symbols b.cpp v1.def -o v1_union.dll
[Ordinal/Name Pointer] Table -- Ordinal Base 1
	[   0] +base[   1]  0000 v1_decl
	[   1] +base[   2]  0001 v1_deff
=> 2 个导出
$ g++ -shared -Wl,--exclude-all-symbols b.cpp -o v1_declonly.dll     # 对照：只有 dllexport
=> 1 个导出（v1_decl）
```

**结论：** 两机制取并集且不报重复（`--warn-duplicate-exports` 默认关闭）。S02 与 S01 可解耦、可切片过渡成立。**本仓实测印证**：S02 打开后导出 = 1259 行 `.def` ∪ `MINENGINE_API` 标出的符号 = 1545 条。

### V2 · CMake 把 `.def` 传给 MinGW 链接的正确写法 —— **四种都可，选 `target_sources`**

同一个 `src.cpp` + `exp.def`（只导出 `v2_deff`），四种写法各自构建，产物均只导出 `v2_deff`：

```text
(a) target_sources(v2a PRIVATE ${DEF_FILE})
    link: c++.exe -g -Wl,--exclude-all-symbols -shared D:\Dev\f02exp\v2\exp.def -o libv2a.dll ...
(b) target_link_options(v2b PRIVATE "LINKER:${DEF_FILE}")
    link: ... -Wl,--exclude-all-symbols -Wl,D:/Dev/f02exp/v2/exp.def -shared ...
(c) set_target_properties(v2c PROPERTIES LINK_FLAGS "-Wl,... -Wl,${DEF_FILE}")
    link: 同 (b)
(d) target_link_libraries(v2d PRIVATE ${DEF_FILE})
    link: 同 (b)
四者 [Ordinal/Name Pointer] 均只有 v2_deff 一条
```

**结论：** `.def` 作为普通输入文件即可，不需要 `SHELL:` 包裹；`LINKER:` 也不必。选 **(a) `target_sources`**：语义最清晰，且 **`touch exp.def` 后重建会重新链接**（实测 `[50%] Linking CXX shared library libv2a.dll`）——白名单改一行就能触发重链，正是切片燃烧需要的。

### V3（补充）· `dllimport` 类 + 头内 inline 成员，GCC 是否导出/是否破坏消费者

```text
class API Foo { public: void OutOfLine(); int InlineMember() const { return 42; } virtual void V() {} };
mod.dll 导出: GData, _Z16FreeInlineHelperi, _ZN3Foo9OutOfLineEv, _ZTI3Foo, _ZTV3Foo
=> InlineMember 未导出；消费者编译链接运行成功: "42 2 7"
```

**结论：** MinGW GCC 对类级 `dllexport` **不**把 inline 成员拉进导出表（与 MSVC 不同），消费者仍本地实例化 inline 成员。设计 F6「`dllexport` 类导致导出膨胀」在本仓的实测放大率很低（1259 → 1545，+286 条，远低于 3000 预算）。

### V4（补充，**设计未覆盖，必须记录**）· `.def` 里的**数据**导出会生成代码 thunk

S01+S02 首次全量验证时 `libGameModuleStubd.dll` / `minEngineTests.exe` 链接失败：

```text
ld.exe: libminEngined.dll.a(libminEngined_dll_d002500.o):(.text+0x0):
        multiple definition of 'vtable for minEngine::Math::Noise::Noise';
        ...NoiseTest.cpp.obj:(.rdata$_ZTVN9minEngine4Math5Noise5NoiseE...) first defined here
ld.exe: libminEngined.dll.a(...): multiple definition of 'vtable for minEngine::Reflection::MEStruct';
        ...SampleTableDataAsset.gen.cpp.obj:MEProperties.h:247: first defined here
```

import lib 符号类型对照（`nm libminEngined.dll.a`）：

| 导出来源 | import lib 里的符号 |
|---|---|
| `.def` 普通条目（被当作**函数**） | `I __imp__ZTV...` + **`T _ZTV...`（.text 代码 thunk）** ← 冲突源 |
| `.def` 条目 + `DATA` 关键字 | `I __imp__ZTV...` + `I __nm__ZTV...`（纯导入桩） |
| `__declspec(dllexport)` 导出 | `I __imp__ZTV...` + `I __nm__ZTV...`（纯导入桩） |

**结论：** `.def` 的条目默认按"函数"导出，数据符号必须在 `.def` 里写 `name DATA`，否则 import lib 生成代码 thunk，与消费者本地 COMDAT 的 vtable/typeinfo 冲突。生成器改用 **PE 节属性**（导出 RVA 是否落在可执行节）判定数据/代码，自动加 `DATA`。改后 124 条数据条目，四类 target 全部链接通过。

---

## 2 · 切片落地清单

### S00 · 观测与护栏

| 交付 | 位置 | 说明 |
|---|---|---|
| 审计脚本 | `scripts/audit_dll_exports.py`（新增） | 纯 PE 解析（不运行 DLL）：导出目录（`OrdinalBase`/`NumberOfFunctions`/`NumberOfNames`/最大具名序号/余量/导出目录字节）、导出构成归类、消费者 import 表 |
| `--report` | 同上 | 打印导出数/余量/构成 + 逐消费者 import 数与缺失；`--json` 落盘 |
| `--max-exports 3000 --fail` | 同上 | 断言 `NumberOfFunctions <= 3000` 且 `65535-maxOrdinal >= 60000`，违反退出码 1；**快路径 0.84s**（不扫消费者） |
| `--emit-whitelist PATH` | 同上 | 从**消费者 import 表**并集生成 `.def`（∩ 当前 DLL 导出集合，报告"导入但不存在"的陈旧项） |
| `--verify-coverage` | 同上 | 逐消费者 `exported ⊇ consumed`，输出 `coverage total missing = 0` |
| `--emit-export-dump PATH` | 同上 | B′ 审计产物（整表 dump，**明确不是导出机制**） |
| CMake 目标 | `minEngine/minEngine/CMakeLists.txt` `export-audit` | `--report --verify-coverage`，`DEPENDS minEngine` |
| 构建内断言 | `cmake/MinEngineExports.cmake` POST_BUILD | `whitelist`/`explicit` 模式每次链接后跑 `--fail`，把 O1/O2 打进构建日志（`auto` 模式跳过，否则回退必然失败） |
| verify.ps1 断言 | `scripts/verify.ps1` | 构建后 3 行：调用 `--max-exports 3000 --fail` + 退出码检查 |

### S01 · 三段式状态机 + 白名单

| 交付 | 位置 |
|---|---|
| `MINENGINE_EXPORT_MODE` = `auto` | `whitelist` | `explicit`（cache 变量，默认 **`whitelist`**） | `minEngine/cmake/MinEngineExports.cmake`（新增） |
| `MINENGINE_EXPORTS_DEF` / `MINENGINE_EXPORT_BUDGET` / `MINENGINE_EXPORT_MIN_HEADROOM` / `MINENGINE_AUDIT_SCRIPT` | 同上（cache，可覆盖） |
| 链接选项切换 | `minengine_apply_export_mode(<target>)` |
| 白名单 | `minEngine/cmake/minEngine_exports.def`（新增，**1259 行**，其中 **124 行 `DATA`**） |

链接行为（实测链接行）：

```text
auto      : -Wl,--export-all-symbols  <exclude-libs> <exclude-modules-for-implib>
whitelist : -Wl,--exclude-all-symbols <exclude-libs> <exclude-modules-for-implib>  + minEngine_exports.def
explicit  : -Wl,--exclude-all-symbols <exclude-libs> <exclude-modules-for-implib>
```

`--exclude-libs` / `--exclude-modules-for-implib` 在非 `auto` 模式下已是冗余（无自动导出可排除），**保留**以避免切换默认值时引入行为差异；按设计 S06 再删。

白名单构成（1259 条，互斥归类）：

| 类别 | 条数 |
|---|---|
| minEngine 自有 | 992 |
| imgui / ImGuizmo | 253 |
| glfw | 8 |
| glad | 5 |
| 其它（`TAG_Channel_Default`） | 1 |

刷新命令（唯一入口，写进 `.def` 头部注释）：

```text
python scripts/audit_dll_exports.py --emit-whitelist minEngine/cmake/minEngine_exports.def
```

### S02 · 宏转正

| 交付 | 位置 | 内容 |
|---|---|---|
| Windows 语义 | `Runtime/Core/EngineAPI.h` | `#ifdef MINENGINE_BUILD → __declspec(dllexport)` / `#else → __declspec(dllimport)`；GCC/Clang 非 Windows 分支保持 `visibility("default")` |
| 删除误用 | `minEngine/Tests/CMakeLists.txt` | 删掉 `target_compile_definitions(minEngineTests PRIVATE MINENGINE_BUILD)`，原地留 I4 不变式注释 |
| 直接 include | 8 个头文件 | `Log/LogSystem.h`、`Log/LogConsole.h`、`Serialization/{Serializer,SerializationTypes,DiskVersion,JsonArchive,BinaryArchive}.h`、`Reflection/ReflectionSample.h`（设计估计 ~5 处，实测 8 处；这些文件原先靠传递 include 才能展开 `MINENGINE_API`） |

S02 实测代价：**596 个 TU 重编译**，6 个链接步骤，导出 1259 → 1545（+`MINENGINE_API` 标出的 286 条）。

---

## 3 · 状态机与不变量现状

```text
auto ──(本次已切)──▶ whitelist ──(S05，未做)──▶ explicit
 │                      │
 └ 回退兜底 ◀────────────┘   唯一总闸: -DMINENGINE_EXPORT_MODE
```

| 不变量 | 现状 |
|---|---|
| **I1** 导出条目 ≤ 3000 | 通过（1545），构建末尾 + `verify.ps1` 双断言 |
| **I2** `65535-maxOrdinal ≥ 60000` | 通过（63990） |
| **I3** 导出集合 ⊇ 消费者 import 并集 | 通过（6 个消费者，逐项 0 缺失） |
| **I4** `MINENGINE_BUILD` 只在产出 DLL 的 target 上 | 通过（全仓 1 处，`grep` 复核） |

---

## 4 · 回退说明（一行）

```text
cmake -S minEngine -B minEngine/build -DMINENGINE_EXPORT_MODE=auto
cmake --build minEngine/build --target minEngine
```

**实测证据（本次已跑）：**

```text
-- minEngine exports: mode=auto (--export-all-symbols; ordinal bomb ARMED)
[ 41%] Linking CXX shared library ...\bin\libminEngined.dll
NumberOfFunctions ... 65535   (NumberOfNames 65535)
maxNamedOrdinal ... 65535   PE limit 65535   headroom 0
```

回退**可用**（链接通过、ELF 消费者照旧可链），但要注意两条必须写进下一轮文档的事实：

1. **回退后余量是 0，不是 2。** S02 的 `dllexport` 把自动导出从 65533 推到 **65535**（`minEngine` 类符号 11818 → 11820）。回退只能作为"临时排障"，不能作为长期状态——再新增一个导出就会重演"产物被删"。
2. `auto` 模式**故意不做**导出预算断言（否则回退后构建必然失败）；预算断言只在 `whitelist`/`explicit` 生效。

第二条回退路径（彻底撤销本切片）：`git checkout --` 上表 12 个受版本控制的文件 + 删除 4 个新增文件（`MinEngineExports.cmake`、`minEngine_exports.def`、`audit_dll_exports.py`、本实现文档）。因为 `whitelist` 需要在消费者 import 变化后重新生成 `.def`，撤销 S02（`EngineAPI.h`）但保留 `whitelist` 时必须同时重新生成 `.def`——两者是同一契约的两半，已在 `.def` 头部与 `MinEngineExports.cmake` 注释中标注。

---

## 5 · 导出数前后对比与消费覆盖（实测输出）

### 改前（`auto`，本任务开工时的 `bin/libminEngined.dll`）

```text
OrdinalBase .... 1
NumberOfFunctions ... 65533   (NumberOfNames 65533)
maxNamedOrdinal ... 65533   PE limit 65535   headroom 2
export dir bytes ... 7473708
composition: minEngine 11818 | libstdcxx 35926 | imgui 2453 | glad 720 | glfw 134 |
             jolt 380 | stb 43 | fmt/spdlog 1178 | nlohmann/sol2 6875 |
             system-guid 301 | other 5705
```

### 改后（`whitelist` + `--exclude-all-symbols` + `.def`，S02 已开）

```text
OrdinalBase .... 1
NumberOfFunctions ... 1545   (NumberOfNames 1545)
maxNamedOrdinal ... 1545   PE limit 65535   headroom 63990
export dir bytes ... 113712
composition: minEngine 1278 | imgui 253 | glad 5 | glfw 8 | other 1
```

**中途态（只切 S01、S02 未开）**：`NumberOfFunctions 1250`、`headroom 64285`、导出目录 89,512 字节——即白名单本身就足以解除炸弹，S02 只是把宏转正并把标注面并入导出（+295 → 1545）。

### 消费覆盖（逐消费者 0 缺失，含外仓 minCraft）

```text
coverage (exported >= consumed):
  MaximumEditor.exe          imported   579  missing 0
  SampleEditor.exe           imported   579  missing 0
  SampleGame.exe             imported     7  missing 0
  libGameModuleStubd.dll     imported    26  missing 0
  libminCraftd.dll           imported   153  missing 0     <- 外仓，只读扫描
  minEngineTests.exe         imported   807  missing 0
coverage total missing = 0
consumed union ... 1259   (imported but not exported: 0)
```

对照：S01-only 时并集 1250（578/564/9/20/153/802）；S02 打开后变 1259——`dllimport` 让部分 vtable/数据符号从"各模块本地实例化"改为"从 DLL 导入"，这些符号由 `MINENGINE_API` 导出，已重新生成 `.def` 纳入白名单（1259 行），因此两种状态都自洽。

---

## 6 · 验收证据

### 四类 target 链接（一次构建全部通过）

```text
[ 41%] Linking CXX shared library ...\bin\libminEngined.dll
PASS: NumberOfFunctions=1545 <= 3000, ordinal headroom=63990 >= 60000
[ 98%] Linking CXX shared library ...\bin\libGameModuleStubd.dll
[ 88%] Linking CXX executable ...\bin\minEngineTests.exe
[100%] Linking CXX executable ...\bin\MaximumEditor.exe
[100%] Linking CXX executable ...\bin\SampleGame.exe
[100%] Linking CXX executable ...\bin\SampleEditor.exe
```

### `verify.ps1`（本人执行一次）

```text
verify: cmake --build (minEngineTests) ...
PASS: NumberOfFunctions=1545 <= 3000, ordinal headroom=63990 >= 60000
verify: minEngineTests.exe test smoke ...
[doctest] test cases:  2 |  2 passed | 0 failed | 231 skipped
[doctest] assertions: 11 | 11 passed | 0 failed |
...
[doctest] test cases:  4 |  4 passed | 0 failed | 229 skipped
[doctest] assertions: 77 | 77 passed | 0 failed |
VERIFY_EXIT=0
```

### CMake 目标

```text
$ cmake --build minEngine/build --target export-audit
NumberOfFunctions ... 1545   (NumberOfNames 1545)
coverage total missing = 0
[100%] Built target export-audit
```

### 断言可用性

```text
$ python scripts/audit_dll_exports.py --fail --max-exports 3000     # 改前
FAIL: NumberOfFunctions 65533 > budget 3000
FAIL: ordinal headroom 2 < required 60000          -> exit 1
$ python scripts/audit_dll_exports.py --fail --max-exports 3000     # 改后
PASS: NumberOfFunctions=1545 <= 3000, ordinal headroom=63990 >= 60000   -> exit 0
```

---

## 7 · 关键发现、偏差与不在范围内的事

### 7.1 `.def` 数据导出必须 `DATA`（新增约束，设计未覆盖）
见 V4。影响：`--emit-whitelist` 必须按 PE 节属性区分数据/代码；手工编辑 `.def` 时给 vtable/typeinfo/单例补 `DATA`。否则 `auto`→`whitelist` 的切换对任何"消费者自己也定义同名 vtable"的 target（本例 `GameModuleStub`、`minEngineTests`）都会以 `multiple definition` 失败。

### 7.2 数字偏差（都以实测为准）
| 设计值 | 实测 | 原因 |
|---|---|---|
| in-repo 并集 1216，含外仓 1250 | S01-only 1250 / S02 后 1259 | S02 让 9 个数据/vtable 符号改走导入 |
| imgui 252、glad+glfw 13、其它 2 | 253 / 5+8 / 1 | 归类规则与设计不同（设计用 c++filt 人工归因，本脚本按 mangled 前缀 + 节属性） |
| `auto` 65533 | 回退后 65535 | `dllexport` 新增 2 条自动导出 |
| 补 `~5` 处 include | 8 处 | 逐个 `grep` 复核，8 个头文件缺直接 include |

### 7.3 缺口清单（明确未做，属档 2/3）
1. **未切 `explicit`**（S05）：`whitelist` 已可一条命令切到 `explicit`，但标注面只有 33 个头文件 / 94 处，切过去会大量缺符号，属于档 3。
2. **未做 S03 第三方契约**：imgui 仍靠白名单导出 253 条，glad 5 条、glfw 8 条同理；`IMGUI_API` 覆盖与 `glfw3.dll` 迁移未动（设计 Q3/Q4）。
3. **未全面标注 182 个类型**（S04）：白名单 992 条 minEngine 符号仍靠 `.def` 导出，不是 `MINENGINE_API`。
4. **未删 `--exclude-libs` / `--exclude-modules-for-implib`**（S06）：非 `auto` 模式下冗余但保留。
5. **白名单是"消费者契约快照"**：新增公开 API 后如果消费者尚未使用，不会自动进白名单；消费者一旦使用，链接期立刻 loud 失败 → 按 `.def` 头部命令重新生成即可。这是 `whitelist` 过渡期的设计代价（设计 Failure Model F1/F8）。
6. **外仓 minCraft 未重建**：只读扫描其 `libminCraftd.dll`（153 个 import，全部在白名单内）；如外仓以后用到新符号，按 F8 重新生成白名单。
7. `Maximum.exe`（陈旧 Playground）按设计 P3 排除在消费者之外（其 34 个 import 在当前 DLL 中已不存在）。

---

## 8 · 碰过的文件清单

**修改（12）**
```text
minEngine/minEngine/CMakeLists.txt                    链接选项 → minengine_apply_export_mode() + export-audit 目标
minEngine/Tests/CMakeLists.txt                        删除 MINENGINE_BUILD
minEngine/minEngine/src/Runtime/Core/EngineAPI.h      dllexport/dllimport 转正
minEngine/minEngine/src/Runtime/Core/Log/LogSystem.h            +include EngineAPI.h
minEngine/minEngine/src/Runtime/Core/Log/LogConsole.h           +include EngineAPI.h
minEngine/minEngine/src/Runtime/Core/Serialization/Serializer.h          +include
minEngine/minEngine/src/Runtime/Core/Serialization/SerializationTypes.h  +include
minEngine/minEngine/src/Runtime/Core/Serialization/DiskVersion.h         +include
minEngine/minEngine/src/Runtime/Core/Serialization/JsonArchive.h         +include
minEngine/minEngine/src/Runtime/Core/Serialization/BinaryArchive.h       +include
minEngine/minEngine/src/Runtime/Core/Reflection/ReflectionSample.h       +include
scripts/verify.ps1                                     +3 行 O1/O2 断言
```

**新增（4）**
```text
scripts/audit_dll_exports.py                          S00 审计脚本（三用途 + 覆盖核对）
minEngine/cmake/MinEngineExports.cmake                S01 三段式状态机
minEngine/cmake/minEngine_exports.def                 白名单（1259 行 / 124 条 DATA，生成物）
docs/ai/Platform/ProductBuild/BUILD-F02_S01_EXPORT_WHITELIST_IMPLEMENTATION.md   本文件
```

**未触碰**：`ScreenUICoords.*`、`SpriteQuadMesh.*`、`PointerConstraintState.*`、`GLFWWindowSystem.*`、`WindowSystem.h`、`PointerPolicyTest.*`、`Material.*`、`ShaderCompiler.*`、`SpriteMaterialFactory.*`、`ScreenUIMaterialFactory.*`、`EngineShaderUtils.*`、`RenderSystem.cpp`、`RHI/RHI.h`、`ScreenUIOrientationTest.*`、`MaterialCompileCacheTest.*`、`Editor/**`、`Examples/**`、`D:\Dev\GitRepo\minCraft*`、设计文档本身。
