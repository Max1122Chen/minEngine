# Engine Launcher — Design Spec

## Meta
- **ID:** `LAUN-F01`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-31（拍板：Rust CLI + Tauri GUI）
- **Related:** [Implementation](./LAUN-F01_ENGINE_LAUNCHER_IMPLEMENTATION.md), [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../../ACTIVE_WORK.md), [CLI_UNIFIED_DESIGN.md](../CLI/CLI_UNIFIED_DESIGN.md), [ENGINE_STARTUP_DESIGN.md](../Startup/ENGINE_STARTUP_DESIGN.md)
- **Branch / worktree:** `feat/launcher` · `D:/Dev/GitRepo/minEngine-launcher`

## TL;DR

独立 **引擎启动器**（仓库根 `Launcher/` 子项目，**不 link** minEngine C++）：定位 Editor、创建/打开/管理 `.meproject` 工程。通过 **文件系统 + JSON 契约 + 进程 CLI** 与引擎协作。

**技术栈（拍板）：**

```text
LAUN-F01:     Rust  →  minlauncher-core  +  minlauncher (CLI, clap)
LAUN-F01-S05+: Rust  →  同一 core crate  +  Tauri 2 app（系统 WebView + Web 前端）
```

F01 **只交付 CLI**；GUI 延后，但架构从第一天按 Tauri 共享 Core 设计。**不做** Build / Package / Game / 引擎多版本安装。

## Scope
- **In:**
  - 仓库内 `Launcher/` Cargo workspace（`minlauncher-core` + `minlauncher` CLI）
  - 解析 / 校验 `.meproject`、`.mesettings`（与 Engine 反射 JSON 字段对齐）
  - **Engine 定位**：配置或自动发现 `Editor` 可执行文件（及可选 `EngineRoot`）
  - **打开工程**：spawn `Editor --project <path>`（Windows 上为 `Editor.exe`）
  - **创建工程**：从 `Templates/Empty` 复制内容，生成 descriptor + settings + GUID
  - **工程管理（CLI）**：最近列表持久化、`open` / `create` / `recent` / `config`
  - 跨平台：`cargo build`（win / linux / macOS）；发布单二进制
  - **架构预留：** Core API 可被未来 Tauri `#[tauri::command]` 直接调用
- **Out（明确非目标）：**
  - **F01 GUI 窗口**（Tauri 应用体 — 延后 **S05**）
  - Build / Clean / Rebuild、Package、Asset Cooking
  - 启动 Game / Playground（无 Game Runtime target）
  - Epic 式引擎版本安装、在线更新、账号、云同步
  - 链接或复用 C++ `ProjectManager` / `PathRegistry`
  - Scene / Asset / 编辑器内功能
  - 改 `.meproject` 扩展名或大规模目录重组

## Reader quick start
1. 本文件 — 边界、技术栈、JSON 契约、模块划分
2. [Implementation](./LAUN-F01_ENGINE_LAUNCHER_IMPLEMENTATION.md) — S01–S05 切片
3. 代码入口（落地后）：`Launcher/crates/minlauncher-core/`、`Launcher/crates/minlauncher/`
4. Engine 侧参考：`ProjectManager.cpp`、`ProjectDescriptor.h`、`ApplicationCommandLine.cpp`

---

## 1) 背景与目标

### Pain
- 开发依赖手动：`Editor.exe --project path/to/My.meproject`，路径易错、无最近工程。
- 无新建工程工具；只能复制 `MyMEProject` 并手改 JSON。
- `ProjectManager` 在 Runtime 进程内；独立工具不应 link 整个 Engine。

### Goals
- **第一版闭环（CLI）：** 找到 Editor → 创建工程 → 打开工程 → 管理最近列表。
- Launcher = **Project Lifecycle Tool**；Engine = **Project Runtime**（Open / Scan / PathRegistry）。
- 与 `feat/render` **零耦合**；合入 `master` 不影响 `verify.ps1`。
- **UI 路径已定：** 同一 Rust Core 上叠 **Tauri 2**（轻量、跨平台、与 Epic Launcher v2 同方向）。

### Success（F01）
```text
minlauncher open MyGame.meproject
minlauncher create MyGame --parent ./Projects
minlauncher recent list
  → spawn Editor --project ...
```
S05 之后：Tauri 窗口调用同一 Core，操作等价于上述命令。

---

## 2) 现状（代码真源）

| 项 | 状态 |
|----|------|
| 工程描述符 | **`.meproject`**；JSON + `ProjectDescriptor` 反射 |
| 工程设置 | **`.mesettings`**；`ProjectSettings`（含 `EditorDefaultSceneName`） |
| `ProjectManager` | 仅 Open / Close / SaveSettings；**无 CreateProject** |
| Editor 启动 | `Editor.exe --project <path>`（`CLI-F01` Done） |
| Engine / Project 路径 | `PathRegistry`：Engine 默认资产 vs `ProjectContent`（`Assets/`） |
| Game / Package | **不存在**；`Playground` deprecated |
| Launcher 代码 | **无**（本 Feature 新建） |

### 2.1 JSON 契约（Launcher 必须兼容）

**`ProjectDescriptor`**（`.meproject`）：

| 字段 | 类型 | 说明 |
|------|------|------|
| `ProjectName` | string | 显示名；通常与目录 stem 一致 |
| `ProjectId` | `{ High, Low }` | `uint64` 对；创建时生成非零 |
| `ProjectRoot` | string | **绝对路径**（当前 Engine 行为）；创建时写入 |

**`ProjectSettings`**（`<Stem>Settings.mesettings`）：

| 字段 | 类型 | 说明 |
|------|------|------|
| `EditorDefaultSceneName` | string | 默认场景名（无扩展名）；Empty → `"default"` |
| `Editor` / `Appearance` | object | 可选；创建时可省略 |

不写 Engine 未识别的顶层键。新增字段须先改 Engine 反射。

### 2.2 `ProjectRoot` 绝对路径（已知债）

`ProjectManager::OpenProject` 用 descriptor 内 `ProjectRoot` 设 `PathRegistry`。

- **F01：** 创建时写入目标目录绝对路径。
- **跟进（非阻塞）：** Engine 改为「descriptor 所在目录推导 root」。

---

## 3) 方案

### 3.1 职责边界

```text
┌──────────────────────────────────────────────┐
│ Launcher                                     │
│  F01:  minlauncher CLI (Rust)                │
│  S05+: Tauri 2 (Rust core + Web frontend)    │
│    EngineLocator · ProjectCatalog            │
│    ProjectFactory · ProcessLauncher          │
└──────────────────┬───────────────────────────┘
                   │ .meproject / .mesettings
                   │ spawn process + CLI args
                   ▼
┌──────────────────────────────────────────────┐
│ Editor (C++ / minEngine)                     │
│  ProjectManager::OpenProject                 │
│  PathRegistry · AssetManager                 │
└──────────────────────────────────────────────┘
```

### 3.2 仓库布局

```text
minEngine-launcher/                 # worktree feat/launcher
├── minEngine/                      # CMake 引擎树（不 link Launcher）
├── Launcher/
│   ├── Cargo.toml                  # workspace
│   ├── crates/
│   │   ├── minlauncher-core/       # 库：定位、校验、创建、进程、recent
│   │   ├── minlauncher/            # F01：CLI 二进制（clap）
│   │   └── minlauncher-app/        # S05：Tauri 2 应用（Deferred）
│   │       └── ui/                 # Web 前端（Vite + TS；框架待 S05 定）
│   ├── Templates/
│   │   └── Empty/
│   └── README.md                   # 工具链：rustup、cargo、（S05）Node
├── docs/
└── scripts/
```

- **不**编入 minEngine CMake；**不**单独 git repo。
- `verify.ps1` **不**默认 `cargo build` Launcher。
- Windows 开发机：需 [rustup](https://rustup.rs/)；S05 另需 Node（前端构建）与 WebView2（Win10/11 通常已有）。

### 3.3 业界参考（摘要）

| 产品 | 技术方向 | 启示 |
|------|----------|------|
| Unity Hub | Electron | Hub 可 Web UI；对我们过重 |
| Epic Launcher v2 | **Tauri** | 逃离引擎壳；轻量跨平台 |
| JetBrains Toolbox | CEF→Compose | 独立 Hub 宜与 IDE/引擎解耦 |
| Adobe CC Desktop | CEF + Node | 商店级壳；过重 |
| Godot 官方 | Editor 内 ProjectManager | 我们已有独立 Editor；不跟 |

**拍板理由：** 未来要好用 GUI → Web 前端表现力足够；不想背整份 Chromium → **Tauri**；F01 无 UI → 先 **Rust CLI**，与 Tauri **同 Core**，避免日后换语言。

### 3.4 技术栈选型（定稿）

| 选项 | 结论 |
|------|------|
| **Rust CLI (clap) + Tauri 2 GUI** | **选用** |
| C# + Avalonia | 否决（与 Tauri 路径冲突；双栈无益） |
| C# + WPF | 否决（不跨平台） |
| Electron | 否决（体积/内存；F01 无必要自带 Chromium） |
| C++ ImGui | 否决（边界易糊、UI 上限低） |
| Go CLI | 否决（GUI 需另起炉灶，与 Tauri 不同语言） |

**分层：**

```text
minlauncher-core (Rust lib)
    ↑                ↑
minlauncher        minlauncher-app (S05)
(clap CLI)         (Tauri commands + Web UI)
```

- 可执行名：`minlauncher`（CLI）；Tauri 打包产物可称 `MinEngine Launcher` / `minlauncher-app`。
- 前端框架（S05 再拍）：React / Vue / Svelte / 纯 TS 均可；**不挡 F01**。建议默认 **Svelte 或 Vue**（轻）或 **React**（资料多）——S05 开工前再定。
- Windows 可执行名：`Editor.exe`；Unix：`Editor`（定位逻辑按平台拼文件名）。

### 3.5 Core 模块（`minlauncher-core`）

| 模块 | 职责 |
|------|------|
| **engine_locator** | 解析 Editor 路径、`EngineRoot`；settings / env / 向上自动发现 |
| **settings** | 读写 `settings.json`（跨平台 config 目录） |
| **project_catalog** | 最近工程、增删、失效标记 |
| **project_validator** | 解析 `.meproject`、必填字段、路径存在性 |
| **project_factory** | 模板复制、写 descriptor/settings、生成 GUID |
| **process_launcher** | spawn Editor + `--project`；不等待 GUI 退出 |
| **guid** | 生成非零 `{ High, Low }`（`rand` / OS RNG） |

错误类型用 `thiserror`/`anyhow`（实现时择一统一）；CLI 与 Tauri 共用同一 `Result` 语义。

### 3.6 模板与目录约定

**用户工程：**

```text
<ProjectsRoot>/MyGame/
├── MyGame.meproject
├── MyGameSettings.mesettings
├── Assets/Scenes/default.mescene
└── Saved/
```

**模板源：**

```text
Launcher/Templates/Empty/
├── Assets/Scenes/default.mescene
└── mesettings.partial.json
```

创建流程：校验名称 → 复制 Empty → 写 `.meproject` / `.mesettings` → 加入 Recent。

`minEngine/MyMEProject` = 仓内 sample，**不是**用户模板源。

### 3.7 Engine 定位策略

优先级高 → 低：

1. `settings.json` → `EditorExecutablePath`
2. 环境变量 `MINENGINE_EDITOR`
3. 从可执行文件目录向上找 `minEngine/bin/Editor[.exe]` 或 `bin/Editor[.exe]`
4. （S05 UI）首次运行选择文件；CLI 则报错并提示 `config set editor`

### 3.8 启动 Editor

```text
"<Editor>" --project "<absolute path to .meproject>"
```

可选：`--engine-root`、`--rhi opengl`（F01 默认不传 `--rhi`）。

### 3.9 `settings.json`

路径：

| OS | 默认 |
|----|------|
| Windows | `%APPDATA%/minEngine/Launcher/settings.json` |
| Linux | `$XDG_CONFIG_HOME/minEngine/Launcher/settings.json` 或 `~/.config/...` |
| macOS | `~/Library/Application Support/minEngine/Launcher/settings.json` |

```json
{
  "EditorExecutablePath": "D:/Dev/GitRepo/minEngine-launcher/minEngine/bin/Editor.exe",
  "EngineRoot": "D:/Dev/GitRepo/minEngine-launcher/minEngine",
  "DefaultProjectsDirectory": "D:/Dev/GitRepo/minEngine-launcher/Projects",
  "RecentProjects": [
    {
      "ProjectName": "MyGame",
      "DescriptorPath": "D:/…/MyGame/MyGame.meproject",
      "LastOpenedUtc": "2026-08-31T12:00:00Z"
    }
  ],
  "MaxRecentProjects": 20
}
```

移除列表项 ≠ 删除磁盘目录。

### 3.10 CLI（F01 主交付）

依赖：`clap`（或 `clap` + `clap_complete` 可选）。

```text
minlauncher open <path>
minlauncher create <name> --parent <dir> [--template Empty]
minlauncher recent list
minlauncher recent remove <path>
minlauncher config show
minlauncher config set editor <path>
minlauncher config set projects-dir <path>
```

退出码：`0` 成功，`1` 运行时失败，`2` 用法错误。  
`open` / `create` 成功后更新 Recent。

### 3.11 GUI（S05 Deferred — 架构已定）

| 项 | 约定 |
|----|------|
| 壳 | **Tauri 2** |
| 后端 | `minlauncher-core` + `#[tauri::command]` 薄封装 |
| 前端 | Web（TS）；框架 S05 开工前拍板 |
| 功能 | 最近列表、打开、新建向导、设置（Editor / 工程目录） |
| 安全 | capability 白名单；仅暴露所需 command |

**不阻塞 F01 Done。**

---

## 4) 备选方案（已否决）

| 选项 | 否决原因 |
|------|----------|
| Link minEngine 复用 `ProjectManager` | 工具链与进程边界错误 |
| `.mproj` | 与全仓 `.meproject` 不一致 |
| F01 含 Build/Package | 无 Game target |
| Electron | 体积；本产品无需自带 Chromium |
| Avalonia / WPF | 与拍板 Tauri 冲突；WPF 不跨平台 |
| F01 直接做完整 Tauri UI | 可做但拉长竖切；先 CLI 验证契约更稳 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| JSON 与 Engine 漂移 | 工程打不开 | §2.1 契约；Validator；打开试跑验收 |
| `ProjectRoot` 绝对路径 | 移动后失效 | F01 接受；打开前校验；Engine 跟进推导 |
| Editor 路径多样 | 发现失败 | config + env + 自动发现；文档写清 layout |
| Rust 学习曲线 | 进度慢 | F01 模块小；clap/serde/std::process 为主 |
| Linux WebView 差异（S05） | UI 不一致 | F01 不碰；S05 多测 webkitgtk；必要时文档要求 |
| Launcher 不进 verify | 回归靠人工 | Impl 列 `cargo test` / 手动命令；不挡引擎 smoke |

---

## 6) 验收标准（F01）

- [ ] `Launcher/` `cargo build` 产出 `minlauncher`
- [ ] `config` + 自动发现 / `MINENGINE_EDITOR`
- [ ] `create` → 合法 `.meproject` + `.mesettings` + Empty 内容
- [ ] `open` → Editor 加载成功
- [ ] `recent list` 跨重启持久化
- [ ] `.\scripts\verify.ps1` 仍通过
- [ ] Design / Impl / Registry 与拍板一致
- [ ] （非 F01）S05 Tauri 骨架可另开；不挡 F01 Done

---

## 7) 后续

| ID | 内容 |
|----|------|
| `LAUN-F01-S05` | Tauri 2 GUI |
| `LAUN-F02` | Build 编排 |
| `LAUN-F03` | Package |
| `LAUN-F04` | 多模板 / 引擎版本关联 |
| Engine chore | `ProjectRoot` 推导 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Placeholder 登记 |
| 2026-08-31 | CLI 优先；曾拟 Avalonia |
| 2026-08-31 | **拍板 Tauri**：Rust Core + clap CLI；S05 Tauri 2 GUI；否决 Avalonia/Electron |
