# WF-F02 协作者文档站 — Implementation Plan

## Meta
- **ID:** `WF-F02`
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-06-01
- **Related:** [HANDBOOK_SITE_DESIGN.md](./HANDBOOK_SITE_DESIGN.md)

## TL;DR

首期 **3 个切片**：handbook 骨架（`index` + `getting-started` 占位 + Runtime 四层 `overview` 占位 + `Editor`/`参考` 暂不展开或仅 Editor 单层占位）→ MkDocs 本地可构建 → GitHub Actions 部署 Pages。  
**不写** Runtime 子系统正文（Reflection / Render / …）；由维护者后续按目录自行增补。  
当前切片：**S03**（S01–S02 已落地，待合并 `main` 后验 Pages）。

## Scope
- **In:** `docs/handbook/` 首期占位页；根目录 `mkdocs.yml`、`requirements-docs.txt`；`.github/workflows/docs.yml`；Design/Registry/ACTIVE_WORK 状态同步；`PROGRESS_LOG` 在 S03 后一条
- **Out:** Runtime 子目录深度 nav（`Function/Render/Material/...`）；handbook 技术正文；`docs/ai` 链接；README 大改（维护者另做）；Doxygen；PR 在线预览站；`reference/` 栏目（第二期）

## Reader quick start
1. [HANDBOOK_SITE_DESIGN.md](./HANDBOOK_SITE_DESIGN.md) — 已定稿边界与 nav 原则
2. 下表 — 切片顺序
3. 实施后本地：`pip install -r requirements-docs.txt` → `mkdocs serve`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `WF-F02-S01` | handbook 目录 + 占位 md + `mkdocs.yml` + `requirements-docs.txt` | Done | `mkdocs build --strict` 本地通过 |
| `WF-F02-S02` | GitHub Actions：PR/build + `main` deploy Pages | Done | `main` push 后 Pages URL 可开 |
| `WF-F02-S03` | 验收收尾：PROGRESS_LOG、Registry 备注、README 文档链接 | Done | 对照 Design §9 勾选 |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

---

## 2) 切片详情

### WF-F02-S01 — Handbook 骨架与 MkDocs 配置

- **Goal:** 可构建的静态站骨架；nav 与 `src/Runtime` 顶层对齐；全部为占位，无子系统正文。
- **Touch:**
  - `docs/handbook/index.md`
  - `docs/handbook/getting-started/index.md`（单页占位）
  - `docs/handbook/runtime/overview.md`
  - `docs/handbook/runtime/core/overview.md`
  - `docs/handbook/runtime/function/{framework,input,render}/overview.md`
  - `docs/handbook/runtime/platform/file-dialog/overview.md`
  - `docs/handbook/runtime/resource/asset-manager/overview.md`
  - `mkdocs.yml`（repo 根）
  - `requirements-docs.txt`（repo 根）
- **不创建（首期）:** `function/framework/`、`function/render/` 等深层路径；`reference/`；`docs/ai` 任意链接

**首期文件清单：**

| 路径 | 用途 |
|------|------|
| `index.md` | 首页速览占位（项目一句话、链到快速开始与 GitHub） |
| `getting-started/index.md` | 快速开始单页占位（环境 / 构建 / 运行 / verify） |
| `runtime/overview.md` | Runtime 总览 |
| `runtime/core/overview.md` | Core |
| `runtime/function/framework|input|render/overview.md` | Function 子系统 |
| `runtime/platform/file-dialog/overview.md` | Platform / FileDialog |
| `runtime/resource/asset-manager/overview.md` | Resource / 资产 |

**Editor Tab：** 首期 **不** 加 `editor/` 页（设计里有独立 Tab，待你写 Editor 文档时再开 nav 项，避免空 Tab）。若你希望 nav 上看见 Editor 但无正文，可在 S01 加 `editor/overview.md` 占位——默认 **Out**，实施时按你口头偏好二选一。

**占位页模板（统一风格，示例）：**

```markdown
# Core

> 文档建设中。代码目录：`minEngine/minEngine/src/Runtime/Core/`。
```

**`mkdocs.yml` 要点：**

| 键 | 值 |
|----|-----|
| `site_name` | minEngine |
| `site_url` | `https://max1122chen.github.io/minEngine/` |
| `repo_url` | `https://github.com/Max1122Chen/minEngine` |
| `edit_uri` | `edit/master/docs/handbook/` |
| `docs_dir` | `docs/handbook` |
| `theme` | Material，`language: zh` |
| `plugins` | `search`；Mermaid（`mermaid2` 或 Material 内置，二选一写死） |
| `nav` | 首页 · 快速开始 · **运行时**（总览 + Core + 功能层/平台层/资源层子页） |

- **DoD:**
  - [x] 上述 md 均已创建，无 `docs/ai` 链接
  - [x] `mkdocs build --strict` 在仓库根成功
  - [x] `nav` 仅到 Runtime 四层 overview，无 Render/Material 子树
  - [x] `requirements-docs.txt` 固定 mkdocs + mkdocs-material 版本范围
- **Verify:**
  ```powershell
  cd <repo-root>
  pip install -r requirements-docs.txt
  mkdocs build --strict
  mkdocs serve   # 可选，浏览器抽查首页与 nav
  ```

---

### WF-F02-S02 — GitHub Actions 与 Pages 部署

- **Goal:** `main` 上 handbook / mkdocs 变更自动部署；PR 仅构建不发布。
- **Touch:** `.github/workflows/docs.yml`
- **DoD:**
  - [x] `push` → `main`（paths 过滤 handbook + mkdocs + requirements + workflow）→ build + `deploy-pages`
  - [x] `pull_request` → 同 paths → build（deploy 仅 main）
  - [x] `permissions`: `contents: read`, `pages: write`, `id-token: write`
  - [ ] 仓库 Settings → Pages → Source: **GitHub Actions**（人工一次性，合并后确认）
- **Verify:**
  - 合并 S01+S02 到 `main` 后访问 `https://max1122chen.github.io/minEngine/`
  - 开 PR 改 handbook，Checks 中 docs job 绿

**Workflow 结构（实施时照抄设计 §6.2，补全 `pull_request` job）：**

```text
on: push(main) + pull_request → paths filter
jobs:
  build (PR + main)
  deploy (main only, needs build)
```

---

### WF-F02-S03 — 验收与留档

- **Goal:** Feature 首期闭环；便于你后续自行填正文。
- **Touch:**
  - `docs/ai/PROGRESS_LOG.md`（追加一条）
  - `docs/ai/FEATURE_REGISTRY.md`（Status → `Done` 仅当 S01–S03 全完成；或保持 `In Progress` 若 Editor/reference 仍算后续 — **首期 Done 定义：骨架+CI 上线**）
  - `docs/ai/Platform/Docs/HANDBOOK_SITE_DESIGN.md` Meta `Status: Done`（基础设施）或保留 `In Progress` 直至有实质 handbook 内容 — **建议：** 基础设施 Done 后 Registry 标 `In Progress`，正文由你写满后再标 Feature Done；或拆 `WF-F02` 仅 infrastructure Done，正文不计入 Feature Done。默认：**S03 后 Registry = `In Progress`**，备注「骨架已上线，子系统文档待补」。
  - `README.md`（**可选**）：增加一行「[文档](https://max1122chen.github.io/minEngine/)」
  - `docs/ai/ACTIVE_WORK.md` 更新下一动作
- **DoD:**
  - [ ] Design §9 基础设施项已满足（build、Pages、无 docs/ai 链）
  - [ ] PROGRESS_LOG 有 WF-F02-S01–S03 记录
  - [ ] 维护者知悉：后续在 `docs/handbook/<layer>/` 自行加页并改 `mkdocs.yml` nav
- **Verify:** 与设计 §9 清单人工勾选

---

## 3) 依赖顺序

```text
S01 → S02 → S03
```

无其他 Feature 硬依赖。可与代码开发并行。

---

## 4) 延后切片（不由首期实施；维护者或后续 WF 切片）

| 工作项 | 说明 | 建议归属 |
|--------|------|----------|
| `editor/overview.md` + nav | Editor 模块文档 | 你撰写时加页 |
| `function/render/…` 深度 nav | Render / Material / Pipeline 分册 | 按 `src` 子目录逐步 mirror |
| `reference/cli.md` 等 | CLI、术语、FAQ | `WF-F02` 续切片或维护者 |
| README 全面整理 | 与 `index.md` 去重 | 维护者 |
| `git-revision-date-localized` | 页脚日期 | 第二期插件 |
| PR 预览环境 | 在线 preview | 不做（设计已定） |

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：S01 骨架 + S02 CI + S03 验收；Runtime 子系统正文 Out |
| 2026-06-01 | 实施：快速开始合并单页；S01–S03 落地 |
