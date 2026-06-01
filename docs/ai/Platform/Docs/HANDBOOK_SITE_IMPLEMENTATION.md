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
当前切片：**S04–S07 Done**；子系统正文仍由维护者补充。

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
| `WF-F02-S04` | 自动 nav：`awesome-pages` + `runtime/**/.pages`，根 nav 精简 | Done | 增页改 `.pages` 即可 |
| `WF-F02-S05` | 右侧 TOC：`toc_depth`、`toc.follow`、写作约定 | Done | `render/overview` 样例 |
| `WF-F02-S06` | 侧栏：`navigation.prune/path/top/indexes` + 联调 | Done | Material features |
| `WF-F02-S07` | 页脚最后编辑：`git-revision-date-localized` + CI `fetch-depth: 0` | Done | `exclude_docs` 隐藏 `_authoring` |

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
S01 → S02 → S03 → S04 → S05 → S06 → S07
```

S04–S07 可同 PR；S07 修改 `docs.yml` 与 S04 一并合并。

---

## 2b) 切片详情 — 二期（Design §11）

### WF-F02-S04 — 自动 nav

- **Goal:** Runtime 子树由目录 + `.pages` 驱动；根 `mkdocs.yml` 仅保留首页、快速开始、`运行时: runtime/`。
- **Touch:** `requirements-docs.txt`（`mkdocs-awesome-pages-plugin`）；`mkdocs.yml`；`docs/handbook/runtime/.pages` 及 `core/`、`function/`、`platform/`、`resource/` 下 `.pages`。
- **DoD:**
  - [ ] 删除根 nav 中 Runtime 下逐条 md 枚举
  - [ ] 在 `runtime/function/render/` 新增测试页（可 `_tmp` 后删）证明仅改 `.pages` 或目录即可出现侧栏
  - [ ] `mkdocs build --strict` 通过
- **Verify:** 本地 `mkdocs serve`，侧栏结构与现网一致或更清晰

### WF-F02-S05 — 右侧 TOC

- **Goal:** 长文右侧章节目录 + 滚动跟随。
- **Touch:** `mkdocs.yml` — `toc.follow`、`toc_depth: 2-3`；可选 `docs/handbook/_authoring.md`（##/### 约定）。
- **DoD:**
  - [ ] 至少一页（如扩写 `runtime/function/render/overview.md`）含多个 `##`/`###`，右侧 TOC 正常
- **Verify:** 桌面宽度浏览器目视 + build strict

### WF-F02-S06 — 侧栏导航

- **Goal:** prune/path/top/indexes。
- **Touch:** `mkdocs.yml` `theme.features`。
- **DoD:**
  - [ ] `navigation.prune`、`navigation.path`、`navigation.top`、`navigation.indexes` 已启用
  - [ ] 从首页点入 Runtime 深页，面包屑与展开行为符合预期
- **Verify:** 浏览器目视

### WF-F02-S07 — 最后编辑时间 + 二期验收

- **Goal:** 页脚显示每页最后 Git 提交日期（中文）；二期总验收。
- **Touch:** `requirements-docs.txt`；`mkdocs.yml` 插件；`.github/workflows/docs.yml`（`fetch-depth: 0`）；`PROGRESS_LOG`。
- **DoD:**
  - [ ] `git-revision-date-localized` 已配置（`locale: zh`，仅最后更新，无创建日）
  - [ ] docs workflow checkout 使用 `fetch-depth: 0`
  - [ ] 至少一页已知较早 commit 的文件，页脚日期早于「今天」（证明非纯 build date）
  - [ ] Design §9.2 全部勾选；PROGRESS_LOG 记录 S04–S07
- **Verify:** `mkdocs build --strict`；`mkdocs serve` 抽查页脚；CI 绿

---

## 4) 延后（非 S04–S06）

| 工作项 | 说明 | 建议归属 |
|--------|------|----------|
| `editor/overview.md` + nav | Editor 模块文档 | 维护者加页 + `.pages` |
| `function/render/…` 深度正文 | Pipeline / Material 分册 | 维护者 |
| `reference/cli.md` 等 | CLI、术语、FAQ | 新 Tab 或 runtime 外 `.pages` |
| README 全面整理 | 与 `index.md` 去重 | 维护者 |
| `git-revision-date-localized` | 页脚日期 | 可选插件 |
| PR 预览环境 | 在线 preview | 不做 |

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：S01 骨架 + S02 CI + S03 验收；Runtime 子系统正文 Out |
| 2026-06-01 | 实施：快速开始合并单页；S01–S03 落地 |
| 2026-06-02 | 追加 S04–S06（Design §11 自动 nav / TOC / 侧栏） |
| 2026-06-02 | 追加 S07（最后编辑时间 + §9.2 总验收） |
