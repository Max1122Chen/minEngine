# Engine Launcher — Implementation Plan

## Meta
- **ID:** `LAUN-F01`
- **Type:** Implementation
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-31（拍板：Rust CLI + Tauri S05）
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
| LAUN-F01-S05 | Tauri 2 app + Web UI（**Deferred**） | Deferred | 操作等价 CLI |

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

### LAUN-F01-S05 — Tauri GUI（Deferred）

- **Goal:** 图形界面；**不阻塞 F01 Done**。
- **Touch:**
  - `crates/minlauncher-app`（Tauri 2）
  - `ui/` Web 前端；`#[tauri::command]` 调 core
  - capability 白名单
- **Unblock:** S01–S04 Done + 用户要 GUI；开工前拍板前端框架（React / Vue / Svelte）
- **Verify:** 主窗口 open/create/recent/settings 与 CLI 等价

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
| S05 Tauri GUI | F01 聚焦 CLI 契约 | S04 Done + 审批开 GUI |
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
