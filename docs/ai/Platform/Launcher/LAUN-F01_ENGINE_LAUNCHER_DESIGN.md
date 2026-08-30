# Engine Launcher — Placeholder

## Meta
- **ID:** `LAUN-F01`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../../ACTIVE_WORK.md), [ENGINE_STARTUP_DESIGN.md](../Startup/ENGINE_STARTUP_DESIGN.md)
- **Branch / worktree:** `feat/launcher` · `D:/Dev/GitRepo/minEngine-launcher`

## TL;DR
独立 **引擎启动器**：工程选择、最近项目、启动 Editor/工具。与渲染轨弱耦合；从 `master` 竖切，合入 `master`。

## Scope
- **In:** 启动器 exe/shell、`.meproject` 解析与打开、最近列表、调用 Editor CLI
- **Out:** 安装包/自动更新、账号登录、云工程同步

## 依赖
- **Hard:** `CLI-F01`（统一命令行）
- **Soft:** 无

## 验收（草案）
- [ ] 从启动器打开 `MyMEProject`（worktree 本地路径）
- [ ] 最近项目列表持久化
- [ ] `verify.ps1` / smoke 不受破坏

## Status note（Planned）
| 字段 | 内容 |
|------|------|
| Why now | 与 `feat/render` VK 障碍解耦；可全并行 |
| What's not | 正式 Design 正文、Implementation Plan、代码 |
| Resume when | 维护者拍板竖切 S01（壳 + 打开工程） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry + ACTIVE_WORK 登记；`feat/launcher` + worktree 初始化 |
