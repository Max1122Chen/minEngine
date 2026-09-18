# CLI-F02 — Automation Launch — Implementation Plan

## Meta
- **ID:** `CLI-F02`
- **Status:** Done
- **Last updated:** 2026-09-18
- **Related:** [Design Spec](./CLI-F02_AUTOMATION_LAUNCH_DESIGN.md) · [ED-F17](../../Editor/ED-F17_AUTOMATION_HOST_DESIGN.md) · [MCP-F02 schema](../../../../schemas/automation/v1/)

## TL;DR

扩展统一 CLI：解析 automation flags、启动后按 `--open-scene` 打开 Session，并把 pipe/ready 配置交给 Editor 供 ED-F17 Host 消费。**写 ready-file 与 Listen 属 ED-F17**。三切片 **Done**。

## Scope
- **In:** `CommandLineResult` 字段；CLI11 options + help；Editor 消费 `--open-scene`；缓存 pipe/ready 路径供 Host；无第二套 argv 扫描
- **Out:** Named Pipe Listen、写 ready-file、协议分派（ED-F17）；MCP Spawn（MCP-F03）

## Reader quick start
1. [Design](./CLI-F02_AUTOMATION_LAUNCH_DESIGN.md) §3.1–3.2
2. 下表切片
3. ED-F17 读 `Editor::GetAutomationPipeName` / `GetAutomationReadyFilePath`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `CLI-F02-S01` | `CommandLineResult` + `ApplicationCommandLine` 解析三 flag | **Done** | `--help` 含 automation；仅 ready-file → exit 2 |
| `CLI-F02-S02` | Editor：缓存 launch 配置；`--open-scene` 覆盖默认场景并入 DocumentHost | **Done** | `OpenSceneByPath`；getters 供 Host |
| `CLI-F02-S03` | Design/Registry/Progress/ACTIVE_WORK 收口 | **Done** | ready-file DoD → ED-F17 |

## 2) 切片详情

### CLI-F02-S01 — Parse automation flags

- **Goal:** 单一 CLI11 树增加 automation 选项，填入 `CommandLineResult`。
- **Touch:**
  - `minEngine/.../CLI/CommandLineResult.h`
  - `minEngine/.../CLI/ApplicationCommandLine.cpp`
- **Fields:**
  - `AutomationPipeName` (`--automation-pipe`)
  - `AutomationReadyFilePath` (`--automation-ready-file`)
  - `OpenSceneAssetPath` (`--open-scene`) — MVP：**资产路径**（项目相对）；Guid 后置
- **Rules:**
  - `--automation-ready-file` 而无 `--automation-pipe` → UsageError（2）
  - `test` 子命令带 automation flags → UsageError（2）
- **DoD:**
  - [x] `--help` 可见三 option
  - [x] 解析结果字段正确；无第二套扫描
- **Verify:** `MaximumEditor.exe --help`；`--automation-ready-file` alone → exit 2

### CLI-F02-S02 — Editor wire-up

- **Goal:** Initialize 后项目打开流程尊重 `--open-scene`；Host 可读取 pipe/ready。
- **Touch:** `Editor.h` / `Editor.cpp`
- **Behavior:**
  - 有 `OpenSceneAssetPath`：跳过 `EditorDefaultSceneName`，走 `SceneEditor::OpenSceneByPath`（失败 → 启动失败）
  - `NormalizeOpenSceneAssetPath`：去 `Assets/` 前缀、generic 分隔符
  - 存 `m_AutomationPipeName` / `m_AutomationReadyFilePath` + getters
  - **不**写 ready-file / Listen
- **DoD:**
  - [x] `--open-scene` 走 OpenSceneByPath + BootstrapDocumentHost
  - [x] getters 在 flag 给出时非空
- **Verify:** 编过 `Editor`；`--help` / usage；GUI 手验 open-scene 可选

### CLI-F02-S03 — Docs

- **Goal:** Feature 标 Done（启动面）；Design 验收中 ready-file / get_state 注明依赖 ED-F17。
- **Touch:** Design Meta；FEATURE_REGISTRY；ACTIVE_WORK；PROGRESS_LOG
- **DoD:**
  - [x] Registry Status **Done（启动面）**
  - [x] ACTIVE_WORK 下一刀 → ED-F17 Impl
- **Verify:** 链接可点

## 3) 依赖顺序

```text
CLI-F02-S01 → CLI-F02-S02 → CLI-F02-S03   (Done)
                ↓
         ED-F17（Listen → ready-file；get_state）
```

## 4) 延后

| Item | Reason | Unblock |
|------|--------|---------|
| `--open-scene` 接受 Guid | Design §7 仍可议 | 需要时再加解析 |
| 默认 pipe 名模板 | 调用方传全名 | MCP-F03 Spawn |
| stdout `AUTOMATION_READY` | ready-file 为主 | 可选后置 |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| open-scene 路径格式与 AssetPath 不一致 | `NormalizeOpenSceneAssetPath` |
| 过早写 ready | **禁止**本 Feature 写；仅缓存路径 |

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | Impl 开工；S01–S03；ready-file 归属 ED-F17 |
| 2026-09-18 | S01–S03 Done；Feature Done（启动面） |
