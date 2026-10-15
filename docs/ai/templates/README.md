# 文档模板与协作规范

本目录是 minEngine **文档形态 + 标记 + 协同流程** 的单一来源。新文档从这里复制模板；旧文档不强制一次性迁移。

## 先读

| 文件 | 用途 |
|------|------|
| **[DOC_GOVERNANCE.md](./DOC_GOVERNANCE.md)** | ID 规则、L0–L3、readiness、状态机、Process/Quality DoD、Bug/Defer、Handoff |
| **[DESIGN_PRINCIPLES.md](./DESIGN_PRINCIPLES.md)** | 结构透镜（内聚/耦合/SOLID-shaped/YAGNI）；勿把矩阵塞进每篇 Design |
| **[WORKFLOW_PRESETS.md](./WORKFLOW_PRESETS.md)** | 协作预设目录（部署/重配 `WORKFLOW_PROFILE`） |
| **[FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md)** | 新 Feature 先登记 ID |
| 下方模板 | 复制后填占位符，删除说明性注释 |

## 模板清单

| 模板 | 何时用 | 落盘位置 |
|------|--------|----------|
| [roadmap.template.md](./roadmap.template.md) | 多模块优先级与里程碑 | `Platform/*_ROADMAP.md` 等 |
| [design-spec.template.md](./design-spec.template.md) | 单能力设计定稿（含 readiness） | `Platform/<Topic>/`、`Editor/`、`Render/`；技能 `.agents/skills/engineering-design/` |
| [implementation-plan.template.md](./implementation-plan.template.md) | 设计 → 可提交切片（含 design 追溯） | 与设计同目录或 `*_ROLLOUT*.md` |
| [adr.template.md](./adr.template.md) | 有争议的架构取舍 | 同主题目录或 `Platform/ADR/` |
| [bug-record.template.md](./bug-record.template.md) | 缺陷跟踪 | `bugs/` 或 `<Domain>/bugs/` |
| [progress-log-entry.template.md](./progress-log-entry.template.md) | 会话/任务收尾 | 追加到 `PROGRESS_LOG.md` |
| [session-note.template.md](./session-note.template.md) | 临时讨论上下文 | `sessions/` |
| [system-engineering-review.template.md](./system-engineering-review.template.md) | 子系统工程复盘（0.1.0 / 合入前） | `Platform/<Topic>/`、`Editor/`、`Render/` 下 `*_SYSTEM_REVIEW.md`；流程见 `.agents/skills/engine-system-review/` |
| [design-review.template.md](./design-review.template.md) | Design Review 报告落盘（可选） | 与 Design 同目录或 chat-only |

## 快速规则（摘要）

- **功能 ID：** `<DOMAIN>-F<nn>`（例 `ED-F03`）— 写在协作文档，**不要**写进源码注释
- **切片 ID：** `<FeatureID>-S<nn>`（例 `ED-F03-S02`）
- **Bug ID：** `BUG-<DOMAIN>-<nnn>`
- **ADR ID：** `ADR-<yyyyMMdd>-<nn>`
- **分支：** `feat/<topic>[/<subtopic>]`，禁止 ID 命名
- **新文档禁止**混用 Phase / M / E / P 作为切片编号；历史文档可保留旧标记，文首注明映射即可。

## 与 bootstrap 的关系

- 稳定快照仍写 `PROJECT_CONTEXT.md`
- 时间线仍写 `PROGRESS_LOG.md`
- 本目录只管**怎么写**、**怎么标**、**怎么协作**
