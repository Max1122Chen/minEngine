# Platform — 跨模块引擎平台

与渲染/材质解耦的**长期基础设施**：启动配置、内存管理、反射扩展、脚本、Content Browser、编辑器事务等。

| 文档 | 状态 |
|------|------|
| [PLATFORM_ROADMAP.md](./PLATFORM_ROADMAP.md) | 大方向与优先级（UE 化） |
| [Startup/ENGINE_STARTUP_DESIGN.md](./Startup/ENGINE_STARTUP_DESIGN.md) | 草稿 — 配置化启动 |
| [MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md](./MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) | 草稿 — 内存管理 |

后续（占位，尚未写详细设计）：

- `Reflection/` — `MEFunction`、脚本绑定
- `Scripting/` — Lua
- `EditorCore/` — Command/Undo、子系统解耦
