# Engine Launcher — Implementation Plan

## Meta
- **ID:** `LAUN-F01`
- **Type:** Implementation
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-31（S05 GUI 已实现）
- **Related:** [Design](./LAUN-F01_ENGINE_LAUNCHER_DESIGN.md)

## TL;DR

**F01 = Rust CLI：** S01 workspace + `open` → S02 `config`/定位 → S03 `create` → S04 `recent`。  
**S05 = Tauri 2 GUI（Deferred）**，复用 `minlauncher-core`。不改 minEngine CMake。

## Scope
- **In:** `Launcher/` Cargo workspace、`Templates/Empty`、settings 约定
- **Out:** Tauri UI（S05）、Engine `ProjectManager` 改动、Build/Package、强制 CI `cargo`

## Reader quick start
1. [Design](./LAUN-F01_ENGINE_LAUNCHER_DESIGN.md)
2. 下表切片
3. `PROGRESS_LOG.md` — 落地后追加

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| LAUN-F01-S01 | Cargo workspace + core + `minlauncher open` | Done | `open MyMEProject` → Editor pid |
| LAUN-F01-S02 | `engine_locator` + `config show/set` + 自动发现 | Done | `config set editor` |
| LAUN-F01-S03 | `project_factory` + `create` + `Templates/Empty` | Done | `create LaunSmokeTest` |
| LAUN-F01-S04 | `project_catalog` + `recent list/remove` | Done | 列表跨命令持久化 |
| LAUN-F01-S05 | Tauri 2 + React UI | Done | `cargo build -p minlauncher-app`；GUI 手动验收 |

---

## 2) 切片详情

### LAUN-F01-S01 — workspace + open

- **Goal:** 最小可运行 CLI，能打开已有工程。
- **Touch:**
  - `Launcher/Cargo.toml`（workspace）
  - `crates/minlauncher-core`：`process_launcher`, `project_validator`
  - `crates/minlauncher`：`clap`，子命令 `open`
  - `Launcher/README.md`（rustup / `cargo run`）
- **DoD:**
  - [ ] `cargo build -p minlauncher` 成功
  - [ ] `minlauncher open <path>` → spawn Editor `--project <abs>`
  - [ ] Editor 路径：`--editor` 和/或 `MINENGINE_EDITOR`（S02 收拢到 config）
- **Verify:**
  ```powershell
  cd Launcher
  cargo run -p minlauncher -- open ..\minEngine\MyMEProject\MyMEProject.meproject --editor ..\minEngine\bin\Editor.exe
  ```

### LAUN-F01-S02 — Engine 定位 + config

- **Goal:** 持久化设置与自动发现。
- **Touch:** `engine_locator`, `settings`, `config show` / `config set`
- **DoD:**
  - [ ] 跨平台 config 路径（Design §3.9）
  - [ ] 向上查找 `minEngine/bin/Editor[.exe]`
  - [ ] `MINENGINE_EDITOR` 覆盖 settings
- **Verify:** 不传 `--editor`，靠 config / 发现 `open` 成功

### LAUN-F01-S03 — create + Empty

- **Goal:** 命令行创建新工程。
- **Touch:** `project_factory`, `guid`, `Launcher/Templates/Empty/`
- **DoD:**
  - [ ] `minlauncher create <name> --parent <dir>`
  - [ ] 生成 descriptor、settings、最小场景；不覆盖已有目录
- **Verify:** create → open → Editor 无 OpenProject 失败

### LAUN-F01-S04 — recent

- **Goal:** 最近列表持久化。
- **Touch:** `project_catalog`；`recent list` / `remove`；`open`/`create` 写 Recent
- **DoD:**
  - [ ] 列表含名称、路径、时间；失效标记 missing
  - [ ] remove 仅删列表项
- **Verify:** open → list → 重启 CLI → 仍在

### LAUN-F01-S05 — Tauri GUI + React（Review — 待设计审批）

- **Goal:** 图形界面；行为与 CLI 等价；视觉对齐 Editor Dark 工业灰（Design §3.14）。
- **Design:** [§3.11–§3.15](./LAUN-F01_ENGINE_LAUNCHER_DESIGN.md#311-gui-s05--设计草案待审批)
- **Touch:**
  - `crates/minlauncher-app`（Tauri 2）
  - `ui/` — React + Vite + TypeScript + `theme/tokens.css`
  - `#[tauri::command]` 薄封装（Design §3.12）
  - Tauri dialog plugin；capability 白名单
- **子切片（审批后执行）：**

| 子步 | 内容 | 验证 |
|------|------|------|
| S05a | `tauri init`、workspace 接入、空窗口 + tokens.css | `cargo tauri dev` 黑灰壳 |
| S05b | commands 对接 core（recent / settings / editor status） | 列表与 CLI 一致 |
| S05c | Projects 视图：列表、Open、New Project 模态 | open/create → Editor |
| S05d | Settings 视图、底栏状态、错误提示 | config 读写一致 |

- **Unblock:** S01–S04 Done ✅；**用户审批 Design §3.11–§3.15**
- **Verify:** Design §3.15 勾选全部通过

---

## 3) 依赖顺序

```text
S01 → S02 → S03 → S04
                    ↘ S05（Deferred）
```

---

## 4) 延后切片

| Slice ID | Reason | Unblock |
|----------|--------|---------|
| S05 Tauri GUI | 设计草案 Review | S04 Done + **Design §3.11–§3.15 审批** |
| Build/Package | 无 Game target | LAUN-F02+ |

---

## 5) 工具链备忘（开发机）

| 阶段 | 需要 |
|------|------|
| F01 | rustup（stable）、cargo |
| S05 | + Node.js（前端）、Windows WebView2 Runtime（通常已装） |

建议 core 单测：`cargo test -p minlauncher-core`（JSON round-trip、路径解析）。

---

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 初版（曾拟 .NET / Avalonia） |
| 2026-08-31 | **拍板 Tauri**：Rust S01–S04；S05 Tauri 2 Deferred |
| 2026-08-31 | S05 细化：React + 子切片 S05a–d；状态 → **Review**（Design §3.11–§3.15） |
