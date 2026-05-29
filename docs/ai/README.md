# minEngine AI 文档索引

本目录供 AI 与开发者共享**设计案、路线图、会话记录**。请遵循 [文档布局约定](#布局约定) 放置新文件。

## Agent：读哪些文档（必读）

**排期与「还有什么没做」** — 只认：

1. [ACTIVE_WORK.md](./ACTIVE_WORK.md) — 当前短 backlog（人维护）
2. [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) — 仅 **In Progress** / **Planned** 行
3. [TECH_DEBT.md](./TECH_DEBT.md) — 仅 **Open** 行
4. [PROGRESS_LOG.md](./PROGRESS_LOG.md) — 近期已落地事实
5. **代码与测试** — `verify.ps1`、`minEngineTests`；与文档冲突时以代码为准

规则全文：`.cursor/rules/docs-trust-tiers.mdc`（always apply）。  
**不要**根据下方「快速入口」里的旧路线图、未勾选清单或 `Status: Snapshot/Archived/Reference` 文档自动推导待办。

### Planning sources（可驱动工作）

| 文件 | 用途 |
|------|------|
| [ACTIVE_WORK.md](./ACTIVE_WORK.md) | 你现在关心的 1–5 件事 |
| [PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md) | 稳定架构快照 |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | 命令、DoD、协作习惯 |
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | 新功能 ID 与进行中登记 |
| [TECH_DEBT.md](./TECH_DEBT.md) | 明确推迟的问题 |

### Reference only（背景；勿当 backlog）

- `Platform/PLATFORM_ROADMAP.md`、`Editor/EDITOR_PLATFORM_PLAN.md` 等 — 历史排期与架构意图
- `*_CURRENT_STATE.md` — 时间点快照（如方法反射之前的反射说明）
- `Status: Done` 的 `*_ROADMAP.md` / `*_PLAN.md` / 已 **Archived** 的 issue 记录
- [sessions/](./sessions/) — 会话笔记

---

## 布局约定

| 路径 | 用途 |
|------|------|
| **`PROJECT_CONTEXT.md`** | 稳定、高层项目快照（bootstrap 必读） |
| **`PROGRESS_LOG.md`** | 按时间线的变更记录（bootstrap 必读） |
| **`FEATURE_REGISTRY.md`** | Feature ID 登记册（新功能先登记再写 Design） |
| **`ACTIVE_WORK.md`** | 当前短 backlog（agent 排期首选；人维护） |
| **`BOOTSTRAP_DIGEST.md`** | 一页会话恢复：规则摘要、命令、DoD（bootstrap 必读） |
| **`TECH_DEBT.md`** | 技术债登记册（Pre-flight / 路线图引用） |
| **`WORKING_WITH_AI.md`** | 与 AI 协作约定 |
| **`templates/`** | 文档模板 + [协作规范](./templates/DOC_GOVERNANCE.md)（新文档从此复制） |
| **`Platform/`** | 跨模块平台能力：启动、内存管理、反射/脚本路线图等 |
| **`Render/`** | 渲染管线、资源导入、非材质专项 |
| **`Render/Material/`** | 材质 IR、编译器、编辑器、Phase 1–5 |
| **`Editor/`** | 编辑器 UI、视口、ImGui 等 |
| **`bugs/`** | 跨领域缺陷记录（领域专项 bug 可放在对应子目录，如 `Render/Material/bugs/`） |
| **`sessions/`** | 单次会话笔记（临时，可归档） |
| **`../external/`** | 外部 AI / UE 参考讨论存档 | 定稿以 `docs/ai/` 为准 |

**规则（仓库约束）：** 见 `.cursor/rules/docs-ai-layout.mdc`。

**新功能：** 先在 [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) 登记 ID，再读 [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) 并选 [templates/](./templates/)。切片 `<FeatureID>-S<nn>`。

## 快速入口（参考索引 — 非自动待办）

### 平台

- [基建路线图](./Platform/INFRASTRUCTURE_ROADMAP.md) — **Done**；CLI/Test/verify 已落地，维护用
- [CLI 统一化设计（CLI-F01）](./Platform/CLI/CLI_UNIFIED_DESIGN.md) — 子命令=模式，`--`=参数
- [Platform 路线图](./Platform/PLATFORM_ROADMAP.md) — **Reference**；UE 化大方向与历史排期
- [函数反射设计](./Platform/Reflection/REFLECTION_FUNCTIONS_DESIGN.md) — Invoke、阶段切片（实施时以代码为准）
- [函数反射现状](./Platform/Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md) — **Snapshot**（方法反射之前）；勿当现状
- [委托系统（占位）](./Platform/Reflection/REFLECTION_DELEGATES_DESIGN.md)
- [Lua 脚本（占位）](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md)
- [引擎启动 / 配置](./Platform/Startup/ENGINE_STARTUP_DESIGN.md)
- [内存管理](./Platform/MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md)
- [Content Browser](./Platform/ContentBrowser/CONTENT_BROWSER_DESIGN.md)
- [序列化扩展（Binary Archive / Property API）](./Platform/Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md)

### 渲染 / 材质（已基本完成）

- [渲染重构](./Render/RENDER_REFACTOR_PLAN.md)
- [资源管线 R2](./Render/RESOURCE_PIPELINE_PLAN.md)
- [材质系统路线图](./Render/Material/MATERIAL_SYSTEM_ROADMAP.md)

### 编辑器

- [**Editor Shell 设计**](./Editor/EDITOR_SHELL_DESIGN.md)
- [**Editor 架构复盘**](./Editor/EDITOR_ARCHITECTURE_REVIEW.md)
- [Editor 平台化规划](./Editor/EDITOR_PLATFORM_PLAN.md) — **Reference**；E0–E4 等历史板块状态
- [**E2 Previewer / Editor 视口**](./Editor/PREVIEWER_DESIGN.md)
- [**Editor 上下文菜单系统**](./Editor/EDITOR_CONTEXT_MENU_DESIGN.md)（Context + Registry + Command）
- [Command Stack / Undo](./Editor/EDITOR_COMMAND_HISTORY.md)
- [视口窗口](./Editor/EDITOR_VIEWPORT_WINDOWS.md)
- [Content Browser 产品意图](./Platform/ContentBrowser/CONTENT_BROWSER_DESIGN.md)
- [材质编辑器计划](./Render/Material/MATERIAL_EDITOR_PLAN.md)

## 路径迁移（2026-05-23）

原 `docs/ai/*.md` 根目录下的 Material / Render 文档已迁入上表对应子目录。旧链接请改相对路径；会话笔记中的历史路径可保留或按需更新。
