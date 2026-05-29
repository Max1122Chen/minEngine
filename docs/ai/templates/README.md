# 文档模板与协作规范

本目录是 minEngine **文档形态 + 标记 + 协同流程** 的单一来源。新文档从这里复制模板；旧文档不强制一次性迁移。

## 先读

| 文件 | 用途 |
|------|------|
| **[DOC_GOVERNANCE.md](./DOC_GOVERNANCE.md)** | ID 规则、状态机、Slice DoD、Bug/Defer、Handoff |
| **[FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md)** | 新 Feature 先登记 ID |
| 下方模板 | 复制后填占位符，删除说明性注释 |

## 模板清单

| 模板 | 何时用 | 落盘位置 |
|------|--------|----------|
| [roadmap.template.md](./roadmap.template.md) | 多模块优先级与里程碑 | `Platform/*_ROADMAP.md` 等 |
| [design-spec.template.md](./design-spec.template.md) | 单能力设计定稿 | `Platform/<Topic>/`、`Editor/`、`Render/` |
| [implementation-plan.template.md](./implementation-plan.template.md) | 设计 → 可提交切片 | 与设计同目录或 `*_ROLLOUT*.md` |
| [adr.template.md](./adr.template.md) | 有争议的架构取舍 | 同主题目录或 `Platform/ADR/` |
| [bug-record.template.md](./bug-record.template.md) | 缺陷跟踪 | `bugs/` 或 `<Domain>/bugs/` |
| [progress-log-entry.template.md](./progress-log-entry.template.md) | 会话/任务收尾 | 追加到 `PROGRESS_LOG.md` |
| [session-note.template.md](./session-note.template.md) | 临时讨论上下文 | `sessions/` |

## 快速规则（摘要）

- **功能 ID：** `<DOMAIN>-F<nn>`（例 `ED-F03`）
- **切片 ID：** `<FeatureID>-S<nn>`（例 `ED-F03-S02`）
- **Bug ID：** `BUG-<DOMAIN>-<nnn>`
- **ADR ID：** `ADR-<yyyyMMdd>-<nn>`
- **新文档禁止**混用 Phase / M / E / P 作为切片编号；历史文档可保留旧标记，文首注明映射即可。

## 与 bootstrap 的关系

- 稳定快照仍写 `PROJECT_CONTEXT.md`
- 时间线仍写 `PROGRESS_LOG.md`
- 本目录只管**怎么写**、**怎么标**、**怎么协作**
