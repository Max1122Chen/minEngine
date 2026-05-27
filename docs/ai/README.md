# minEngine AI 文档索引

本目录供 AI 与开发者共享**设计案、路线图、会话记录**。请遵循 [文档布局约定](#布局约定) 放置新文件。

## 布局约定

| 路径 | 用途 |
|------|------|
| **`PROJECT_CONTEXT.md`** | 稳定、高层项目快照（bootstrap 必读） |
| **`PROGRESS_LOG.md`** | 按时间线的变更记录（bootstrap 必读） |
| **`WORKING_WITH_AI.md`** | 与 AI 协作约定 |
| **`Platform/`** | 跨模块平台能力：启动、内存管理、反射/脚本路线图等 |
| **`Render/`** | 渲染管线、资源导入、非材质专项 |
| **`Render/Material/`** | 材质 IR、编译器、编辑器、Phase 1–5 |
| **`Editor/`** | 编辑器 UI、视口、ImGui 等 |
| **`bugs/`** | 跨领域缺陷记录（领域专项 bug 可放在对应子目录，如 `Render/Material/bugs/`） |
| **`sessions/`** | 单次会话笔记（临时，可归档） |

**规则（仓库约束）：** 见 `.cursor/rules/docs-ai-layout.mdc`。

## 快速入口

### 平台（当前主线）

- [Platform 路线图](./Platform/PLATFORM_ROADMAP.md) — UE 化大方向与优先级
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
- [**Editor 平台化规划**](./Editor/EDITOR_PLATFORM_PLAN.md) — E0–E4（P2 主线）
- [**E2 Previewer / Editor 视口**](./Editor/PREVIEWER_DESIGN.md)
- [Command Stack / Undo](./Editor/EDITOR_COMMAND_HISTORY.md)
- [视口窗口](./Editor/EDITOR_VIEWPORT_WINDOWS.md)
- [Content Browser 产品意图](./Platform/ContentBrowser/CONTENT_BROWSER_DESIGN.md)
- [材质编辑器计划](./Render/Material/MATERIAL_EDITOR_PLAN.md)

## 路径迁移（2026-05-23）

原 `docs/ai/*.md` 根目录下的 Material / Render 文档已迁入上表对应子目录。旧链接请改相对路径；会话笔记中的历史路径可保留或按需更新。
