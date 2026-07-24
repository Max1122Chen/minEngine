# Feature Registry

Last updated: 2026-07-24 (RND-F01 S05 Done)  
Purpose: **single source of truth** for `<DOMAIN>-F<nn>` IDs. Avoid duplicate or conflicting Feature IDs between you and AI.

**Rules (mandatory for new work):**

1. **Register a row before** creating Design Spec or assigning `<DOMAIN>-Fnn` in docs.
2. Pick the next free number for that `DOMAIN` (see [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) §3).
3. Set Status: `Planned` → `In Progress` → `Done` | `Deferred` | `Cancelled`.
4. Link the Design (or Implementation) path in **Design** column.
5. Do not reuse IDs; deprecate by setting Status `Cancelled` and a note — do not recycle numbers.

---

## Active & recent features

| Feature ID | Title | Status | Owner | Design / plan |
|------------|-------|--------|-------|----------------|
| `CLI-F01` | Unified command-line interface | Done | — | [CLI_UNIFIED_DESIGN](./Platform/CLI/CLI_UNIFIED_DESIGN.md) |
| `TEST-F01` | Test runner, suite registry, verify integration | Done | — | [TEST_UNIFIED_DESIGN](./Platform/Test/TEST_UNIFIED_DESIGN.md) |
| `TEST-F02` | Test layout migration, doctest, minEngineTests exe | Done | — | [TEST_F02_LAYOUT_MIGRATION](./Platform/Test/TEST_F02_LAYOUT_MIGRATION.md) |
| `TEST-F03` | Suite slim-down, doctest cases, fixture B reflection | Done | — | [TEST_F03_SUITE_SLIM_PLAN](./Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md) |
| `WF-F01` | Documentation templates and collaboration governance | Done | — | [templates/](./templates/), [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) |
| `WF-F02` | 协作者文档站（MkDocs 公开手册 + GitHub Pages） | In Progress | — | [Design](./Platform/Docs/HANDBOOK_SITE_DESIGN.md) · [Impl](./Platform/Docs/HANDBOOK_SITE_IMPLEMENTATION.md) — 骨架 Done，子系统文档待补 |
| `RND-F01` | RenderGraph（Manual → Baked；图 `RenderPass`；S0–S05 Done） | **Draft** | — | [RND-F01_RENDER_GRAPH_DESIGN](./Render/RND-F01_RENDER_GRAPH_DESIGN.md) · **下一步 S06 Bake** |
| `RND-F02` | Modern RHI（GPU 工作模型抽象；GL 首适配 + Pass CommandList） | Done | — | [RND-F02_MODERN_RHI_DESIGN](./Render/RND-F02_MODERN_RHI_DESIGN.md) · S0–S5 on `render` |
| `RND-F03` | Legacy RHI removal（调用面 + 管线重构 M4 + 后端绞杀 M3） | In Progress | **M1–M2 Done**；**M4** 管线 §16；M3 §15；EnvMap 停用 | [RND-F03_LEGACY_RHI_REMOVAL_DESIGN](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · §16 |
| `RND-F04` | Modern RHI further evolution（语义终态：PipelineLayout、Packet、Setup/Execute、缓存） | **Done** | — | [RND-F04_MODERN_RHI_EVOLUTION_DESIGN](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) · S01–S04 on `render` |
| `RND-F05` | Vulkan backend + modern RHI completion（第二后端 + 契约在 VK 补全） | Planned | — | [RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · 依赖 **F03 Done** + F04 Done |
| `RND-F06` | ForwardRenderer（Renderer / RenderGraph 职责分离；删除 `RenderPipeline`） | **In Progress** | — | [RND-F06_FORWARD_RENDERER_DESIGN](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md) · **当前主线** |

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | Command-line / tools entry |
| `TEST` | F04 | Automated tests (follow-on) |
| `WF` | F03 | Workflow / docs |
| `CORE` | F01 | Reflection, object, startup |
| `ASSET` | F01 | Asset pipeline extensions |
| `ED` | F01 | Editor productization (new IDs only) |
| `RND` | F07 | F01 Draft（S05+ after F06）；F02/F04 Done；F03 In Progress；F05 Planned；F06 Planned |
| `MAT` | F01 | Material (new IDs only; legacy Phase docs keep old names) |

Update **Next Feature #** when you register a new row.
