# 文档与协作规范 v1

Last updated: 2026-05-28  
Status: **Active**  
Owner: project maintainer + AI collaborator

---

## 1) 目标

让**不了解开发上下文**的读者，仅凭文档就能回答：

1. 这项工作在解决什么？
2. 当前做到哪、还缺什么？
3. 下一步是谁做、怎么验收？
4. 若暂停/取消，原因和重开条件是什么？

---

## 2) 文档类型（六类）

| 类型 | 回答的问题 | 典型文件名 |
|------|------------|------------|
| **Roadmap** | 为什么做、先后顺序 | `*_ROADMAP.md` |
| **Design Spec** | 做什么、不做什么、方案与风险 | `*_DESIGN.md`、`*_PLAN.md` |
| **Implementation Plan** | 怎么拆成可提交切片 | `*_IMPLEMENTATION.md`、`*_ROLLOUT*.md` |
| **ADR** | 为什么选 A 不选 B | `ADR-*.md` |
| **Progress Log** | 何时做了什么、验了什么 | `PROGRESS_LOG.md`（追加条目） |
| **Bug Record** | 出了什么问题、如何修复与回归 | `bugs/*.md` |

**原则：** Design 写「是什么」；Implementation 写「怎么切 PR」；ADR 只写决策，不重复设计正文。

---

## 3) 统一 ID 与领域代号

### 3.1 领域（DOMAIN）

**本表是常用领域示例，不是封闭枚举。** 新能力域可随开发扩展。

- **新增领域：** 在本表或 `Platform/PLATFORM_ROADMAP.md` 登记一行（代号 + 范围）；代号全大写、2–8 字符；不得与已有代号冲突或复用。
- **废弃领域：** 标记 `Deprecated`，不回收代号。
- **Feature / Slice / Bug ID** 仍使用登记后的 `<DOMAIN>` 前缀。

| 代号 | 范围（示例） |
|------|----------------|
| `WF` | 工程流程、文档、CI、测试基础设施 |
| `CORE` | 反射、对象、序列化、启动、内存 |
| `ASSET` | AssetManager、Registry、导入、Content Browser 数据层 |
| `ED` | 编辑器 UI、视口、Command、Inspector |
| `RND` | 渲染管线、RHI、Pass、网格导入 |
| `MAT` | 材质 IR、编译器、材质编辑器 |
| `TEST` | 测试 runner、fixture、自动化 |

### 3.2 功能点（Feature）

- 格式：**`<DOMAIN>-F<nn>`**（两位数字，从 01 起）
- 例：`WF-F01` 文档规范、`TEST-F01` 统一 CLI
- **登记：** 分配 ID 前必须在 [`FEATURE_REGISTRY.md`](../FEATURE_REGISTRY.md) 新增一行（标题、Status、Design 链接）；禁止未登记占用编号。

### 3.3 实施切片（Slice）

- 格式：**`<FeatureID>-S<nn>`**
- 例：`WF-F01-S01` 建模板目录、`WF-F01-S02` 更新 README

**禁止：** 同一语义混用 Phase / M / E / P / S 多套编号。  
**历史文档：** 保留旧编号，在文首 **Legacy mapping** 一行对照到新 `F/S`（可选）。

### 3.4 Bug

- 格式：**`BUG-<DOMAIN>-<nnn>`**（三位数字）
- 文件：`docs/ai/bugs/BUG-ED-014.md` 或 `Render/Material/bugs/BUG-MAT-001.md`

### 3.5 ADR

- 格式：**`ADR-<yyyyMMdd>-<nn>`**
- 例：`ADR-20260528-01-cli-parser-library.md`

---

## 4) 每篇长文档必填页眉（防失忆五件套）

Design / Roadmap / Implementation / ADR 顶部必须包含：

```markdown
## Meta
- **ID:** ED-F03（或 Roadmap 无单 ID 时写 N/A）
- **Status:** Draft | In Progress | Review | Done | Deferred | Cancelled
- **Owner:** <name>
- **Last updated:** YYYY-MM-DD
- **Related:** [链接 1](./foo.md), [链接 2](../Platform/bar.md)

## TL;DR
（3–5 行：问题、方案一句话、当前状态）

## Scope
- **In:** …
- **Out:** …

## Reader quick start
1. 先读 …
2. 再看 …
```

Bug Record 用 Bug 模板中的 Meta 块（见 `bug-record.template.md`）。

---

## 5) 状态机

### 5.1 功能 / 设计文档

```text
Draft → Planned → In Progress → Review → Done
                    ↓
                 Blocked → Deferred → Planned（重开）
                    ↓
                Cancelled（终止）
```

| 状态 | 含义 |
|------|------|
| Draft | 讨论中，不可作为实施依据 |
| Planned | 方案已定，未开工 |
| In Progress | 正在实施 |
| Review | 待验收 / PR 审查 |
| Done | 验收通过 |
| Blocked | 被外部依赖卡住 |
| Deferred | 主动推迟 |
| Cancelled | 不再做 |

### 5.2 Blocked / Deferred / Cancelled 必填

在文档中增加 **Status note** 小节：

- **Reason** — 为何卡住或推迟
- **Impact** — 影响哪些功能/模块
- **Options considered** — 至少 2 个选项
- **Decision** — 最终选择
- **Unblock condition** — 什么条件满足可继续（Deferred/Blocked）
- **Next check date** — 下次复盘日期（YYYY-MM-DD）

禁止只写「先放一放」。

---

## 6) Bug 流程

1. **新建** `bug-record.template.md` 副本，分配 `BUG-*` ID。
2. **Severity：** S0 阻塞 / S1 高 / S2 中 / S3 低。
3. **修复后必填：** Root cause、Fix、Regression test、关联 Slice ID（若有）。
4. **Status：** Open → In Progress → Fixed → Verified（或 Won't fix / Duplicate）。

跨领域 → `docs/ai/bugs/`；单领域 → `<Domain>/bugs/`。

---

## 7) Slice 完成定义（DoD）

每完成一个 **Slice**（或等效、可独立验收的一轮工作），**文档 + 工程** 均需满足（微小 chore 可逐项标注 N/A）。

### 7.1 文档 DoD

| 动作 | 位置 |
|------|------|
| 追加一条进展 | `PROGRESS_LOG.md`（用 progress 模板） |
| 更新 Design 状态 / 风险 / 验收勾选 | 对应 `*_DESIGN.md` |
| Implementation 切片行 | 对应 `*-Snn` → **Done** |
| `FEATURE_REGISTRY` | Feature 行 Status 与整体进度一致 |
| 有架构取舍 | 新建或更新 ADR |
| 修 Bug | 更新 Bug Record → Fixed/Verified |
| 大任务结束 | 可选 `sessions/` 笔记归档 |

### 7.2 工程 DoD

| 检查 | 要求 |
|------|------|
| **验证** | 至少一种：构建通过、或写明测试/CLI 命令且已执行、或记录手动验收步骤。基础设施切片默认：`.\scripts\verify.ps1`（build + `Editor.exe test smoke`） |
| **缺陷** | 无未记录的 S0/S1 问题；已知问题写入 Bug 或 Design 风险 |
| **公共 API** | 若改 Runtime/Editor 契约：调用方已更新，或 ADR/Design 标明 intentional breaking |
| **产品快照** | 仅当方向/架构级变化时更新 `PROJECT_CONTEXT.md` |

### 7.3 Commit 与追溯

- **Commit message：** 用**具体事项**概括（见 `git-commit-mentor`）；不以 Feature/Slice ID 代替说明。
- **追溯：** ID 写在 Progress、Registry、Design Meta；commit 可选 `Refs: ED-F03-S02` 脚注。
- **准备 commit 时：** 跑 Doc DoD + 工程 DoD 自查（`git-commit-mentor`）；缺项列出，由用户决定是否仍提交。

### 7.4 工作边界（先 commit，再开下一项）

完成一个 **Slice**、一个 **Feature**，或一整批同属一个 Feature 的改动（含仅 docs/rules/skills）后：

1. 完成 §7.1–7.2（及 Progress）。
2. **默认下一步：提议「准备 commit」**（`git-commit-mentor` 草稿 + 审批），**不要**直接拉起另一个 Feature 的实现或 Pre-flight。

**例外（用户明确说才可跳过 commit 提议）：**「先不提交」「继续在同一分支做 Y」「只给建议不写代码」。

**Agent 给路线图/评估时：** 将 **「提交当前工作」** 与 **「后续 Feature（如 TEST-F01）」** 分两段写；后者标为 *after commit* 或 *next session*。

**新 Feature 的 Pre-flight 前：** 若工作区仍有上一 Feature 的未提交改动，先提醒 commit 或 `git stash`，再登记/设计新 Feature。

---

## 8) 目录与命名（与 docs-ai-layout 一致）

- 路线图 → 各域 `*_ROADMAP.md`
- 设计 → `*_DESIGN.md` 或 `*_PLAN.md`
- 实施切片 → `*_IMPLEMENTATION.md` 或带日期的 `*_ROLLOUT*.md`
- 模板与本规范 → **`docs/ai/templates/`**（只读参考，业务文档不放进 templates）

---

## 9) AI 协作约束

- 新功能：**先登记 `FEATURE_REGISTRY` → Design Spec → Implementation Plan**（可同次会话，须分文件或分节）。
- **开工门槛：** Design Status 为 `Planned` 或更高，且用户已确认方案（或 Design 内验收/Approval 已勾选）；`Draft` 不得大规模改代码。
- Agent 不得只在聊天里留决策；争议点落 ADR。
- 会话结束：默认追加 `PROGRESS_LOG`；复杂讨论补 `sessions/`。
- **Handoff（交接）：** 换 chat、长按程暂停、或用户说 handoff/交接 → 见 §9.1。
- 引用 ID 时使用完整形式（`ED-F03-S02`），避免「见上文 M2」。

### 9.1 Handoff（会话交接）

触发：用户说 **handoff / 交接 / 换会话 / 下次继续**，或 agent 判断上下文即将丢失且 Slice 未完成。

必做：

1. `docs/ai/sessions/` — `session-note.template.md`（TL;DR、决策、未决、**下一步第一条可执行动作**）。
2. `PROGRESS_LOG.md` — 一条 progress 条目。
3. 若 Slice 未完成：Implementation 中该切片 → **Blocked**（或 Deferred），填 Status note（含 Unblock condition）。
4. 回复中给出 **3–5 行交接摘要** + 下一会话建议首句（含 Feature ID 与文件路径）。

不必 handoff：单次问答、已完成 Slice 且已 Progress + commit 草稿/提交。

### 9.2 跨模块缺陷（做 A 时发现 B/C/D）

与 `engine-learning-mentor` § Defects 一致：

- **挡当前 slice：** 最小修复或 Blocked。
- **别模块、工作量大：** 先 `BUG-*`，不在 A 的 PR 里顺手大修，除非用户明确例外。
- **学习动机：** 作者通过做引擎学习；**项目仍按专业引擎标准**维护文档与核心架构。

### 9.3 Agent 角色（合作伙伴）

- 温和语气、对范围与质量严厉、技术有依据；必要时 challenge 并给出推荐方案。
- 新模块 / 新 Feature：Pre-flight（依赖、技术债、WIP）后再大规模开工。
- 详见 `.cursor/skills/engine-learning-mentor/SKILL.md` § Partner stance、Pre-flight。

### 9.4 重构（Refactor）

与 **新功能同级流程**：登记 `FEATURE_REGISTRY` → Design Spec 或 `*_REFACTOR_PLAN.md` → Implementation 切片 → Slice DoD。

- **真重构：** 明确目标结构、删除旧路径/临时 shim、分步迁移与验证；禁止默认「屎山上加补丁」（双 API、无限 wrapper、不删旧代码）。
- **开工前：** Pre-flight + 区分结构问题 vs 单点 Bug。
- **Foundation 层：** 必须先有 Design/ADR。
- 执行细则：`.cursor/skills/engine-learning-mentor/SKILL.md` § Refactoring discipline。

Design Meta 可选 **`Type: Feature | Refactor`** 便于检索。

---

## 10) 旧标记迁移（渐进）

| 旧术语 | 新术语建议 |
|--------|------------|
| Phase n（材质） | 保留在 MAT 路线图内作**主题名**；新切片用 `MAT-Fxx-Snn` |
| M0/M1（平台） | Roadmap 里程碑名可保留；Implementation 切片改用 `CORE-Fxx-Snn` |
| E0–E4（编辑器） | Roadmap 模块名可保留；新切片用 `ED-Fxx-Snn` |
| P1–P7（资产管线） | 已完成阶段作历史章节；新工作 `ASSET-Fxx-Snn` |

不批量重命名旧文件；新文档从本规范起严格执行。
