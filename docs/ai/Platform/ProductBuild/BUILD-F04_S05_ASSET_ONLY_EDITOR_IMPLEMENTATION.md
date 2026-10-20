# BUILD-F04 S05 — 通用编辑器重新定位为「资产-only 项目编辑器」· 缺匹配游戏模块硬报错 · 实现记录

## Meta
- **ID:** `BUILD-F04-S05`
- **Type:** Implementation record（BUILD-F04 切片 S05）
- **Status:** `Implemented — 已实测（正例 / 两条反例 / 反例变红 / 外仓核对 / verify.ps1 / .meta 恢复 全部有证据）`
- **Owner:** project maintainer
- **Last updated:** `2026-10-20`
- **设计依据（冻结，未修改）:** [BUILD-F04 §6 S05 / §12 Q1 / §12 Q4 / §8 O8](./BUILD-F04_EDITOR_MODULE_BOUNDARY_DESIGN.md)
- **前序实现记录:** [S01–S03 编辑器 DLL](./BUILD-F04_S01_S03_EDITOR_DLL_IMPLEMENTATION.md)（编辑器已是 SHARED + explicit，导出 1 条）、
  [S04 imgui 迁入编辑器 DLL](./BUILD-F04_S04_IMGUI_RELOCATION_IMPLEMENTATION.md)（引擎导出 1288）

---

## TL;DR

**`MaximumEditor.exe` 现在在产品语义上是「资产-only 项目编辑器」：它把自己的编译期游戏模块身份 `(none)` 报出来；项目描述符可以可选地声明 `RequiredGameModule`；两者不匹配时编辑器在启动期硬失败，退出码 `3`，并在日志里给出完整的恢复指引 —— 而不是今天这样"打开了、成功、但你的组件全丢了"。**

四件必须知道的事：

1. **身份到底放哪：不是编辑器 DLL，是宿主 exe。** 一条 `libMaximumEditord.dll` 被 `MaximumEditor.exe`、`SampleEditor.exe` 与外仓 `<Project>Editor.exe` 共用，"我用哪个游戏模块编的"在编译期只是 **宿主** 的属性。设计 §6/S05 里的两个宏正是注入到宿主 target 上，再由 `main.h`（所有宿主共用的唯一注入点，含外仓）交给 DLL。**外仓 `EditorMain.cpp` 零改动**。
2. **字段缺省 = 无要求，行为与今天逐字节一致。** 三个外仓 `minCraft*` 的描述符都**没有**声明该字段（§5 实测），因此它们的行为没有变化 —— 这一点是硬约束，不是"应该没问题"。
3. **硬报错是可自动验证的，并且被证明会变红。** `scripts/smoke_editor_module_gate.py` 跑完整四情形矩阵；把 gate 摘掉（模拟实现缺失）后同一脚本 **FAIL、退出码 1**（§4）。
4. **引擎导出未增加（1288），编辑器 DLL 导出 1 → 2**（新增 `SetHostEditorGameModule`，预算 ≤ 8）。

---

## 1. 机制

### 1.1 身份归属：为什么不能在编辑器 DLL 里

S02 之后编辑器是一份 **共用** 的 `libMaximumEditord.dll`。若把"我是哪个游戏模块"定义在编辑器 target 上，那么：

- 一份 DLL 只能有一个值 → `SampleEditor`（链接 `GameModuleStub`）与 `MaximumEditor`（无模块）会互相污染；
- 每项目编辑器也就无法再用同一条 DLL。

因此身份是 **宿主 exe 的编译期属性**，运行期交给 DLL。

### 1.2 注入链（三段，全部自动，外仓零改动）

| 段 | 位置 | 做什么 |
|---|---|---|
| ① CMake | `minEngine/cmake/MinEngineTargets.cmake`（`minengine_add_editor_executable`） | `GAME_MODULE X` → `MINENGINE_EDITOR_GAME_MODULE="X"`；无 `GAME_MODULE` → `MINENGINE_EDITOR_ASSET_ONLY=1` |
| ② 宿主头 | `minEngine/minEngine/src/main.h` | 解析出 `MINENGINE_EDITOR_HOST_MODULE_ID`（宏值，或 `kEditorNoGameModule`），在 `CreateApplication()` **之前** 调 `SetHostEditorGameModule(...)` |
| ③ 编辑器 DLL | `minEngine/Editor/src/EditorHostGameModule.{h,cpp}` | 保存身份（一个 `const char*`，常量初始化，无静态初始化顺序风险 —— 设计 F3）、判定矩阵、打印日志 |

编译期身份实测（构建目录里的真实编译选项）：

```text
minEngine/build/Editor/CMakeFiles/Editor.dir/flags.make
  → MINENGINE_EDITOR_ASSET_ONLY=1
minEngine/build/Examples/SampleGameHost/CMakeFiles/SampleEditor.dir/flags.make
  → MINENGINE_EDITOR_GAME_MODULE=\"GameModuleStub\"
```

`main.h` 的注入只出现在 `MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL` 分支内 —— 即"这个宿主链接编辑器 DLL"这一既有的事实，游戏宿主与测试宿主完全不受影响。

### 1.3 退出码通路（为什么加了一个虚函数）

今天 `main.h` 一律 `return 0`：编辑器"拒绝启动"这件事到不了 shell，也到不了 CI。改法是给 `Application` **追加**一个虚函数：

```cpp
// Application.h（声明在最后，追加不改变既有 vtable 槽位）
virtual int GetExitCode() const { return 0; }
```

- `Editor` 覆盖它，返回 `m_ExitCode`（gate 拒绝时 3）；
- `main.h` 在 `delete app` 之前取一次，其余宿主默认 0，**行为不变**；
- 选虚函数而不是新增引擎导出函数：**引擎导出条目数不增加**（`Application` 全内联，无新导出符号，见 §6）。

### 1.4 项目侧声明

`ProjectDescriptor`（`ProjectManager::OpenProject` 读取的那个 `.meproject` 描述符）新增可选字段 `RequiredGameModule`，序列化/反射代码由 `scripts/minEngine_header_tool.py` 在构建期自动再生成（两个 `.gen` 文件各 +1 行，diff 见 §9）。

---

## 2. 行为矩阵（四情形）与实测

判定函数是纯函数 `EvaluateEditorModuleGate(项目声明, 编辑器身份)`（`EditorHostGameModule.h`，inline，无日志无状态），执行动作在 `CheckProjectModuleRequirement`：

| # | 项目声明 | 编辑器编译期身份 | 判定 | 行为 | 实测 |
|---|---|---|---|---|---|
| 1 | 空 / 缺省 / `(none)` | `M` | `Pass` | 正常启动，无额外噪音 | `PASS match booted=True, identity GameModuleStub` |
| 2 | 空 / 缺省 | `(none)` | `PassAssetOnlyNotice` | **允许启动** + 一条明确提示 | `PASS asset-only booted=True, identity (none)` |
| 3 | `M` | `(none)` | `FailNoModule` | **硬报错，退出码 3** | `PASS no-module exit 3` |
| 4 | `M` | `N ≠ M` | `FailMismatch` | **硬报错，退出码 3** | `PASS mismatch exit 3` |

（情形 1/4 用 `SampleEditor.exe`（`GAME_MODULE GameModuleStub`）驱动；情形 2/3 用 `MaximumEditor.exe`。）

矩阵脚本一次跑完的实测输出：

```text
$ python scripts/smoke_editor_module_gate.py
PASS identity    both hosts import the identity setter
PASS asset-only  booted=True, identity (none)
PASS no-module   exit 3, 52 log line(s)
PASS mismatch    exit 3, 52 log line(s)
PASS match       booted=True, identity GameModuleStub
module gate matrix PASS
（退出码 0）
```

### 2.1 硬报错原文（情形 3，`MaximumEditor.exe` + 声明 `MinCraft-gameplay` 的项目，退出码 3）

```text
[Editor] Editor: host game module identity '(none)' (compiled into this host by minengine_add_editor_executable).
[Editor] Editor: refusing to open project 'GateNoModuleQuote': it declares RequiredGameModule 'MinCraft-gameplay'.
[Editor] Editor: this editor host was built with no game C++ module (identity '(none)'), so not one of the project's C++ component types could be registered - the project would open looking like its components were lost.
[Editor] Editor: recovery - build a project editor that links this module: minengine_add_editor_executable(<Project>Editor MAIN_SOURCE <main.cpp> GAME_MODULE MinCraft-gameplay), or run the editor shipped with the project: <Project>Editor.exe. MaximumEditor.exe is the asset-only editor and cannot load game C++ modules.
[Editor] Editor: exiting with code 3 (hard error, no silent downgrade - BUILD-F04 S05 / Q4).
（进程退出码 3）
```

三条设计要求逐条对上：项目需要哪个模块 ✅、当前编辑器没带模块（并说明后果）✅、如何获得正确的编辑器（`minengine_add_editor_executable(... GAME_MODULE M)` 与 `<Project>Editor.exe` 两条路）✅。

### 2.2 资产-only 提示原文（情形 2，无声明 + `MaximumEditor.exe`）

```text
[Editor] Editor: host game module identity '(none)' (compiled into this host by minengine_add_editor_executable).
[Editor] Editor: project 'GateAssetOnlyQuote' declares no RequiredGameModule; this editor was built WITHOUT a game C++ module (asset-only), so only asset-only projects are supported here.
```

### 2.3 正例（今天的真实项目，不带任何声明）

```text
$ python scripts/smoke_editor_automation.py
PASS editor.get_state         defaultSessionId=sess-1 projectPath=D:/Dev/GitRepo/minEngine/minEngine/MyMEP ready=True sessions=1
PASS editor.get_capabilities  domains=9 editorProduct=Maximum editorVersion=0.0.9 protocolVersion=1
PASS editor.get_log           entries=110 ... ringTruncated=False
PASS reflect.list_classes     classes=131
PASS scene.get_hierarchy      roots=8
smoke PASS
（退出码 0）
```

`MyMEProject.meproject` 不含 `RequiredGameModule`，行为与 S05 之前完全一致（131 个反射类、8 个场景根，与 S04 冒烟一致）。

---

## 3. 描述符字段格式

文件：项目根下的 `<Project>.meproject`（JSON，`ProjectManager::OpenProject` 读取）。

```json
{
    "$schemaVersion": 1,
    "$engineVersion": "0.0.9",
    "ProjectName": "MyProject",
    "ProjectId": { "High": 1234567890123456789, "Low": 9876543210987654321 },
    "ProjectRoot": "D:/Projects/MyProject",
    "RequiredGameModule": "MyProject-gameplay"
}
```

| 规则 | 说明 |
|---|---|
| 字段名 | `RequiredGameModule`（`ProjectDescriptor::RequiredGameModule`，`ME_PROPERTY`） |
| 类型 | 字符串，取值就是 CMake `GAME_MODULE` 的 target 名（例：`GameModuleStub`、`MinCraft-gameplay`） |
| 缺省 | **省略该字段 = 空字符串 = 「无要求」**，任何编辑器都可打开（向后兼容的唯一依据） |
| 空白 | 纯空白视同省略 |
| `"(none)"` | 显式写 `(none)` 等同于省略（与编辑器侧的哨兵值同一个字面量，避免"我想声明没有模块"被当成模块名） |
| 未声明的旧描述符 | `ProjectManager::LoadProjectDesc` 用 `skipUnknownField=true, strictTypeCheck=false`，缺字段保持默认值，因此**不会因为少了字段而失败** |
| 未知字段 | 旧引擎读新描述符时 `skipUnknownField` 丢弃，不会报错 |

**为什么不用"必有字段 + 默认值"**：那会让三个外仓的描述符在语义上被"补写"，而它们是只读的；缺省即无要求是唯一既满足硬报错又不触碰外仓的形状。

---

## 4. 反例自动化验证

新增 `scripts/smoke_editor_module_gate.py`（无依赖、纯标准库；复用 `scripts/audit_dll_exports.py` 的 PE 解析做静态断言）。

| 情形 | 载体 | 断言语义 |
|---|---|---|
| `identity` | 静态，**不需要桌面** | 两个编辑器宿主都从 `libMaximumEditord.dll` 导入 `SetHostEditorGameModule` → 编译期身份注入真实存在 |
| `asset-only` | `MaximumEditor.exe` + `MyMEProject.meproject` | 启动到 automation ready 文件 + 身份行 `(none)` + asset-only 提示 |
| `no-module` | `MaximumEditor.exe` + 临时项目（声明 `MinCraft-gameplay`） | **退出码必须精确等于 3**（不是"非零"—— 非零也接受崩溃）、未写出 ready 文件、日志含 6 段必需文本 |
| `mismatch` | `SampleEditor.exe` + 临时项目（声明 `NotTheStubModule`） | 同上，且日志含编辑器自身身份 `GameModuleStub` |
| `match` | `SampleEditor.exe` + 临时项目（声明 `GameModuleStub`） | 启动成功、无 `refusing to open project`、身份行 `GameModuleStub` |

临时项目的 `Assets/` 是空目录：**该脚本不向仓库写入任何东西**（不触碰 `MyMEProject`，除了 `asset-only` 情形按设计使用真实项目）。

用法：

```text
python scripts/smoke_editor_module_gate.py                 # 全部 5 个情形
python scripts/smoke_editor_module_gate.py --case no-module # 单个情形
```

### 4.1 「实现缺失时变红」的证据（变异测试）

把 gate 摘掉（在 `CheckProjectModuleRequirement` 开头临时 `return EditorModuleGateResult{};`，即 S05 之前的静默行为），重新构建编辑器 DLL 后跑反例：

```text
$ python scripts/smoke_editor_module_gate.py --case no-module   # 摘掉 gate 之后
     - editor booted (automation ready file appeared) - the gate did not fire
     - exit code 1, expected 3
     - log is missing the required module name ('MinCraft-gameplay')
     - log is missing the GAME_MODULE argument in the recovery step ('GAME_MODULE')
     - log is missing the project-editor recovery step ('<Project>Editor.exe')
     - log is missing an explicit refusal instead of a silent downgrade ('refusing to open project')
FAIL no-module   exit 1, 39 log line(s)
module gate matrix FAIL
RED_SCRIPT_EXIT=1
```

`--case mismatch` 同时变红（同一份输出里 6 条断言全部命中，原因正是"编辑器静默启动了"）。恢复源码、`touch` 后重建，同一脚本回到 `PASS`（§2 的绿色输出即恢复后的回归）。

> 附注（这次真的踩到了）：恢复文件用 `mv` 把备份移回原位会保留**旧 mtime**，`make` 判定目标文件已是最新 → 不重编 → 变异体残留。第一次"恢复后仍红"就是这个原因；加 `touch` 后重编才真正恢复。记录在此，避免下次误读为"改回去还是红的"。

### 4.2 进 `verify.ps1` 的那一半

`scripts/verify.ps1` 增加了一条**不需要桌面**的静态断言（`--case identity`）：注入被删掉 → 宿主不再导入 `SetHostEditorGameModule` → verify 直接失败。需要窗口的四情形矩阵不放进 `verify.ps1`（它至今不启动编辑器，保持无 GUI 依赖）；脚本头注释里写明手动入口。

---

## 5. 外仓兼容性核对（只读，未修改、未构建）

三个外仓的 `.meproject` 全量内容已读：

| 仓库 | 描述符 | 声明 `RequiredGameModule`？ | 结论 |
|---|---|---|---|
| `D:/Dev/GitRepo/minCraft` | `minCraft/minCraft.meproject` | **否** | 行为与今天完全一致 |
| `D:/Dev/GitRepo/minCraft-gameplay` | `minCraft/minCraft.meproject` | **否** | 同上 |
| `D:/Dev/GitRepo/minCraft-ui` | `minCraft/minCraft.meproject` | **否** | 同上 |

三个描述符均为 `{$schemaVersion, $engineVersion, ProjectName, ProjectId{High,Low}, ProjectRoot}`，无一含 `RequiredGameModule` 或任何近似字段（`GameModule` 子串同样为 0 命中）。

推理链（说明为什么"没声明 → 行为不变"是可证的而非猜测）：

1. 缺字段 → 反序列化保持默认值 `""`（`skipUnknownField=true / strictTypeCheck=false`，且字段本身有 `std::string` 默认构造）；
2. `""` → `EvaluateEditorModuleGate` 走 `PassAssetOnlyNotice` 或 `Pass`；
3. 两者都 `AllowProject = true`，`m_ExitCode` 保持 0；
4. 因此外仓 `minCraftEditor.exe`（源码未改，但重新构建即可获得新形态）与今天的启动路径一致，只多一行 asset-only 提示日志（Warn 级，不改退出码）。

外仓 `CMakeLists.txt` 调用的 `minengine_add_editor_executable(MinCraftEditor ...)` 签名不变 —— 它们**不需要**在 `CMakeLists.txt` 里加任何东西；想让 minCraft 获得"项目编辑器"语义时，才需要在自己描述符里写 `RequiredGameModule`。

---

## 6. 导出面 / 契约影响

| 量 | 改前 | 改后 | 说明 |
|---|---|---|---|
| 引擎 DLL 导出 | 1288 | **1288** | 未增加（`Application::GetExitCode` 全内联；引擎侧无新 `MINENGINE_API` 函数、无新 `.def` 行） |
| 编辑器 DLL 导出 | 1（`CreateApplication`） | **2**（+`SetHostEditorGameModule`） | 预算 ≤ 8（O6）；覆盖断言 `missing 0` |
| 编辑器宿主导入编辑器 DLL | 1 | **2** | `min_editor_dll_imports` 断言是 `>=`，通过 |
| 引擎宿主导入 | 3 | 3 | `max_engine_imports` 上限 3，未变 |
| 宿主体积 | 323,536 B | 324,266 B（MaximumEditor）/ 324,652 B（SampleEditor） | 上限 16 MiB，通过 |

`verify.ps1` 实测（退出码 0）：

```text
PASS: NumberOfFunctions=1288 <= 3000, ordinal headroom=64247 >= 60000
PASS: export table contains no entry of bucket(s) glfw,imgui
coverage total missing = 0 / in-repo missing = 0 / unexpected missing = 0
PASS engine exports did not increase                1288 (baseline 1549)
PASS editor DLL exports within budget               2 <= 8
PASS editor DLL exports CreateApplication           _ZN9minEngine17CreateApplicationEv
PASS MaximumEditor.exe imports the editor DLL       2 symbol(s)
PASS identity    both hosts import the identity setter
module gate matrix PASS
[doctest] test cases:  4 |  4 passed | 0 failed | 229 skipped
[doctest] assertions: 77 | 77 passed | 0 failed |
（verify.ps1 退出码 0）
```

编辑器 DLL 的两个导出名（实测）：

```text
_ZN9minEngine17CreateApplicationEv
_ZN9minEngine23SetHostEditorGameModuleEPKc
```

---

## 7. 回退

| 想撤销什么 | 改哪里 | 影响 |
|---|---|---|
| 只撤销"硬报错"（回到"仅提示"） | `EditorHostGameModule.cpp` 把两个 `Fail*` 分支改成打 Warn 后 `return EditorModuleGateResult{}` | 一处函数；字段与身份仍在，可继续观测 |
| 撤销整个 S05 | 删 `EditorHostGameModule.{h,cpp}`、`main.h` 的注入与 `GetExitCode` 使用、`CMakeLists` 两个宏、`Editor.cpp` 两处调用；`ProjectDescriptor` 字段可留（缺省即无要求，留着无害） | 无产物形态变化，纯行为回退 |
| 撤销退出码通路 | `main.h` 里 `return exitCode;` 改回 `return 0;`（`Application::GetExitCode` 留着无害） | gate 仍会拒绝打开，但 shell 看不到 |
| 撤销 `verify.ps1` 的新断言 | 删掉 `--case identity` 那三行 | 只丢静态护栏 |

---

## 8. 偏差与未完成项

1. **gate 在资产扫描之后执行（顺序偏差，已权衡）**：`ProjectManager::OpenProject` 内部已经完成描述符解析与前期的资产扫描，编辑器侧拿到 `Descriptor` 时扫描已发生。要把 gate 提前到"扫描之前"，需要在引擎侧新增一个"只读描述符"的公开入口 —— 那会给引擎 **+1 条导出**，与本片"引擎导出未增加"的硬要求冲突。代价：打开错编辑器时可能触发一次 `.meta` 迁移写盘（BUG-ASSET-003 的那类一次性迁移）。反例脚本用**无 Assets 的临时项目**规避，不影响仓库；作为后续优化登记（Deferred，需要引擎侧一条只读接口）。
2. **`Application` 增虚函数的 ABI 形态**：为了不改 `main.h` 之外的调用方，虚函数**追加在最后**，既有 vtable 槽位不变；但严格 ABI 兼容仍是"重编即可"级别，与 S02/S03 已经引入的 DLL 边界同性质（本仓无跨版本二进制契约，见 Deferred-7）。
3. **未做**：设计 §8 O7（在编辑器进程内断言游戏模块的每个 `ME_CLASS` 可查）不属于本片范围；本片只保证"错编辑器不再静默打开"。
4. **未做**：`package-editor`（S06）未动；`MaximumEditor` 的"资产编辑样例"登记文字随 S06/BUILD-F01 一起处理。
5. **反例脚本需要桌面**：与 `smoke_editor_automation.py` 同样的限制（本仓没有 `--headless` 编辑器模式），因此它不进 `verify.ps1`；进 `verify.ps1` 的是不需要桌面的 `--case identity`。
6. **外仓 `minCraft*` 一个字节都没改**（只 `Get-Content` 读过描述符），也没有构建它们。

---

## 9. 本次碰过的文件

| 文件 | 改动 |
|---|---|
| `minEngine/minEngine/src/Runtime/Function/Framework/Project/ProjectDescriptor.h` | +`ME_PROPERTY() std::string RequiredGameModule;`（9 行，含注释） |
| `minEngine/minEngine/src/Generated/Reflection/ProjectDescriptor.gen.h` | 构建期 codegen 自动 +1 行（`ME_REFLECTION_ACCESSOR_FIELD`） |
| `minEngine/minEngine/src/Generated/Reflection/ProjectDescriptor.gen.cpp` | 构建期 codegen 自动 +1 行（`ADD_FIELD_ACCESSORS`） |
| `minEngine/minEngine/src/Application.h` | +`virtual int GetExitCode() const { return 0; }`（追加在最后） |
| `minEngine/minEngine/src/main.h` | 身份宏解析 + `SetHostEditorGameModule(...)` 注入 + `return app->GetExitCode()` |
| `minEngine/cmake/MinEngineTargets.cmake` | `minengine_add_editor_executable` 注入 `MINENGINE_EDITOR_GAME_MODULE` / `MINENGINE_EDITOR_ASSET_ONLY` |
| `minEngine/Editor/src/EditorHostGameModule.h`（新） | 身份 API、纯判定函数、退出码常量、gate 声明 |
| `minEngine/Editor/src/EditorHostGameModule.cpp`（新） | 身份存储、日志与恢复指引 |
| `minEngine/Editor/src/Editor.cpp` | 启动身份行；`OpenProject` 成功分支里的 gate（`m_ExitCode` + `return false`） |
| `minEngine/Editor/src/Editor.h` | `GetExitCode()` 覆盖 + `m_ExitCode` |
| `scripts/smoke_editor_module_gate.py`（新） | 四情形矩阵 + 静态身份断言 |
| `scripts/verify.ps1` | 新增 `--case identity` 断言（无桌面依赖） |
| `docs/ai/Platform/ProductBuild/BUILD-F04_S05_ASSET_ONLY_EDITOR_IMPLEMENTATION.md`（新） | 本文 |

**未碰**：`Render/ScreenUI/*`、`Sprite*`、`PointerConstraintState.*`、`GLFWWindowSystem.*`、`WindowSystem.h`、`Material*`、`ShaderCompiler.*`、`SpriteMaterialFactory.*`、`ScreenUIMaterialFactory.*`、`EngineShaderUtils.*`、`RHI/RHI.h`、`RenderSystem.cpp`、`Tests/Suites/*`、`minEngine_exports.def` 的 LEGACY-EXTERNAL-DEBT 区块、`FEATURE_REGISTRY.md`、`docs/ai/bugs/*`、`BUILD-F03_*` / `BUILD-F04_*` 设计文档、`docs/ai/Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md`。

---

## 10. 复现命令

```text
# 构建（verify.ps1 内部就会构建这三个 target）
cmake --build minEngine/build --target Editor SampleEditor minEngineTests -j 8

# 四情形矩阵（需要桌面；不改仓库）
python scripts/smoke_editor_module_gate.py
python scripts/smoke_editor_module_gate.py --case identity      # 静态，无需桌面

# 正例（沿用 S04 载体：真实项目 + automation 管道）
python scripts/smoke_editor_automation.py

# 全量
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1

# 复现「反例会红」：在 EditorHostGameModule.cpp 的 CheckProjectModuleRequirement 开头插入
#   return EditorModuleGateResult{};
# 重建 Editor/SampleEditor，再跑 --case no-module → FAIL。恢复源码后记得 touch 该文件再重建。

# 启动编辑器会一次性迁移 MyMEProject 的 .meta（BUG-ASSET-003）：
git checkout -- $(git status --porcelain | awk '/\.meta$/ {print $2}')
```
