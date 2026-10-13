# BUILD-F01 — 产品构建链 · 项目编辑器模板 · Game/Shipping 宿主预留

## Meta
- **ID:** `BUILD-F01`
- **Type:** Feature
- **Complexity:** `L3`
- **Status:** `In Progress`
- **Owner:** project maintainer
- **Last updated:** `2026-10-13`（移除 `MINENGINE_GAME_DIR`；代码注释不写 Feature ID）
- **Related:**
  - 取代/收敛权宜：[ED-F20](../../Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)（引擎反向 add 游戏）、KeepAlive/Marker
  - 对称链接文档：[ED-F21](../../Editor/ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_DESIGN.md)
  - 取消的玩法钩子：[ED-F22](../../Editor/ED-F22_GAME_MODULE_STARTUP_DESIGN.md)（精神保留：加载≠建世界）
  - 多模块反射：[CORE-F27](../Core/CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md)
  - 子系统发现依赖「模块进进程」：[CORE-F34](../Core/CORE-F34_GAME_INSTANCE_AND_SUBSYSTEM_LIFECYCLE_DESIGN.md)（**本 Feature 完成后再开**）
  - Launcher 创建工程：[LAUN-F01](../Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md)
  - UE 对照：`TargetType.Editor` / `TargetType.Game`、Development vs Shipping、`LyraEditor.Target` / `Lyra.Target`
- **Depends on:** 无硬依赖代码 Feature；**实施顺序：本 Feature → CORE-F34**
- **Blocks:** 干净的 minCraft/外仓 Dev 体验；Launcher 可编译项目模板；游戏独立 exe / 后续打包管线挂点

## TL;DR

今日构建图是 **引擎 CMake 反向拉取游戏 + KeepAlive 防裁库**，依赖箭头与产品心智相反；也没有「最终给玩家的 Game.exe」产品位。本 Feature 建立可复用产品构建规则：项目工程为根；同时产出 **ProjectEditor**（Dev）与 **ProjectGame**（Standalone / Shipping 宿主）；模板供 Launcher 填充。完整 Cook/安装器后置，但 **目标拓扑与 Stage 布局必须本 Feature 留好**。

## Problem

1. **依赖反向：** `MINENGINE_GAME_DIR` → 引擎 `add_subdirectory(游戏)` → `MaximumEditor` link 游戏；游戏无法独立表达「我的编辑器 / 我的游戏」。  
2. **权宜补丁：** KeepAlive / Marker / `--as-needed -u` 强迫保留 DLL。  
3. **CMake 杂乱：** 引擎 DLL、Editor、Tests、外仓游戏、拷贝规则、反射 codegen 路径纠缠，难复用。  
4. **Launcher 创建工程** 只能拷贝空内容模板，不能生成「可 compile 的项目骨架」。  
5. **无 Game 产品位：** 只有 Editor 宿主；玩家向 Standalone exe、Shipping 配置、Stage 目录均未预留，后续打包只能再拧一次依赖图。  
6. **监管债：** 上述为赶 minCraft 通车的临时设计，需显式退役路径。

## Requirements

- **R1** 推荐 Dev 图：项目根 CMake → 链接引擎 + 游戏 → 产出 `<Project>Editor`。  
- **R2** 引擎仓 **默认不** `add_subdirectory` 外仓游戏；Sample 可留在引擎仓内自测。  
- **R3** 项目模板（CMake + 目录骨架 + 占位符），Launcher `create` 可填充。  
- **R4** 明确输出布局（`bin/` Dev 输出；游戏 DLL 旁路 Editor/Game exe）。  
- **R5** 删除或替换 KeepAlive/Marker；用 **显式 link 游戏** 保证进进程（Editor **与** Game 宿主皆如此）。  
- **R6** 梳理引擎侧 CMake：引擎库、EditorLib、Editor/Game 可执行、Tests、Examples 边界清晰。  
- **R7** 与 CORE-F27 反射 codegen 路径兼容（游戏模块仍各自 Generated）。  
- **R8** **Game / Shipping 宿主预留（本 Feature 必做骨架）：**
  - 项目根同时声明 `<Project>Game`（或 `{{PROJECT_NAME}}`）可执行目标；link `minEngine` + Game SHARED；**不** link Editor/ImGui 编辑壳。  
  - 提供 `minengine_add_game_executable(...)`（或等价）与最小 `GameMain`（窗口 + 引擎循环占位；玩法仍在 Game 模块）。  
  - 预留 **Configuration** 语义：至少区分 `Development`（Dev 符号/断言友好）与 `Shipping`（`NDEBUG` / 可选剥离编辑器-only 宏的挂点）；首期可用现有 CMake `Debug`/`Release` 映射，文档写清。  
  - 预留 **Stage / Package** 自定义目标（如 `package-game`）：把 Game exe + 依赖 DLL + `Content/`（或约定资源根）拷到 `Staging/<Project>/`；**不**在本 Feature 实现 Cook、压缩包、安装器、平台商店。  
- **R9** 模板同时生成 Editor + Game 两个可执行目标（Game 可为「能启动空循环」的最小宿主）。

## Constraints

- Windows Dev 为主；Linux/mac 不挡设计。  
- 首期 **链接期** 模块（重启换 DLL）；LoadLibrary 热插拔后置。  
- 不强制一夜搬完 minCraft；提供迁移指南与双轨窗口。  
- 不做 Epic 式多引擎版本安装器。  
- Game 宿主首期 **不要求** 完整 PIE/存档/联机；能启动引擎 + 加载游戏模块即可。

## Preferences

- 心智对齐 UE：**Editor Target + Game Target**；Shared Engine binaries 可选。  
- 模板优先「能编过的最小项目」，不是空文件夹。  
- 一步步退役 ED-F20，而非静默并存十年。  
- 打包复杂度后置，但 **不省略 Game.exe 产品位**（避免只优化 Editor Dev 路径）。

## Assumptions

- 短期引擎仍以源码树 `add_subdirectory(minEngine)` 或 `ENGINE_ROOT` 引用为主；`find_package` / install export 后置。  
- `MaximumEditor` 保留为引擎默认 Editor 产物名；项目模板生成 `{{PROJECT_NAME}}Editor` 与 `{{PROJECT_NAME}}`（Game exe，或 `{{PROJECT_NAME}}Game`——见变量表）。  
- 废弃 Playground 不复活为产品路径；其「薄 main + link 引擎」精神由 **Game 宿主** 正式承接。

## Non-Goals

- 完整复制 UnrealBuildTool。  
- 在本 Feature 实现 CORE-F34 子系统。  
- **完整** Cook / 资产烘焙 / 安装器 / 代码签名 / 多平台 Store 流水线（另开 Feature，挂在本 Feature 的 Stage 契约上）。  
- 强制 minCraft 本周切根（迁移切片可按 S05 节奏）。

## Current Architecture

```text
引擎仓根 CMake
  ├─ minEngine (SHARED)
  ├─ Editor → MaximumEditor.exe  (可选 link GAME via ED-F20)
  ├─ Tests / Playground(deprecated)
  └─ add_subdirectory(MINENGINE_GAME_DIR)  // 反向

游戏仓（minCraft）
  └─ 假设已被引擎 add_subdirectory；FATAL 若无 minEngine target
  └─ 额外 target_link_libraries(Editor) + Marker
  └─ 无独立 Game.exe 产品目标
```

## Proposed Architecture

### 产品目标三角（锁定）

| 目标 | 角色 | Link | 对应 UE |
|------|------|------|---------|
| `{{GAME_MODULE}}` SHARED | 游戏代码 / 反射模块 | → minEngine | Game module |
| `{{PROJECT_NAME}}Editor` | Dev 编辑器宿主 | → EditorLib + minEngine + Game | Editor target |
| `{{PROJECT_NAME}}`（Game） | 玩家/Standalone 宿主 | → minEngine + Game（**无** EditorLib） | Game target |

```text
选项 A（推荐：项目根）
┌──────────────────────────────────────────────────┐
│ <GameRepo>/CMakeLists.txt                        │
│  add_subdirectory/find ENGINE_ROOT               │
│  add_library(Game SHARED)                        │
│  minengine_add_editor_executable(ProjectEditor)  │
│  minengine_add_game_executable(ProjectGame)      │
│  optional: add_custom_target(package-game)       │
└──────────────────────────────────────────────────┘

选项 B（引擎仓 Sample，无外仓）
┌──────────────────────────────────────────────────┐
│ Examples/<Sample>/                               │
│  Sample SHARED + SampleEditor + SampleGame       │
│  不 add 外仓；证明三角目标可编                     │
└──────────────────────────────────────────────────┘
```

### Editor 包装形态（OQ2 — Accepted）

**决定：抽 `MaximumEditorLib`（OBJECT 或 STATIC），薄 exe 包装。**

1. 今日 `Editor/CMakeLists.txt` 把全部 `Editor/src` 打进单一 `add_executable(Editor)`。  
2. 本 Feature：**先**把 Editor 源编为 `MaximumEditorLib`；`MaximumEditor` = 薄 `main` + link Lib。  
3. 项目 / Sample 的 `{{PROJECT}}Editor` = 同一薄 main 模式 + link `MaximumEditorLib` + Game。  
4. 若 S02 工期紧：允许 **过渡** 用 `minengine_add_editor_executable` 内部复用同一份源列表（尚未拆 .lib 文件），但 **API 形态必须是「可复用的 add_* 宏」**，禁止项目复制粘贴整棵 Editor 树。  
5. 引擎仓无游戏时仍可编 `MaximumEditor`（不 link Game）。

### Game 宿主与 Shipping / Stage（R8）

```text
Runtime（Dev 输出，例）
  bin/{{PROJECT_NAME}}Editor.exe
  bin/{{PROJECT_NAME}}.exe          # Game host
  bin/minEngine.dll / Game.dll / …

Staging（package-game，预留）
  Staging/{{PROJECT_NAME}}/
    {{PROJECT_NAME}}.exe
    *.dll
    Content/                        # 或项目约定资源根（首期原样拷贝，不 Cook）
    Manifest.txt                    # 可选：引擎版本、配置名
```

| 层面 | 本 Feature 做 | 后置 Feature |
|------|---------------|--------------|
| Game exe 目标 + 薄 Main | ✅ | — |
| Development / Shipping 配置映射与宏挂点 | ✅ 文档 + CMake 选项骨架 | 细粒度 Shipping 剥离 |
| `package-game` Stage 拷贝 | ✅ 最小可用 | — |
| Cook / 依赖裁剪 / 压缩 / 安装器 | ❌ | BUILD-F0x / 平台管线 |
| 多平台（consoles 等） | ❌ | 另议 |

**GameMain 职责（机制，非玩法）：** 解析 CLI → 初始化进程设施 → 创建/运行引擎循环 → 退出码。会话/子系统细节属 CORE-F34；本 Feature 只保证 **模块已 link 进进程** 且宿主可启动。

### 模板（Launcher）

```text
Templates/GameProject/
  CMakeLists.txt.template
  Source/{{GAME_MODULE}}/...
  Source/Programs/EditorMain.cpp.template   # 可选；或用引擎提供的默认 main
  Source/Programs/GameMain.cpp.template
  Content/
  .meproject.template
  README.md
```

**变量表（实现用）**

| 变量 | 含义 | 示例 |
|------|------|------|
| `PROJECT_NAME` | 产品名；默认 Game exe 名 | `MinCraft` |
| `PROJECT_EDITOR_NAME` | Editor 目标/产物名 | `MinCraftEditor` |
| `GAME_MODULE_NAME` | SHARED 目标名 | `MinCraftGame` |
| `ENGINE_ROOT` | 引擎仓根（含 `minEngine/` CMake） | `D:/Dev/GitRepo/minEngine` |
| `PROJECT_ROOT` | 项目根（生成后） | （create 输出目录） |

Launcher `create`：复制 + 替换 → configure/build 两个 exe；文档写清前置（引擎路径）。

### ED-F20 退役

| 阶段 | 行为 |
|------|------|
| S01 | 设计冻结；废弃声明；变量表 |
| S02 | Sample：SampleEditor + **SampleGame**；无反向外仓；无 KeepAlive |
| S03 | 模板 + Launcher create |
| S04 | 引擎 CMake 分层（EditorLib / Tests / Examples）；`package-game` 骨架 |
| S05 | minCraft 迁项目根；双 exe；删 Marker/KeepAlive |
| S06 | 移除或硬废弃 `MINENGINE_GAME_DIR` |

### CMake 治理（引擎侧）

- 分层：`minEngine` / `MaximumEditorLib` / `MaximumEditor` / **可选引擎侧 GameHost 默认 main 库** / `Tests` / `Examples`。  
- 公共模块：`MinEngineReflection.cmake`、`MinEngineTargets.cmake`（`minengine_add_editor_executable` / `minengine_add_game_executable` / Stage 辅助）、输出目录宏。  
- 选项表写入 Handbook / BOOTSTRAP。

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| BUILD-F01 | 依赖图、EditorLib、Game 宿主、模板、Stage 挂点、退役 ED-F20 | 玩法、Cook、安装器 |
| Launcher | 填充模板、打开工程 | 第二套 CMake 语义 |
| 游戏仓 | 拥有 Editor + Game 目标与 Content | 改引擎反向拉取 |
| CORE-F34 | GI/Subsystem 运行时 | 重写 CMake |

## Ownership & Lifetime

**Build artifact：** Dev 产出属项目/引擎约定 `bin/`；Stage 属项目 `Staging/`（不进 VCS）。Shared 预编引擎属引擎 `bin/`（若采用）。

## Alternatives & Trade-offs

### A（推荐）项目根 + Editor/Game 双目标 + 模板

- Pros：箭头正确；对齐 UE；打包有挂点。  
- Cons：minCraft 迁移一次性成本；EditorLib 抽取有工期。

### B 只做 Editor Dev，Game 以后再说

- Why not：用户已指出缺口；事后再插 Game 目标易再次拧 CMake。

### C 继续 ED-F20

- Why not：加深反向依赖。

## Integration Impact

- 引擎根 / `Editor/CMakeLists.txt`、Examples、可能新增 `cmake/*.cmake`、Launcher Templates、BOOTSTRAP/Handbook、minCraft CMake。  
- CORE-F34 S06：模块进进程 = ProjectEditor **与** ProjectGame 显式 link。  
- 废弃 Playground 作为推荐路径（可保留目录但不文档化）。

## Migration / Compatibility

- 双轨窗口：旧 `MINENGINE_GAME_DIR` → Deprecated + WARNING。  
- minCraft：S05 迁移；可记游戏仓 INFRA。

## Implementation Slices

| Slice | 范围 | 验证 |
|-------|------|------|
| **S01** | 本节定稿；变量表；废弃声明；Handbook 草图 | 文档 Review — **Done** |
| **S02** | EditorLib + Sample：SampleEditor **与** SampleGame；无反向拉取 | 两 exe 已产出；`game-module-stub` 绿 — **Done**（KeepAlive TU 仍在 EditorLib，S05/S06 删） |
| **S03** | `Templates/GameProject` + Launcher create 填变量 | 新目录 cmake 配置成功；两目标存在 |
| **S04** | 引擎 CMake 分层 + `package-game` Stage 最小拷贝 | 默认无外仓 verify 绿；Stage 目录有 exe+dll |
| **S05** | minCraft 迁项目根；双 exe；删 Marker | 文档路径；反射在 Editor/Game 均可 |
| **S06** | 移除 `MINENGINE_GAME_DIR` / KeepAlive | **Done**（S02 后直接删除，无双轨窗口） |

## Verification Strategy

- 干净 clone：仅引擎 → Sample Editor + Sample Game 可编。  
- Launcher create → configure → build 两 exe。  
- `package-game` → Staging 含 Game exe + DLL。  
- 无 KeepAlive TU；显式 link 可见。  
- `verify.ps1` 默认无外仓游戏；Sample 可选 job。

## Open Questions

1. **项目根放游戏仓还是 umbrella？** 默认：游戏仓即产品根。 — **Accepted**  
2. **Editor 包装？** **MaximumEditorLib + 薄 exe**（过渡允许源列表宏）。 — **Accepted**  
3. **引擎引用？** 首期 `ENGINE_ROOT` + `add_subdirectory`；export 后置。 — **Accepted**  
4. **Game exe 命名？** 默认 `{{PROJECT_NAME}}.exe`（玩家向）；Editor 为 `{{PROJECT_NAME}}Editor.exe`。若与模块名冲突则 Game 用 `{{PROJECT_NAME}}Game.exe`。 — **Accepted**  
5. **Shipping 首期深度？** 仅配置/宏挂点 + Stage 拷贝；Cook 另 Feature。 — **Accepted**

## Design Review

- **Verdict:** `Ready with deferred items`（完整 Cook/安装器后置；不挡 S01–S06）
- **Reviewer / date:** maintainer + agent 2026-10-13
- **Link or summary:** 用户确认设计大体 OK；补 Game/Shipping 预留后可实施；顺序 BUILD → CORE-F34

## Implementation Readiness

- [x] Requirements / 问题清楚  
- [x] Editor 包装形态（OQ2）钉死  
- [x] Game / Shipping / Stage 预留钉死（R8）  
- [x] 与 F34/Launcher 边界清楚；实施顺序 BUILD → F34  
- [x] 退役路径有切片  
- [x] 模板变量表可实现  

**Ready for implementation?** `yes` — 从 **S01**（定稿落手册/废弃声明）→ **S02**（Sample 双 exe）。

## Acceptance Checklist

- [x] Design Review（Ready with deferred）
- [ ] 新项目不依赖引擎反向 add 游戏  
- [ ] 模板含 Editor + Game 两可执行目标  
- [ ] `package-game` Stage 最小可用  
- [ ] Launcher 可从模板生成可配置工程  
- [ ] KeepAlive/Marker 从推荐路径消失  
- [ ] Handbook / BOOTSTRAP 更新  
- [ ] CORE-F34 开工前本 Feature 主链（至少 S02–S04）可用  
