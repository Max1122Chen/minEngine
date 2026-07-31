# Platform — 跨模块引擎平台

与渲染/材质解耦的**长期基础设施**：启动配置、内存管理、反射扩展、脚本、Content Browser、编辑器事务等。

| 文档 | 状态 |
|------|------|
| [PLATFORM_ROADMAP.md](./PLATFORM_ROADMAP.md) | 大方向与优先级（UE 化） |
| [INFRASTRUCTURE_ROADMAP.md](./INFRASTRUCTURE_ROADMAP.md) | CLI · Test · Verify 近程路线 |
| [CLI/CLI_UNIFIED_DESIGN.md](./CLI/CLI_UNIFIED_DESIGN.md) | Done — 统一 CLI |
| [Test/TEST_UNIFIED_DESIGN.md](./Test/TEST_UNIFIED_DESIGN.md) | Done — TestRunner + registry |
| [Test/TEST_F02_LAYOUT_MIGRATION.md](./Test/TEST_F02_LAYOUT_MIGRATION.md) | Done — doctest + Tests/ + minEngineTests |
| [Docs/HANDBOOK_SITE_DESIGN.md](./Docs/HANDBOOK_SITE_DESIGN.md) | Planned — `WF-F02` 设计 |
| [Docs/HANDBOOK_SITE_IMPLEMENTATION.md](./Docs/HANDBOOK_SITE_IMPLEMENTATION.md) | Planned — `WF-F02` 实施（首期骨架） |
| [Startup/ENGINE_STARTUP_DESIGN.md](./Startup/ENGINE_STARTUP_DESIGN.md) | 草稿 — 配置化启动 |
| [MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md](./MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) | 草稿 — 内存管理 |

| [Reflection/REFLECTION_FUNCTIONS_DESIGN.md](./Reflection/REFLECTION_FUNCTIONS_DESIGN.md) | 设计 — P4 函数反射（阶段切片） |
| [Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md](./Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md) | 现状 — 当前反射系统基线 |
| [Reflection/UE_FUNCTION_REFLECTION_NOTES.md](./Reflection/UE_FUNCTION_REFLECTION_NOTES.md) | 学习笔记 — UE 方法反射做法（阶段 1） |
| [Scripting/LUA_SCRIPTING_DESIGN.md](./Scripting/LUA_SCRIPTING_DESIGN.md) | Draft — `CORE-F01` Lua（分支 `luaScript`） |
| [Reflection/REFLECTION_ENUM_PROPERTY_PLAN.md](./Reflection/REFLECTION_ENUM_PROPERTY_PLAN.md) | Done — Enum Size/绑定 |

其他：

- `EditorCore/` — Command/Undo（见 `docs/ai/Editor/EDITOR_COMMAND_HISTORY.md`）
