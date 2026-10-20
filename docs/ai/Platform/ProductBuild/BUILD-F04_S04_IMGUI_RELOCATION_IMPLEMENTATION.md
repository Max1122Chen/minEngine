# BUILD-F04 S04 — imgui 家族迁入编辑器 DLL · 实现记录

## Meta
- **ID:** `BUILD-F04-S04`
- **Type:** Implementation record（BUILD-F04 切片 S04）
- **Status:** `Implemented — 已实测（构建 / 断言 / 反向验证 / 回退 / 启动冒烟全绿）`
- **Owner:** project maintainer
- **Last updated:** `2026-10-20`
- **设计依据（冻结，未修改）:** [BUILD-F04 §4/§6/§7/§8](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md)、
  [BUILD-F03 C1/C2/F4](./BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md)
- **前序实现记录:** [BUILD-F04_S01_S03_EDITOR_DLL_IMPLEMENTATION.md](./BUILD-F04_S01_S03_EDITOR_DLL_IMPLEMENTATION.md)（编辑器已是 SHARED + explicit + 导出 1 条）

---

## TL;DR

**imgui 家族（core + glfw/opengl3/vulkan 三个 backend + `imgui_freetype.cpp` + ImGuizmo）已从引擎 target 整体迁入 `MaximumEditorLib`（编辑器 DLL）。引擎导出 1549 → 1288，`imgui` 桶 = 0，`glfw` 桶 = 0。编辑器 DLL 对引擎的导入 577 → 324（其中 imgui 253 → 0）。进程内 `GImGui` 只剩一份，位于编辑器 DLL 内且不导出。**

三件必须知道的事：

1. **C2 成立并已接入断言**：`--forbid-buckets imgui`（连同 C1 的 `glfw`）进 `verify.ps1`，未知桶名、DLL 缺失、导出表为空都会**失败而不是跳过**。
2. **本片被迫提前做了 BUILD-F03 刀1 step①**：`imgui_impl_glfw.cpp` 需要 48 个 glfw 符号，而 glfw 的实现体原本**静态链在引擎 DLL 里**。经主人裁决（选项 A），glfw 改成**共享 `glfw3.dll`**（引擎 + 测试 + 编辑器 DLL 三方都链同一份），顺带达成 F03 C1（glfw 出表 −8）。**这超出"只删 imgui 桶"的范围，已在 §5 与 §10 逐条登记。**
3. **外仓 `minCraft*` 是 pre-S04 产物**（3 个编辑器宿主仍导入 imgui 253 条），C2 与"外仓覆盖 0 缺"在它们重建前互斥。经主人裁决，用**按桶、可自退役的过渡例外**处理（§7），`verify.ps1` 退出码仍为 0。

---

## 1. 迁移清单

### 1.1 源归属（引擎 target → 编辑器 DLL target）

| 单元 | 迁移前归属 | 迁移后归属 | 判定依据（实测） |
|---|---|---|---|
| imgui core（`imgui.cpp`/`imgui_draw.cpp`/`imgui_tables.cpp`/`imgui_widgets.cpp`） | 引擎 DLL | 编辑器 DLL | 引擎 src `ImGui::` 调用 **0**、`GImGui` **0**、`imgui_internal.h` **0**；253 条导出消费者只有编辑器 |
| `imgui_demo.cpp` | **两份都不编**（排除逻辑随迁） | 同左 | 编辑器也不用它；保留原排除规则 |
| `backends/imgui_impl_glfw.cpp` | 引擎 DLL | 编辑器 DLL | 唯一使用者 `Editor/src/Platform/EditorImGuiBackend.cpp` |
| `backends/imgui_impl_opengl3.cpp` | 引擎 DLL | 编辑器 DLL | 同上 |
| `backends/imgui_impl_vulkan.cpp`（`Vulkan_FOUND` 时） | 引擎 DLL（`target_sources`） | 编辑器 DLL（同条件） | 见 §1.2 |
| `imgui-upstream/misc/freetype/imgui_freetype.cpp` | 引擎 DLL | 编辑器 DLL | 只在 `IMGUI_ENABLE_FREETYPE` 下被 imgui core 引用，必须与 core 同 DLL |
| `ImGuizmo/*.cpp` | 引擎 DLL | 编辑器 DLL | 引擎侧仅一条注释（`SceneComponent.h:78`）；编辑器 3 个文件使用 |
| imgui-node-editor（4 cpp） | 编辑器 DLL（S02 起） | 不变 | 迁移后它的 `ImGui::` 变成同 DLL 内部引用 |

**"引擎侧是否真实引用 imgui"的判定（说明"没有停下来"）**：迁移前引擎 src 里唯一接触 imgui 的是 `Runtime/Function/Render/RenderSystem.cpp:3-5` 三行 include，该文件 **0** 个 `ImGui::` 调用。没有第二个接触点 —— 因此不存在"引擎真实引用 imgui（不止 include）"的情形。

### 1.2 编译选项与链接的迁移

| 项 | 迁移前 | 迁移后 |
|---|---|---|
| `IMGUI_ENABLE_FREETYPE` | 引擎 target `PRIVATE` | **编辑器侧 `PUBLIC`**（经 `minengine_imgui_headers` INTERFACE 传递，见 §1.4） |
| imgui 头目录 | 引擎 `PUBLIC`（`imgui`、`imgui-upstream/misc/freetype`、`ImGuizmo`） | 引擎**不再声明**；由 `minengine_imgui_headers` 提供给编辑器 DLL 与 minEngineTests |
| FreeType | 引擎 `PRIVATE freetype` | 引擎一份 + **编辑器 DLL 再链一份**（`PRIVATE freetype`，§4） |
| Vulkan backend | 引擎 `target_sources(+imgui_impl_vulkan.cpp)` | 编辑器 DLL：`find_package(Vulkan QUIET)` → `imgui_impl_vulkan.cpp` + `Vulkan::Vulkan`（引擎已把 Vulkan 头目录与 `MINENGINE_HAS_VULKAN=1` 设为 PUBLIC，编辑器继承） |
| glfw | 引擎 `-lglfw3`（静态 `libglfw3.a`） | **三方都链 `minengine_glfw`（`glfw3.dll` 导入库）**，见 §5 |

**§4.6 的配置漂移顺手修掉**：迁移前只有引擎 target 定义 `IMGUI_ENABLE_FREETYPE`，node-editor 的 TU 看不到它（走 `IMGUI_ENABLE_STB_TRUETYPE` 分支）——"侥幸正确"。迁移后该宏在编辑器侧 PUBLIC，imgui core / `imgui_freetype.cpp` / node-editor / 编辑器自身 TU **同一套配置**。

### 1.3 引擎侧死 include 删除

`minEngine/minEngine/src/Runtime/Function/Render/RenderSystem.cpp`：删掉
`#include "imgui.h"`、`#include "imgui/backends/imgui_impl_glfw.h"`、`#include "imgui/backends/imgui_impl_opengl3.h"`（3 行）。
**证据（比 grep 更强）**：引擎 target 的 imgui 头目录已被移除，引擎在这种情况下**全量重编 353 个 TU 并链接通过** —— 若还有任何一处 `#include "imgui*.h"`，编译会直接失败。

### 1.4 `minengine_imgui_headers` INTERFACE target（新增，非绕过）

`minEngine/CMakeLists.txt`（引擎根，`add_subdirectory` 之前）：

```cmake
add_library(minengine_imgui_headers INTERFACE)
target_include_directories(minengine_imgui_headers INTERFACE
    ${THIRD_PARTY_PATH}/imgui
    ${THIRD_PARTY_PATH}/imgui-upstream/misc/freetype
    ${THIRD_PARTY_PATH}/ImGuizmo)
target_compile_definitions(minengine_imgui_headers INTERFACE IMGUI_ENABLE_FREETYPE)
```

**为什么需要**：`minEngineTests` 会**编译 Editor/src 的一个子集**（`Tests/CMakeLists.txt:9-25`：DebugCommand/*、PropertyPath/*、PropertyEditPolicy、EditorCommandStack、EditorSettingsDefaults、EditorSpriteCommands、SpriteEditorServices、AssetEditorServices、AutomationPipeServer），这些 TU 经 `Editor/src/Shell/EditorSubModule.h:6` include `imgui.h`。迁移前它们**白蹭**引擎 target 的 `PUBLIC` imgui 头目录；把 imgui 头目录从引擎摘掉后，实测报错：

```
Editor/src/Shell/EditorSubModule.h:6:10: fatal error: imgui.h: No such file or directory
mingw32-make[3]: *** [Tests\CMakeFiles\minEngineTests.dir\build.make:1414: .../AssetEditorServices.cpp.obj] Error 1
```

**职责边界**：这个 INTERFACE target **只提供"谁能 include imgui.h"**（头目录 + 唯一配置的宏），**不编译任何 TU、不链接任何库**，因此不违反 I-onedll（imgui 的编译单元仍然只在编辑器 DLL 里）。它把"imgui 头文件属于编辑器"这件事写在一个地方，而不是在每个需要它的 target 上重复三行 `-I`。

> 提醒（不在本片修复）：`minEngineTests` 依赖 Editor/src 子集本身是 F03 §"测试独占 510 条"登记的历史债；本片只是把"imgui 头的提供者"从引擎换成显式接口。

### 1.5 引擎白名单 `.def`

`minEngine/cmake/minEngine_exports.def`：

| 桶 | 迁移前 | 迁移后 | 说明 |
|---|---|---|---|
| minEngine | 997 | 997 | 不动 |
| **imgui** | **253** | **0** | 224 `IMGUI_API` + 20 `IMGUI_IMPL_API` + 9 ImGuizmo，含 `GImGui DATA` |
| **glfw** | **8** | **0** | 引擎不再**定义** glfw（改成导入 `glfw3.dll`），`.def` 留着未定义符号会链接失败 |
| glad | 5 | 5 | 不动（详见 §10 的 glad 报告） |
| other | 1 | 1 | 不动 |
| **合计** | **1264** | **1003** | 配置日志实测：`minEngine exports: mode=whitelist, 1003 .def entries` |

**`LEGACY-EXTERNAL-DEBT` 区块逐字保留**（5 条），实测校验：

```
diff <(sed -n '/BEGIN LEGACY-EXTERNAL-DEBT/,$p' <S04 前快照>) \
     <(sed -n '/BEGIN LEGACY-EXTERNAL-DEBT/,$p' <当前 .def>)
→ LEGACY_IDENTICAL   （无输出即一致）
```

---

## 2. 数字（前后对照，全部本轮实测）

### 2.1 引擎 DLL

| 量 | 迁移前 | 迁移后 | Δ |
|---|---|---|---|
| 导出条目 `NumberOfFunctions` | **1549** | **1288** | **−261**（imgui −253、glfw −8） |
| `maxNamedOrdinal` / headroom | 1549 / **63986** | 1288 / **64247** | +261 余量 |
| 导出目录字节 | 113969 | 102590 | −11379 |
| 桶构成 | minEngine 1282 / **imgui 253** / glad 5 / **glfw 8** / other 1 | minEngine 1282 / glad 5 / other 1 | **imgui = 0，glfw = 0** |
| 文件体积 | 531,407,389 B | **525,364,432 B** | −6,042,957 B |
| `GImGui` | 在导出表（`DATA`） | **不存在（导出表与符号表都没有）** | — |

`--report` 原文（迁移后）：

```
NumberOfFunctions ... 1288   (NumberOfNames 1288)
maxNamedOrdinal ... 1288   PE limit 65535   headroom 64247
export dir bytes ... 102590
composition:
  minEngine          1282
  glad                  5
  other                 1
```

### 2.2 编辑器 DLL（`libMaximumEditord.dll`）

| 量 | 迁移前 | 迁移后 | 说明 |
|---|---|---|---|
| 对**引擎**导入 | **577**（imgui **253** + minEngine 324） | **324**（imgui **0**） | 本片的主指标 |
| 导入总数 | 858 | 779 | 新增 `glfw3.dll`(52)、`vulkan-1.dll`(78)；libstdc++ 156 → 156 |
| 导出条目 | 1 | **1**（`_ZN9minEngine17CreateApplicationEv`） | explicit 模式，imgui 不外泄 |
| 文件体积 | 187,973,485 B | **196,157,738 B** | **+8,184,253 B**（imgui 全族 + 第二份 FreeType + Vulkan backend） |

### 2.3 宿主与测试

| 产物 | 迁移前 | 迁移后 |
|---|---|---|
| `MaximumEditor.exe` | 323,536 B / 引擎导入 3 | **323,536 B** / 引擎导入 **3** / 编辑器 DLL 导入 1 |
| `SampleEditor.exe` | 323,922 B / 引擎导入 3 | **323,922 B** / 引擎导入 **3** / 编辑器 DLL 导入 1 |
| `minEngineTests.exe` | 137,419,254 B / 引擎导入 807（glad 5 + glfw 8 + minEngine 793 + other 1） | **137,419,430 B** / 引擎导入 **799**（glad 5 + minEngine 793 + other 1；glfw 改为直接链 `glfw3.dll`） |
| `SampleGame.exe` | 341,235 B / 导入 7 | 341,235 B / 导入 7 |
| `libGameModuleStubd.dll` | 10,075,465 B / 导入 26 | 10,075,465 B / 导入 26 |
| `glfw3.dll` | **bin 里不存在** | **305,664 B**（bin 与 `Staging/SampleGame` 都有） |

**五 target 全部链接**（`BUILD_EXIT=0`）：`minEngineTests` · `MaximumEditor.exe` · `SampleEditor.exe` · `SampleGame.exe` · `libGameModuleStubd.dll`。

---

## 3. `GImGui` 唯一性（I-gimgui / O4）

### 3.1 实测证据（nm 符号表，不是导出表）

```
D:/Dev/mingw64/bin/nm.exe --defined-only minEngine/bin/libminEngined.dll     | grep -w GImGui
→ （无输出：引擎 DLL 里连符号都没有）

D:/Dev/mingw64/bin/nm.exe --defined-only minEngine/bin/libMaximumEditord.dll | grep -w GImGui
→ 0000000350eee580 B GImGui          ← 唯一一份，在编辑器 DLL 内
→ 0000000350e1a920 R .refptr.GImGui  （引用桩）
```

交叉计数（同名符号定义数）：

| DLL | `GImGui` | imgui 命名的已定义符号 | 导出表里的 imgui 条目 |
|---|---|---|---|
| `libminEngined.dll` | **0** | 1（`_ZN9minEngine20GetImGuiSceneColorUvEv`，引擎自有 API，非 imgui） | **0** |
| `libMaximumEditord.dll` | **1**（`B`） | **5611** | **0**（编辑器只导出 1 条 `CreateApplication`） |

### 3.2 三层断言（缺一层就会出现"导出看不见、进程里两份"的盲区）

1. **CMake 源表断言（最早、最强）** —— `minEngine/minEngine/CMakeLists.txt` 末尾：
   `get_target_property(... SOURCES)` 后逐条匹配 `(^|/)(imgui|imgui-upstream|imgui-node-editor|ImGuizmo)(/|$)` → `FATAL_ERROR`。
   它拦的是"把 imgui 源搬回引擎但**没**把它写进 .def"这种情形 —— 那时导出表干净，但进程里会有两份 `GImGui`。
2. **导出桶断言（二进制级）** —— `--forbid-buckets imgui,glfw`（§6.1）。
3. **编辑器导出预算 + 无 imgui 泄漏** —— `audit_module_boundaries.py`（S01 起就有，本片继续 PASS：`editor DLL leaks no imgui export 0`）。

三层都做过**反向验证**（§6.4）。

---

## 4. FreeType 双实例（设计 §4.5 选项① / Q7 已接受的代价）

**事实**：引擎自身要用 FreeType 渲染字体（`Runtime/Function/UI/Font/FontGlyphAtlas.cpp`，`FT_Init_FreeType` / `FT_New_Memory_Face`），`imgui_freetype.cpp` 也要。二者必须在**各自 DLL 内**各有一份，因此 FreeType 是 I-onedll 的**唯一被批准的例外**。

**实测（两份都在，且是两份独立实例）**：

```
libminEngined.dll      FT_ defined symbols: 256   FT_Init_FreeType @ 0x2904f98d5
libMaximumEditord.dll  FT_ defined symbols: 297   FT_Init_FreeType @ 0x350b22ef5
```

两个不同的地址 = 两份真实的代码副本（不是同一个符号被两个模块看见）。符号数不同是因为各自从 `libfreetype.a` 里拉取的对象集合不同。

**为什么安全**：FreeType 没有跨实例全局契约（每个 `FT_Library` 自包含），两份实例互不可见也无共享状态；代价是 ~1–2 MB 代码重复与两次初始化。**这份重复不是 bug，是 Q7 的裁决结果，不要试图消除**（转第三方 DLL 是 Deferred-4）。
`--exclude-libs=...:libfreetype.a:libfreetyped.a` 仍然生效：编辑器 DLL 导出表里没有任何 `FT_` 符号（导出 1 条）。

---

## 5. glfw 共享化（被迫提前的架构变更 · 主人裁决 A）

### 5.1 根因（实测）

imgui 迁走后，`imgui_impl_glfw.cpp` 在编辑器 DLL 里链接失败：

```
imgui_impl_glfw.cpp:466: undefined reference to `glfwGetKey'
... 48 个不同符号 / 85 处引用：glfwCreateStandardCursor glfwSetErrorCallback glfwGetWindowAttrib
    glfwCreateWindowSurface glfwGetMonitors glfwSetClipboardString glfwGetPlatform ...
```

因为 glfw 的进程级唯一实例**住在引擎 DLL 内部**：

- `libminEngined.dll` 的导入表里**没有** glfw3.dll（只有 kernel32/user32/libstdc++/vulkan-1/libassimp-6 …）；
- 引擎 DLL 自己**定义**了 8 个 glfw 导出（`glfwInit`/`glfwCreateWindow`/`glfwMakeContextCurrent`/`glfwGetCurrentContext`/`glfwDestroyWindow`/`glfwTerminate`/`glfwWindowHint`/`glfwGetProcAddress`）→ 说明链的是**静态 `libglfw3.a`**；
- `bin` 里没有 `glfw3.dll`。

给编辑器再静态链一份 = 进程内两份 glfw 状态（窗口/回调/输入全部分裂），比双 `GImGui` 更致命。

### 5.2 决策与实施

主人裁决 **A**（= BUILD-F03 刀1 step① 提前执行）。实施：

| 位置 | 改动 |
|---|---|
| `minEngine/CMakeLists.txt` | 新增 **IMPORTED GLOBAL** target `minengine_glfw`（`IMPORTED_LOCATION=glfw3.dll`、`IMPORTED_IMPLIB=libglfw3dll.a`、INTERFACE 头目录）；`glfw3.dll`/导入库缺失 → `FATAL_ERROR` |
| `minEngine/minEngine/CMakeLists.txt` | `PRIVATE glfw3` → `PRIVATE minengine_glfw`；新增 POST_BUILD `copy_if_different glfw3.dll → ${EXECUTABLE_PATH}` |
| `minEngine/Editor/CMakeLists.txt` | `target_link_libraries(MaximumEditorLib PRIVATE minengine_glfw)` |
| `minEngine/Tests/CMakeLists.txt` | `target_link_libraries(minEngineTests PRIVATE minengine_glfw)` |
| `.def` | 删 glfw 8 条（链接器强制） |
| `minEngine/Examples/SampleGameHost/CMakeLists.txt` | `RUNTIME_TARGETS GameModuleStub minEngine **minengine_glfw**` |
| `Launcher/Templates/GameProject/CMakeLists.txt` | `RUNTIME_TARGETS ${GAME_MODULE_NAME} minEngine **minengine_glfw**` |

### 5.3 安全闸（主人硬条件 1，实测通过）

只读扫描 `minEngine/bin/*` + `../minCraft*/bin/*`（16 个 PE）对引擎契约的 glfw 导入：

```
minEngine/bin/minEngineTests.exe    engine_imports 807  glfw 8  ← 唯一消费者
其余 15 个（含全部外仓 MinCraft*/MaximumEditor/MinCraftEditor/VoxelWorldTests/libminCraft）
                                   glfw 0
```

**外仓没有任何消费者导入 glfw** → 外仓零改动即可受益（它们下次构建时拿到的引擎 DLL 自己也不再导出 glfw）。

### 5.4 缺 `glfw3.dll` 必须是响亮的失败（主人硬条件 6，实测）

```
mv bin/glfw3.dll glfw3.dll.s04bak
MaximumEditor.exe --version   → 0xC0000135  (STATUS_DLL_NOT_FOUND，进程根本没起来)
minEngineTests.exe test smoke  → 0xC0000135
mv back                        → Maximum 0.0.9 (exit 0)
```

没有"静默降级"路径：Windows 加载器在 `DllMain` 之前就拒绝。

### 5.5 Staging 实测（主人硬条件 4）

`cmake --build --target package-SampleGame` → `minEngine/Staging/SampleGame/`：

```
SampleGame.exe 341235 | glfw3.dll 305664 | libGameModuleStubd.dll 10075465 | libminEngined.dll 525364432
```

---

## 6. 断言与验证

### 6.1 `--forbid-buckets`（`scripts/audit_dll_exports.py`，新增）

- 用法：`--forbid-buckets imgui,glfw`（重复或逗号分隔），**必须与 `--fail` 同用**（否则报错退出 2 —— 不允许"传了参数却什么都没断言"）。
- **未知桶名 → 退出 2**（防拼写错误静默通过）。
- **DLL 不存在 → 退出 2**；**导出表为空/无导出目录 → 失败**（"assertion would pass vacuously"），这两条专治"扫不到就当作通过"。
- 命中时打印每个违禁桶的条数与示例名。

`verify.ps1` 中的接入（位于 `minEngineTests` 构建之后、边界断言之前）：

```powershell
python ...\audit_dll_exports.py --max-exports 3000 --fail --forbid-buckets imgui,glfw
```

实测输出：

```
PASS: NumberOfFunctions=1288 <= 3000, ordinal headroom=64247 >= 60000
PASS: export table contains no entry of bucket(s) glfw,imgui
```

### 6.2 in-repo 覆盖（新增断言，永不豁免）

`verify.ps1` 增加一步：对**引擎契约**跑 `--verify-coverage`（不传 `--transitional-missing-bucket`）。这是"删掉的 253 条不是某个 in-repo 产物真正需要的东西"的直接证明：

```
coverage (exported >= consumed), contract libminengine:
  minEngine/bin/MaximumEditor.exe        imported     3  missing     0
  minEngine/bin/libMaximumEditord.dll    imported   324  missing     0
  minEngine/bin/minEngineTests.exe       imported   799  missing     0
  ... (7 个 in-repo 消费者)
coverage total missing = 0
coverage in-repo missing = 0 (never tolerated)
```

### 6.3 `verify.ps1` 最终结果

```
PASS: NumberOfFunctions=1288 <= 3000, ordinal headroom=64247 >= 60000
PASS: export table contains no entry of bucket(s) glfw,imgui
coverage total missing = 0                         ← 引擎 / in-repo
PASS: editor/module boundary holds                  ← 含 "editor DLL leaks no imgui export 0"
coverage total missing = 0                         ← 编辑器 DLL 契约
coverage total missing = 759
coverage in-repo missing = 0 (never tolerated)
coverage unexpected missing = 0
WARN TRANSITIONAL EXCEPTION: 759 missing symbols tolerated in bucket 'imgui' - external editor hosts are pre-S04 artifacts and must be rebuilt against a post-S04 engine
[doctest] Status: SUCCESS!
[17:08:22] [Core] LogSystem Shutdown
VERIFY_EXIT=0
```

### 6.4 反向验证汇总（每条断言怎么弄红、原文）

| 断言 | 制造红灯的方式 | 实测结果 |
|---|---|---|
| CMake I-gimgui（源表） | 把 `${THIRD_PARTY_PATH}/imgui/imgui.cpp` 加回引擎 target | `CMake Error ... I-gimgui violated (BUILD-F04 S04): the engine target compiles '.../Third-Party/imgui/imgui.cpp'. The imgui family belongs to MaximumEditorLib ... a second copy in the engine DLL means two GImGui contexts in one process.` → `CONFIGURE_EXIT=1` |
| `--forbid-buckets imgui,glfw` | 完整回退（imgui 源 + .def 253 行 + 静态 glfw + .def 8 行，见 §9） | `FAIL: forbidden bucket 'glfw': 8 export(s), e.g. glfwCreateWindow, glfwDestroyWindow, glfwGetCurrentContext` / `FAIL: forbidden bucket 'imgui': 253 export(s), e.g. GImGui, _Z21ImGui_ImplVulkan_InitP25ImGui_ImplVulkan_InitInfo ...` → `EXIT=1` |
| `--forbid-buckets` 非空泛 | 迁移中途（glfw 尚在引擎定义时）用 `glfw` 桶做对照 | `FAIL: forbidden bucket 'glfw': 8 export(s) ...` → exit 1 |
| 未知桶名 | `--forbid-buckets imguiu` | `error: unknown bucket 'imguiu' in --forbid-buckets (known: ...)` → exit 2 |
| DLL 缺失 | `--dll minEngine/bin/libNope.dll` | `error: DLL not found` → exit 2 |
| 过渡例外撤掉 | 外部覆盖检查去掉 `--transitional-missing-bucket` | `FAIL: 759 consumer imports are not exported ... (0 tolerated, 0 in-repo)` → exit 1 |
| 例外不作用于 in-repo | 把 `../minCraft/bin/MinCraftEditor.exe` 复制到 `minEngine/build/s04_inrepo_probe/` 再扫 | `FAIL: 253 ... (0 tolerated, 253 in-repo)`；`coverage in-repo missing = 253 (never tolerated)` → exit 1 |
| 例外不掩盖非 imgui 缺失 | 在仓库**外**临时目录造一个真消费者：复制 `MinCraftEditor.exe` 并把一条非 imgui 导入名首字节改掉 | `imported 573 missing 254 (253 tolerated: imgui)` + `coverage unexpected missing = 1` + `FAIL: 1 consumer imports are not exported` → exit 1 |
| 缺 `glfw3.dll` | 移走 `bin/glfw3.dll` | 编辑器与测试均 `0xC0000135`；还原后 `Maximum 0.0.9` |

---

## 7. 过渡例外（`--transitional-missing-bucket`）

### 7.1 为什么需要

C2 规定"引擎导出中不得出现 imgui 桶"。一旦成立，**任何消费者仍导入 imgui 符号，在语义上就等价于该消费者是 pre-S04 陈旧产物** —— 它不可能是契约回归。外仓 `minCraft*` 的三个编辑器宿主正是这种产物（实测各导入 imgui 253 条：`minCraft-ui/bin/MaximumEditor.exe`、`minCraft-ui/bin/MinCraftEditor.exe`、`minCraft/bin/MinCraftEditor.exe`），而它们**必须由各自仓库重建**才能吃到 S02/S04 的新形态。本片被禁止构建/修改 `minCraft*`。

在"不放宽"与"verify 永远是红的"之间，主人裁决采用**有界、可见、可自退役**的过渡例外。

### 7.2 为什么按桶而不是按路径

按路径允许（例如"允许 `../minCraft-ui/bin/MinCraftEditor.exe`"）需要有人记得在重建后**删除那条允许**，否则它会永久掩盖该消费者的真实回归。
按**桶**允许则**自退役**：外仓重建后不再导入 imgui → 容忍计数自然归零 → 参数即使留着也什么都不放过。它的"豁免域"就是"我们刚刚删掉的那一类符号"，逻辑上与 C2 同源。

### 7.3 实现（四条硬性质 + 实测）

| 性质 | 实现 | 实测 |
|---|---|---|
| 只容忍指定桶 | `classify(name) in tolerated_buckets` | 253 tolerated，其余 0 |
| **逐条打印全部 missing** | 每个容忍名一行 `tolerated[imgui]: <name>`（不截断） | `grep -c 'tolerated\[imgui\]'` = **759** |
| 非该桶缺失仍 fail | `unexpected_missing > 0 → exit 1` | 外部造 1 条非 imgui 缺失 → exit 1 |
| **绝不作用于 in-repo** | 消费者路径落在仓库根内 → 一律 fatal | 探针 253 → `in-repo missing = 253` → exit 1 |
| 醒目 WARN | `WARN TRANSITIONAL EXCEPTION: ...` | 见 §6.3 |
| 未知桶名报错 | 与 `--forbid-buckets` 同一套校验 | exit 2 |
| 无 `--verify-coverage` 时报错 | 不允许"传了但不起作用" | exit 2 |

**摘要行**（每次外部扫描都打印，醒目且可被 CI grep）：

```
coverage total missing = 759
coverage in-repo missing = 0 (never tolerated)
coverage unexpected missing = 0 (anything outside the tolerated bucket(s))
WARN TRANSITIONAL EXCEPTION: 759 missing symbols tolerated in bucket 'imgui' - external editor hosts are pre-S04 artifacts and must be rebuilt against a post-S04 engine
WARN the tolerated names are printed above, one per line; this exception retires itself when the external hosts stop importing that bucket
```

### 7.4 为什么这不是"放宽断言变绿"

1. **它不改变任何绿/红边界之外的判定**：in-repo 消费者、非 imgui 缺失、DLL 缺失、空导出表，全部照旧失败（§6.4 逐条实测）。
2. **它的豁免域与 C2 同源且自退役**：容忍的恰好是"引擎已被禁止导出的那一类符号"，且外仓重建后自动失效（无需人删）。
3. **它可见**：759 个名字逐个打印 + 每次一条 WARN + 摘要把三个 missing 口径分开报。
4. **它有移除条件，且写在调用点**：`scripts/verify.ps1` 注释：

   > REMOVAL CONDITION: delete `--transitional-missing-bucket` below as soon as the external
   > `minCraft*` editor hosts are rebuilt against a post-S04 engine (their imgui imports go to 0).

5. **反向证明它在兜底**：去掉该参数 → 同一命令 exit 1（759 missing）。也就是说绿不是别的原因造成的。

---

## 8. 编辑器启动验证（不是"链接通过"就算数）

### 8.1 结论：**存在自动化启动路径**

本仓有 Editor Automation Host（ED-F17 / CLI-F02）：

```
MaximumEditor.exe --automation-pipe <name> --automation-ready-file <path> -p <project.meproject>
```

编辑器 `Listen()` 成功后写 ready-file（`AutomationHost.cpp:57`），之后可通过命名管道收发长度前缀的 JSON 帧（`AutomationPipeServer`：4 字节小端长度 + UTF-8 body）。协议 v1，请求形如 `{protocolVersion, requestId, kind, op, sessionId, payload}`。

**没有** headless / `-batch` 的**编辑器**模式（`test` 子命令跑的是无窗口的测试套件，不是编辑器）；因此冒烟仍需要一个桌面窗口。

新增 `scripts/smoke_editor_automation.py`（可复现）：

```powershell
python scripts/smoke_editor_automation.py --project minEngine/MyMEProject/MyMEProject.meproject
```

它启动编辑器 → 等 ready-file → 连接管道 → 依次 `editor.ping` / `editor.get_state` /
`editor.get_capabilities` / `editor.get_log` / `reflect.list_classes` / `scene.get_hierarchy` → 结束进程。

**实测（最终产物，`SMOKE_EXIT=0`）**：

```
ready: AUTOMATION_READY pipe=\\.\pipe\me-smoke-45456-f119d101
PASS editor.ping              alive=True editorProduct=Maximum protocolVersion=1
PASS editor.get_state         defaultSessionId=sess-1 projectPath=.../MyMEProject ready=True sessions=1
PASS editor.get_capabilities  domains=9 editorProduct=Maximum editorVersion=0.0.9 protocolVersion=1
PASS editor.get_log           entries=108 logFile=... ringTruncated=False
PASS reflect.list_classes     classes=131
PASS scene.get_hierarchy      roots=8
editor stopped (pid 48116)
smoke PASS
```

会话日志（`minEngine/bin/Logs/minEngine-20261020-164042-314-8796.log`）**0 条 Error/Critical、7 条 Warn（历史资源迁移提示）**，且有：

```
[Platform] [Info] GLFW Window Initialized (OpenGL 4.6) (GLFWWindowSystem.cpp:200)
[Editor]  [Info] Editor default scene 'test' loaded successfully. (Editor.cpp:276)
[Editor]  [Info] AutomationHost: listening on '\\.\pipe\me-smoke-...'
```

即：窗口起来了、imgui 帧循环在跑（自动化请求被逐帧处理）、场景加载成功、反射 131 个类可查。

### 8.2 验证缺口（明确登记）

自动化冒烟**不能**证明以下内容，本轮**未**做像素/交互级验证：

- imgui 面板/停靠布局是否绘制正确、字体图集是否正确（FreeType 第二实例的真实渲染质量）；
- ImGuizmo 操作器、imgui-node-editor（Material/AnimGraph）的交互；
- 中文/自定义字体字形是否完整；
- PIE 进入/退出；
- 键鼠输入链路（设计 F7：imgui 的 GLFW backend 与引擎 `GLFWWindowSystem` 的回调链顺序）。

### 8.3 手工冒烟清单（精确到操作）

1. `cd minEngine\bin`，`MaximumEditor.exe -p ..\MyMEProject\MyMEProject.meproject`
2. 窗口打开后**目视**：菜单栏、Content Browser、Inspector、Console 面板齐全；**字体清晰无方块**（中文字符串若出现在面板标题上应正常显示）。
3. 拖动面板/停靠/重置布局：无闪烁、无 `IM_ASSERT` 弹窗、无窗口 ID 冲突导致的点击错位。
4. 点击层级中的对象 → Inspector 属性可编辑；拖动数字框。
5. 打开 Material 图表（node-editor）：节点可拖动/连线；保存材质。
6. 打开 AnimGraph（node-editor）：同上。
7. 在视口中用 ImGuizmo 拖动物体（移动/旋转/缩放）。
8. 键盘/鼠标输入：在 Inspector 文本框输入（含退格、Ctrl+V）、鼠标滚轮缩放。
9. 点击 Play 进入 PIE（约 5 秒）→ 停止 → 场景恢复。
10. File → Open Project 一个**资产-only** 项目；退出编辑器（`Alt+F4`），确认无崩溃、`Logs/*.log` 尾部无 Error。
11. （Vulkan 路径）`MaximumEditor.exe --rhi vulkan -p ...` 重复 2/3/9。

---

## 9. 回退

### 9.1 回退到 pre-S04（imgui 回引擎 + glfw 回静态）—— **本轮已实测**

快照位置：`minEngine/build/s04_before/`（迁移前）与 `minEngine/build/s04_after/`（迁移后）。

```powershell
# 0) 备份当前 S04 状态（否则没有回头路）
cp minEngine/minEngine/CMakeLists.txt                 minEngine/build/s04_after/minEngine_CMakeLists.txt
cp minEngine/Editor/CMakeLists.txt                    minEngine/build/s04_after/Editor_CMakeLists.txt
cp minEngine/minEngine/src/Runtime/Function/Render/RenderSystem.cpp minEngine/build/s04_after/RenderSystem.cpp
cp minEngine/cmake/minEngine_exports.def              minEngine/build/s04_after/minEngine_exports.def

# 1) 源与白名单回退（4 个文件）
cp minEngine/build/s04_before/minEngine_CMakeLists.txt minEngine/minEngine/CMakeLists.txt
cp minEngine/build/s04_before/Editor_CMakeLists.txt    minEngine/Editor/CMakeLists.txt
cp minEngine/build/s04_before/RenderSystem.cpp         minEngine/minEngine/src/Runtime/Function/Render/RenderSystem.cpp
cp minEngine/build/s04_before/minEngine_exports.def    minEngine/cmake/minEngine_exports.def

# 2) 重配置 + 重建（引擎与编辑器 DLL）
cmake -S minEngine -B minEngine/build
cmake --build minEngine/build --target minEngine Editor -j 6
```

**不依赖 `build/s04_before` 快照时的等价手工操作**（快照只是本轮实测用的便利副本，`build/` 不入库）：

- `minEngine/minEngine/CMakeLists.txt`：把 `file(GLOB IMGUI ...)` / `list(FILTER ... imgui_demo)` /
  `IMGUI_UPSTREAM_DIR` / `IMGUI_FREETYPE_SRC` / `file(GLOB IMGUIZMO ...)` 段加回；
  `add_library(${ENGINE_NAME} SHARED ...)` 源列表加回 `${IMGUI} ${IMGUI_FREETYPE_SRC} ${IMGUIZMO}`；
  `target_compile_definitions(...)` 加回 `IMGUI_ENABLE_FREETYPE`；
  `target_include_directories` 加回 `imgui`、`imgui-upstream/misc/freetype`、`ImGuizmo`；
  Vulkan 分支加回 `target_sources(... imgui_impl_vulkan.cpp)`；删除末尾的 I-gimgui 断言块；
  `PRIVATE minengine_glfw` 改回 `PRIVATE glfw3`；删除 POST_BUILD 的 glfw3.dll 拷贝。
- `minEngine/Editor/CMakeLists.txt`：删除 `file(GLOB IMGUI/IMGUIZMO)` 段与 `${IMGUI} ${IMGUI_FREETYPE_SRC} ${IMGUIZMO}`
  三个源；删除 `freetype` / `minengine_glfw` / `Vulkan` / `minengine_imgui_headers` 四处链接与对应注释。
- `RenderSystem.cpp`：加回 3 行 imgui include。
- `.def`：加回 253 条 imgui + 8 条 glfw（`--emit-whitelist` 从**重建后的外仓消费者**导入表重生成是最省事的做法）。

注意：pre-S04 的引擎 CMakeLists 里 `target_link_libraries(... PRIVATE glfw3 ...)` 现在解析为**静态 `libglfw3.a`**（`minengine_glfw` 是另一个 target 名），所以"glfw 退回静态"与"imgui 退回引擎"是同一次文件回退，不需要额外操作。`minengine_imgui_headers`/`minengine_glfw` 两个顶层 INTERFACE/IMPORTED target 保持存在是**无害的**（只是没人再链它们，除 Tests）；若要彻底回到 pre-S04，把 `minEngine/CMakeLists.txt` 里这两段一并删除即可。

**实测结果（本轮）**：

| 观察点 | 回退后 |
|---|---|
| `cmake --build --target minEngine Editor` | `BUILD_EXIT=0`（引擎 + 编辑器都链接成功） |
| 引擎导出 | **1549**（与迁移前完全一致） |
| 桶构成 | minEngine 1282 / **imgui 253** / glad 5 / **glfw 8** / other 1 |
| `.def` 条目 | **1264**（minEngine 997 / imgui 253 / glad 5 / glfw 8 / other 1） |
| 引擎 DLL 体积 | **531,407,389 B —— 与迁移前逐字节同尺寸** |
| `--forbid-buckets imgui,glfw --fail` | **exit 1**（imgui 253 + glfw 8 报红） |

回退可用性结论：**可用，已实测**。回退后 C1/C2 断言按设计变红（它们在保护"imgui/glfw 不得在引擎导出里"这件事），恢复 S04 状态后重新变绿（§9.2）。

### 9.2 恢复（本轮实测）

```powershell
cp minEngine/build/s04_after/*（4 个文件）→ 原位
cmake -S minEngine -B minEngine/build
cmake --build minEngine/build --target Editor SampleEditor SampleGame GameModuleStub minEngineTests -j 6
```

→ `BUILD_EXIT=0`，引擎导出回到 **1288**（imgui/glfw 桶 0），`verify.ps1` **exit 0**（§6.3）。

---

## 10. 偏差、未完成项、需要主人知情的事

| # | 事项 | 性质 | 处置 |
|---|---|---|---|
| D1 | **glfw 归属变更**（引擎静态 → 三方共享 `glfw3.dll`） | 超出任务书；设计 §Scope Non-goals #5 说"不动 glfw 归属（归 BUILD-F03 刀1）" | 阻塞上报 → 主人裁决 **A** → 执行。属 BUILD-F03 刀1 step① 提前落地 |
| D2 | **`.def` 删了 glfw 8 条**，任务书写的是"只删 imgui 桶" | 链接器强制（引擎不再定义这些符号） | 已记录；`LEGACY-EXTERNAL-DEBT` 逐字未动 |
| D3 | `minengine_imgui_headers` INTERFACE target（新增） | 修 minEngineTests 的真实编译失败 | 已接受（§1.4），职责边界已写明 |
| D4 | `minEngine/Tests/CMakeLists.txt`、`Examples/SampleGameHost/CMakeLists.txt`、`Launcher/Templates/GameProject/CMakeLists.txt` 被改 | 链接/打包的必要改动 | 已记录 |
| D5 | 新增 `scripts/smoke_editor_automation.py` | 新工具（启动冒烟的自动化载体） | 已记录；S05/S06 可复用 |
| D6 | **外仓 759 条过渡债**（3 个编辑器宿主 × imgui 253） | 必须在 `minCraft*` 各自仓库重建后消失 | 过渡例外（§7）+ 移除条件写在 `verify.ps1` |
| D7 | `glad` 仍有 **5 条**引擎导出（消费者 = `minEngineTests`） | 与 glfw 同性质（第三方 C 符号出表，F03 C1 的一部分） | **本轮未动**，仅报告（见下） |
| D8 | 未做 MSVC 实测 / ASan / UI 像素验证 / 编辑器插件 ABI | 设计 Deferred + §8.2 验证缺口 | 已在 §8.2/§8.3 登记 |
| D9 | **观察（与本片无关的既有行为）**：任何一次编辑器启动都会把 15 个 `MyMEProject/Assets/**/*.meta` 重写成新 schema（`$schemaVersion` 顺序变化、新增 `RuntimeClassName`/`SourcePath`、结尾换行变化） | 既有资产系统行为，不是 S04 引入 | 本轮已把这 15 个文件按 HEAD 原样恢复（`git status` 干净）；**建议单独开 bug/feature**：编辑器不该在只读打开时升级 meta 版本 |

**D7 的 glad 情况（按要求只报告）**：引擎导出里 `glad` 5 条（`GLAD_GL_VERSION_4_6` 等），唯一消费者是 `minEngineTests.exe`（它自己 `gladLoadGLLoader`，但仍从引擎导入这 5 个）。这正是 BUILD-F03 刀1 step② 的内容（"把 `glad.c` 编进 minEngineTests"）。它与本片无耦合：本片既没有让 glad 变多也没有变少。建议留给 F03 刀1 一并处理，**不要**在 S05/S06 顺手改。

---

## 11. 碰过的文件清单

**源码 / 构建**
- `minEngine/CMakeLists.txt`（新增 `minengine_imgui_headers`、`minengine_glfw`）
- `minEngine/minEngine/CMakeLists.txt`（移除 imgui 家族源/头/宏/Vulkan backend；glfw 共享；POST_BUILD 配 glfw3.dll；新增 I-gimgui 源表断言）
- `minEngine/Editor/CMakeLists.txt`（接收 imgui 家族源；链接 freetype / minengine_glfw / Vulkan；`IMGUI_ENABLE_FREETYPE` PUBLIC 经头接口）
- `minEngine/Tests/CMakeLists.txt`（链接 `minengine_imgui_headers`、`minengine_glfw`）
- `minEngine/minEngine/src/Runtime/Function/Render/RenderSystem.cpp`（删 3 行死 include）
- `minEngine/cmake/minEngine_exports.def`（−253 imgui、−8 glfw；legacy 逐字保留）
- `minEngine/Examples/SampleGameHost/CMakeLists.txt`、`Launcher/Templates/GameProject/CMakeLists.txt`（打包带 glfw3.dll）

**脚本**
- `scripts/audit_dll_exports.py`（`--forbid-buckets`、`--transitional-missing-bucket`、`check_coverage` 重写）
- `scripts/verify.ps1`（C1/C2 断言、in-repo 覆盖、过渡例外 + 移除条件）
- `scripts/smoke_editor_automation.py`（新增）

**文档**
- 本文件

**明确未碰**：`ScreenUICoords.*`、`SpriteQuadMesh.*`、`PointerConstraintState.*`、`GLFWWindowSystem.*`、`WindowSystem.h`、`Render/Material.*`、`ShaderCompiler.*`、`SpriteMaterialFactory.*`、`ScreenUIMaterialFactory.*`、`EngineShaderUtils.*`、`RHI/RHI.h`、`Tests/Suites/*`、`FEATURE_REGISTRY.md`、`bugs/*`、`BUILD-F03_*`、`BUILD-F04_*`（设计冻结）、`CORE-F38_*`、`D:\Dev\GitRepo\minCraft*`、`.def` 的 `LEGACY-EXTERNAL-DEBT` 区块。

---

## 12. 可复现命令

```powershell
# 导出面与桶构成（O1/O2 + C1/C2）
python scripts/audit_dll_exports.py --report
python scripts/audit_dll_exports.py --max-exports 3000 --fail --forbid-buckets imgui,glfw

# 覆盖（in-repo，永不豁免）
python scripts/audit_dll_exports.py --dll minEngine/bin/libminEngined.dll --verify-coverage
python scripts/audit_dll_exports.py --dll minEngine/bin/libMaximumEditord.dll --verify-coverage

# 覆盖（外仓，含过渡例外）
python scripts/audit_dll_exports.py --verify-coverage --consumer-dir ../minCraft/bin \
    --consumer-dir ../minCraft-ui/bin --consumer-dir ../minCraft-gameplay/bin \
    --transitional-missing-bucket imgui

# GImGui 唯一性
D:/Dev/mingw64/bin/nm.exe --defined-only minEngine/bin/libminEngined.dll     | grep -w GImGui   # 无
D:/Dev/mingw64/bin/nm.exe --defined-only minEngine/bin/libMaximumEditord.dll | grep -w GImGui   # 1 条 'B'

# FreeType 两份实例
D:/Dev/mingw64/bin/nm.exe --defined-only minEngine/bin/libminEngined.dll     | grep -c FT_      # 256
D:/Dev/mingw64/bin/nm.exe --defined-only minEngine/bin/libMaximumEditord.dll | grep -c FT_      # 297

# 编辑器启动冒烟（需要桌面窗口）
python scripts/smoke_editor_automation.py --project minEngine/MyMEProject/MyMEProject.meproject

# 全量门禁
powershell -File scripts/verify.ps1
```
