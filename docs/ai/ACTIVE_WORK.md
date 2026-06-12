# Active work (agent backlog)

Last updated: 2026-06-11  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

- **RND-F02** Modern RHI — **Done**（S0–S5：`RHICreate*`/`RHICmd*`、GL 实现、Pass CommandList）。设计案保留作教案与契约真源。
- **RND-F03** Legacy RHI removal — **In Progress（M3/M4 尾）**。M1–M2 Done。M4 P0–P3 Done。**F03 Done** = Legacy 清零 + M3；**现代语义终态** 见 F04。
- **RND-F04** Modern RHI further evolution — **Done**（S01–S04）。PipelineLayout、MeshDrawPacket、PSO/SRV 缓存、`setIndex`、`RHICmdTransition`（GL no-op）、删 Legacy Submit API。小尾巴见 [TECH_DEBT](./TECH_DEBT.md) TD-013–TD-014。见 [F04](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md)。
- **RND-F05** Vulkan + modern RHI completion — **Planned**（依赖 **F03 Done** + F04 Done）。见 [RND-F05](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md)。
- **RND-F01** RenderGraph — **Draft**（图 **`RenderPass`**；逻辑纹理 **字符串名**；`RenderPasses/*` ≈ `RenderPassInterface`）。见 [F01](./Render/RND-F01_RENDER_GRAPH_DESIGN.md)。**S0 + S01 Done**；**下一步：S02 Post 链样板**。
- **下一步主线：** F01-**S02** 或 F03-M3 尾；F03 Done / F05 仍按原依赖。
- **分支约定：** `render` 分支继续承载 F03/F04/F05 实现；planning/registry 可合 `master`。

### RND-F03 边界（草案）

| In | Out |
|----|-----|
| 删 Legacy `RHI` API；资源/网格/场景 RT 现代所有权 | Vulkan（F05） |
| 终局：删 Legacy API；BindingSet + GLSL 420 binding；合并 `OpenGLRHI` | F04 语义终态（见 F04） |
| Material 编译 + 去 `Shader` Asset | 多队列、真 barrier（F05） |

**验证：** `verify.ps1` + `material-ir` + **黄金场景目视**（dir/point/spot + intensity=1 + 阴影）；见 F03 §8.4。

---

## Maintenance (not blocking render track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补，不与 RND 抢 `render` 分支。

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |
| Material | `minEngineTests.exe test material-ir` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## Explicitly not backlog (unless you promote them)

- **RND-F01** RenderGraph（Deferred）。
- Editor: unified Inspector target model, Material graph Undo, texture preview in Inspector.
- Core: delegates, Lua scripting bindings.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [RND-F02](./Render/RND-F02_MODERN_RHI_DESIGN.md) | Modern RHI 教案 + 契约（Done） |
| [RND-F03](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) | Legacy 清零 |
| [RND-F04](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) | 现代 RHI 语义终态 |
| [RND-F05](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) | Vulkan + 补全 |
