# minEngine Project Context (for AI)

Last updated: 2026-09-10

## 1) Project Goal

minEngine is a personal C++ game engine project; the author learns **through** building it—learning is a motivation, not a license for unprofessional core engineering.

Primary objective:
- Build a clear and extensible engine architecture (professional bar on platform/render/asset foundations).
- Practice rendering pipeline design and graphics feature implementation.
- Keep the codebase understandable and incrementally improvable.
- **Expand overall engine capability and developer experience** under [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md): capabilities not opinions; mechanism over policy; minimal Core; Agent-friendly via shared APIs.

## 2) Current High-Level Architecture

Main structure observed in repository:
- Engine core under minEngine/minEngine/src/Runtime.
- Playground app under minEngine/Playground/src as testbed.
- Rendering: Manual **RenderGraph** on the main frame path; frame strategy still lives in **`RenderPipeline`** (to become **`ForwardRenderer`** under **RND-F06**). Passes under `RenderPipeline/RenderPasses/`.
- Third-party dependencies include glfw, glad, glm, imgui, assimp, spdlog.

## 3) Rendering Status (Current Understanding)

Implemented or in-progress capabilities:
- Modern RHI + MeshDrawPacket (RND-F02/F04 Done).
- Manual RenderGraph: Shadow → Scene → Post → Present (RND-F01 S0–S04 Done).
- Base / translucency / present / directional (and related) shadows.
- Next render side-track: Sort/Batch (Capability Roadmap); **RND-F06** continues without blocking experience tracks. Animation / Sprite / ScreenUI MVP already on `master`.

Known risk themes from recent work:
- Per-frame container cleanup must be explicit.
- GPU resources must be properly released to avoid leaks.
- `RenderPipeline` still mixes strategy and graph hosting (addressed by F06).
- Pass order and viewport restore are easy to break when adding new passes.

## 4) Development Facts from User

The project does not currently maintain a formal development diary in repository.

Useful history sources are:
- Git commit messages (main in-repo source).
- User personal daily notes (external source, not in this repo).

This docs/ai folder exists to convert those sources into AI-readable context snapshots.

**文档布局：** 见 `docs/ai/README.md` 与 `.cursor/rules/docs-ai-layout.mdc`（`Platform/`、`Render/Material/`、`Editor/`）。

## 5) Collaboration Conventions (for AI)

When starting a new coding task in this repo, AI should:
- Read `docs/ai/ENGINE_DESIGN_PHILOSOPHY.md` (constraints) and `docs/ai/PROJECT_CONTEXT.md`.
- Read docs/ai/PROGRESS_LOG.md for recent timeline.
- Read docs/ai/ACTIVE_WORK.md for backlog; Capability Roadmap for multi-track direction.
- If present, read latest session note in docs/ai/sessions/.
- For platform/render/editor design, use the subdirectory under docs/ai/ (see README).
- Summarize current understanding in 5-8 lines before major edits.
- Remind the maintainer when plans drift from the design philosophy.

When finishing a task, AI should:
- Append a short entry to docs/ai/PROGRESS_LOG.md.
- Create or update a session note under docs/ai/sessions/ if the task is non-trivial.
- Place new design docs in the correct docs/ai/ subtree per docs-ai-layout rule.

## 6) Current product direction (2026-09-10)

- **Stage:** M1 Animation + M2 2D + M3 UI MVP **landed on `master`**; next Primary **unset** — choose in [ACTIVE_WORK.md](./ACTIVE_WORK.md). Parallel Infra / Render / DX — see [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md).
- **Play Mode:** **CORE-F05** MVP Done (dual Scene PIE, Inspecting Context).
- **Platform Core:** Lua + script binding Done；**CORE-F19** Call-by-name **Done**；Dynamic multicast **CORE-F20** Done（Lua `Add(fn)` = **CORE-F21** Planned）；serialization usable (Binary protocol debt)；Parameter Store **CORE-F13** Done；Hierarchy **CORE-F14/F15** Done.
- **Editor:** Console MVP (**ED-F04**); Workflow **ED-F02** In Progress（S03/S05 余量）; Inspector UX **ED-F05** Done; Anim SM Canvas **ED-F06/F07** Done; Hierarchy Tree **ED-F08** Done.
- **Gameplay Framework / Networking:** Future, plugin-oriented — light Tag/Event (**GP-F01/F02**) only; do not rush full Framework into Core.
- **Active backlog:** [ACTIVE_WORK.md](./ACTIVE_WORK.md) · IDs [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md).

## 7) Material Editor (stable)

- **Editor-only** under `Editor/src/Material/` + `Material*Window`；数据真源 `Material::m_Graph`。
- **E0–E4 done:** 见 `docs/ai/Render/Material/MATERIAL_EDITOR_PLAN.md`。

## 8) Next Suggested Maintenance

Keep this file stable and high-level. Put fast-changing details into:
- docs/ai/PROGRESS_LOG.md (timeline)
- docs/ai/sessions/*.md (task-level temporary context)
- docs/ai/ACTIVE_WORK.md (current focus)
- docs/ai/Platform/* (platform design drafts)

## 9) Input System and Playground Controls

Recent input-related architecture and behavior changes:
- Added mouse wheel callback flow through WindowSystem -> GLFWWindowSystem -> InputSystem.
- Unified wheel key naming to MouseScroll in input key definitions.
- Added player vertical flight control action in Playground (up/down movement).
- Added mouse-driven camera look behavior in Playground with pitch clamp and tunable sensitivity.
- Corrected horizontal mouse look direction sign to match expected control feel.

Current practical note:
- Mouse2D value in InputSystem is currently used by Playground as a cursor-position-like stream and then converted to delta inside Playground logic.
- MouseScroll is handled as event-driven input via OnMouseScroll callback path.
