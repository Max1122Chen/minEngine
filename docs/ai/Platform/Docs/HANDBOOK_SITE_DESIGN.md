# 协作者文档站 — Design Spec

## Meta
- **ID:** `WF-F02`（已登记于 [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md)）
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-06-01
- **Related:** [HANDBOOK_SITE_IMPLEMENTATION.md](./HANDBOOK_SITE_IMPLEMENTATION.md)

## TL;DR
为 minEngine 建立**公开技术手册站**（nav 与 `src/Runtime` 分层对齐）：`docs/handbook/` + MkDocs Material → GitHub Pages（`https://max1122chen.github.io/minEngine/`）；**首页**承担路人速览，其余按**引擎架构与子模块**组织；**不展示、不链接** `docs/ai/`。README 由维护者另行精简，与 handbook 首页分工。实施见 [HANDBOOK_SITE_IMPLEMENTATION.md](./HANDBOOK_SITE_IMPLEMENTATION.md)（首期仅骨架占位）。

## Scope
- **In:** 文档三层模型；UE 式顶级 nav 与页面清单（标题级）；MkDocs/Material/插件与 CI 决策；`mkdocs.yml` 职责；与 README 的分工；GitHub Pages URL（无自定义域名）；GitHub Actions 部署
- **Out:** 各子系统正文（首期由 Implementation 限定为占位）；Doxygen/API 参考；多语言 i18n；任何指向 `docs/ai/` 的站点内链接或镜像

## Reader quick start
1. 本文件 — 站是什么、用什么建、怎么部署
2. 本文 §5.3 — 推荐 nav 树（可微调页面标题）
3. [HANDBOOK_SITE_IMPLEMENTATION.md](./HANDBOOK_SITE_IMPLEMENTATION.md) — 切片与验收

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
| `git-revision-date-localized` | 页脚最后更新 | 可选第二期 |

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
| 误将 `docs/ai` 纳入构建 | 内部草稿公开、构建慢 | 固定 `docs_dir` + 手写 nav + `--strict` |
| `site_url` 与真实 Pages URL 不一致 | 搜索/社交预览错链 | 实施时与 Settings 中 URL 对齐 |
| 中文内容与代码路径混排 | 链接失效 | 链接用仓库相对路径；CI strict |

---

## 9) 验收标准（Feature 级，实施时细化）

- [ ] `mkdocs build --strict` 在本地与 CI 通过
- [ ] `main` push 后 GitHub Pages 可访问且与 `site_url` 一致
- [ ] 站点内无指向 `docs/ai/` 的链接；构建产物不包含 `docs/ai`
- [ ] README 含文档站入口链接
- [ ] Implementation Plan 已撰写且切片可执行

---

## 10) 仍待你确认后定稿（再写 Implementation Plan）

| # | 项 | 状态 |
|---|-----|------|
| 1 | 顶级 nav 与 Runtime 目录对齐（§5.3） | **已定**；首期仅四层 `overview` 占位 |
| 2 | README 与 `index.md` | 维护者整理 README；S03 可只加文档站一行链接 |
| 3 | 不链 `docs/ai` | **已定** |
| 4 | PR docs build、`edit_uri`、首期插件 | **已定**（见 §6.4） |
| 5 | 仅 GitHub Pages 默认 URL | **已定** |
| 6 | Implementation Plan | **已撰写** — 见 [HANDBOOK_SITE_IMPLEMENTATION.md](./HANDBOOK_SITE_IMPLEMENTATION.md) |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿：WF-F02 登记；技术选型、分层架构、GHA Pages 部署；不含 handbook 正文与 Implementation Plan |
| 2026-06-01 | 修订：UE 式 nav 建议；不按人群分区；禁止链 `docs/ai`；PR build + edit_uri；固定 Pages URL |
| 2026-06-01 | nav 改为与 `src/Runtime` 分层对齐；Editor 独立 Tab；Test 不进公开站 |
| 2026-06-01 | Status → Planned；链接 Implementation Plan |
