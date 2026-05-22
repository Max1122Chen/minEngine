# minEngine Project Context (for AI)

Last updated: 2026-05-22

## 1) Project Goal

minEngine is a personal C++ game engine learning project.

Primary objective:
- Build a clear and extensible engine architecture.
- Practice rendering pipeline design and graphics feature implementation.
- Keep the codebase understandable and incrementally improvable.

## 2) Current High-Level Architecture

Main structure observed in repository:
- Engine core under minEngine/minEngine/src/Runtime.
- Playground app under minEngine/Playground/src as testbed.
- Rendering built around RenderPipeline with multiple passes.
- Third-party dependencies include glfw, glad, glm, imgui, assimp, spdlog.

## 3) Rendering Status (Current Understanding)

Implemented or in-progress capabilities:
- Base pass rendering.
- Translucency pass.
- Present pass (offscreen result to final output).
- Uniform buffer usage for per-frame camera data.
- Light UBO and per-frame light upload optimization.
- Directional light shadow casting integrated into pipeline.

Known risk themes from recent work:
- Per-frame container cleanup must be explicit.
- GPU resources must be properly released to avoid leaks.
- Per-frame proxy/entry generation should avoid repeated allocation without reuse.
- Pass order and viewport restore are easy to break when adding new passes.

## 4) Development Facts from User

The project does not currently maintain a formal development diary in repository.

Useful history sources are:
- Git commit messages (main in-repo source).
- User personal daily notes (external source, not in this repo).

This docs/ai folder exists to convert those sources into AI-readable context snapshots.

## 5) Collaboration Conventions (for AI)

When starting a new coding task in this repo, AI should:
- Read docs/ai/PROJECT_CONTEXT.md first.
- Read docs/ai/PROGRESS_LOG.md for recent timeline.
- If present, read latest session note in docs/ai/sessions/.
- Summarize current understanding in 5-8 lines before major edits.

When finishing a task, AI should:
- Append a short entry to docs/ai/PROGRESS_LOG.md.
- Create or update a session note under docs/ai/sessions/ if the task is non-trivial.

## 6) Material Editor (current)

- **Editor-only** module under `Editor/src/Material/` + `Material*Window`；数据真源 `Material::m_Graph`。
- **E2 done:** `MaterialGraphWindow` + imgui-node-editor；`MaterialEditor` 命令中枢；Preview 由 `MaterialEditorPreview` + `MaterialPreviewViewportClient` 分工。
- **Reflection helper added:** `GetDerivedClasses<TBase>()` for future node palette / registry.
- **Plan / log:** `docs/ai/MATERIAL_EDITOR_PLAN.md`, `docs/ai/PROGRESS_LOG.md` (Material Editor E0–E2 section).

## 7) Next Suggested Maintenance

Keep this file stable and high-level. Put fast-changing details into:
- docs/ai/PROGRESS_LOG.md (timeline)
- docs/ai/sessions/*.md (task-level temporary context)

## 7) Input System and Playground Controls (Today)

Recent input-related architecture and behavior changes:
- Added mouse wheel callback flow through WindowSystem -> GLFWWindowSystem -> InputSystem.
- Unified wheel key naming to MouseScroll in input key definitions.
- Added player vertical flight control action in Playground (up/down movement).
- Added mouse-driven camera look behavior in Playground with pitch clamp and tunable sensitivity.
- Corrected horizontal mouse look direction sign to match expected control feel.

Current practical note:
- Mouse2D value in InputSystem is currently used by Playground as a cursor-position-like stream and then converted to delta inside Playground logic.
- MouseScroll is handled as event-driven input via OnMouseScroll callback path.
