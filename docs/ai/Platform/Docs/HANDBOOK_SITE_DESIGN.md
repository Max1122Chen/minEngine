# 协作者文档站 — Design Spec

## Meta
- **ID:** `WF-F02`（已登记于 [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md)）
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-06-02
- **Related:** [HANDBOOK_SITE_IMPLEMENTATION.md](./HANDBOOK_SITE_IMPLEMENTATION.md)

## TL;DR
为 minEngine 建立**公开技术手册站**（nav 与 `src/Runtime` 分层对齐）：`docs/handbook/` + MkDocs Material → GitHub Pages；首期骨架已上线。本设计 **§11** 定义二期体验：**自动 nav**、**右侧 TOC**、**左侧栏导航**、**页脚最后编辑时间** 四项优化。不链接 `docs/ai/`。

## Scope
- **In（一期，已实施）：** 文档分层、Runtime 树 nav 占位、MkDocs/CI/Pages（§4–§6）
- **In（二期，§11）：** 自动 nav、TOC、侧栏、**Git 最后编辑时间**（页脚展示）、验收标准
- **Out:** handbook 子系统技术正文（维护者撰写）；Doxygen；i18n；`docs/ai` 展示；从 `src/` 全自动扫出 nav（仅 handbook 目录）

## Reader quick start
1. §4–§6 — 基建与部署
2. §5.3 — Runtime nav 树
3. **§11 — 二期体验（nav / TOC / 侧栏 / 最后编辑）**
4. [HANDBOOK_SITE_IMPLEMENTATION.md](./HANDBOOK_SITE_IMPLEMENTATION.md) — 切片 S04–S07

---

## 1) 背景与目标

### 1.1 问题
- 仓库已有长篇 `README.md` 与成熟的 `docs/ai/`（设计、路线图、AI 协作），但**缺少**面向不同读者的、可搜索、可深链的**稳定公开入口**。
- 个人开发期注释与文档不完整；引入协作后需要**可留档、可分享**的对外说明，且不与内部设计草稿混为一谈。

### 1.2 目标（成功长什么样）
- **首页**（handbook `index.md`）让路人几分钟内明白项目在做什么（职能类似更新后的 README，但更适合作站首页）。
- 需要改代码或理解子系统的人，沿 **UE 式主题 nav** 进入架构说明与各子模块文档，而非按「路人/用户/开发者」三分区。
- 公开站**自洽**：所需内容写在 `docs/handbook/`（可从内部设计**改写**），站点内**不出现** `docs/ai/` 链接。
- `main` 合并后文档站自动更新；`mkdocs build --strict` 在 CI 中拦截断链。

### 1.3 非目标（本 Feature 边界）
- 不替代 `docs/ai/` 的 Feature ID、Design、Progress 工作流（见 `WF-F01`）。
- 不在第一期追求 API 级自动生成文档。

---

## 2) 信息架构原则（已定）

| 原则 | 说明 |
|------|------|
| **不按人群分顶级 nav** | 不设「路人专区 / 用户专区 / 开发者专区」 |
| **首页 = 速览** | 单页：是什么、总架构图、链到 GitHub 与「快速开始」 |
| **主体 nav = Runtime 树** | 顶级 Tab **「运行时」** 下挂 `runtime/` 子树，与 `src/Runtime/` 逐级 mirror；**Editor** 等后续另开 Tab |
| **`docs/ai/` 隔离** | 仅仓库内设计与决策记录；**禁止**在 handbook 或 MkDocs 站点中链接、索引、构建该目录 |
| **README 独立维护** | 当前 README 已过时，由维护者另行整理；短期可与 handbook 首页并存，长期 README 保持短链入口即可 |

---

## 3) 文档分层（仓库内信息架构）

```text
┌─────────────────────────────────────────────────────────────┐
│  README.md          门面：电梯演讲 + 徽章 + 链到文档站       │
├─────────────────────────────────────────────────────────────┤
│  docs/handbook/     公开手册源（MkDocs docs_dir）— 稳定、可发布 │
├─────────────────────────────────────────────────────────────┤
│  docs/ai/           内部档案：Design、Roadmap、Progress、AI 协作 │
├─────────────────────────────────────────────────────────────┤
│  docs/external/     外部参考讨论稿（非定稿）                  │
└─────────────────────────────────────────────────────────────┘
```

| 层 | 构建进 MkDocs | 公开程度 | 变更节奏 |
|----|---------------|----------|----------|
| `docs/handbook/` | 是 | GitHub Pages 静态站 | 慢，面向读者 |
| `docs/ai/` | **否** | **仅克隆仓库后本地阅读**；公开站不引用 | 快，设计迭代 |
| `README.md` | 否（独立渲染） | GitHub 仓库首页 | 维护者整理为短门面；详述以 handbook 为准 |

**内容来源：** 实施时可将 `docs/ai/` 或代码中的事实**改写**进 handbook，但站点内无「去看 ai 目录」类链接。禁止 symlink / glob 纳入 `docs/ai`。

---

## 4) 技术选型

### 4.1 静态站生成器：**MkDocs**

| 维度 | 说明 |
|------|------|
| 选用理由 | 与现有 Markdown 工作流一致；配置简单；社区与 GitHub Pages 集成成熟 |
| 版本策略 | 在 CI 与本地文档中**固定**主版本（如 `mkdocs>=1.6,<2`），避免构建漂移 |
| 构建命令 | `mkdocs build --strict`（断链、缺失引用失败） |

### 4.2 主题：**Material for MkDocs**

| 维度 | 说明 |
|------|------|
| 选用理由 | 搜索、目录、暗色模式、移动端；与 README 中 Mermaid 图一致性好 |
| 中文 | 默认 `language: zh`；代码与标识符保持英文 |
| 扩展（候选，下轮确认是否启用） | `navigation.tabs` / `navigation.sections`、`content.code.copy` |

### 4.3 插件（候选清单）

| 插件 | 用途 | 首期建议 |
|------|------|----------|
| `search` | 全文搜索 | **启用** |
| `mermaid2` 或 Material 内置 Mermaid | 架构图 | **启用**（与仓库现有 Mermaid 一致） |
| `mkdocs-awesome-pages-plugin` | 灵活 nav | 可选；**优先手写 nav** 以免误收录 |
| `git-revision-date-localized`（§11.5） | 页脚最后编辑时间 | **二期启用** |
| `awesome-pages`（§11.2） | 按目录生成/维护 nav | **二期启用** |

**不选用（本期）：** Docusaurus（前端栈过重）、mdBook（更适合线性书而非多读者分区）、将 Sphinx 作为主站（更适合 API，与 handbook 定位不符）。

### 4.4 工具链位置

| 项 | 建议 |
|----|------|
| 依赖声明 | 仓库根 `requirements-docs.txt`（或 `pyproject.toml` 可选组 `[docs]`） |
| 本地预览 | `pip install -r requirements-docs.txt` → `mkdocs serve` |
| Python 版本 | CI 使用 3.11+（与 Actions `setup-python` 一致） |

---

## 5) 站基础架构（仓库布局与配置）

### 5.1 目录约定（拟建，实施阶段创建）

```text
minEngine/                          # 仓库根
├── mkdocs.yml                      # 站点配置、theme、plugins、nav
├── requirements-docs.txt           # MkDocs 依赖 pin
├── docs/
│   ├── handbook/                   # MkDocs 唯一 docs_dir（拟）
│   │   └── .gitkeep 或 index.md    # 实施时添加
│   └── ai/                         # 不纳入 docs_dir
└── .github/
    └── workflows/
        └── docs.yml                # 构建 + 部署（拟）
```

### 5.2 `mkdocs.yml` 职责（配置契约）

| 键 | 职责 |
|----|------|
| `site_name` / `site_url` | 站点标题；`site_url` 与 GitHub Pages URL 一致（利于 canonical / sitemap） |
| `repo_url` / `edit_uri` | 指向 GitHub 仓库；可选「编辑此页」链到 `docs/handbook/` |
| `docs_dir` | **固定** `docs/handbook` |
| `nav` | **显式列表**；不自动发现 `docs/ai` |
| `theme` | Material + `language: zh` |
| `plugins` | 见 §4.3 |
| `markdown_extensions` | `admonition`、`tables`、`toc`、`pymdownx` 代码高亮等（实施时按 Material 推荐集） |

### 5.3 推荐 nav：**与 `src/Runtime` 分层对齐**（已定方向）

**原则：** 公开手册的顶级 Tab 与 **`minEngine/minEngine/src/Runtime/` 下的一级目录并列**（Core · Function · Platform · Resource），子 nav **逐级 mirrors 子文件夹**；路人入口与「能跑起来」单独两栏，不强行塞进 Runtime 树。`docs/ai/` 仍不链接。

**与代码库的对应关系：**

```text
docs/handbook/                         src/Runtime/
├── index.md
├── getting-started/index.md
└── runtime/
    ├── overview.md                    Runtime/
    ├── core/overview.md               Core/
    └── function/
        ├── framework/overview.md      Function/Framework/
        ├── input/overview.md          Function/Input/
        └── render/overview.md         Function/Render/
    ├── platform/file-dialog/…         Platform/FileDialog/
    └── resource/asset-manager/…       Resource/（AssetManager、Loaders）
```

| 顶级 Tab | 代码锚点 | 说明 |
|----------|----------|------|
| **首页** | — | 项目速览、架构图 |
| **快速开始** | Playground、`verify.ps1` | 单页占位，后续扩展 |
| **运行时** | `Runtime/` | 侧栏分组：**总览 · 核心 · 功能层 · 平台层 · 资源层**；子页 mirror 子目录 |
| **Editor / 参考** | `minEngine/Editor/` 等 | 第二期再开 Tab |

**刻意不 mirror：**

| 项 | 处理 |
|----|------|
| `Runtime/Test/` | 不进公开 nav；测试说明可写在「快速开始 / verify」或 `reference/` |
| `Generated/` | 不文档化 |
| 过深叶子（如单个 `.cpp`） | 文档挂在**子系统目录**一级，不写「一文件一页」 |

**命名建议：**
- Tab 标签可与目录同名（**Core / Function / Platform / Resource / Editor**），中文正文标题再用「核心 / 功能 / 平台 / 资源」。
- handbook 路径统一在 **`runtime/`** 下、**小写** kebab-case（如 `file-dialog`），与 `src` 目录可辨；Linux CI 上路径大小写敏感。
- 每篇结构默认三节：**概览 · 所有权/数据流 · 主要类型与代码入口**（路径指向 `src/...`，非 `docs/ai`）。

**首期最小可发布：** `index` + `getting-started/*` + `core/overview` + `function/render/overview` + `editor/overview`；其余 nav 项可 stub「建设中」。

**`mkdocs.yml` nav（当前实施）：** 见仓库根 `mkdocs.yml` — 顶级 **运行时** 下含 Core / 功能层（Framework·Input·Render）/ 平台层（File Dialog）/ 资源层（Asset Manager）。

**维护规则：** 新增 `src/Runtime/...` 子系统时，在对应 handbook 分支增加 nav 项；禁止 glob 扫 `docs/ai`。

### 5.4 与 README 的关系（已定方向）

- **README：** 维护者自行更新为短门面（徽章、一句话、文档站 URL、极简 build 命令）；不要求与旧版长篇结构一致。
- **handbook `index.md`：** 可吸收 README 中**仍准确**的介绍与架构图；README 不必与 index 逐段同步，避免双份长篇。
- 仓库首页链到文档站；文档站首页链回 GitHub 仓库即可，**无需**链到 `docs/ai`。

---

## 6) GitHub Actions 部署方式

### 6.1 托管：**GitHub Pages**

| 项 | 选择 |
|----|------|
| 发布分支 | `gh-pages`（仅含构建产物）或 **Actions 工件 + Pages 新流程**（推荐后者，见下） |
| 站点 URL | **`https://max1122chen.github.io/minEngine/`**（GitHub Pages project site；**无自定义域名**；`mkdocs.yml` 的 `site_url` 固定为此） |
| 触发 | `push` 到 `main`（路径过滤：`docs/handbook/**`、`mkdocs.yml`、`requirements-docs.txt`、`.github/workflows/docs.yml`） |
| 并发 | 同一 workflow 使用 `concurrency: group: pages` 避免重叠部署 |

### 6.2 推荐 Workflow 形态（GitHub Actions 官方 Pages 流程）

```yaml
# 结构说明（非最终文件）— 实施时写入 .github/workflows/docs.yml
on:
  push:
    branches: [main]
    paths: [docs/handbook/**, mkdocs.yml, requirements-docs.txt, .github/workflows/docs.yml]

permissions:
  contents: read
  pages: write
  id-token: write

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.11"
      - run: pip install -r requirements-docs.txt
      - run: mkdocs build --strict
      - uses: actions/upload-pages-artifact@v3
        with:
          path: site/

  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/deploy-pages@v4
        id: deployment
```

**仓库设置（一次性，实施时）：** Settings → Pages → Source: **GitHub Actions**。

### 6.3 备选：mkdocs gh-deploy

| 方式 | 说明 | 结论 |
|------|------|------|
| `mkdocs gh-deploy` | 推送到 `gh-pages` 分支 | 简单，但分支与 Actions 工件二选一；**默认不采用**，以免与 Pages 新流程混用 |
| `peaceiris/actions-gh-pages` | 第三方推 `gh-pages` | 可用；优先官方 `deploy-pages` 减少维护面 |

### 6.4 CI 质量门与协作选项（已定）

| 检查 / 选项 | 决策 | 说明（给人看的解释） |
|-------------|------|----------------------|
| 严格构建 | **启用** | `mkdocs build --strict`：断链、坏引用直接失败 |
| **PR 文档构建** | **启用**（仅 build，不 deploy） | 有人提 PR 改了 handbook 时，CI 先建一遍站，合并前就能发现链接错误；**不会**给每个 PR 单独开预览网址 |
| **`edit_uri`** | **启用** | Material 页脚「编辑此页」跳到 GitHub 上对应 `docs/handbook/...md`，方便协作者改文档 |
| 插件首期 | `search` + Mermaid | 见 §4.3；`git-revision-date` 第二期 |
| 失败策略 | 构建失败不上线 | — |

### 6.5 本地预览

- 协作者：`pip install -r requirements-docs.txt` → `mkdocs serve`（`127.0.0.1:8000`）。
- **PR 在线预览站**：首期不做（成本高）；需要时用本地 serve 或合并后在 Pages 查看。

---

## 7) 备选方案（摘要）

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| MkDocs + Material + GHA Pages | 轻量、与 Markdown 一致、官方 Pages 集成 | 无内置 API 参考 | **选用** |
| mdBook | 线性阅读体验好 | nav 与多读者分区较弱 | 不选用 |
| Docusaurus | 生态大 | Node 栈、对本仓库偏重 | 不选用 |
| 仅 README + `docs/ai` | 零成本 | 路人/用户门槛高、内部稿易误读 | 保留为底层，**上叠 handbook** |

---

## 8) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| handbook 与 README 内容重复 | 维护双倍 | §5.4 分工；迁移时删减 README 长文 |
| 误将 `docs/ai` 纳入构建 | 内部草稿公开、构建慢 | 固定 `docs_dir` + `--strict` |
| 自动 nav 扫入未就绪草稿 | 侧栏出现空页 | `.pages` 显式列表或忽略 `_` 前缀；strict 断链 |
| `site_url` 与真实 Pages URL 不一致 | 搜索/社交预览错链 | 实施时与 Settings 中 URL 对齐 |
| 中文内容与代码路径混排 | 链接失效 | 链接用仓库相对路径；CI strict |

---

## 9) 验收标准

### 9.1 一期（骨架 + CI）

- [x] `mkdocs build --strict` 本地与 CI 通过
- [ ] `main` push 后 Pages 可访问（维护者验证）
- [x] 无 `docs/ai` 链接
- [x] README 文档站链接
- [x] Implementation Plan 存在

### 9.2 二期（§11 体验）

- [x] 新增页仅需改对应 `.pages`（根 nav 由 `docs/handbook/.pages` + `awesome-pages` 生成）
- [x] 长文右侧 TOC 至 `###`，`toc.follow` 已启用
- [x] 侧栏 `navigation.prune` / `path` / `top` / `indexes`
- [x] 页脚最后编辑（`git-revision-date-localized`；CI `fetch-depth: 0`）
- [x] `mkdocs build --strict` 通过

---

## 10) 一期决策归档

| # | 项 | 状态 |
|---|-----|------|
| 1 | Runtime 树 nav（§5.3） | **已定** |
| 2 | README / `index` 分工 | 维护者整理 README |
| 3 | 不链 `docs/ai` | **已定** |
| 4 | PR build、`edit_uri`、插件 | **已定** |
| 5 | GitHub Pages 默认 URL | **已定** |
| 6 | 二期体验（§11） | **已定方向**，见 Implementation S04–S07 |

---

## 11) 二期：站点体验（自动 nav · TOC · 侧栏 · 最后编辑）

### 11.1 目标与四项诉求

| 诉求 | 用户价值 | 成功标准 |
|------|----------|----------|
| **① 自动 nav** | 新增 handbook 页时少改根配置，结构与 `runtime/` 目录一致 | 在约定目录下新建 `.md` 后，侧栏自动出现；中文分组标题可配置 |
| **② 右侧 TOC** | 长文内快速跳转章节 | 桌面端右侧显示标题树；层级清晰；滚动跟随 |
| **③ 左侧栏** | 深树（Runtime 子系统）下仍易浏览 | 当前分支展开；面包屑 / 回顶 |
| **④ 最后编辑** | 读者判断文档是否过时 | 每页页脚可见该文件**最后一次 Git 提交**日期（中文格式） |

**约束（继承一期）：**

- 仅扫描 `docs/handbook/`；**不**读取 `docs/ai/` 或 `src/Runtime` 自动生成正文。
- 顶级 Tab 仍固定三项：**首页 · 快速开始 · 运行时**（不因自动 nav 而增加未审核的顶层 Tab）。
- 自动 nav **只作用于「运行时」子树**；首页与快速开始仍在根 `mkdocs.yml` 显式声明。

---

### 11.2 ① 自动 nav — 方案

#### 选用：**混合 nav**（根手写 + Runtime 子树 `mkdocs-awesome-pages-plugin`）

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| 根 `mkdocs.yml` 手写全部 nav | 完全可控 | 每增一页改 yaml，易漏 | 仅保留顶层 |
| 脚本扫描 `src/Runtime` 生成 yaml | 与代码严格同步 | 分组语义（「功能层」）与文件夹不一致；需维护映射表 | **不选用** |
| **`awesome-pages` + `.pages`** | 按 handbook 目录生成；标题/顺序/localize 在目录内 | 需学习 `.pages` 语法 | **选用** |

#### 配置契约

**根 `mkdocs.yml`（精简后示意）：**

```yaml
plugins:
  - search
  - awesome-pages

nav:
  - 首页: index.md
  - 快速开始: getting-started/index.md
  - 运行时: runtime/    # 子树交给 awesome-pages，不再逐条列 md
```

**`docs/handbook/runtime/.pages`（示意）：**

```yaml
title: 运行时
nav:
  - overview.md
  - 核心: core
  - 功能层: function
  - 平台层: platform
  - 资源层: resource
```

**`docs/handbook/runtime/function/.pages`（示意）：**

```yaml
title: 功能层
nav:
  - framework
  - input
  - render
  - ...   # 通配：该目录下未来新增子目录/文档自动纳入（受 .pages 规则约束）
```

**子目录默认规则：**

- 每个子系统目录放 `overview.md`（或 `index.md`，全站统一一种，**推荐 `overview.md`** 与现有一致）。
- 新增文档：例如 `runtime/function/render/pipeline.md` → 在 `runtime/function/render/.pages` 用 `nav: [overview.md, pipeline.md, ...]` 或 `...` 包含全部子项。
- **中文标题：** 在 `.pages` 用 `title:` 或 `*.md` 旁映射；若需从 Markdown 取标题，依赖文件首行 `#`（插件行为以 awesome-pages 文档为准，实施时验证）。

**依赖：**

```text
mkdocs-awesome-pages-plugin>=2.9,<3
```

写入 `requirements-docs.txt`。

#### 与「镜像 src/Runtime」的关系

- 目录布局仍建议与 `src/Runtime` **同名可辨**（已有一期约定）。
- 自动 nav **镜像的是 handbook 文件夹**，不是直接扫 `src/`；你加代码目录时，**同步**在 handbook 建同名路径 + md，nav 即跟随。
- 语义分组（「功能层」vs 文件夹 `function`）由 **`.pages` 的 `title`/`nav` 键** 表达，不强迫文件夹改名。

#### 风险与缓解

| 风险 | 缓解 |
|------|------|
| 通配 `...` 扫进草稿 `_draft.md` | 约定前缀 `_` 忽略；或不用通配、显式列表 |
| 与手写 nav 混用导致重复 | 二期后删除 `mkdocs.yml` 中 Runtime 下逐条 `md` 路径 |
| 构建顺序/plugin 冲突 | CI 保持 `--strict`；PR 必跑 docs build |

---

### 11.3 ② 右侧目录（TOC）— 方案

#### 选用：**Material 内置 TOC + pymdownx.toc 深度控制**

不在二期引入单独 TOC 插件；通过主题与扩展配置。

**`theme.features` 增加：**

```yaml
features:
  - toc.follow          # 滚动时高亮当前章节
  - content.code.copy   # 已有
```

**`markdown_extensions` 调整：**

```yaml
  - toc:
      permalink: true
      toc_depth: 2-3     # 右侧 TOC 显示到 ###；## 必显
  - pymdownx.highlight
  - pymdownx.superfences: ...
```

**作者约定（handbook 正文）：**

| 层级 | Markdown | 用途 |
|------|----------|------|
| 页标题 | `#` 一个 | 页名；可不进 TOC 或作为根（Material 默认行为实施时确认） |
| 节 | `##` | TOC 主条目 |
| 小节 | `###` | TOC 子条目 |
| 更细 | `####` 及以下 | 默认不进 TOC（避免右侧过长） |

**可选（实施时二选一）：**

- `toc.integrate`：宽屏将 TOC 并入左侧（与「右侧 TOC」诉求可能冲突）→ **默认不启用**，保持右侧独立 TOC。
- 单页覆盖：页首 YAML `toc_depth: 4` 用于超长设计页。

**验收：** 在 ≥800 字、含 `##`/`###` 的占位页上，右侧 TOC 可见且 `toc.follow` 生效。

---

### 11.4 ③ 左侧栏导航 — 方案

#### 选用：**Material navigation 特性组合**

在 §11.3 基础上，为 **Runtime 深树** 增加：

```yaml
theme:
  features:
    - navigation.tabs          # 已有：首页 / 快速开始 / 运行时
    - navigation.sections      # 已有：侧栏分组
    - navigation.expand        # 默认展开侧栏一级（可选，见下）
    - navigation.prune         # 仅展开当前活动分支，减少噪音
    - navigation.path          # 页顶面包屑（显示层级路径）
    - navigation.top           # 返回顶部按钮
    - navigation.indexes       # 节索引页：目录页可展示子链接列表
    - toc.follow
```

| 特性 | 建议 | 说明 |
|------|------|------|
| `navigation.prune` | **启用** | 深树下只展开当前路径；与 expand 同时开时以 Material 文档行为为准，**优先 prune** |
| `navigation.expand` | **首期不启用** | 全展开在子页增多后噪音大；若你偏好「一眼见全树」，实施时可改为只 expand + 不用 prune |
| `navigation.path` | **启用** | 弥补 Tab 内层级深时的定位感 |
| `navigation.indexes` | **启用** | `runtime/overview.md`、各 `overview.md` 作节索引，列出子文档链接 |
| `navigation.top` | **启用** | 长页滚动后回顶 |

**`navigation.sections`：** 保持；与 awesome-pages 生成的分组标题配合。

**移动端：** Material 自动折叠侧栏；不要求二期单独适配。

---

### 11.5 ④ 最后编辑时间 — 方案

#### 选用：**`mkdocs-git-revision-date-localized-plugin`**

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| 页内手写 `updated: 2026-06-02` | 完全可控 | 易忘改、与 Git 脱节 | **不选用** |
| **`git-revision-date-localized`** | 与提交历史一致；Material 自动注入页脚；支持中文 locale | 依赖 Git；CI 需拉全历史 | **选用** |
| 仅用 `fallback_to_build_date` | 无 Git 也能构建 | 日期变为构建时间，误导「时效性」 | 仅作**回退**，不作主路径 |

#### 展示契约

| 项 | 决策 |
|----|------|
| 展示内容 | **仅最后更新**（`enable_creation_date: false`），避免页脚两行信息过长 |
| 粒度 | `type: date`（日历日期即可；若需时分再改为 `datetime`） |
| 时区 | `Asia/Shanghai` |
| 文案语言 | `locale: zh` → 页脚如「最后更新: 2026年6月2日」类格式（以插件实际渲染为准） |
| 位置 | Material 主题在页面**底部**（与「编辑此页」相邻区域），不需改 handbook 模板 |
| 新文件 / 未提交 | `fallback_to_build_date: true`，避免本地或浅克隆构建失败 |

#### `mkdocs.yml` 插件段（示意）

```yaml
plugins:
  - search
  - awesome-pages
  - git-revision-date-localized:
      enable_creation_date: false
      type: date
      timezone: Asia/Shanghai
      locale: zh
      fallback_to_build_date: true
```

**依赖：**

```text
mkdocs-git-revision-date-localized-plugin>=1.2,<2
```

写入 `requirements-docs.txt`（与 S04 同次变更即可）。

#### CI / 本地构建要求

| 环境 | 要求 |
|------|------|
| **GitHub Actions** | `actions/checkout@v4` 设置 **`fetch-depth: 0`**（完整历史），否则所有页显示同一日期 |
| **本地** | 在 git 仓库内执行 `mkdocs build`；未 commit 的新文件用 fallback 日期 |
| **浅克隆** | 日期可能不准 → 文档维护者 clone 时建议正常深度 |

**Workflow 补丁（`.github/workflows/docs.yml`）：**

```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 0
```

#### 与「时效性」相关的读者提示（可选）

- 在 `index.md` 或 `_authoring.md` 加一句站级说明：「页脚日期为该页 **最后一次修改** 的 Git 提交日，不代表引擎运行时版本。」
- 不要求每页手写「可能已过时」免责声明。

#### 风险与缓解

| 风险 | 缓解 |
|------|------|
| CI 浅 checkout 导致日期全为今天 | `fetch-depth: 0` + S07 验收对照已知旧文件的日期 |
| fork PR 自 fork 无历史 | PR build 仅验证能构建；Pages 以 `main` 为准 |
| 合并多文件一次 commit | 同 commit 的页显示相同日期（符合 Git 语义） |

---

### 11.6 二期实施顺序与切片映射

```text
S04（自动 nav）→ S05（TOC）→ S06（侧栏）→ S07（最后编辑 + 二期总验收）
```

| 切片 | 内容 | 主要 Touch |
|------|------|------------|
| **WF-F02-S04** | awesome-pages；`runtime/**/.pages`；精简根 `nav` | `mkdocs.yml`, `requirements-docs.txt`, `.pages` |
| **WF-F02-S05** | `toc_depth`、`toc.follow`；写作约定 | `mkdocs.yml`, 可选 `_authoring.md` |
| **WF-F02-S06** | `navigation.prune/path/top/indexes`；样例长文 | `mkdocs.yml`, 示例页 |
| **WF-F02-S07** | `git-revision-date-localized`；CI `fetch-depth: 0` | `mkdocs.yml`, `requirements-docs.txt`, `docs.yml` |

S04–S07 可在一个 PR 完成；S07 依赖 Git 仓库，宜与 S02 workflow 同文件修改。

---

### 11.7 二期 Out of scope

- 从 `src/Runtime` 自动生成 markdown 或 API 文档。
- PR 临时预览环境（仍不做）。
- 全文搜索增强（Algolia 等）。
- 自定义域名。

---

### 11.8 备选方案（二期摘要）

| 议题 | 备选 | 结论 |
|------|------|------|
| 自动 nav | `mkdocs-literate-nav` + `SUMMARY.md` | 多一层文件 → 不选用 |
| TOC | 第三方 TOC 插件 | 与 Material 重复 → 不选用 |
| 侧栏 | `navigation.expand` 常开 | 子页多难扫 → prune + path |
| 最后编辑 | 仅显示 `build_date` | 不能反映内容陈旧度 → Git revision 为主 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：WF-F02 登记；技术选型、分层架构、GHA Pages 部署；不含 handbook 正文与 Implementation Plan |
| 2026-06-01 | 修订：UE 式 nav 建议；不按人群分区；禁止链 `docs/ai`；PR build + edit_uri；固定 Pages URL |
| 2026-06-01 | nav 改为与 `src/Runtime` 分层对齐；Editor 独立 Tab；Test 不进公开站 |
| 2026-06-01 | Status → Planned；链接 Implementation Plan |
| 2026-06-02 | §11 二期：自动 nav（awesome-pages）、TOC、侧栏 Material 特性；§9 分一期/二期验收 |
| 2026-06-02 | §11.5 ④ 最后编辑：`git-revision-date-localized` + CI `fetch-depth: 0`；S07 |
