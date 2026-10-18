# minEngine Project Context (for AI)

Last updated: 2026-10-16

## 1) Project Goal

minEngine is a personal C++ game engine project; the author learns **through** building it—learning is a motivation, not a license for unprofessional core engineering.

Primary objective:
- Build a clear and extensible engine architecture (professional bar on platform/render/asset foundations).
- Practice rendering pipeline design and graphics feature implementation.
- Keep the codebase understandable and incrementally improvable.
- **Expand overall engine capability and developer experience** under [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md): capabilities not opinions; mechanism over policy; minimal Core; Agent-friendly via shared APIs.

## 2) Current High-Level Architecture

Main structure observed in repository:
- Engine core under `minEngine/minEngine/src/Runtime`. Editor under `Editor/`.
- Product hosts own Editor + Game executables and link a game SHARED library (`GameModuleEntry`). In-tree sample: `SampleGame` / `SampleEditor`. `Playground/` remains in tree; it is not the product path.
- Frame path: manual **RenderGraph**; strategy host is **`ForwardRenderer`** (`RND-F06` Done). Passes under the render pipeline.
- Execution today is **single-threaded and synchronous**. Mutexes exist around log, profiling names, native tags, and editor filesystem mutations. Physics uses Jolt's single-threaded job system. There is no engine TaskSystem and no logic/render thread split.
- Third-party dependencies include glfw, glad, glm, imgui, assimp, spdlog, Jolt, efsw. TaskFlow is **not** vendored yet.

## 3) Rendering Status (Current Understanding)

Implemented or in-progress capabilities:
- Modern RHI + MeshDrawPacket; OpenGL and Vulkan backends. RHI downcasts in the backend are `static_cast` (`CORE-F37` S02).
- Manual RenderGraph: Shadow → Scene → Post → Present. `ForwardRenderer` hosts the frame (`RND-F06` Done).
- CPU mesh visibility, cull, and sort (`RND-F18`–`F20` Done). CSM / light frustum (`RND-F21`) stays Deferred.
- Material graph product path including `.memi` (`MAT-F01`–`F07` Done). Vertex Color and Custom0/1 consumption (`RND-F24` S01–S03 Done; S04 factory sugar optional).
- Environment map asset exists. **Skylight / Atmosphere / height fog are not scheduled** — research only, until a new Feature is registered.
- Animation, Sprite, and ScreenUI MVP are on `master`.

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

## 6) Current product direction (2026-10-16)

- **Stage:** M1 Animation + M2 2D + M3 UI MVP are on `master`. A minCraft-driven catch-up wave (lifecycle, FixedTick, GI hub, product build, mesh vertex semantics, RTTI hygiene) has also landed. **Primary is unset.**
- **Open leftovers (do not treat as the next theme):** `CORE-F33` personal review, `ASSET-F04` human acceptance, optional `RND-F24` S04. `CORE-F37` code for this campaign is closed; remaining clusters are deferred.
- **Next multithread candidate:** `CORE-F38` TaskSystem ([Design Draft](./Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md)) — TaskFlow wrapper + game-thread Drain, proved only by **synthetic CPU jobs** (1 vs N wall-clock). Real async asset load (`ASSET-F09`) is **Deferred**. Logic/render split, Atmosphere, Plugin/Package remain later goals. See [ACTIVE_WORK.md](./ACTIVE_WORK.md).
- **Play Mode:** **CORE-F05** MVP Done (dual Scene PIE, Inspecting Context).
- **Platform Core:** Lua + script binding Done; call-by-name and Dynamic multicast subscribe Done (`CORE-F19`–`F21`). GameInstance + subsystems + project GI class Done (`CORE-F34`/`F35`). FixedTick Done (`CORE-F36`). Serialization usable (binary protocol still debt). Game module SHARED is force-loaded via `GameModuleEntry` (`BUILD-F01`).
- **Editor:** Console MVP; workflow leftovers `ED-F02` S03/S05; tab host mostly landed (`ED-F11`).
- **Gameplay Framework / Networking:** still plugin-oriented. Tag/Event (`GP-F01`/`F02`) only. Do not put gameplay business types into Core.
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
