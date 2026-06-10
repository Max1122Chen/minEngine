# Active work (agent backlog)

Last updated: 2026-06-01  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

- **RND-F02** Modern RHI — **Done**（S0–S5：`RHICreate*`/`RHICmd*`、GL 实现、Pass CommandList）。设计案保留作教案与契约真源。
- **RND-F03** Legacy RHI removal — **In Progress（M3/M4）**。M1–M2 Done。**主线 M4**：按 [F03 §16](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) **重构管线编排**（权威 PSO、Pass 内 RHICmd、统一节拍）— **非** 用现代 API 套旧 GL 状态机。**M3** 后端类型绞杀配套。**本阶段停用 EnvMap/IBL 捕获**，验收不要求 IBL。
- **RND-F04** Vulkan + modern RHI completion — **Planned**（依赖 F03 **含 M3** Done）。见 [RND-F04](./Render/RND-F04_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md)。
- **分支约定：** `render` 分支继续承载 F03/F04 实现；planning/registry 可合 `master`。

### RND-F03 边界（草案）

| In | Out |
|----|-----|
| 删 Legacy `RHI` API；资源/网格/场景 RT 现代所有权 | Vulkan 代码 |
| 终局：删 Legacy API；BindingSet + GLSL 420 binding；合并 `OpenGLRHI` | 横向薄片 / 混合运行时（废止） |
| Material 编译 + 去 `Shader` Asset | 多队列、真 barrier（F04） |

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
| [RND-F04](./Render/RND-F04_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) | Vulkan + 补全 |
