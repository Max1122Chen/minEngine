# minEngine Progress Log (for AI)

Last updated: 2026-09-02 (ED-F03 Console C-minimal)

### 2026-09-02 - ED-F03: Console Command Tab (C-minimal, 目视验收延后)
- **Editor:** `ConsoleWindow` Output | Command Tab；`CommandConsolePresenter` / `CommandConsoleStyle`；分色输出、↑↓ 历史、Tab 命令名前缀补全。
- **Runtime:** `CommandHistory`；Editor `list_go` 命令。
- **Note:** `CompletionService` / `find` / `editor.undo` / ExportSchema 未做；智能补全验收留待 S03–S04。
- **Verified:** `Editor.exe` 编译；ImGui Command Tab 崩溃已修；`command-system` 测试仍 PASS。

### 2026-09-02 - ED-F03: command system Runtime S00–S02 (验收 B)
- **Runtime:** `CommandRegistry` / `CommandExecutor` / `CommandResult`；Builtin `help` / `get` / `set` / `inspect`。
- **PropertyPath:** GameObject 名解析 + Component 属性回退；`set` primitive 校验；复用 Serializer path。
- **Tests:** `minEngineTests.exe test command-system` — 6 cases, 23 assertions PASSED。
- **Verified:** `verify.ps1`。

### 2026-09-02 - CORE-F07: reflection display names (验收 A Done)
- **Runtime:** `ReflectionDisplayNames` — strip `m_`/`x_`/`b_` prefix + camelCase word breaks; `GetPropertyDisplayName`.
- **Editor:** `PropertyEditPolicy::GetDisplayName` delegates to runtime API.
- **Tests:** `minEngineTests.exe test reflection-display-names` — 2 cases, 15 assertions PASSED.
- **Verified:** `verify.ps1`; Editor 目视确认 Inspector 展示名。

### 2026-09-01 - BUG-RENDER-014: point light radius, attenuation, shadow cutoff (pending commit)
- **Shaders:** `PointLightAttenuation` / `PointLightShadowFactor`; Phong 移除 per-point-light ambient；PBR 点光衰减 + 阴影 mask。
- **Scene:** `test.mescene` 补点光/聚光衰减字段。
- **Docs:** Design `BUG-RENDER-014_POINT_LIGHT_RADIUS_ATTENUATION_DESIGN.md`; Bug record updated。
- **Verified:** `shader-compiler`, `physics-shapes` / `physics-smoke` / `physics-sync`；Editor 目视待确认。

### 2026-09-01 - Merge `feat/launcher` → `master`（LAUN-F01）
- **Merged:** `Launcher/` Rust workspace — CLI (`minlauncher`) + Tauri GUI (`minlauncher-app`).
- **Verified (branch):** `cargo test -p minlauncher-core` 5/5; manual `cargo tauri dev`.

### 2026-09-01 - Merge `feat/audio` → `master`（AUD-F01 MVP）
- **Merged:** `IAudioBackend` + miniaudio、`AudioSystem`/`AudioComponent`/`AudioListenerComponent`、`AudioClip` asset、3D spatial sync。
- **SceneComponent:** world matrix + attach/detach `KeepWorldTransform`（以 audio 分支为准）。
- **Tests:** `audio-smoke` + existing `shader-compiler` suites both registered.

### 2026-09-01 - Merge `feat/debug-drawing` → `master`（含 `feat/render` 全量）
- **Merged:** ED-F01 S01–S07、RND-F05/F12–F14、RND-F11 DebugDrawing MVP、VK shadow playbook 等 27 commits。
- **Docs:** 合并 ACTIVE_WORK / REGISTRY / PROGRESS_LOG；保留多轨 worktree 登记。

### 2026-09-01 - AUD-F01 Audio System MVP Done (`feat/audio`)
- **Runtime:** `IAudioBackend` + `MiniaudioBackend`, `AudioClip`/`AudioVoice`/`AudioSystem`, `AudioComponent`/`AudioListenerComponent`, Bus mixer, 2D/3D spatial audio, lifecycle on scene unload.
- **Fixes (post-initial MVP):** `SceneComponent` world transform + `AttachToComponent`/`DetachFromParent`; 3D sync via `GetWorldPosition()`; disable miniaudio default listener; listener enable/sync + tick order; attenuation model wired to backend.
- **Tests:** `minEngineTests.exe test audio-smoke` PASSED.
- **Manual:** Editor ear-test on `test.mescene` — spatialized audio audible with listener on camera.
- **Deferred (AUD-F02+):** gentler default attenuation / `AttenuationModel` in Inspector, custom curves, Inspector live volume/pitch.

### 2026-08-31 - LAUN-F01-S05: Tauri 2 + React GUI (`feat/launcher`)
- Added `crates/minlauncher-app` (Tauri 2) + `ui/` (React + Vite + TS, industrial dark theme).
- Tauri commands wrap `minlauncher-core`; shared `settings.json` with CLI.
- Core: `templates` module, `EditorStatus`, `clear_all_recent`.
- **Verified:** `cargo test -p minlauncher-core` 5/5; `cargo build -p minlauncher-app`.
- **Manual:** `cargo tauri dev` → Projects / Settings / New Project.

### 2026-08-31 - LAUN-F01 CLI: Rust minlauncher S01–S04 (`feat/launcher`)
- Added `Launcher/` Cargo workspace: `minlauncher-core` + `minlauncher` CLI (clap).
- Commands: `open`, `create`, `recent`, `config`; spawn `Editor.exe --project <path>`.
- Empty template under `Launcher/Templates/Empty/`; settings at `%APPDATA%/minEngine/Launcher/`.
- Design + Implementation plan finalized (Tauri 2 for S05); Registry **In Progress** → CLI slice **Done**.
- **Verified:** `cargo test -p minlauncher-core` (5/5); manual `open` MyMEProject + `create` LaunSmokeTest.

### 2026-08-31 - Multi-track backlog: LAUN / AUD / ANIM / UI / PHYS thaw (`master`)
- Registered **LAUN-F01**, **AUD-F01**, **UI-F01**, **ANIM-F01**, **PHYS-F04**; **PHYS-F03** Deferred → Planned.
- Branches from `master`: `feat/launcher`, `feat/audio`, `feat/ui-anim` (`feat/physics` 已存在).
- Worktrees: `D:/Dev/GitRepo/minEngine-launcher`, `minEngine-audio`; `MyMEProject` ProjectRoot per worktree.
- **Next:** merge feature branches after render track lands.


### 2026-09-01 - RND-F11 MVP 收尾：范围收窄 + wireframe z-fighting (`feat/debug-drawing`)
- **Scope:** MVP 定为 S01–S02 only；S03 contact/trace、S04 toggle **移出**本 Feature（Design/Impl/Registry 已更新，Status → Done）。
- **Code:** 回退 S03 实验；`PhysicsDebugDraw` 仅 collider + `SubmitScene(scene, options)`；`DebugDraw.vert` 保留 3mm view bias。
- **Principle:** Debug 为底层服务；不编排 Physics/Editor 如何调用；Persistent + toggle → 后续新 Feature。
- **Next:** commit + merge `feat/debug-drawing`；新 Feature 讨论 Persistent / toggle。

### 2026-09-01 - RND-F11-S02: Physics collider wireframe (`feat/debug-drawing`, commit 8c3ac07)
- **Delivered:** `PhysicsDebugDraw::SubmitScene`; `DebugDraw::Sphere` / `Capsule`; sphere/capsule wireframe tessellation; Editor submits colliders before `SubmitSceneDraw` (S01 axis smoke removed).
- **Verified:** User visual acceptance (wireframe visible); `physics-smoke` / `physics-shapes` / `render-graph` pass.
- **Known issue:** [BUG-PHYS-003](./bugs/BUG-PHYS-003.md) — intermittent crash on Add `BoxColliderComponent` (not reproduced after retry).
- **Next:** S03 contact + LineTrace visualization.

### 2026-09-01 - RND-F11-S01: DebugDraw pass + editor axis smoke (`feat/debug-drawing`)
- **Delivered:** `DebugDrawService` / `DebugDraw::` API; `DebugDrawPass` + shaders; `Scene.Debug` RDG slot; `EnableDebugDraw` flag.
- **Editor:** `SceneEditingViewportClient` submits RGB axis lines before `SubmitSceneDraw` (smoke until S02).
- **Fix:** OpenGL `RHICmdDraw` honors PSO `LineList` (was hardcoded `GL_TRIANGLES`).
- **Docs:** Full design spec + implementation plan; registry/active work updated.
- **Verified:** `cmake --build` minEngine+Editor; Editor GL+VK axis visual acceptance; `test render-graph` pass.
- **Next:** S02 `PhysicsDebugDraw` collider wireframe.

### 2026-08-31 - RND-F11 DebugDrawing branch kickoff (`feat/debug-drawing`)
- **Branch:** `feat/debug-drawing` from `feat/render` after shadow quality handoff commit.
- **Registry:** `RND-F11` → **In Progress**; placeholder [Design](./Render/RND-F11_DEBUG_DRAWING_DESIGN.md).
- **Goal:** Editor viewport debug primitives for Physics collider/contact/trace visualization.
- **Parallel:** VK shadow quality remains on `feat/render` (RenderDoc / cull-winding).
- **Next agent:** Expand Design → Implementation Plan → S01 lines+boxes MVP.

### 2026-08-31 - VK shadow self-shadow handoff (`feat/render`)
- **Symptom refined:** Dir **and** Spot show receiver self-shadow / false shadows on VK; **Point** not observed; **different objects** per light type → winding/orientation, not global bias off.
- **E3 recap:** `MAX_CASCADES=1` + force cascade 0 — unchanged → CSM **index** ruled out; camera coupling persists (dir matrix still camera-frustum-derived).
- **Depth bias audit (read-only):** Two layers — (A) raster via `RHIClipSpaceCapabilities` → ShadowPass PSO (`glPolygonOffset` / VK PSO `depthBiasEnable`); (B) shader receiver bias in `MaterialSceneShadows.glslinc`. VK raster bias **should be active** for Dir/Spot; Point bypasses via `gl_FragDepth`. Raising VK slope/constant inconclusive → prioritize write-path cull/winding.
- **Docs:** `sessions/2026-08-31-vk-shadow-self-shadow-handoff.md`; playbook `VK_SHADOW_DEBUGGING.md` §4.5–4.6, §7; `ACTIVE_WORK.md` updated.
- **Next agent:** Debug 5/6; RenderDoc; scheme B or frontFace A/B; restore TEMP limits/shader defines before production fix.

### 2026-08-31 - VK dir self-shadow isolation: single cascade (`feat/render`)
- **Experiment:** `MAX_CASCADES=1` + `DIR_SHADOW_FORCE_CASCADE=0` + Front cull restored (Back reverted); P1 (`gl_FragDepth` omit Dir/Spot) still in tree.
- **User result:** Symptoms **unchanged** vs multi-cascade — ground self-shadow acne; cube/sphere false shadows still **camera-coupled**; PCF soft edges visible.
- **Conclusion:** **Not** multi-cascade index / cascade-boundary mixing (rules out P5 as primary). Issue is **directional-light path** specific (Spot/Point not implicated in this round).
- **Interpretation:** Single-cascade CSM still builds ortho frustum from **camera view frustum** + texel snap — camera coupling can persist without cascade *selection*. Combined with prior「关 Cast Shadow → map 消失」→ receiver still **writes** into dir shadow map on VK (cull/winding class), not read-only PCF artifact.
- **Next (analysis / no code yet):** Debug 5/6 binary; RenderDoc face/winding; scheme B (shadow viewport flip + `GetEffectiveCullMode`) or `VK_FRONT_FACE_CLOCKWISE` A/B. Restore `MAX_CASCADES=4` / `DIR_SHADOW_FORCE_CASCADE=-1` before production fix lands.
- **Doc:** `playbooks/Render/VK_SHADOW_DEBUGGING.md` §4.4.

### 2026-08-31 - Playbooks + VK shadow cull audit (`feat/render`)
- Added `docs/ai/playbooks/` (README + `Render/VK_SHADOW_DEBUGGING.md`) for reusable bug patterns.
- Face cull audit: ShadowPass **already** sets Front cull on VK (`RHIClipSpaceCapabilities` → `VK_CULL_MODE_FRONT_BIT`). Next round: RenderDoc PSO verify, frontFace/winding A/B, VK depth bias constant (0 vs GL 4).

### 2026-08-31 - RND-F14 Phase A: ShadowPass UBO lifetime fix (`feat/render`)
- **Root cause:** ShadowPass overwrote shared host-visible ViewProj/Params UBO at offset 0 per draw; Vulkan deferred execution → all shadow draws read last-written matrix.
- **Fix:** `ShadowUniformBuffers` — Dir/Spot fixed slots, Point ViewProj ring, Params ring; per-command `BufferOffset` in `ShadowDrawCommand`; removed single-mat4 path.
- **Diagnostic:** `ManualRenderer` (`--renderer manual`) helped isolate non-RDG root cause (RND-F13 Done).
- **Verified:** User VK full-map — Dir / Spot / Point shadows each work independently (2026-08-31).
- **Closed:** BUG-RENDER-013, BUG-RENDER-010, BUG-RENDER-011; TD-025 Done.
- **Next:** VK receiver self-shadow acne — verify ShadowPass Front face cull (user observation).

### 2026-08-31 - BUG-RENDER-013 reframed: Manual == RDG wrong shadows (`feat/render`)
- Full-map VK: ManualRenderer shows **same** wrong shadows as Forward+RDG → **RDG not primary**.
- Work shifts to ManualRenderer diagnosis: shadow PSO/attachments, VK depth array/cube create/update, set1/UBO.
- RND-F12 demoted to hygiene; fix on Manual first, then regress Forward.

### 2026-08-30 - RND-F13: dir-only isolation + full map restore (`feat/render`)
- User confirmed ManualRenderer; viewport fix (manual sky clear pass).
- Dir-only + single cascade: Manual ≈ Forward, faint shadow → not RDG-only in isolation (BUG-013 note).
- Restored: `MAX_*_SHADOW_MAPS=2`, `MAX_CASCADES=4`, `DIR_SHADOW_FORCE_CASCADE=-1` (C++ + shader fallbacks).
- **Next:** VK `--renderer forward` vs `manual` on full map / `test` scene.

### 2026-08-30 - RND-F13-S01: ManualRenderer (`feat/render`)
- Renamed from HandPassProbe → **ManualRenderer** per maintainer.
- `ManualRenderer` subclasses `ForwardRenderer`; manual Shadow→Base→Present; no RenderGraph.
- CLI: `--renderer manual` (`handpass` alias); default Forward unchanged.
- Build: minEngine + Editor OK. **Next:** S02 GL/VK parity run on `test` scene → BUG-013 note.

### 2026-08-30 - RND-F13: design draft (`feat/render`)
- Diagnostic renderer proposal: manual Shadow→Base→Present, no RDG; GL/VK parity to isolate BUG-013.
- BUG-RENDER-010: user confirmed single-cascade → one shadow (multi-copy = CSM).
- Pending: design approval → Implementation Plan.

### 2026-08-30 - RND-F12-S06 + isolation experiment committed (`feat/render`)
- `3fed4ef`: set1 physical lifecycle; dir-only + single-cascade experiment toggles; set1 OOB fix when MAX maps=0.
- `ShadowResourceHandle::RdgPhysicalIndex`; `BindGraphShadowTextures` clears stale texture refs then binds physical.
- `EngineSceneBindingSets`: dirty on ptr + physical index + texture desc; invalidate when `SetupAttachments` recreates.
- User re-tested BUG-013: still open after S01–S03; S06 pending VK verify.

### 2026-08-30 - RND-F12-S03: pass input barriers (`feat/render`)
- `RenderGraph::InsertPassInputBarriers` — before each pass `RunBuildRenderPass`, transition texture/depth/color-alias inputs via `RHICmdTransition` (shader-read layout on VK).
- S03 partial: `RDGTextureAccess` metadata deferred; barrier hook landed.
- Next: user VK visual verify BUG-013; then **S06** binding lifecycle.

### 2026-08-30 - RND-F12-S01/S02: read edge + per-frame Bake (`feat/render`)
- S01: `AddSceneLitShadowTextureInputs` on Base/Translucent; remove shadow `ForceIncludePass`; `pass_dependencies` + missing-writer throw; render-graph tests (6/6).
- S02: delete `BuildShadowResourceFingerprint` / pending invalidate; `Bake()` every frame in `SetupFrameRenderGraph`.
- Next: **S03** VK pass barriers (`RHICmdTransition`).

### 2026-08-30 - RND-F12: Granite RDG full semantic parity design (`feat/render`)
- North star: replicate Granite RenderGraph **semantics** (not copy code); Phase A–D.
- Design §3 Adapter boundary; §4 full parity checklist; §6 delete non-Granite patches; Bake policy = per-frame until proven safe.
- Impl: S01–S07 Phase A (BLOCK 013); S04–S15 Phase B–D.

### 2026-08-30 - RND-F12: Granite RDG bake semantics (`feat/render`)
- Reframe BUG-RENDER-013: not shadow-only fix — incomplete F07 bake vs Granite (`read edge`, `barrier`, `invalidate`).
- Docs: recover `RND-F07` Design UTF-8; add `RND-F12` Design + Impl; Registry / ACTIVE_WORK / BUG-013 links.
- Next: **RND-F12-S01** — Scene pass `AddTextureInput` shadow atlases; remove shadow `ForceIncludePass`.

### 2026-08-30 - BUG-RENDER-013: partial commit + RDG gap analysis (`feat/render`)
- User: pass-filter / split enqueue is patchwork; fix must be proper RDG.
- Reverted: `RenderGraph::EnqueueRenderPasses(filter)`, `ForwardRenderer` shadow→set1→scene order.
- Kept: `VulkanRHIResources` depth shadow SRV `DEPTH_STENCIL_READ_ONLY_OPTIMAL`.
- Next: Granite-aligned RDG read edges + bake invalidate for shadow atlases.

### 2026-08-30 - BUG-RENDER-013 reframe: RDG not VK binding (`feat/render`)
- User: S2–S4 ineffective; pivot to Granite RDG reference; TD-025 convention largely ruled out (dir-only OK).
- Docs: BUG-013 status → Open, root cause class = RDG scheduling/lifetime; BUG-010 + TD-025 §8 P6/P7 updated.
- Workspace: S1-only (pass filter enqueue, set1 after shadow, VK depth SRV layout) — documented as thin baseline, not root fix.
- Next: Compare minEngine `RenderGraph` bake/deps to Granite `render_graph.cpp`; shadow atlas read edges + fingerprint invalidate.

### 2026-08-30 - BUG-RENDER-013 rollback to S1-only (`feat/render`)
- User: S2–S4 fixes ineffective; suspect RDG/fingerprint path; request revert to S1 baseline for fresh investigation.
- Reverted: dynamic fingerprint, RDG shadow texture inputs, transition/retain, shadow ring split, set0 offset cache, limits co-location, ShaderCompiler macro inject, pool 8192.
- Kept (S1): shadow→set1→scene enqueue order; `RenderGraph::EnqueueRenderPasses(filter)`; VK depth SRV `DEPTH_STENCIL_READ_ONLY_OPTIMAL`.
- Next: Re-diagnose from clean S1 state; consider RDG architecture review before more binding patches.

### 2026-08-30 - BUG-RENDER-013 S4 binding fixes (`feat/render`)
- User: Dir shadow intermittent with point off; point position couples to Dir when on; latched state after point off.
- S4: separate `m_ShadowPerObjectUniformBuffer`; set0 descriptor cache validates ring byte offset; force set1 rebuild after shadow passes.
- Next: User VK retest on `test` — Dir stable with point Cast Shadow on/off; move point should not move Dir shadow.

### 2026-08-30 - BUG-RENDER-013 confirmed via dir-only isolation (`feat/render`)
- Result: With `MAX_*_SHADOW_MAPS=0`, user reports **VK directional shadow visually correct**.
- Conclusion: Full-scene failure was **binding/pipeline state pollution** when point/spot shadow passes participate — not Dir light-space math as primary cause.
- Next: Implement fix slices S1–S3 (set1 after shadow writes, VK depth layout, RDG read deps); restore shadow map budget=2; regress multi-light.

### 2026-08-30 - Dir-only shadow isolation + limits co-location (`feat/render`)
- Goal: BUG-RENDER-013 isolation — shut down point/spot shadow passes at engine budget; co-locate light vs shadow-map limits.
- Main changes: `EngineRenderLimits.h` owns `MAX_*_LIGHTS` / `MAX_*_SHADOW_MAPS` / sampler slots + `static_assert`; experiment `MAX_*_SHADOW_MAPS=0`; ShaderCompiler injects macros; set1 arrays sized by sampler slots.
- Next: User VK visual verify — Dir alone with point Cast Shadow on/off should no longer allocate point maps; if Dir still missing when point off, prioritize descriptor layout / set1 dirty (P0/P1).

### 2026-08-30 - BUG-RENDER-013 RDG/VK shadow binding investigation (`feat/render`)
- Goal: Explain VK Dir shadow visibility coupling to Point Cast Shadow; separate pollution vs cascade math.
- Findings (code review): VK depth descriptor layout mismatch; `BuildSceneSet1` dirty only on texture cache change; `BasePass` missing `DirShadowAtlas` RDG input; static shadow fingerprint. Documented in BUG-RENDER-013 + RND-TD025 §8 P7 + shadow pass isolation matrix.
- Next: User experiments via `MAX_*_SHADOW_MAPS` / `MAX_CASCADES` or scene Cast Shadow toggles; then fix P0–P3.

Last updated: 2026-08-28 (TD-025 clip-space caps + VK shadow fix)

## Purpose

This file is an AI-oriented progress digest converted from commit messages.
It is not a full changelog. It focuses on architecture moves, rendering milestones, and known pitfalls.

## Timeline Summary

### 2026-08-28 - TD-025 RHI clip-space capabilities + VK shadow fix (`feat/render`)
- Goal: Unify clip/viewport/cull policy; fix BUG-RENDER-010 (VK shadows) and BUG-RENDER-011 (disable point/spot shadow crash).
- Main changes:
  `RHIClipSpaceCapabilities`, `RHIClipSpace`, `RHIViewportConvention`; Shadow scheme A (no flipY + Front cull).
  ShadowPass / ForwardRenderer / RenderCamera / EnvMapCapture / ShaderCompiler / Editor ImGui UV migrated.
  `EngineSceneBindingSets` clears unused spot/point shadow SRV slots; dir shadow index gated in shader.
  Docs: [RND-TD025 design](./Render/RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md), BUG-RENDER-010/011 updated.
- Validation: cmake build minEngine + Editor (pending user VK visual verify on `test` scene).

## Timeline Summary

1. Initial framework stage
- Set up basic engine framework and RenderSystem foundations.
- Built simple mesh/texture support and early input handling.

2. Scene and component architecture stage
- Introduced GameObject/Component model, Level/WorldManager, Transform.
- Added render-side abstraction using SceneProxy.
- Started AssetManager and resource-oriented structure.

3. Lighting and model import stage
- Added light components and light scene proxies.
- Extended Phong shader for directional/point/spot lights.
- Added basic model import via Assimp.

4. Rendering pipeline structuring stage
- Reorganized render files (RHI, light proxies, primitive proxies).
- Added translucency pass.
- Added present pass for offscreen-to-screen output.

5. Data upload optimization stage
- Added per-frame camera UniformBuffer.
- Added separate light UBO.
- Reduced redundant light data upload frequency.

6. Shadow and stability stage (recent)
- Added directional light shadow pass and shadow map sampling.
- Fixed repeated queue growth issue (missing clear each frame).
- Fixed Texture2D release issue causing GPU memory leak.
- Fixed light proxy write-back issue causing repeated creation/leak risk.

7. Reflection modernization stage (2026-04-10 ~ 2026-04-12)
- Introduced MEReflection in parallel with legacy reflection for migration.
- Refactored reflection/serialization boundaries and startup finalization flow.
- Added better reflection finalization error logging and repaired GameObject reflection path.

8. Serialization capability expansion stage (2026-04-12 ~ 2026-04-14)
- Implemented pointer deserialization foundation in Serializer.
- Added primitive codec registry and MEEnum codec support.
- Unblocked GameObject serialization for owned inline component arrays (shared_ptr<Component>).
- Reenabled Inspector path for GameObject and component fields.

9. Asset identity and registry kickoff stage (WIP, 2026-04-16)
- Started GUID type and generation utility for asset identity.
- Introduced AssetMeta and initial in-memory asset registry shape.
- Added AssetManager scan/register flow and Texture2DResource reflection stub.

## Current Technical Focus (Inferred)

- Complete reflection/serialization migration without breaking editor workflows.
- Finalize pointer/reference semantics for MEObject relationships.
- Build asset metadata persistence (.meta read/write) and GUID lifecycle stability.
- Keep scene serialization, inspector, and resource loading behavior aligned.

## Known Pitfalls to Recheck During Refactors

- Any per-frame vector/list/map used in pipeline build path must be cleared or reused safely.
- Any GL resource wrapper must own and release native handles in destructor.
- Scene proxies or render entries must not be recreated indefinitely without ownership policy.
- Shadow pass changes (viewport/state/targets) must be restored before later passes.
- Large flat shadow casters: do not expand CSM Z with bounding-sphere radius (see BUG-RENDER-004).
- Pointer deserialization must clearly separate ownership and reference semantics.
- Serializer signature changes must be synchronized across all callsites.
- Asset scanning must avoid duplicate registration and accidental GUID regeneration.

## Entry Template (Append for each meaningful task)

### 2026-08-30 - BUG-RENDER-010: rollback fixed ortho box; suspect GPU shader

- Reverted `kDirShadowUseFixedOrthoBox` + `kDirShadowForceCascade=0` → normal CSM path.
- Fixed ortho box: no Dir shadow on **both** GL and VK (box params invalid; experiment inconclusive for ortho depth).
- FORCE=0 prior result kept: multi mesh shadows → one; still GL≠VK → cascade mixing + shader read path.
- Next: GPU — `MinEngineShadowMapCoords`, Dir PCF, `sampler2DArray` layer. BUG-RENDER-013 (point shadow gate) still open.

### 2026-08-30 - BUG-RENDER-010/013: FORCE_CASCADE=0 + fixed ortho box experiment

- FORCE cascade 0: multiple Dir mesh shadows → one; GL vs VK (both FORCE=0) still mismatch → single-cascade Dir write/read still broken.
- Added `kDirShadowUseFixedOrthoBox` in `ForwardRenderer.cpp` (cascade 0 fixed light-space ortho ±50, near/far 1/200) to isolate CSM frustum→AABB vs ortho depth.
- Filed **BUG-RENDER-013**: VK Dir shadow visibility depends on Point Cast Shadow (suspect set1 bindings); keep separate from matrix experiments.
- Keep `kDirShadowForceCascade=0` while running fixed-box visual on GL+VK.

### 2026-08-29 - BUG-RENDER-010: Dir shadow debug modes (P1)

- Added `DIR_SHADOW_DEBUG_MODE` via `TryDirShadowDebugVisual` in `MaterialSceneShadows.glslinc`; wired in Phong/PBR graph lighting.
- Toggle: `kDirShadowDebugMode` in `ShaderCompiler.cpp` (inject after `#version`). Default **1** = cascade colors.
- Modes: 1 cascade / 2 single-tap / 3 UV / 4 current Z / 5 sampled Z / 6 Z delta. See Gap Design §8.
- Also fixed inject skip: only skip if `#define MINENGINE_CLIP_DEPTH_ZERO_TO_ONE` already present (not bare token in `#if`).

### 2026-08-29 - BUG-RENDER-010: commit `3154700` + FlipY 共识修正

- **Commit:** ZO depth read (`MinEngineShadowMapCoords`) + `MinEngineShadowMapSlot` (BUG-RENDER-012); scheme A only.
- **Reverted:** scheme B shadow viewport flip, read `uv.y` flip, point `ShadowMap2D`, P4-A/P4-B experiments.
- **共识:** Shadow 写→读独立闭环；Main Pass Scene flip **不**参与 shadow 采样。VK Spot ~OK.
- **Open:** Dir (CSM), Point (cube / 四重鬼影). Docs: `RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md` §2/§8.

### 2026-08-29 - BUG-RENDER-010: P0 read-side uv.y补偿 (B+C for manual ndc→uv) — **reverted**
- VK `MinEngineShadowMapCoords`: `uv.y = 1.0 - uv.y` when ZO define set; keeps shadow viewport flip (B).
- Point cube path unchanged. Design §8 backlog recorded.
- Pending: VK visual Dir/Spot/multi-light.

- **Reverted:** 见上条 `3154700` 共识；勿再按 Main Pass flip 推导读侧补偿。

### 2026-08-29 - BUG-RENDER-012 + TD-025 Step 2B shadow viewport flip — **reverted**
- BUG-RENDER-012: `MinEngineShadowMapSlot` — gate `Params.w < 0` before shadow sample (all light types).
- TD-025 Step 2B: revert sample Y flip; VK `kVulkanShadowPass.ViewportFlipY=true` + Back cull; point faces use ShadowMap2D viewport convention.
- Pending: GL/VK visual on `test` scene.

### 2026-08-29 - BUG-RENDER-010: Step 2 shadow sample Y flip (option C)
- Goal: Close Gap 2 on read path only — VK `uv.y = 1.0 - uv.y` via `MINENGINE_SHADOW_MAP_SAMPLE_FLIP_Y`.
- Main changes: `MinEngineShadowMapCoords`, `InjectClipSpaceDefines` (ZO + flip pair).
- Shadow write viewport unchanged (scheme A).
- Pending: user GL/VK visual Dir/Spot on `test` scene.

### 2026-08-29 - BUG-RENDER-010: Step 1 ZO shadow depth read
- Goal: Close Gap 1 only — Vulkan Dir/Spot CurrentDepth uses ZO `ndc.z`, not N1 `*0.5+0.5`.
- Main changes:
  - `MinEngineShadowMapCoords` in `MaterialSceneShadows.glslinc` / `Phong.frag`.
  - `ShaderCompiler::InjectClipSpaceDefines` — ZO define only (no sample flip); restore pass-local OpenGL flat remap for set=0 ShadowPass.
- Verify: `Editor` + `minEngineTests` build OK; `test smoke` fails known MaterialIR UBO golden (`set=` vs flat) — unrelated.
- Pending: user GL/VK visual on `test` scene (Dir/Spot first).

### 2026-08-29 - BUG-RENDER-010: layered rollback baseline before convention close
- Goal: Freeze workspace as pre-fix baseline — keep TD-025 caps/matrix/viewport infra; roll back failed shader flip/`MinEngineShadowProject` inject stack; document GL→VK shadow convention gaps.
- Main changes:
  - Shader sampling back to `projCoords * 0.5 + 0.5` (`MaterialSceneShadows`, `Phong.frag`); remove `InjectClipSpaceDefines` / sample flip inject from `ShaderCompiler`.
  - Retain `RHIClipSpaceCapabilities` / `RHIClipSpace` / ShadowPass convention + caps bias; dir shadow index gate.
  - Docs: reopen BUG-RENDER-010; gap design `RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md`; session handoff note.
- Verify: build not re-run in this commit; next: Step 1 ZO depth read only.
- Status: Vulkan shadows still Open / incorrect by design at this baseline.

### 2026-08-28 - BUG-RENDER-010: Vulkan directional shadow plane false self-shadow
- Goal: Fix VK Editor CSM shadow — large false shadow on 100×100 plane (plane self-shadow via sampling); cube shadow OK at some angles.
- Root cause: ShadowPass Z remap + OpenGL `glm::ortho` light matrices vs lit-pass `*0.5+0.5` sampling mismatch; CSM frustum used OpenGL NDC corners with Vulkan `perspectiveRH_ZO` camera.
- Main changes:
  - `ForwardRenderer`: `orthoRH_ZO` / `perspectiveRH_ZO` for VK light proj; backend-aware CSM NDC near/far.
  - `ShadowPass.vert`: remove clip-Z remap (light matrices now ZO on VK).
  - `MaterialSceneShadows.glslinc`, `Phong.frag`: `MinEngineShadowProject` — ZO depth = `ndc.z`, GL = `ndc.z*0.5+0.5`.
  - Bug record: `docs/ai/bugs/BUG-RENDER-010.md`.
- Verify:
  - `cmake --build minEngine/build --target minEngine` — OK.
  - `Editor.exe --rhi vulkan` visual A/B on `test` scene — **pending user**.

### 2026-08-28 - ED-F01 S06: Vulkan Editor shadows + post flags
- Goal: Enable shadow/post pipeline on Vulkan Editor (match OpenGL draw flags); fix VK shadow path blockers.
- Main changes:
  - `SceneEditingViewportClient`: VK uses `EnableShadows | EnablePostProcess | EnableSkyBox` (no sky-only fork).
  - `VulkanRHITexture`: `Texture2DArray` create/view for `DirShadowAtlas` CSM atlas.
  - `VulkanRHI`: per-layer depth attachment views for CSM cascade slices.
  - `ShadowPass`: depth-only PSO desc; VK back-face cull; `ShadowPass.*` shaders use `set=0` bindings.
  - `ShaderCompiler`: pass-local OpenGL flat remap for ShadowPass; Vulkan `MINENGINE_CLIP_SPACE_ZO` inject.
- Verify:
  - `cmake --build minEngine/build --target Editor` — OK.
  - `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` — loads `test`, ShadowPass SPIR-V OK, no `DirShadowAtlas` / PSO bind errors in log (~12s smoke).
- Pending: user visual A/B — cube shadow on plane vs GL; then commit.

### 2026-08-26 - ED-F01 Vulkan visual bugfix: UBO ring + bake viewport + retired buffers
- Goal: Fix Cube invisible / plane Y-scale oddity / sky ±Y split / mesh hot-swap DEVICE_LOST on Vulkan Editor.
- Done:
  - Per-Object UBO ring (aligned slots) + `RHIShaderBinding` BufferOffset/Range; scene + shadow draws bind distinct regions
  - EnvMap bake `SetViewport(..., flipY=false)` while scene path keeps Y-flip
  - `VulkanRHI` retires buffers and flushes after in-flight fences (BeginFrame / Shutdown)
  - Bug records `BUG-RENDER-005`…`009`; design Status → In Progress
- Verify: Vulkan Editor smoke loads `test` + HDR bake, no DEVICE_LOST in stderr; **user visual A/B still required**
- Open: `BUG-RENDER-007` plane UV zoom — wait for post-UBO screenshots

### 2026-08-26 - ED-F01 S07 garbled HDR sky: float32→half upload
- Goal: Fix psychedelic/moiré Vulkan sky after successful HDR bake (no DEVICE_LOST).
- Root cause: `CreateFromHdrPixels` passes float32 (stbi_loadf); OpenGL uploads with `GL_FLOAT`, but Vulkan treated `*16F` initialData as raw half bits.
- Main changes: `VulkanRHITexture` converts float32 RGB/RGBA → `R16G16B16A16_SFLOAT` on upload (2D + cube paths).
- Verify: rebuild Editor; user visual check for citrus HDR sky on `--rhi vulkan`.

### 2026-08-26 - ED-F01 S07 HDR bake DEVICE_LOST: descriptor lifetime
- Goal: Fix remaining Vulkan `DEVICE_LOST` after HDR sky bake (ImGui `VkResult=-4`).
- Root cause: EnvMapCapture loop destroyed per-face `RHIShaderBindingSet` (and reused one UBO) while the immediate CB still referenced them.
- Main changes:
  - `EnvMapCapture`: `EnvCapturePendingBindings` keeps per-face UBO + descriptor set until after `RHIEndImmediateCommands`.
  - Keep prior fixes: cube layout defer; submit before PSO destroy; depth `DEPTH_STENCIL_READ_ONLY`.
- Verify:
  - `--rhi vulkan`: HDR bake + ~10s loop + clean shutdown, no `DEVICE_LOST`.
  - `--rhi opengl`: HDR/IBL bake + clean shutdown.

### 2026-08-25 - ED-F01 S07 HDR sky bake on Vulkan (DEVICE_LOST fixed)
- Goal: Restore project HDR → cubemap bake for Vulkan sky (citrus orchard); remove temp debug scaffolding.
- Main changes:
  - `EnvironmentMap`: Vulkan no longer skips `EquirectToCubemap`; IBL irradiance/prefilter still aliased to environment cube.
  - `EnvMapCapture`: defer cube layout transition until all faces captured; submit immediate CB **before** bake PSO destroy.
  - `VulkanRHI`: cube-face `EndRenderPass` skips premature ShaderResource transition; depth → `DEPTH_STENCIL_READ_ONLY`.
  - Cleanup: removed first-frame VK debug logs; validation cube toned to mild blue.
- Verify:
  - `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` — HDR bake log, no `DEVICE_LOST`, clean shutdown.
  - `Editor.exe --rhi opengl` — full HDR/IBL bake + clean shutdown.
- Deferred: VK IBL convolution (irradiance/prefilter passes), S06 shadow/post in editor flags, `TD-025` capabilities API.
- Follow-up 2026-08-26: still saw DEVICE_LOST until descriptor/UBO lifetime fix (entry above).

### 2026-08-25 - ED-F01 S05 viewport parity + S07 SkyPass (Vulkan)
- Goal: Fix VK editor viewport (mesh/gizmo/nav); re-enable Sky with validation cube.
- Main changes:
  - `RenderCamera`: Vulkan `perspectiveRH_ZO` + pick NDC Z=0 (render/pick/gizmo unified).
  - `EditorViewportWindow`: Vulkan ImGui UV `(0,0)-(1,1)` (no double Y-flip with viewport).
  - `SceneEditingViewportClient`: `SyncSceneViewportCameraAspect`; VK flags `EnableSkyBox`.
  - `SkyBoxPass`: explicit no-cull PSO; sky UBO stage `All`; first-prepare log.
  - `EnvironmentMap`: brighter validation cube face colors for debug.
  - Docs: **TD-025** clip/handedness vs `IsVulkan()` coupling.
- Verify: `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` — plane + gizmo OK; sky TBD user.
- Deferred: HDR bake on VK (`EnvironmentMap`), IBL convolution, shadows/post (S06).

### 2026-08-17 - RND-F05-S07d visual smoke confirmed + deferred debt grouped
- Goal:
	Confirm S07d is not just alive but visibly drawing scene geometry, then record the remaining cleanup honestly before the next slice.
- Main changes:
	User visual check confirmed visible mesh output in Vulkan Editor smoke.
	Root cause for the prior blue-only frame was `SkyBoxPass` still entering the graph and clearing `SceneColor` after Opaque; smoke now gates the pass via `NeedRenderPass()`.
	Deferred follow-up grouped into two TDs: `TD-023` (scene pass ordering / clear contract) and `TD-024` (Vulkan frame sync + debug leftovers).
- Risks or caveats:
	S07d is visually accepted, but enabling Sky on the current graph still deserves a proper ordering/clear cleanup.
	Vulkan present semaphore reuse on fast shutdown is not fully clean yet; some diagnostic logs also remain intentionally temporary.
- Validation done:
	Editor `--rhi vulkan --project ...\MyMEProject.meproject` manual visual check: visible mesh output.
	Debug logs also showed BasePass draws and PresentPass blit on the same frame.
- Next step:
	S07e Shadow + scene include `set=`; pay down `TD-023` / `TD-024` when the render track has a natural cleanup window.

### 2026-08-05 - RND-F05-S07b–S07d Vulkan Descriptor/PSO + Forward Unlit Base
- Goal:
	Batch S07b–S07d so `--rhi vulkan` can run Forward Base (Unlit) and Present without ImGui.
- Main changes:
	Vulkan descriptor pool / set layout / pipeline layout / binding sets; lazy graphics PSO per RenderPass;
	frame recording Clear→Cmd→Present; enable `ForwardRenderer` on Vulkan; Editor `OpenProjectForVulkanSmoke`
	loads `default` scene, forces Unlit, `SubmitSceneDraw(PresentToBackBuffer)`.
	Fixes: depth RT no default SAMPLED; `DEPTH24STENCIL8`→`D32_SFLOAT_S8_UINT`; `PerFrame` visibility `All`.
- Validation done:
	Editor `--rhi vulkan --project MyMEProject`: Unlit recompile OK; 16s render loop alive;
	`VK_LAYER_KHRONOS_validation` stderr empty; `test smoke` GL+VK PASSED.
	**Human visual check still required for mesh silhouette.**
- Next step:
	S07e ShadowPass + `MaterialSceneShadows`/`lights` `set=` dialect; then prepare commit for S07a–d WIP.

### 2026-08-05 - RND-F05-S07a Vulkan Buffer/Texture2D/SRV/VertexInputLayout
- Goal:
	Fill VulkanRHI resource create/upload stubs so later S07 draws have a data plane.
- Main changes:
	`VulkanRHIAllocator` + `VulkanRHIBuffer` (host-visible map) / `VulkanRHITexture` (2D + staging) /
	`VulkanRHIShaderResourceView` / `VulkanRHIVertexInputLayout`; wire `RHICreate*`; init probes.
- Validation done:
	Editor `--rhi vulkan`: S07a buffer/texture/SRV/layout probe OK;
	`minEngineTests.exe --rhi opengl|vulkan test smoke` PASSED.
- Next step:
	S07b Descriptor / BindingSet / PipelineLayout.

### 2026-08-05 - RND-F05 S07 sub-slice table drafted (await review)
- Goal:
	Replace vague S07+ with reviewable S07a–S07f before any Vulkan scene implementation.
- Main changes:
	Impl expands S07a resources → S07b descriptor → S07c PSO/Cmd → S07d Forward Base → S07e Shadow+set= → S07f Sky/IBL;
	Design §3.9 pending defaults (classic VkRenderPass, no ImGui-VK, enable Forward at S07d).
- Validation done:
	Docs only; no code.
- Next step:
	User review/approve §3.9 + Impl S07 table; then start S07a.

### 2026-08-04 - RND-F05-S06 complete (SkyBox + EnvMapCapture + Material set=/SPIR-V)
- Goal:
	Finish S06 shader dialect batches so engine fixed passes and graph materials share SPIR-V delivery.
- Main changes:
	SkyBox + EnvMapCapture (equirect/irradiance/prefilter) → `CreateShaderFromSpirvFiles` + location quals.
	MaterialCompiler emits `layout(set=kSetMaterial, binding=…)`; ShaderCompiler flattens set= for OpenGL.
	`Material::CommitCompileResult` → `CreateShaderFromSpirvSources`; S05 marked Done in same docs pass.
- Validation done:
	`minEngineTests.exe --rhi opengl test material-ir` PASSED;
	`test shader-compiler` PASSED; `test smoke` PASSED.
	Flatten strips `set=` inside `layout(std140, set=…, binding=…)`.
	Editor startup: material varyings `layout(location=…)` + remove dead `u_Material` in shadows include.
- Next step:
	S07+ — write VK scene sub-slice table before filling Vulkan resource stubs.

### 2026-08-04 - RND-F05-S05 Done + S06 SkyBox background SPIR-V
- Goal:
	Close Present-path slice; start engine-shader SPIR-V batches with SkyBox.
- Main changes:
	S05 marked Done (`PresentFrame` / no upper-layer `vulkan.h`).
	`background.vert/frag` explicit varyings/`out` locations; `SkyBoxPass` → `CreateShaderFromSpirvFiles`.
- Validation done:
	Editor build OK; `minEngineTests.exe --rhi opengl test smoke` PASSED;
	background.vert/frag SPIR-V-ready locations; SkyBoxPass loads via CreateShaderFromSpirvFiles.
- Next step:
	S06 next batch — EnvMapCapture bake shaders and/or MaterialCompiler `set=`.

### 2026-08-04 - BUG-RENDER-004 directional CSM self-shadow acne (+ BUG-RENDER-003 gate)
- Goal:
	Remove texture-following stripe banding on large ground planes under directional light; restore RND-F05 track after diagnosis.
- Main changes:
	Shadow depth PSO front-face cull + polygon offset; cascade Z expand via AABB corners (not sphere);
	receiver bias / light-dir offset; shadow map 1024; dir shadow sample gated on `Params.w` (also closes BUG-RENDER-003);
	TBN: tangent uses model matrix under non-uniform scale.
- Validation done:
	User A/B: disable dir shadow pass → stripes gone; after fix → stripes gone with CSM on (Editor OpenGL).
- Next step:
	Resume **RND-F05-S05** (Present / post-process neutrality; more SPIR-V).

### 2026-08-04 - RND-F05-S05 Present 路径收口（进行中）
- Goal:
	Align Editor/Engine frame present on neutral RHI path (no direct `SwapBuffers` in Editor OpenGL loop).
- Main changes:
	`RenderSystem::PresentFrame()` → `RHI::RHIPresent()`; `Engine::Run` and Editor (OpenGL + Vulkan) call it.
- Validation done:
	Editor OpenGL + Vulkan startup OK (user visual).
- Next step:
	Continue S05 — PresentPass / post-process path parity; avoid `vulkan.h` in upper layers.

### 2026-08-04 - RND-F05 post-process OpenGL SPIR-V + S04 Vulkan triangle
- Goal:
	Extend SPIR-V hot path to FXAA/Sharpen; land Vulkan minimal graphics draw for smoke validation.
- Main changes:
	ForwardRenderer FXAA/Sharpen → `CreateShaderFromSpirvFiles`; `FXAA.frag` / `Sharpen.frag` add `layout(location=...)`.
	`VulkanRHI` render pass + pipeline; embedded triangle SPIR-V (RGB gradient).
- Validation done:
	Editor `--rhi vulkan` colored triangle PASS; OpenGL SPIR-V compile errors fixed.
- Next step:
	S05 present-path alignment.

### 2026-08-04 - RND-F05-S03 CLI `--rhi` + Vulkan clear/present vertical slice
- Goal:
	Enable runtime backend switch (`--rhi opengl|vulkan`) and land Vulkan swapchain clear/present with backend-internal sync only.
- Main changes:
	CLI adds global `--rhi` (opengl|vulkan, aliases gl|vk); TestMain synthetic argv updated so option-first test invocations keep `test` subcommand semantics.
	Introduce `RHIBackendSelection` and route Window/Render boot via selected backend.
	GLFW adds Vulkan `GLFW_NO_API` path; OpenGL path stays 4.6 + glad.
	`RHI` adds neutral `RHIPresent()`; OpenGL uses `SwapBuffers`, Vulkan owns acquire/submit/present with internal semaphore/fence.
	`VulkanRHI` S03 scope: instance/device/surface/swapchain + clear color present; resource/draw APIs intentionally stubbed for S04+.
	Editor Vulkan path runs smoke mode without ImGui/editor modules (clear/present validation only).
- Validation done:
	`minEngineTests.exe --rhi opengl test smoke` PASSED
	`minEngineTests.exe --rhi vulkan test smoke` PASSED
	`Editor.exe --rhi vulkan --project ...` startup log confirms NO_API + VulkanRHI clear/present loop.
- Next step:
	S04 — Vulkan minimal graphics pipeline + SPIR-V shader path (first visible triangle/fullscreen draw).

### 2026-08-04 - RND-F05-S02 OpenGL 4.6 + Present SPIR-V hot path
- Goal:
	Consume OpenGL SPIR-V on Present; keep Material GLSL string path as migration window.
- Main changes:
	GLFW / MaterialIR / RenderGraph contexts → **4.6**.
	`RHIShaderCreateDesc` + bytecode `RHICreateShader`; OpenGL `glShaderBinary` + `glSpecializeShader`.
	`EngineShaderUtils::CreateShaderFromSpirvFiles`; PresentPass uses SPIR-V path.
	`test shader-compiler` adds GL specialize load case.
- Validation done:
	`minEngineTests.exe test shader-compiler` PASSED (2 cases); `test smoke` PASSED; `test render-graph` PASSED earlier.
- Next step:
	S03 — CLI `--rhi opengl|vulkan` + VulkanRHI Clear/Present (frame sync internal).

### 2026-08-04 - RND-F05-S01 ShaderCompiler（Present → VK/GL SPIR-V）
- Goal:
	Land GLSL→SPIR-V toolchain without switching GL runtime hot path yet.
- Main changes:
	`Render/ShaderCompiler/` (glslangValidator invoke + disk cache); CMake finds Vulkan SDK / glslang.
	`Present.vert/frag`: `#version 420` + explicit `location` (SPIR-V requirement); varyings `v_TexCoord`.
	Suite `test shader-compiler` (not in smoke).
- Validation done:
	`minEngineTests.exe test shader-compiler` PASSED; `test smoke` PASSED.
- Next step:
	S02 — GL context 4.6 + Present loads SPIR-V via `RHICreateShader(bytecode)`.

### 2026-08-04 - RND-F05 Design Draft（地基评估 + SPIR-V 双端）
- Goal:
	Assess render foundation for Vulkan; design SPIR-V for both GL and VK; multi-slice plan.
- Main changes:
	Expanded `RND-F05_*_DESIGN.md` + new `*_IMPLEMENTATION.md`; Registry Draft; ACTIVE_WORK pointer.
	Key finding: GL SPIR-V requires DescriptorSet=0 → dual SPIR-V artifacts (VK multi-set / GL flat kGL_*).
- Validation done:
	Code survey (RHI/OpenGL/shaders/CMake); local `VULKAN_SDK` 1.4.350 + glslangValidator present.
- Next step:
	User confirms Design §7 → Planned → S01 ShaderCompiler.

### 2026-08-04 - RND-F03 关账 + 主线改为 F05（docs）
- Goal:
	Close stale F03 In Progress; lock render-first roadmap (Vulkan multi-slice, physics after DebugDrawing).
- Main changes:
	F03 Design Meta/§12 → Done；Registry F03 Done；F05 Design 明确多刀竖切 + GL/VK 终态；ACTIVE_WORK 主线 `feat/render`；PHYS-F03 等 F11。
- Validation done:
	Legacy grep (`WrapLegacy`/`RHIShaderLegacy`/`OpenGLRHIModern`/…) production src = 0.
- Next step:
	`feat/render` rebase master → F05 Pre-flight + Implementation 切片表。

### 2026-08-04 - CORE-F04 Native Multicast Delegates（实现 Done）
- Goal:
	Ship Native multicast delegates; unlock PHYS-F03; close TD-006 (Native).
- Main changes:
	`Runtime/Core/Delegates/` — `DelegateHandle`, `MulticastDelegate`, macros; AddRaw / AddLambda / AddMEObject; Broadcast snapshot + stale MEObject compact.
	`Tests/Suites/DelegateTest.cpp` + suite id `delegates` (not in smoke).
	Docs: Design/Impl Done；TD-006 Done；Registry/ACTIVE_WORK.
- Risks or caveats:
	Not thread-safe; prefer AddMEObject over AddRaw for gameplay; Dynamic/Lua still future.
	MulticastDelegate must include ObjectManager before bare GUID.h (Win32 `GetClassName` macro).
- Validation done:
	`minEngineTests.exe test delegates` — 5/5 PASSED.
- Next step:
	Optional: PHYS-F03 Design on physics track; or feat/render F05.

### 2026-08-04 - CORE-F04 Native Multicast Delegates Design Draft
- Goal:
	Formalize Native-only multicast delegates (unlock PHYS-F03); split from reflection/Dynamic path.
- Main changes:
	`docs/ai/Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_{DESIGN,IMPLEMENTATION}.md`（Status Draft）.
	Old `REFLECTION_DELEGATES_DESIGN.md` → Archived pointer; Registry / ACTIVE_WORK / TD-006 Notes / PHYS-F03 dependency updated.
- Risks or caveats:
	Superseded by implementation entry above.
- Validation done:
	Docs only.
- Next step:
	(done) implement S01–S03.

### 2026-08-03 - TD-013：enum codec 按 MEEnum::GetSize 读写（`master`）
- Goal:
	Stop treating every reflected enum as in-memory int64 (overran uint8 neighbors; bad scene JSON).
- Main changes:
	`ReflectionSystem::SetCodecForEnums` loads/stores via size 1/2/4/8; archive wire still WriteInt64/ReadInt64.
	`serialization-archive` smoke adds uint8 `m_ShadingModel` round-trip + neighbor `m_BlendMode` corruption check.
	Registry: `CORE-F04` Delegates + `RND-F11` DebugDrawing Planned；ACTIVE_WORK 顺序 Vulkan→DebugDrawing.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngineTests.exe test smoke` / `physics-load` → PASSED
- Next step:
	Base `render`/`physics` onto master；分支改名 `feat/*`；开 CORE-F04 或 F05 按 ACTIVE_WORK.

### 2026-08-03 - master ← merge render（现代 RHI/RDG/EnvMap + physics/Lua）
- Goal:
	Integrate `render` (RND-F02–F10) onto `master` (Lua/physics/Transform quat) without dropping either track.
- Main changes:
	Union AssetManager LuaScript + EnvironmentMap; TestSuiteRegistration all suites; docs ACTIVE_WORK/REGISTRY/TECH_DEBT (CORE TD-013 enum kept; render Set0 cache → TD-022).
	`test.mescene` keeps physics + SkyBox EnvironmentMap.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngineTests.exe test smoke` / `render-graph` / `lua-script-mvp` / `physics-smoke` → PASSED
	(merge follow-up: drop nonexistent `RenderGraphTest.h` include in TestSuiteRegistration)
- Next step:
	Push when ready; resume RND-F05 discussion.

### 2026-05-25 - Asset Pipeline 业务线设计草案
- Goal:
	End-to-end design for AssetManager CRUD/events, cross-platform import dialog, project file watcher, Content Browser framework (asset-workflow worktree).
- Main changes:
	Added `docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md` (data flow, R0–R2, E4, AssetWorkflow, Browser v0, implementation order P1–P7, decision table §10).
	Linked from `CONTENT_BROWSER_DESIGN.md`.
- Risks or caveats:
	Awaiting user sign-off on §10 (NFD vs Win32, watcher polling vs native, Import rename policy, Browser list scope).
- Validation done:
	Reviewed `AssetManager` / `AssetWorkflowModule` / `ProjectManager::OpenProject` scan path against design.
- Next step:
	§10 已拍板（NFD、efsw、D12、相对 AssetPath §14）→ P1 + P1b 实现。

### 2026-05-25 - P1 API 定稿（待审批）
- Goal:
	Freeze header-level P1/P1b interface before coding.
- Main changes:
	`docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_P1_API.md` — AssetRegistryTypes, AssetTypeRegistry, AssetManager deltas, contracts, migration table, acceptance §9, approval checklist §10.
- Risks or caveats:
	Breaking `FindAssetMetasByType` return type; Editor Material/Scene call sites updated in same PR.
- Validation done:
	Grep call sites; path rules aligned with §14.
- Next step:
	User approves §10 checklist → implement P1.

### 2026-05-25 - Asset Pipeline P1/P1b implemented
- Goal:
	AssetTypeRegistry, type buckets, registry events, ImportAsset, project-relative AssetPath, Editor EngineDefault scan removed.
- Main changes:
	New `AssetRegistryTypes.h`, `AssetTypeRegistry.{h,cpp}`; `AssetManager` extended (Subscribe, ImportAsset, FindAssetMetasByType/ByRuntimeClass, ResolveAssetAbsolutePath).
	Loaders use absolute resolve for disk IO; Editor Material/Scene call sites updated; `MyMEProject` metas → relative `AssetPath`.
- Risks or caveats:
	RegisterAsset requires `PathRegistry` project root; legacy absolute registry keys only via FindAssetMetaByPath fallback until rescan.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded.
- Next step:
	P2 DeleteAsset/MoveAsset; then P3 NFD, P5 efsw, P6 Content Browser.

### 2026-05-25 - Asset Pipeline P2 implemented
- Goal:
	Delete/Move/Rename/Unregister registry CRUD, Moved events, ClearProjectRegistry on CloseProject, headless tests.
- Main changes:
	`AssetManager` — `DeleteAsset`, `MoveAsset`, `RenameAsset`, `UnregisterAsset`, `ClearProjectRegistry`; `ProjectManager::CloseCurrentProject` clears registry; `SceneManager::IsSceneRegistered`.
	`AssetManagerTest` + `--asset-manager-test` (temp project `Assets/_P2UnitTest/`, copies EngineDefault cube; never deletes MyMEProject assets).
- Validation done:
	`Editor.exe --asset-manager-test` exit 0 (delete, move, rename, extension guard, unregister, clear, scene unregister).
- Next step:
	P3 NFD / P4 AssetWorkflow / P5 efsw.

### 2026-05-25 - Asset Pipeline P3 FileDialog (Runtime Platform)
- Goal:
	Runtime IFileDialogService + NFD; Editor consumes via Engine; asset filters from Registry.
- Main changes:
	`Runtime/Platform/FileDialog/*`, `FileDialogService` in `Engine`; `AssetTypeRegistry::BuildFileDialogFilters`;
	Editor `GetFileDialogService()`; Tools menu **File Dialog (P3)** for Open/Save/Folder smoke.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded (single-threaded link after parallel truncate).
- Next step:
	P4 `AssetWorkflowModule::ImportAssetDialog`.

### 2026-05-26 - Asset Pipeline P5 ProjectAssetWatcher (efsw)
- Goal:
	Editor-only filesystem watcher syncing external disk changes to `AssetManager` registry.
- Main changes:
	`Third-Party/efsw` @ 1.4.0; `ProjectAssetWatcher` (queue + 400 ms debounce + main-thread `Tick`);
	`AssetManager::SuppressExternalSyncScope`; Editor `OpenProject`/`CloseProject`/`Run` lifecycle;
	bulk fallback `ScanAssets`; P5 API doc status → 已实现.
- Validation done:
	`cmake --build minEngine/build --target Editor -j 1` succeeded.
- Next step:
	Manual P5 acceptance (copy/delete in Explorer, Import no storm); P5.1 `Reimported`; P6 Content Browser.

### 2026-05-26 - Asset Pipeline P6 Content Browser
- Goal:
	Content Browser module: directory tree, registered asset list, selection, Inspector bridge, Open/Delete/Import.
- Main changes:
	`ContentBrowserModule`, `AssetTreeModel`, `ContentBrowserWindow`; `FindAssetMetasUnderDirectory`;
	`AssetWorkflowModule` selection/Delete/InspectorSource; `InspectorWindow` focus patch.
- Risks or caveats:
	**Browser UI 展示需后期再设计**（当前裸 ImGui 线框；主题/图标/缩略图待 Appearance merge 后 P6.1）。
- Validation done:
	`cmake --build` OK; user confirmed Content Browser visible and core flows work.
- Next step:
	P7 default Dock + menu integration; P6.1 Browser visual design after Appearance merge; P5.1 Reimported optional.

### 2026-05-26 - Editor / Runtime 文件分层迁移
- Goal:
	Align Editor SubEditor/Inspector layout and consolidate Runtime asset loaders under `Resource/Loaders/`.
- Main changes:
	Editor: `Services/Inspector/InspectorModule`; `SubEditor/Material|Scene` + viewport clients; plan `docs/ai/Editor/EDITOR_FILE_LAYOUT_MIGRATION.md`.
	Runtime: all `*Loader` → `Runtime/Resource/Loaders/`; merge `MaterialAssetLoader` into `MaterialLoader::Load`; new `ShaderLoader`; `TextureCubeLoader` unchanged in `Render/`.
- Risks or caveats:
	`MaterialEditor::OpenSession` may still double-compile after `LoadAsset<Material>` (pre-existing).
- Validation done:
	`cmake --build minEngine/build --target minEngine Editor`; `--asset-manager-test` / `--material-ir-test` exit 0.
- Next step:
	Editor 目视 spot-check; optional `MaterialEditor` dedupe compile.

### 2026-05-25 - Seed EngineDefault BasicShapes into MyMEProject
- Goal:
	Restore scene mesh GUID refs after stopping EngineDefault Registry scan.
- Main changes:
	Copied 7 meshes to `MyMEProject/Assets/Meshes/BasicShapes/` with relative `.meta` (GUID preserved); script `scripts/seed_basic_shapes_to_project.py`.
- Validation done:
	`cube`/`plane` GUIDs match `default.mescene` and `test.mescene` mesh references.
- Next step:
	Re-open project in Editor to `ScanAssets` register new paths (or rely on existing meta on disk).

### 2026-05-25 - Asset Pipeline 设计拍板 + 相对路径策略
- Goal:
	Freeze §10 decisions; resolve Engine vs Project asset roots and meta relative paths.
- Main changes:
	`ASSET_PIPELINE_DESIGN.md` → Status 已拍板; D8=NFD, D9=efsw, D12=registered-only; new §14 dual-root + relative `AssetPath`.
- Risks or caveats:
	P1b migration of existing absolute metas; remove Editor scan of EngineDefault from Registry.
- Validation done:
	User confirmed defaults + path strategy discussion.
- Next step:
	P1 code slice (type bucket + events + ImportAsset); P1b relative path + stop EngineDefault Registry scan.

### 2026-03-26 - Playground player control enhancement
- Goal:
	Add vertical flight and improve camera control experience in Playground.
- Main changes:
	Added IA_UpAndDown for E/Q vertical movement.
	Added IA_Look camera rotation logic with pitch clamp and sensitivity.
	Added wheel input support path via WindowSystem/GLFWWindowSystem/InputSystem and mapped MouseScroll in Playground.
	Fixed horizontal look direction sign to match expected user control direction.
- Risks or caveats:
	Mouse2D and MouseScroll semantics are still evolving between engine-side abstraction and gameplay-side handling.
	Future refactor may separate event-like input and state-like input more clearly.
- Validation done:
	CMake build completed successfully after control/input changes.
	Manual interaction checks performed in Playground for movement and look direction.
- Next step:
	Standardize mouse delta semantics at one layer only (engine or gameplay) to avoid double-delta bugs.

### 2026-04-06 - Scene serialization MVP pipeline wiring
- Goal:
	Wire a minimal end-to-end scene save/load loop for editor usage.
- Main changes:
	Added SceneSerializer with JSON save/load for scene name and game object transform list.
	Extended Scene object creation to support explicit object IDs for deterministic reload.
	Implemented SceneManager::CreateNewScene/LoadScene and default scene path resolution.
	Wired Editor File menu actions (New/Open/Save/Exit) to SceneManager and SceneSerializer.
	Created default editor scene on startup for immediate save/load testing.
- Risks or caveats:
	Current MVP only serializes scene name, object id, and transform; component graph is not serialized yet.
	Open Scene currently uses a fixed default path and has no file dialog.
- Validation done:
	File-level diagnostics passed for all touched scene/editor source files.
- Next step:
	Add component type registry and polymorphic component serialization/deserialization.

### 2026-04-10 - MEReflection parallel migration start
- Goal:
	Introduce a safer migration path by running new reflection flow in parallel with legacy reflection.
- Main changes:
	Added MEReflection alongside legacy reflection system.
	Prepared architecture boundaries for incremental migration instead of one-shot replacement.
- Risks or caveats:
	Dual-stack reflection can drift if registration/finalization behavior diverges.
- Validation done:
	Integrated into branch history and enabled follow-up serializer/reflection work.
- Next step:
	Consolidate registration/finalization ownership and remove duplicate paths gradually.

### 2026-04-12 - Reflection finalization reliability and GameObject fix
- Goal:
	Restore reflection correctness and unblock dependent serialization/editor features.
- Main changes:
	Added reflection finalization into engine startup flow.
	Added reflection finalization error logs to surface missing registration issues.
	Fixed GameObject-related reflection path issues and generated required reflection artifacts.
- Risks or caveats:
	Generated code order and startup timing still require strict consistency.
- Validation done:
	Reflection path recovered enough to continue pointer/serializer feature work.
- Next step:
	Continue serializer refactor with stronger pointer handling and field coverage.

### 2026-04-14 - Serializer pointer path, enum codec, inspector recovery
- Goal:
	Expand serialization coverage to component graphs and reconnect editor inspection flow.
- Main changes:
	Implemented pointer deserialization baseline in Serializer.
	Added primitive codec registry with MEEnum support.
	Successfully serialized a GameObject containing owned inline shared_ptr<Component> arrays.
	Reenabled Inspector for GameObject and component fields.
- Risks or caveats:
	Non-owned shared_ptr<MEObject> reference serialization is not finished.
	Pointer ownership/memory behavior still needs hardening in edge cases.
- Validation done:
	Commit notes confirm end-to-end success for owned component array scenario.
- Next step:
	Implement reference-mode object identity path and resolve semantics for external object references.

### 2026-04-16 - Asset metadata and GUID foundation (WIP)
- Goal:
	Prepare asset identity and registry infrastructure for future asset database workflow.
- Main changes:
	Added GUID type and GenerateGUID utility.
	Introduced AssetMeta (path/type/guid) and AssetManager in-memory registry map.
	Added AssetManager::ScanAssets and RegisterAsset initial flow.
	Added Texture2DResource and generated reflection header for resource-level metadata serialization prep.
	Refactored editor default-scene population helper and aligned serializer callsites with updated parameter flow.
- Risks or caveats:
	.meta persistence is still TODO; GUID reuse/load policy is not finalized.
	Current workspace includes uncommitted WIP changes and may still need integration fixes.
- Validation done:
	Local scope and file topology are in place; full fresh build validation has not been recorded yet for this WIP batch.
- Next step:
	Implement meta read/write + GUID reuse strategy, then run full build and editor open/save smoke test.

### 2026-04-17 - ObjectPtr probe-based GUID reference path
- Goal:
	Continue serializer evolution after ObjectManager became the central owner of runtime objects.
- Main changes:
	Added GUID reference node interfaces to Archive/JsonArchive (`BeginGuidRef`/`EndGuidRef`).
	Implemented probe-based object pointer deserialization in Serializer: try inline object node first, then GUID reference node.
	Implemented direct-Outer ownership rule in object pointer serialization: direct child uses inline serialization, otherwise writes GUID reference.
	Hooked GUID reference resolve chain in deserialization: ObjectManager first, then AssetManager fallback for known asset types.
	When deserializing inline MEObject-derived pointers, assign Class/Outer and register into ObjectManager.
- Risks or caveats:
	Current object-pointer serializer path still assumes MEObject-derived pointer property types for full ownership semantics.
	Asset fallback currently relies on known class-name mapping (`StaticMesh`/`Texture2D`) and may need extension once more resource wrapper types are reflected.
- Validation done:
	File-level diagnostics passed for touched serialization files (Archive/JsonArchive/Serializer).
- Next step:
	Add a focused round-trip case for mixed inline + GUID references (including cross-owner references) to lock behavior.

### 2026-04-18 - Scene vector ownership serialization alignment
- Goal:
	Align scene save/load flow with vector-owned GameObject data and non-persistent runtime IDs.
- Main changes:
	Switched Scene tick path to iterate m_GameObjects vector (ownership source of truth).
	Added Scene::RebuildRuntimeGameObjectIndex to compact null entries, rebuild id->pointer query map, and reassign runtime GameObject IDs after load.
	Refactored SceneSerializer to serialize/deserialize Scene directly through Serializer (sceneData payload) and removed persisted per-GameObject id fields.
	Added Scene reflection fields for sceneName and m_GameObjects in generated Scene.gen.h so direct Scene serialization includes required data.
	Kept pending object reference workflow intact: clear queue before load, deserialize scene, rebuild runtime index, resolve pending refs at load-unit end.
- Risks or caveats:
	Scene file format changed to version 2 and currently follows forward-only path (no legacy id-based scene schema compatibility).
	Pylance/Problems diagnostics for SceneSerializer still showed stale legacy entries in this session despite file content update; no legacy symbols remain in source file.
- Validation done:
	File-level diagnostics passed for Scene.h, Scene.cpp, Scene.gen.h.
	Source grep confirmed legacy id-serialization symbols were removed from SceneSerializer.cpp.
- Next step:
	Run editor-level save/load smoke test with at least one cross-GameObject reference case to verify runtime id remap and pending reference resolve behavior end-to-end.

### 2026-04-18 - Editor Scene API alignment follow-up
- Goal:
	Migrate editor-side scene/gameobject queries to the new Scene storage model and encapsulated GameObject ID accessor.
- Main changes:
	Updated Editor::GetHierarchyGameObjects to read from Scene::GetGameObjects() and sort by GameObject::GetID().
	Updated Editor::GetSelectedGameObject / RenameGameObject to use vector scan by GetID instead of old map-style find on m_GameObjects.
	Updated Editor::SyncSelectionWithScene to use Scene::GetGameObjectsById() for existence check and fallback to hierarchy list front item.
	Updated HierarchyWindow and InspectorWindow to use GameObject::GetID() instead of direct m_ID field access.
- Risks or caveats:
	Current editor selection lookup by ID is linear over scene gameobject vector when shared_ptr ownership is needed.
	Pylance diagnostics may lag in this session; source-level grep confirms old m_ID and old m_GameObjects.find usage in editor code paths were removed.
- Validation done:
	File contents verified for Editor.cpp / HierarchyWindow.h / InspectorWindow.h with new Scene/GameObject access pattern.
- Next step:
	Decide whether to introduce non-owning raw pointer query APIs for hierarchy/selection to reduce shared_ptr churn in per-frame UI code.

### 2026-04-18 - Editor non-owning raw pointer query pass
- Goal:
	Adopt non-owning raw pointer query APIs for per-frame editor scene inspection paths.
- Main changes:
	Changed Editor API signatures: GetActiveScene -> Scene*, GetHierarchyGameObjects -> vector<GameObject*>, GetSelectedGameObject -> GameObject*.
	Updated Editor.cpp callsites accordingly, while preserving Scene/SceneManager ownership via shared_ptr internally.
	Updated HierarchyWindow and InspectorWindow to consume raw pointer query results.
	Selection and rename lookups now use Scene::GetGameObjectsById() map for direct ID lookup.
- Risks or caveats:
	Returned raw pointers are non-owning and only valid while scene ownership remains stable in current frame.
	Callers must not cache returned pointers across scene reload/destruction events.
- Validation done:
	File-level diagnostics passed for Editor.h, Editor.cpp, HierarchyWindow.h, InspectorWindow.h, MainMenuWindow.h.
- Next step:
	Optionally add naming convention docs (e.g., *Raw suffix) if both owning and non-owning query APIs coexist later.

### 2026-04-18 - Reflection derived-check centralization
- Goal:
	Eliminate duplicated ad-hoc inheritance checks in serialization paths by adding a reusable reflection-level API.
- Main changes:
	Added `MEClass::IsA(const MEClass*)` as the canonical class ancestry query (self match included by default).
	Added `ReflectionSystem::IsClassSameOrDerived` and `ReflectionSystem::IsClassNameSameOrDerived` utility methods.
	Refactored Serializer to remove local `IsClassSameOrDerived` helper and use `ReflectionSystem` APIs.
	Refactored JsonArchive object/object-ptr type checks to use reflection system class-name derived matching instead of direct-child-only scans.
- Risks or caveats:
	Type-name checks now require reflected type names to be registered in `ReflectionSystem`; unresolved names fail fast.
- Validation done:
	File-level diagnostics passed for MEClass.h, Reflection.h, Serializer.cpp, JsonArchive.cpp.
	Source grep confirmed serializer-local inheritance helper was removed.
- Next step:
	Re-run scene load smoke test and confirm no "neither inline object node nor GUID reference node" error on multi-level inheritance pointer fields.

### 2026-04-18 - Resource reference GUID stabilization and scene migration
- Goal:
	Fix unresolved asset references during scene deserialization by ensuring serialized resource GUIDs match asset meta GUIDs.
- Main changes:
	Updated AssetManager static-mesh loading path to normalize cache key/path consistently.
	Updated `LoadStaticMeshByMeta` and `LoadStaticMesh` to bind loaded `StaticMesh` runtime object GUID to asset meta GUID.
	Migrated `bin/Assets/Scenes/EditorDefault.scene.json` mesh GUID nodes from stale random values to current mesh meta GUIDs.
- Risks or caveats:
	If older scene files contain random runtime GUIDs that have no corresponding `.meta` entries, they still require migration.
- Validation done:
	Rebuilt CMake targets successfully (minEngine + Editor).
	Runtime log verification shows pending reference resolve pass `resolved=5, unresolved=0`.
- Next step:
	Optionally add an automatic scene GUID migration pass for legacy files with unresolved asset GUID references.

### 2026-04-18 - ProjectManager skeleton bootstrap
- Goal:
	Start project-system architecture separation by introducing a dedicated `ProjectManager` subsystem scaffold.
- Main changes:
	Added `Runtime/Function/Framework/Project/ProjectManager.h/.cpp` with empty open/close interfaces and baseline data-model structs (`ProjectDescriptor`, `ProjectContext`, `ProjectOpenResult`).
	Added project file naming constants for new convention: `.meproject` and `.measset`.
	Added startup-scene resolution interface with engine-default fallback path placeholder.
	Registered `ProjectManager` in `RuntimeGlobalContext` startup/shutdown lifecycle.
- Risks or caveats:
	Current `OpenProject` behavior is intentionally non-functional (`NotImplemented`) to keep this iteration architecture-only.
	No runtime feature wiring to editor startup path yet.
- Validation done:
	File-level diagnostics passed for new and updated framework files.
	No build/run step executed in this task (per user request).
- Next step:
	Implement minimal project discovery/load flow and wire editor startup to `ProjectManager` scene resolution chain.

### 2026-04-18 - ProjectManager descriptor split and open-flow implementation
- Goal:
	Move project metadata into a dedicated header and replace `OpenProject` placeholder logic with real descriptor loading/validation.
- Main changes:
	Split project metadata into `Runtime/Function/Framework/Project/ProjectDescriptor.h`.
	Added `ProjectDescriptorSerializer.h/.cpp` to load/save descriptor files through the generic archive layer (`JsonArchive` read/write APIs).
	Implemented `ProjectManager::OpenProject` flow: normalize root, locate descriptor, deserialize, validate required fields, fill defaults (`Assets` / `Config`), resolve startup scene with engine-default fallback and diagnostics.
	Moved `ProjectManager::Get()` implementation to cpp, decoupling header from direct `RuntimeGlobalContext` include.
- Risks or caveats:
	Descriptor format is now strict on required fields (`schemaVersion`, `projectName`, `projectId`).
	Startup-scene fallback currently records diagnostics but does not yet emit UI-level notifications.
- Validation done:
	File-level diagnostics passed for `ProjectDescriptor.h`, `ProjectDescriptorSerializer.h/.cpp`, `ProjectManager.h/.cpp`.
	No build/run step executed in this task (per user request).
- Next step:
	Wire editor startup bootstrap to call `ProjectManager::OpenProject`, then consume `ResolveEditorStartupScenePath()` as the scene-entry source.

### 2026-05-04 - Spot/Point shadow pass MVP
- Goal:
	Add shadow rendering support for spot lights and point lights without changing the base pass sampling yet.
- Main changes:
	Added spot/point shadow requests and draw command building paths.
	Implemented spot and point shadow rendering in ShadowPass, including cube face rendering for point lights.
	Added spot 2D depth and point cube depth resource allocation in ShadowResourceManager.
	Extended OpenGL cubemap creation to respect depth texture formats.
- Risks or caveats:
	Spot/point shadow resources are single-instance MVP allocations; multiple shadow-casting lights will overwrite in this stage.
	Point light shadow range uses a fixed near/far plane; sampling path must match this later.
- Validation done:
	No build/run step executed in this task.
- Next step:
	Wire base pass sampling for spot and point shadows and pass down required parameters (shadow map index, far plane).

### 2026-05-04 - BasePass shadow sampling (spot/point)
- Goal:
	Connect spot and point shadow maps into BasePass sampling with a fixed resource limit of two each.
- Main changes:
	Added spot/point shadow sampler arrays and view-projection UBO in the Phong shader.
	Bound spot/point shadow textures in BasePass and wired shadow indices through light Params.w.
	Added spot view-projection UBO updates and point shadow far-plane data in light UBO updates.
	Made point-shadow cubemap depth linear in shadow pass for correct distance comparisons.
- Risks or caveats:
	Shadow resource maps are capped at two per light type; extra shadow-casting lights skip shadows.
	Point/spot shadow near/far planes are fixed constants; keep sampling logic consistent if they change.
- Validation done:
	No build/run step executed in this task.
- Next step:
	Run an editor scene with multiple spot/point lights to verify shadow indexing and bias tuning.

### 2026-05-08 - Material IR MVP pipeline bootstrap
- Goal:
	Stand up a minimal MaterialEdGraph -> MIR -> GLSL flow with a startup test dump.
- Main changes:
	Added MIR graph/value/node data structures and literal value support for float/vector constants.
	Implemented MIRBuilder with caching, binary ops, texture parameter, and texture sampling support.
	Expanded material node defs (constant2, multiply, texture param/sample) and wired Add/Multiply builds.
	Extended GLSL compiler with texture uniform tracking and texture() sampling emission.
	Added MaterialIR test helpers and editor startup logging for IR dump and GLSL output.
	Fixed editor graph pin ownership to allow graph connections in tests.
- Risks or caveats:
	No constant folding, dead code elimination, or stage separation yet; scalar-only type promotion.
	Texture sampling uses a simple sampler2D uniform and constant UVs in the test graph.
- Validation done:
	Not run (log-based test added; no runtime shader compile executed).
- Next step:
	Add basic diagnostics surfaced in MaterialCompileResult and extend node set (lerp, params).

### 2026-05-08 - MaterialOutput Albedo wiring
- Goal:
	Force compilation through a MaterialOutput node and map Albedo to MIR/GLSL output.
- Main changes:
	Added MaterialOutput node and wired Albedo into MIRGraph outputs.
	Enforced compile entry to be MaterialOutput only and removed arbitrary node compile path.
	Updated MVP test graph to end in MaterialOutput and compile through that entry.
	GLSL output now names Albedo explicitly for the final fragment color.
- Risks or caveats:
	Only Albedo is supported; other material properties are ignored.
	Missing MaterialOutput now hard-fails compilation.
- Validation done:
	Not run (startup log is available when the editor launches).
- Next step:
	Add Emissive/Opacity outputs or wire defaults for a fuller unlit MVP.

### 2026-05-19 - Material IR P8 texture and scalar parameters
- Goal:
	Compile texture sampling and scalar uniforms through MIR to GLSL (minimal P8).
- Main changes:
	Added MIR instructions ExternalInput, TextureObject, TextureRead, UniformParameter.
	MIREmitter: ExternalInput(TexCoord0), TextureObject, TextureSample, UniformScalar; Cast float4->float3 via subscripts.
	MIRToGLSLTranslator: shader preamble (sampler2D, in vec2 v_TexCoord0, uniform float), texture() lowering.
	Graph nodes: TextureCoordinate, TextureObject, TextureSample (RGBA+RGB outputs), ScalarParameter.
	Smoke test uses texture->Albedo, uniform->Metallic, constant Emissive.
- Validation done:
	Editor.exe --material-ir-test exit 0.
- Next step:
	S2 cross-stage TexCoords (UE-style ExternalInput -> MaterialParameters.TexCoords), then runtime bridge / P9 DefaultLit.

### 2026-05-19 - Material compile S0+S1 (multi-stage contract + FragmentMaterialInputs)
- Goal:
	UE-aligned compile contract: per-stage MIR lowering, ShadingModel assembler, FragmentMaterialInputs naming.
- Main changes:
	MaterialCompileTypes.h: MaterialCompileEnvironment, MaterialCompiledShader (Stages, FullVertex/FragmentShader).
	MaterialCompiler::Compile(graph, translator, env); MaterialShaderAssembler + UnlitShadingModel.
	MIRToGLSLTranslator lowers Vertex+Fragment bodies; FragColor only in Unlit assembler.
	FragmentMaterialInputs.Albedo etc.; MP_WorldPositionOffset placeholder + MaterialShadingPropertyCount.
	SetMaterialOutput registers outputs per MaterialPropertyEvaluatesInStage (UE-style).
	Removed MaterialCompileResult; smoke tests assert stage body + full shaders.
- Validation done:
	cmake build minEngine + Editor; Editor.exe --material-ir-test exit 0.
- Next step:
	Runtime bridge (compile to Shader + viewport) or DefaultLit shading model.

### 2026-05-19 - Material compile S2 UE-style MaterialParameters.TexCoords
- Goal:
	Cross-stage UV like UE: ExternalInput lowers to MaterialParameters.TexCoords[N]; assembler fills/interpolates.
- Main changes:
	MaterialShaderParameters.h: TexCoords access + v_MaterialTexCoordN varying names.
	Unlit assembler: vertex sets MaterialParameters from a_TexCoord, fragment restores from varying.
	MIRToGLSLTranslator: texture() uses MaterialParameters.TexCoords[0] (not v_TexCoord0 in material body).
	MaterialIRTest: logs full shaders; writes Saved/Materials/GeneratedVertex.glsl and GeneratedFragment.glsl.
- Validation done:
	Editor.exe --material-ir-test exit 0.

### Material MIR – roadmap snapshot (Path A, for planning)
- **P9 (shading):** Replace unlit `FragColor = vec4(Albedo + Emissive, Opacity)` with minimal lit shading that consumes Metallic/Roughness (minimal metallic-roughness BRDF or interim Blinn-Phong); extend smoke asserts for lighting-related GLSL.
- **Runtime bridge (often before or overlapping P9):** Vertex stage `v_TexCoord0` from mesh UV; compile `MaterialEdGraph` → `Shader` at load/startup; `Material` asset holds graph + compiled shader; BasePass binds `u_TextureN` / scalar uniforms from asset; assign material on scene mesh — **first viewport-visible custom material without graph UI**.
- **Editor graph UI (parallel track):** Visual material editor, pin wiring, recompile-on-save, preview mesh — **“真正在编辑器里改图看效果”** depends on this plus runtime bridge.
- **P10 (polish):** Vertex/custom interpolants beyond UV, `TrySimplifyOperator`, boolN Select, `Step_Finalize`, float2/float4 expansion, etc.

## Deferred Reminders (for future sessions)

### 2026-05-19 - Material R1: C1a vertex transform + BasePass graph branch
- Goal:
	Viewport-visible compiled graph unlit materials (R1).
- Main changes:
	Unlit assembler: PerFrameData + u_Model + ViewProj * u_Model * vec4(a_Position,1).
	Material: m_bUsesCompiledGraph, m_GraphTextureSlots, m_GraphScalarParams, BindCompiledGraph().
	BasePass: legacy Phong vs compiled graph binding split.
	Editor smoke scene: graph binding + SyncSceneToRenderPipeline.
	MaterialIRTest: vertex transform assertions.
- Validation done:
	cmake build Editor; --material-ir-test exit 0.
- Next step:
	R2 polish / R3 or visual confirm in editor viewport.

### 2026-05-19 - Shader asset API + editor material smoke scene
- Goal:
	Converge file→GPU shader on Shader class; editor startup active scene with MIR smoke material quad.
- Main changes:
	Removed ShaderSourceIO; Shader.cpp owns ReadSourceFile, CompileFromFiles, CreateFromSource, TryCompileSourcesOnGpu, EngineShaderPath.
	MaterialSmokeGraph shared by MaterialIRTest and EditorDefaultScene.
	SetEditorMaterialIRSmokeActiveScene() after OpenProject; quad + camera + light.
	Fixed GameObject::AddComponent return type (return newComponentBase).
	Reverted Playground shader changes (legacy, BUILD_PLAYGROUND OFF).
- Validation done:
	cmake build Editor; --material-ir-test exit 0.

### 2026-05-19 - Material R0: RHIShader source compile + GPU test
- Goal:
	R0: RHIShader accepts GLSL source strings; file IO at AssetManager; GPU compile smoke test.
- Main changes:
	OpenGLShader(vertexSource, fragmentSource) with IsValid/GetCompileLog; CreateRHIShader returns nullptr on failure.
	ShaderSourceIO: ReadShaderSourceFile, CreateRHIShaderFromFiles, TryCompileShaderSourcesOnGpu, EngineShaderPath.
	AssetManager/PresentPass/ShadowPass/RenderPipeline/Playground use file read + source API (no paths in RHI).
	MaterialIRTest: headless GL context when needed; GPU compile/link assert on smoke shaders.
- Validation done:
	cmake build Editor; Editor.exe --material-ir-test exit 0 (includes GPU compile PASSED).
- Next step:
	R1 C1a Unlit vertex PerFrameData + BasePass graph-unlit branch.

### 2026-05-19 - Material runtime bridge checklist
- Goal:
	Document GPU/viewport integration path after S0–S2 compile pipeline.
- Main changes:
	Added docs/ai/MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md (layer checklist A–E, risks R1–R7, phases R0–R4, PR split).
- Next step:
	User confirm C1a (Unlit VS + PerFrameData/u_Model); implement R0 GPU compile test then R1 BasePass branch.

### Material compiler – shader formatting (deferred)
- **Indentation:** Generated GLSL stage body / main blocks use inconsistent leading whitespace; normalize in Assembler or MIRGLSLPrinter when convenient (not blocking compile or tests).

### Material IR – cross-stage modeling (locked for S2, UE-aligned)
- **Decision (2026-05-19):** No separate MaterialInterpolator IR. Default UV = `ExternalInput(TexCoordN)` lowered to `MaterialParameters.TexCoords[N]`; vertex pass-through + varying owned by Assembler/模板. User-modified UV later via `MP_CustomizedUVs*` (vertex MaterialProperty), like UE.
- **S2 scope:** Replace translator `v_TexCoord0` with Parameters naming; vert fills TexCoords from `a_TexCoord`.

### Material IR – texture sampling variants (post-P8)
- **When to surface:** After runtime can display at least one compiled material with `texture()` in the viewport (see P9 / runtime-bridge milestones below), or when a graph needs non-default mip/grad behavior.
- **What (incremental, UE-aligned):**
  - `TextureSampleLevel` → GLSL `textureLod` (+ mip level input on node)
  - `TextureSampleBias` → `texture(..., bias)`
  - `TextureSampleGrad` → `textureGrad` (+ ddx/ddy inputs)
  - `TextureGather`, Cube/Volume/Array object types (later)
  - Sampler state (wrap/filter) separate from texture asset
  - UE-style stage switch (hardware vs analytic gradient path) — only if needed
- **Already supported (do not re-implement):** multiple textures via `TextureSlotIndex` (`u_Texture0`, `u_Texture1`, …); single 2D path `MIRTextureRead` → `texture(sampler2D, vec2)`.
- **Why deferred (2026-05-19):** P8 goal was one end-to-end 2D sample path + uniforms; more modes add MIR/GLSL/node/test surface before Editor/GPU binding exists.
- **Suggested order:** multi-slot smoke → Level → Bias/Grad → Gather / exotic types → sampler metadata.

### Material editor – vector node pin expansion (UE-style)
- **Done (E4, 2026-05-19):** `MaterialGraphNodeDef_Constant3` 增加 Value/R/G/B 四个 output；`BuildIR` 用 `SubscriptChannel`；编辑器多引脚 UI 已随 `m_Outputs` 显示。
- **Still deferred:** `MakeFloat3` 等其它向量节点多引脚；见 `MATERIAL_EDITOR_PLAN.md` 后续迭代。

### Material asset file round-trip (Instanced graph)
- **2026-05-21:** `EditorGraph` / `EditorGraphNode` → `MEObject`; `MaterialEdGraph` / `MaterialEdGraphNode` inherit; `Material::m_Graph` → `shared_ptr` + `ME_PROPERTY(Instanced)`.
- **Test:** `RunMaterialAssetSerializationTests()` writes `%TEMP%/minengine_material_asset_roundtrip.memtl`, `ToFile` → `FromFile` → `FinalizeGraphAfterLoad`; checks inline JSON types, editor fields, Metallic link, Outer chain.
- **CLI:** `Editor.exe --material-serialize-test` (serialize only); `--material-ir-test` includes file round-trip at end.

### Material system Phase 1 design (2026-05-22)
- **Plan:** `docs/ai/MATERIAL_SYSTEM_PHASE1.md` — P1.1 Capability+Blend, P1.2 Normal, P1.3 AO, P1.4 nodes; Translucent deferred to Phase 2.
- **Next implement:** P1.1 recommended first.

### Bug: MATERIAL-001 Constant3 → Normal GLSL lowering crash (2026-05-23) — **Fixed**
- **Doc:** `docs/ai/bugs/MATERIAL-001-constant3-normal-glsl-lowering.md`；管线审查 `docs/ai/MATERIAL_PIPELINE_REVIEW.md`
- **Fix:** live-reachability `NumUsers`；`Constant3`/`TextureSample` 按需子 output；GLSL lowering diagnostic + foldable multi-use inline；`VerifyConstant3ToNormalBlinnPhong` in `--material-ir-test`.
- **Also:** `ME_ASSERT` 现输出 message 到 stderr。

### Material system Phase 3 accepted + Phase 4 IBL design (2026-05-23)
- **P3 done:** `MaterialShadingModel::PBR`、GGX、`MaterialPBR.glslinc`、`PBR.*.template`、ComponentMask、TextureSample.R、BlinnPhong Roughness；`VerifyPBRWorkflow`；用户目视 OK。
- **Docs:** [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) checklist ✅；[MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) **IBL only**（split-sum、三贴图 + BRDF LUT）。
- **Deferred:** Parallax、WPO、编辑器 Undo、可选 `MaterialIRSmoke_PBR.memtl`。
- **Next:** 用户更高优先级非材质工作；实现 P4 时从 P4.1 环境 cubemap 加载 + Pass 绑定开始。

### Material system P2.3 Normal map workflow (2026-05-23)
- **Done:** `MaterialGraphNodeDef_NormalUnpack`（`rgb*2-1`）；调色板；`VerifyNormalMapWorkflow`。
- **Editor recipe:** TexCoord → TextureSample(Normal) → NormalUnpack → MaterialOutput.Normal（见 PHASE2 §5.1）。
- **Next:** 目视 + 可选 `MaterialIRSmoke_NormalMap.memtl` 金样例。

### Material system Phase 4 IBL — closed (2026-05-23)
- **P4.1–P4.4:** `EngineIBLEnvironment` + unit 4–6 bind（`pipeline` 修复）；`MaterialIBL.glslinc` / `CalcIndirectPBR`；移除常数 ambient；`u_EnvIntensity`。
- **R2:** `EnvMapCapture` HDR → mipmapped cubemap；`brdf_lut.png` 或 `BrdfLutGenerator`。
- **Test:** `--material-ir-test`（PBR IBL shader、环境 init、missing-assets fallback）。
- **Doc:** [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) §5 目视指南；IBL README。
- **Deferred:** irradiance 卷积、专用 prefilter pass、skybox、Translucent IBL。

### Material system P2.2 Tangent + TBN (2026-05-23)
- **Done:** `Vertex` + `a_Tangent`（Assimp `CalcTangentSpace` + fallback）；`UsesTangentFrame`；`MaterialTangentFrame.glslinc`；BlinnPhong TBN 路径；默认 Normal TSN `(0,0,1)`。
- **Next:** P2.2 目视；P2.3 `NormalUnpack` + 法线贴图节点。

### IBL load timing owned by RenderSystem (2026-05-23)
- **Change:** Removed `EngineIBLEnvironment::Initialize(rhi, "")` from `RenderPipeline::Initialize`; single load via `RenderSystem::LoadEngineRenderingAssets()` after `PathRegistry` in `Engine::Initialize`.
- **Removed:** `PathRegistry::ReloadEngineDependentSystems`; Editor duplicate config/IBL bootstrap.

### Platform M0 PathRegistry + engine startup (2026-05-23)
- **Code:** `Runtime/Core/Paths/PathRegistry` — discover `EngineConfig.meconfig` (cwd, parent walk, CLI, env); relative `EngineDefaultAssetsRoot`; `ProjectManager` sets `ProjectContent`.
- **Config:** `EngineConfig.meconfig` → `"Assets/EngineDefault"` relative to engine root (config file directory).
- **CLI:** `--engine-config=`, `--engine-root=`; env `MINENGINE_ENGINE_CONFIG`, `MINENGINE_ENGINE_ROOT`.
- **Validation:** build Editor; `Editor.exe --material-ir-test` from `minEngine/bin`.

### docs/ai reorg + Platform design drafts (2026-05-23)
- **Layout:** Material/Render docs → `docs/ai/Render/Material/`；`RENDER_REFACTOR`、`RESOURCE_PIPELINE` → `docs/ai/Render/`；Editor 视口 → `docs/ai/Editor/`；新增 `Platform/`、`README.md`、`.cursor/rules/docs-ai-layout.mdc`。
- **Design:** [PLATFORM_ROADMAP.md](./Platform/PLATFORM_ROADMAP.md)、[ENGINE_STARTUP_DESIGN.md](./Platform/Startup/ENGINE_STARTUP_DESIGN.md)、[MEMORY_MANAGEMENT_DESIGN.md](./Platform/MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md)（启动 M0 → **原地重构 ObjectManager** M1/M2）。
- **Direction:** UE-like；反射/Lua/Content Browser/Undo 排在平台线之后。

### Material system Phase 5 P5.3 Skybox (2026-05-23)
- **Done:** `SkyBoxComponent` + `SkyBoxSceneProxy` + `SkyBoxPass`（`background.*`）；`RenderPipeline` Clear → SkyBox → BasePass；`SceneDrawFlags::EnableSkyBox`；编辑器场景视口启用 flag；`test.mescene` SkyBox GO。
- **Fix:** `SceneEditingViewportClient` 未传 `EnableSkyBox` 导致天空不绘制。
- **Validation:** Editor `test` 场景目视 OK；`cmake --build Editor`；`--material-ir-test`（与 P5.1–P5.2 同批）。
- **Docs:** [MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md) §6.7 勾选；§10 6a–6c/7 ✅。

### Material system P2.1 Translucent (2026-05-23)
- **Done:** `MaterialBlendMode::Translucent`；`Material::IsTranslucent()`；Capability Opacity；Details **Translucent**；`--material-ir-test`。
- **Fix:** `TranslucencyPass` 与 `BasePass` 一致绑定 `LightsData` + shadow（BlinnPhong Translucent 不再全黑）。
- **Next:** P2.1 目视验收；P2.2 切线 + TBN。

### Material system Phase 1 accepted (2026-05-23)
- **Status:** P1.1–P1.4 目视验收通过；[MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md) checklist 已全部勾选。
- **Automation:** `--material-ir-test`（Unlit/BlinnPhong、Constant3→Normal、IfThenElse、Texture 双属性、divide poison、capability struct）。
- **Next:** **Phase 2.1** — `MaterialBlendMode::Translucent` + `IsTranslucent()` + `TranslucencyPass`（见 [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md) §3）。

### Material system P1.2–P1.4 implemented (2026-05-22)
- **Done:** `MP_Normal` + `EI_WorldNormal` 默认、`MP_AO`、BlinnPhong 模板用 `FragmentMaterialInputs.Normal/AO`；`IsPropertyEmittedAtCompile`（Unlit 不生成 Hidden 属性）；节点 Lerp/Subtract/Divide/Min/Max 可创建 + Lerp `Alpha` 类型；`MaterialIRSmoke.memtl` MaterialOutput pin 顺序迁移；`--material-ir-test` Unlit + BlinnPhong GPU 双路径。

### Material system P1.1 + Phase 2 design (2026-05-22)
- **Done:** P1.1 `MaterialBlendMode` (Opaque/Masked), `MaterialCapabilityUtil`, editor Blend Mode, Masked `discard`, golden `MaterialIRSmoke.memtl` + `m_BlendMode`, on-disk / IR smoke 字段校验。
- **Plan:** `docs/ai/MATERIAL_SYSTEM_PHASE2.md` — P2.1 Translucent+Pass, P2.2 TBN+tangent, P2.3 normal map; Masked 不再重复。

### Material system Phase 0 implemented (2026-05-22)
- **P0-A:** Preview camera `eye(1.15, 0.8, 1.15)` in `MaterialPreviewViewport`.
- **P0-B:** `MaterialValueType` + `MaterialValueTypeUtil`；`MaterialEdGraph::CanConnectPins`；Editor `TryConnectPins` 拒绝非法连线；`ValidateMaterialAsset` 校验；`--material-ir-test` 增加 pin 测试 + `MaterialIRTestObjectManagerScope`（friend `ObjectManager`）。
- **Next:** Phase 1 详细设计（Normal、Capability、少量节点）；`MaterialIRSmoke.memtl` Editor 目视回归。

### Material system roadmap — Phase 0 spec (2026-05-22)
- **Plan:** `docs/ai/MATERIAL_SYSTEM_ROADMAP.md`

### Material Editor E0–E4 plan sign-off (2026-05-22)
- **Plan:** `MATERIAL_EDITOR_PLAN.md` 验收勾选已全部完成（E0–E4）；用户目视 + `--material-ir-test` 确认。
- **Deferred（计划外）:** 画布右键 Palette；独立 `Material*View` 分层；Undo/Command 队列；Content Browser 双击打开。

### Material Editor E0–E2 — UI mode + Preview + node graph (2026-05-19 ~ 2026-05-22)
- **EditorUIMode:** `SceneEditing` ↔ `MaterialEditing`；`EditorWindowSuite`（Shared / Scene / Material）；切换时重建 Dock。
- **Material 套件：** `MaterialGraphWindow`（右，Picker/Compile/Save + node-editor 画布）、`MaterialPreviewWindow`、`MaterialDetailsWindow`。
- **MaterialEditor：** Session/命令中枢（非 Window）；`OpenSession` / `Save` / `Compile` / `NotifyGraphChanged`；`InvalidateGraphCanvas()` 通知图窗刷新。
- **E1.5：** `MaterialEditorPreview` 拥有预览世界；`MaterialPreviewViewportClient` 仅 resize + Submit；ImGui 在 `*Window` 内（无独立 `*View` 层）。
- **E2（已验收）：** imgui-node-editor；`MaterialGraphIds`（Node/Pin/Link 分 tag，避免与 NodeId 冲突）；`MaterialGraphNodeRegistry`；连线/断线 → `ConnectPins` / `DisconnectInput`；全零坐标自动网格布局；仅 Invalidate 时 `SetNodePosition`（可拖节点）；`BeginCreate` 失败也须 `EndCreate`。
- **渲染：** Scene 模式主视口 Submit；Material 模式仅 `material_editor_preview` Submit。
- **本地增量（未全部入库时以工作区为准）：** `Reflection::GetDerivedClasses` + `MEClass::IsA<T>()`（为 NodeDef 注册表/调色板铺路）；更多 `MaterialGraphNodeDef_*` 反射注册；`Runtime/Core/Hash/Hash.h`。
- **E3（2026-05-22）：** E3.1 Details Combo 加节点；E3.2 `Registry::DrawNode` 门面；E3.3 `MaterialNodeDefPropertyDrawer` 反射；E3.4 `RemoveNode` + 编辑器 Delete 节点（禁止删 MaterialOutput）。
- **E4（2026-05-19）：** `MaterialCompileDiagnosticsDrawer`；Compile debounce 0.3s + `MaterialEditor::Tick`；`Constant3` R/G/B 多输出引脚；退出 Material 模式时 `RemoveViewportClient` + Preview 仅 Material 模式 Submit。

### Material Editor plan + imgui-node-editor vendoring (2026-05-19)
- **Plan:** `docs/ai/MATERIAL_EDITOR_PLAN.md` — E0–E4；窗口布局 **左 Preview+Details / 右节点图**。
- **Third-Party:** `minEngine/Third-Party/imgui-node-editor/`（ImGui 1.92 `imgui_extra_math` patch）。

### Golden MaterialIRSmoke + Editor default scene
- **2026-05-21:** `MyMEProject/Assets/Materials/MaterialIRSmoke.memtl` (+ `.meta`) committed as golden IR smoke graph asset.
- **Editor:** `PopulateEditorDefaultScene` loads via `AssetManager::LoadAsset<Material>` then `ApplyMaterialIRSmokeRuntimeDefaults` (white BaseColor texture).
- **Tests:** serialize round-trip also verifies golden `.memtl` deserializes + `FinalizeGraphAfterLoad`.

### Material asset texture refs + test cleanup
- **2026-05-21:** `TextureObject.DefaultTexture` serializes as texture asset `$guid` (`BaseColorWhite.png`); removed `ApplyMaterialIRSmokeRuntimeDefaults`.
- **Deprecated:** `Simple.memtl` (legacy MaterialResource JSON); `test.mescene` / `default.mescene` reference `MaterialIRSmoke` GUID.
- **Tests:** removed `MaterialAssetSerializationTest` and `--material-serialize-test`; `MaterialIRTest` keeps MIR compile/GPU smoke only (asset tests TBD).

### 2026-05-26 - Platform ROADMAP §8 完成情况总览
- Goal:
	Sync PLATFORM_ROADMAP with design docs and repo state (P0–P5, P2 submodules, Material maintenance).
- Main changes:
	`PLATFORM_ROADMAP.md` §8 per-module done/deferred tables + summary; §2/§3/§5 updated (P0/P1 no longer “全后置”).
- Next step:
	E1 → P7 per §8 总结顺序.

### 2026-05-27 - ROADMAP 新任务安排（滚动）落盘
- Goal:
	Record today’s incremental task order aligned with ROADMAP, while leaving room for additional tasks.
- Main changes:
	`PLATFORM_ROADMAP.md` 新增 §10（A/B/C 三条主线）；新增 `docs/ai/Editor/EDITOR_TASK_ROLLOUT_2026-05-27.md`（推进切片 S1–S6）。
- Next step:
	按 S1 开始：右键 action 统一抽象 + Content Browser 先接入。

### 2026-05-27 - M1 收口：Reveal/Rename 暂缓（设计）
- Goal:
	Align design after trial: drop non-cross-platform Reveal and modal CB Rename.
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` §6.1、§12、§13、§15.4；rollout S1b 代码清理项。
- Next step:
	M2 Hierarchy + Inspector.

### 2026-05-27 - CB Import/Delete 全量刷新问题记录
- Doc:
	`docs/ai/Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md`；`CONTENT_BROWSER_DESIGN.md` 链接。
- Next:
	后续优化 AssetTreeModel 增量刷新 / 通知合并。

### 2026-05-27 - M1 代码清理（Reveal/Rename 移除）
- Main changes:
	删 `EditorPlatformShell`；CB Action 仅 Delete/Import/Refresh；移除 AssetWorkflow/ContentBrowser Rename 模态链。
- Next step:
	M2.

### 2026-05-27 - 右键菜单 M1（Content Browser）
- Goal:
	Scheme A visibility + CB context menu actions.
- Main changes:
	`IsVisibleInMenu`；`ContentBrowserMenuContext`；五 CB Action；CB 树/Tile/空白右键；`ImportAssetDialog(rel)`；`EditorPlatformShell` Reveal。
- Next step:
	M2 Hierarchy + Inspector.

### 2026-05-27 - 计划对齐（M0 Done / M1 待批）
- Goal:
	勾选 S0；设计 §15 实现对照；M1 审批项（菜单灰显 A/B）。
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` §13–§15；`EDITOR_TASK_ROLLOUT` 子任务表；`PLATFORM_ROADMAP` §10 备注。
- Next step:
	用户审批 §15.3 → M1 ContentBrowser。

### 2026-05-28 - TEST-F02 doctest, minEngineTests, Tests/ layout
- Goal:
	Move headless suites out of Runtime; vendored doctest; primary entry minEngineTests.exe.
- Main changes:
	`minEngine/Tests/` + `Third-Party/doctest/`; `Editor.exe test` forwards to minEngineTests;
	`scripts/verify.ps1` uses minEngineTests; one `TEST_CASE` per suite with CHECK.
- Validation done:
	`minEngineTests.exe test smoke` exit 0; `verify.ps1` exit 0.
- Next step:
	Optional: split large suites into finer doctest cases; per-suite runtime reset (reflection ordering).

### 2026-05-28 - TEST-F01 unified test runner (S01–S05)
- Goal:
	TestRunner + registry + TestContext; all five suites via `Editor.exe test`; verify.ps1.
- Main changes:
	`Runtime/Test/*`; `main.h` legacy pre-parse + TestRunner dispatch; removed `ShouldRun*` from `*Test.cpp`.
	`scripts/verify.ps1`; smoke order puts reflection-function first (in-process isolation).
- Validation done:
	`cmake --build minEngine/build --target Editor`; `test smoke`/`test material-ir`/legacy flags; `.\scripts\verify.ps1`.
- Next step:
	TEST-F02 — doctest + `minEngine/Tests/` + `minEngineTests.exe`.

### 2026-05-28 - TEST-F03 suite slim-down complete (S00–S06)
- Goal:
	Doctest cases per suite; fixture B; smoke/full tags; faster verify smoke.
- Main changes:
	All five suites split; `EngineReflectionFixture` / `EngineTestFixture`; Material IR smoke vs full;
	reflection meta/invoke smoke, types/static/ref full; active docs CLI strings updated.
- Validation done:
	`minEngineTests test smoke` exit 0.
- Next step:
	Optional: further split reflection/material cases; enable C3 FillOut when registered.

### 2026-05-28 - TEST-F03 S00–S01 fixture B + object-manager split
- Goal:
	Implement fixture B smoke order; split object-manager into doctest cases.
- Main changes:
	`EngineReflectionFixture` / `EngineTestFixture`; `DoctestSuiteRunner::RunSuiteForContext`;
	smoke order in `TestSuiteRegistry`; `ObjectManagerTest` → 3 `TEST_CASE`s; reflection A6/A7/B6 skip when already Ready.
- Validation done:
	`minEngineTests test object-manager`, `test reflection-function`, `test smoke` exit 0.
- Next step:
	F03-S02 serialization-archive split.

### 2026-05-28 - TEST-F03 suite slim-down plan (fixture B)
- Goal:
	Per-suite doctest cases; central reflection init; faster smoke; dependency-ordered registry.
- Main changes:
	`Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md`; Registry `TEST-F03` Planned; INFRASTRUCTURE/TEST_UNIFIED/BOOTSTRAP links.
- Validation done:
	Docs-only; implementation starts at F03-S00 (fixture + smoke order).
- Next step:
	F03-S00 validation gate, then F03-S01 object-manager split.

### 2026-05-28 - Legacy headless test flags removed (commit)
- Goal:
	Drop deprecated `--*-test` argv; align infra docs with minEngineTests primary UX.
- Main changes:
	`TestRunner`, `ApplicationCommandLine`, `main.h`, `TestMain`; TECH_DEBT TD-001–003 Done.
- Validation done:
	`minEngineTests.exe test smoke` exit 0.
- Next step:
	TEST-F03 planning (see entry above).

### 2026-05-28 - TEST-F01/F02 unified test runner design
- Goal:
	Design engine TestRunner (self-built) + doctest in F02; smoke/full tables; verify.ps1 path.
- Main changes:
	`Platform/Test/TEST_UNIFIED_DESIGN.md`, `TEST_F01_IMPLEMENTATION.md`, `TEST_F02_LAYOUT_MIGRATION.md`;
	Registry `TEST-F01`/`TEST-F02`; INFRASTRUCTURE_ROADMAP M1–M2 Done, M3–M4 planned.
- Validation done:
	Docs-only; user approved doctest + Editor-then-minEngineTests exe strategy.
- Next step:
	TEST-F01-S01 implementation.

### 2026-05-28 - CLI-F01 S01–S03 unified command-line parse + dispatch
- Goal:
	Introduce CLI11-backed `ApplicationCommandLine`; single `main.h` dispatch; migrate Material IR test to `test material-ir`.
- Main changes:
	`Third-Party/CLI11/CLI11.hpp` (v2.4.2); `Runtime/Core/CLI/*` (`CommandLineResult`, `ApplicationCommandLine`).
	`main.h` — parse first; `test material-ir` + legacy `--material-ir-test` alias (warn); other `--*-test` flags unchanged until TEST-F01.
	`PathRegistry::LoadEngineConfiguration(CommandLineResult)`; `Editor` project path from parsed result (removed argv scan loop).
- Validation done:
	`cmake --build minEngine/build --target minEngine Editor`.
	`Editor.exe --help`, `Editor.exe test --help`, `Editor.exe test material-ir` exit 0; `--material-ir-test` warns + exit 0.
- Next step:
	TEST-F01 — suite registry, migrate remaining four `--*-test` flags, `test smoke`/`full`.

### 2026-05-28 - CLI-F01 unified command-line design (approved)
- Goal:
	Design CLI: subcommand = mode, -- = params; CLI11 + ApplicationCommandLine.
- Main changes:
	`Platform/CLI/CLI_UNIFIED_DESIGN.md`, `CLI_F01_IMPLEMENTATION.md`; Registry/roadmap/README links; Status Planned.
- Validation done:
	User approved convention and command tree.
- Next step:
	CLI-F01-S01 implementation (vendor CLI11 + parse core).

### 2026-05-28 - Infrastructure roadmap (CLI · Test · Verify)
- Goal:
	Near-term plan: CLI unification, test centralization, verify + DoD; defer product features.
- Main changes:
	`Platform/INFRASTRUCTURE_ROADMAP.md`; Registry `CLI-F01`/`TEST-F01`; `PLATFORM_ROADMAP` §12; README/BOOTSTRAP/TECH_DEBT links.
- Validation done:
	Docs-only.
- Next step:
	`CLI-F01` Design Spec + S01 when implementation starts; commit batch B.

### 2026-05-28 - Bootstrap digest and tech debt register
- Goal:
	One-page session recovery + explicit debt queue before infra roadmap.
- Main changes:
	`BOOTSTRAP_DIGEST.md`; `TECH_DEBT.md` (TD-001..012); `README` links; session-bootstrap reads digest.
- Validation done:
	Docs-only.
- Next step:
	Commit batch A; then infra roadmap (CLI + Test + verify) — not in digest/debt files.

### 2026-05-28 - WF collaboration v2 (registry, Slice DoD, handoff)
- Goal:
	Team workflow: Feature registry, engineering+doc Slice DoD, session handoff; rules/skills wired.
- Main changes:
	`FEATURE_REGISTRY.md`; `DOC_GOVERNANCE` §7 Slice DoD, §9.1 Handoff, registry rule; `docs-workflow-triggers` handoff row; mentor + git-commit-mentor DoD; `WORKING_WITH_AI` handoff prompt.
- Validation done:
	Docs-only; no build required.
- Next step:
	Use registry for next Feature (e.g. TEST-F01 CLI); optional post-commit hook later.

### 2026-05-27 - P4/P5 Core 路线对齐（函数反射 + 委托 + Lua）
- Goal:
	Align platform roadmap with next master work: MEFunction, delegates, Lua scripting.
- Main changes:
	`PLATFORM_ROADMAP.md` §11 Core 主线；P4/P5 设计草稿 `Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md`、`Scripting/LUA_SCRIPTING_DESIGN.md`；`PROJECT_CONTEXT` / `README` 链接更新。
- Next step:
	P4.1 `MEFunction` 描述符 + 手写注册一个测试函数。

### 2026-05-27 - 右键菜单 M0 骨架落地
- Goal:
	EditorContextMenuSystem + Context/Registry/Builder shells; wire IEditorContext.
- Main changes:
	`minEngine/Editor/src/ContextMenu/`（EditorMenuContext、IEditorAction、EditorActionRegistry、EditorMenuBuilder、EditorContextMenuSystem）。
	`IEditorContext::GetContextMenu()`；`Editor` PostInitialize/OpenProject RegisterBuiltInActions、CloseProject/Shutdown Shutdown。
- Next step:
	M1 ContentBrowser 接入 + 首批 Action 注册。

### 2026-05-27 - 右键菜单设计 v2（归属 + 门面）
- Goal:
	Clarify Editor infrastructure ownership; single GetContextMenu() facade.
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` §1 归属与生命周期；`EditorContextMenuSystem` 组合 Registry/Builder；明确不继承 EditorServiceModule。
- Next step:
	User review v2 → M0 implementation.

### 2026-05-27 - 右键菜单设计重置（可扩展架构）
- Goal:
	Replace interim enum/service draft with UE-aligned extensible context-menu system.
- Main changes:
	重写 `EDITOR_CONTEXT_MENU_DESIGN.md`（EditorMenuContext 袋、IEditorAction、Registry、MenuBuilder、Command、M0–M5）。
	`docs/external/README.md`；rollout 更新 S0–S3。

### 2026-05-27 - 右键菜单统一设计草案（已废弃）
- 轻量 `EditorContextMenuService` + enum 方案已由上条重置版替代。

### 2026-05-26 - Content Browser P6.1-polish implemented
- Goal:
	Flat Caption breadcrumb + square icon tile grid per §2.6.
- Main changes:
	`ContentBrowserWindow` — `ViewMetrics`, `DrawBreadcrumbLink`, `DrawAssetTile`; Appearance Caption/Selection/Field tokens.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded.
- Next step:
	Editor 目视 Dark/Light；§2.6.4 主题项勾选。

### 2026-08-02 - master：合并 luaScript + physics；Transform quat → CORE-F03
- Goal:
	Integrate Lua scripting and Jolt physics on `master`; resolve Transform storage + Feature ID collision.
- Main changes:
	`luaScript` fast-forward → `master`；再 merge `physics`。
	`Transform::Rotation` = physics `Quaternion`；保留 Script\*（Position / MakeIdentity / SetPosition / Translate）。
	Physics 原 `CORE-F01` Transform Feature 改号为 **`CORE-F03`**（文档路径同步重命名）；Lua 保留 F01/F02。
	Engine/CMake/friends/test suites 合并两侧系统。
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`lua-script-mvp` + all physics suites + `smoke` → PASSED
- Next step:
	Delegate 系统设计（TD-006 / PHYS-F03）；需要时可准备正式 commit 说明。

### 2026-08-01 - CORE-F02 Done：Script binding codegen 收口
- Goal:
	Close Feature after S01–S07 vertical delivery.
- Main changes:
	Registry / Design / ACTIVE_WORK / Platform README / PROJECT_CONTEXT → Done.
	Out of Feature: broader GO API surface, Matrix/quat ME_STRUCT, weak handles.
- Validation done:
	Prior `lua-script-mvp` PASSED; docs-only closeout amend.
- Next step:
	Optional follow-ups on `luaScript`, or switch tracks.

### 2026-08-01 - CORE-F02 S05+S07：数学原语手写 + 生成规则加厚
- Goal:
	Keep VectorN hand-bound; harden ScriptBinding codegen (bases, Pure/static, topo order).
- Main changes:
	`LuaScriptBindingPrimitives`: Vector2/Vector3/Vector4.
	header tool: dependency topo-sort; `sol::base_classes` + `sol::bases<>`; ScriptPure≡Callable.
	`LuaComponent` ScriptType + `IsScriptLoaded`; inject `self` as LuaComponent*; `Transform::MakeIdentity` ScriptPure.
- Validation done:
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` → PASSED
- Next step:
	准备 commit；可选 Editor 再验 HelloTick。

### 2026-08-01 - CORE-F02 S06：场景入口 self / Owner / Translate
- Goal:
	Let LuaComponent scripts reach Owner and write back Root transform via generated bindings.
- Main changes:
	Inject `self` as `Component*` in Lua env; ScriptType+GetOwner on Component; ScriptType+Get/SetPosition/Translate on GameObject.
	`sol::no_constructor` for MEObject types; register GameObject before Component; `LuaScriptBindingSolTraits`.
	HelloTick scene-entry check + slow Z drift; unit test `TestSceneEntrySelfOwnerTranslate`.
	Fix: `GameObject::AddComponent_Internal` used `IsA(Component)` (all comps) instead of `SceneComponent` — broke adding LuaComponent after root.
- Validation done:
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` → PASSED
- Next step:
	Editor smoke (`SceneEntry OK` + visible drift); 准备 commit.

### 2026-08-01 - CORE-F02 S04：HelloTick 生成绑定自检
- Goal:
	Exercise auto-generated Transform/Vector3 bindings from an editor sample script (compact, not a full matrix).
- Main changes:
	`HelloTick.lua` one-shot verify: new / Position R/W / SetPosition / Translate; then heartbeat log.
- Validation done:
	Editor：HelloTick 挂 LuaComponent → 日志 `HelloTick: ScriptBinding OK`（用户确认 2026-08-01）
- Next step:
	S06 场景对象入口讨论 / 实现。

### 2026-07-31 - CORE-F02 S01–S03：ScriptBinding codegen + Transform
- Goal:
	Opt-in Script* → `Generated/ScriptBinding/` sol2 usertypes; first real type `Transform`.
- Main changes:
	header tool: `--script-binding-*`, `ScriptType`/`ScriptCallable`/`ScriptRead*`; skip reflection thunks for non-MEObject.
	`LuaScriptBindingPrimitives` (`Vector3`); `RegisterGeneratedLuaBindings` after Manual.
	`Transform`: ScriptType + Position ScriptReadWrite + SetPosition/Translate ScriptCallable.
	Test: `lua-script-mvp` Transform case.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` → PASSED
- Next step:
	准备 commit；S04+ 扩类型按需。

### 2026-07-31 - CORE-F01 Done；CORE-F02 Script Binding Draft
- Goal:
	Close Lua runtime Feature; open binding-codegen Feature with Script* specifiers.
- Main changes:
	`LUA_SCRIPTING_DESIGN` Status Done；原 S06 移交。
	Registry: CORE-F01 Done；CORE-F02 Draft → [LUA_SCRIPT_BINDING_DESIGN](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md).
	Generated path plan: `Generated/ScriptBinding/`（与 Reflection 分离）；self=C++ 指针。
- Validation done:
	Docs + prior editor HelloTick acceptance (F01).
- Next step:
	Review F02 Draft（首类型 Transform 子集）；确认后 Planned → S01 header tool.

### 2026-07-31 - CORE-F01 S05：LuaScript 资产竖切
- Goal:
	Treat `.lua` as a first-class asset (Font-style); drive LuaComponent from asset source.
- Main changes:
	`LuaScript` Asset + `LuaScriptLoader`; register `.lua` in `AssetTypeRegistry` / `AssetManager`.
	`LuaComponent` holds `shared_ptr<LuaScript>` (hardcoded chunk removed).
	Suite covers in-memory SetSource + RegisterAsset/LoadAsset disk fixture.
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` → PASSED
- Next step:
	准备 commit；S06 codegen 或 Component 场景序列化按需排期。

### 2026-07-31 - CORE-F01 MVP（S01–S04）落地
- Goal:
	Feasibility vertical slice: sol2 state → probe bindings → hardcoded LuaComponent tick → destroy clears env.
- Main changes:
	CMake: Lua 5.4.5 (`minengine_lua`) + sol2 pin `d805d027`（GCC 15 `optional::emplace` fix）.
	`LuaScriptSystem` / `LuaManualBindings` / `LuaBindProbe`；Engine Init/Shutdown 挂接。
	`LuaComponent` 内嵌 chunk + per-instance env；析构 `UnloadScript`。
	Suite `lua-script-mvp`（非 smoke/full）。
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test lua-script-mvp` → PASSED
	Follow-up: Lua/sol2 moved from FetchContent into `Third-Party/` (offline/reviewable).
- Next step:
	准备 commit；S05+（文件/资产/codegen）待用户排期。

### 2026-07-30 - CORE-F01 Lua scripting：分支 + 登记 + 设计初稿
- Goal:
	Start Lua track on a dedicated branch; register Feature; replace placeholder design with Draft Spec.
- Main changes:
	Branch `luaScript` from `master`.
	`FEATURE_REGISTRY`: `CORE-F01` In Progress → [LUA_SCRIPTING_DESIGN](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md).
	`ACTIVE_WORK`: CORE-F01 focus alongside RND-F02（分轨）.
	Design: System → 手写 sol2 白名单 → LuaComponent → 寿命失效 → 资产 → header tool codegen.
- Validation done:
	Docs-only; no build.
- Next step:
	User review Design Draft；可选补 Implementation Plan；然后 S01 引入 sol2 + `LuaScriptSystem`.
### 2026-06-01 - RND-F02 S5 remaining passes + native texture handle
- Goal:
  Complete migration wave 2: scene render pass via modern RHI; remove glad from RenderPasses.
- Main changes:
  `RenderPipeline` scene path `RHICmdBeginRenderPass`; Base/Translucency draw via `RHICommandList`; PostProcess/SkyBox modern PSO.
  `GetRHINativeTextureHandle` + Editor viewport/thumbnails; PSO `RHIDepthCompareFunc`/`RHICullMode`; fix cull vs depth-clip mapping.
- Risks or caveats:
  Materials still `RHIShaderLegacy` + `BindForDraw`; Point shadow still legacy FBO.
- Validation done:
  `.\scripts\verify.ps1`.
- Next step:
  S5+: material binding migration; delete Legacy `RHI` public API.

### 2026-06-01 - RND-F02 S4 OpenGL modern path + Present/Shadow migration
- Goal:
  Implement S3 contract on OpenGL; migrate Present and Directional/Spot shadow passes to `RHICommandList`.
- Main changes:
  `OpenGLRHIModern.{h,cpp}` (texture/buffer/shader/layout/SRV/binding wrappers); `OpenGLRHI` `RHICreate*`/`RHICmd*` (transient FBO, PSO fallback, draw/bind).
  `PresentPass` / `ShadowPass` use modern render pass + pipeline; `SceneRenderTarget::BuildRenderPassInfo`; `RenderPipeline` passes `RHICommandList`.
  Point shadow still Legacy `FrameBuffer` + `rhi->Clear()`. `RHIGraphicsPipelineStateRef`; raw-pointer legacy wrap overloads.
- Risks or caveats:
  Editor visual regression not automated; Point shadow hybrid until S5.
- Validation done:
  `.\scripts\verify.ps1` (build + smoke).
- Next step:
  S5: remaining passes, ImGui native handle, engine shaders via BindingSet.

### 2026-06-02 - WF-F02 handbook UX phase 2 (S04–S07)
- Goal:
  Auto nav, TOC, sidebar, and git last-updated footer for public handbook.
- Main changes:
  `mkdocs-awesome-pages-plugin` + `docs/handbook/**/.pages` (no root `nav` in mkdocs.yml).
  Material: `toc.follow`, `toc_depth 2-3`, `navigation.prune/path/top/indexes`.
  `mkdocs-git-revision-date-localized-plugin`; docs workflow `fetch-depth: 0`.
  `_authoring.md` + `exclude_docs`; expanded `render/overview.md` for TOC sample.
- Validation done:
  `mkdocs build --strict`.

### 2026-06-12 - Worktree bootstrap: submodules, scene asset migration, Editor run
- Goal:
  Fix physics worktree third-party gitdir paths; migrate scene Rotation to quaternion; ensure Editor launches.
- Main changes:
  `scripts/fix-worktree-submodule-gitdirs.ps1`, `scripts/migrate_transform_rotation_to_quat.py`;
  `default.mescene` / `test.mescene` Rotation → `{W,X,Y,Z}`; `MyMEProject.meproject` ProjectRoot → physics worktree.
- Validation done:
  `git status` OK after submodule gitdir fix; Editor loads `test` scene successfully with explicit `--engine-config`.
- Next step:
  Commit CORE-F01 code + asset migration batch.

### 2026-06-12 - CORE-F01 S01+S02+S04 partial: Quaternion storage and Scene API
- Goal:
  Land Transform quaternion storage (GLM-backed), Scene/RenderCamera API, Inspector Euler widget mapping.
- Main changes:
  `Quaternion.h/.cpp`, `Transform.h`, `SceneComponent`, `GameObject`, `RenderCamera`,
  `TransformWidget`, Editor viewport camera + PreviewScene + Playground call sites;
  reflection codegen for `Quaternion`; serialization round-trip test for `Transform.Rotation`.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor`; `minEngineTests.exe test smoke` PASSED.
- Next step:
  CORE-F01-S05 call-site grep sweep; S06 verify + commit; then PHYS-F01.

### 2026-06-11 - CORE-F01 Transform quaternion design (physics branch)
- Goal:
  Register Transform storage migration as `CORE-F01` before `PHYS-F01` (Jolt); document scope, Inspector Euler widget mapping, and slice plan.
- Main changes:
  `docs/ai/Platform/Core/CORE-F01_TRANSFORM_QUATERNION_DESIGN.md`,
  `CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md`;
  `FEATURE_REGISTRY.md` (`CORE-F01` Planned, `PHYS-F01` blocked);
  `ACTIVE_WORK.md` priority order updated.
- Validation done:
  Docs only; no build.
- Next step:
  Design §6 decisions recorded (D4 no auto read; D5 RenderCamera in scope) → `CORE-F01-S01`+`S02` first landable PR.

### 2026-06-01 - WF-F02 handbook nav under `runtime/` tree
- Goal:
  Align handbook paths and stubs with revised `mkdocs.yml` (Runtime tab + Function/Platform/Resource children).
- Main changes:
  `docs/handbook/runtime/**` placeholders; removed flat `runtime/{function,platform,resource}/overview.md`; design/impl docs updated.
- Validation done:
  `mkdocs build --strict`.

### 2026-06-11 - PHYS-F01 Jolt bootstrap design + implementation plan
- Goal:
  Physics subsystem bootstrap scope: Jolt vendor, thin `Physics/` facade, `RigidBodyComponent` + `BoxColliderComponent`, fixed-step simulate + pose pull before `SendAllEndOfFrameUpdates` (UE-aligned tick order).
- Main changes:
  `docs/ai/Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md`, `PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md`; P1–P9 defaults recorded; `FEATURE_REGISTRY` / `ACTIVE_WORK` → PHYS-F01 In Progress.
- Next step:
  PHYS-F01-S01-a (Jolt submodule + CMake).

### 2026-08-01 - PHYS-F03 deferred pending Delegates (TD-006)
- Goal:
  Avoid shipping Collider virtual contact notify as a permanent API before multicast Delegates exist.
- Main changes:
  PHYS-F03 → Deferred placeholder; deleted Implementation plan draft; TD-006 notes block PHYS-F03 (severity Medium).
- Next step:
  User picks next physics or CORE Delegate work; gameplay can poll GetContactEvents() meanwhile.

### 2026-08-01 - PHYS-F02 Sphere/Capsule colliders + shape traces
- Goal:
  Add Sphere/Capsule colliders and Scene SphereTrace/CapsuleTrace on the F01 channel/filter path.
- Main changes:
  `SphereColliderComponent` / `CapsuleColliderComponent`; `RigidBodyComponent::FindColliderComponent`;
  `PhysicsWorld` polymorphic shape create + `CastShapeTrace`; `Scene::{Sphere,Capsule}Trace`;
  suite `physics-shapes`; editor side effects for `m_Radius` / `m_HalfHeight`.
- Risks or caveats:
  Capsule axis = engine Y (Jolt default); HalfHeight = cylinder half-height only.
  New reflected types need cmake reconfigure after first codegen so `.gen.cpp` enters the GLOB.
- Validation done:
  `minEngineTests.exe test physics-shapes` PASS; regression `physics-linetrace` / `physics-contact` / `physics-smoke` PASS.
- Next step:
  Prepare commit for PHYS-F02; then PHYS-F03 contact gameplay dispatch.

### 2026-08-01 - PHYS-F01-S03 Scene LineTrace
- Goal:
  Public `Scene::LineTrace` (UE UWorld-style); internal Jolt CastRay with Trace×Object matrix filtering.
- Main changes:
  `HitResult` / `CollisionQueryParams` (no F prefix); `PhysicsWorld::LineTrace`; `Scene` forward;
  rename `PhysicsContactEvent`; `physics-linetrace` suite (hit/miss/ignore-self/trigger/visibility).
- Validation done:
  `minEngineTests.exe test physics-linetrace` + `physics-contact` + `physics-smoke` PASS.
- Next step:
  Prepare commit; PHYS-F01 bootstrap slices complete.

### 2026-08-01 - PHYS-F01-S02 collision channels + Contact events
- Goal:
  Land UE-style single `ECollisionChannel` + Ignore/Overlap/Block matrix, Trigger sensors, Contact Begin/End double-buffer.
- Main changes:
  `PhysicsTypes` (Channel/Response/ContactEvent + `CollisionChannelRegistry`); `ColliderComponent` + `BoxCollider` inherit;
  `PhysicsWorld` ObjectLayer/Sensor/`ContactListener` + `GetContactEvents`; `physics-contact` suite.
- Validation done:
  `minEngineTests.exe test physics-contact` + `physics-smoke` + `physics-sync` + `physics-load` (all PASS).
- Next step:
  Prepare commit for S02; then PHYS-F01-S03 (`LineTrace`).

### 2026-06-12 - PHYS-F01-S01-e scene↔physics sync (ETeleportType)
- Goal:
  Close S01 sync gaps before S02: `ETeleportType`, Transform dirty vs render dirty, Push/Pull, `bSimulatePhysics` gate Step + deactivate.
- Main changes:
  `PhysicsTypes.h` (`ETeleportType`); `SceneComponent` authority vs simulation writeback; `PhysicsWorld::SyncBodiesFromScene` / `SyncBodiesToScene`; `RigidBodyComponent::SetSimulatePhysics` hook; `physics-sync` test suite.
- Validation done:
  `minEngineTests.exe test physics-sync` + `physics-smoke` + `smoke`; Editor build.
- Next step:
  Commit S01-e; PHYS-F01-S02 (collision channels + Contact Begin/End).

### 2026-06-12 - PHYS-F01-S01 bootstrap complete (Jolt + physics vertical slice)
- Goal:
  Land S01-a–d: Jolt submodule/CMake, PhysicsSystem/World, RigidBody+BoxCollider (P10 proxy), LogicalTick simulate, physics-smoke falling box.
- Main changes:
  `Runtime/Function/Physics/*`; Jolt submodule; Engine/SceneManager lifecycle; `RigidBodyComponent`/`BoxColliderComponent` + reflection; `PhysicsSmokeTest`; fix `GameObject::AddComponent_Internal` SceneComponent-only attach.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests`; `minEngineTests.exe test physics-smoke` + `test smoke` from `minEngine/bin`.
- Next step:
  Commit S01 batch; then PHYS-F01-S02 (collision layers + contact events).

### 2026-06-12 - PHYS-F01-S01-b PhysicsSystem and PhysicsWorld shell
- Goal:
  Engine singleton + per-Scene Jolt world with fixed-step accumulator; Scene load/unload lifecycle; PhysicsConversion axis helpers.
- Main changes:
  `Runtime/Function/Physics/*`; `Engine` Start/Shutdown; `SceneManager` create/load/unload hooks.
- Validation done:
  `cmake --build minEngine/build --target minEngine minEngineTests`; `minEngineTests.exe test smoke` from `minEngine/bin`.
- Next step:
  PHYS-F01-S01-c (RigidBodyComponent + BoxColliderComponent).

### 2026-06-12 - PHYS-F01-S01-a Jolt submodule and CMake link
- Goal:
  Vendor Jolt via git submodule; link `Jolt` static target into `minEngine` with nested `add_subdirectory(Jolt/Build)`.
- Main changes:
  `.gitmodules` + `Third-Party/Jolt`; `minEngine/CMakeLists.txt` cmake 3.20; `minEngine/minEngine/CMakeLists.txt` Jolt options and `target_link_libraries`.
- Validation done:
  `cmake --build minEngine/build --target minEngine minEngineTests`; `minEngineTests.exe test smoke`.
- Next step:
  PHYS-F01-S01-b (`PhysicsSystem` / `PhysicsWorld` shell).

### 2026-06-12 - PHYS-F01 design: RigidBodyComponent as physics proxy (P10)
- Goal:
  Align rigid body model with user intent: `RigidBodyComponent` is a `Component` agent, not `SceneComponent`; no own Transform; reads/writes GO RootComponent for physics sync.
- Main changes:
  Design §3.2, §4 P10, §5 options E/F; Implementation S01-c/d assembly and sync wording.
- Next step:
  User approval → commit docs; then S01-a.
### 2026-06-01 - RND-F02 planning + Modern RHI design draft (`render`)
- Goal:
  Start GPU-model Modern RHI track; separate from RenderGraph (RND-F01 deferred).
- Main changes:
  `FEATURE_REGISTRY` + `ACTIVE_WORK` render focus; `docs/ai/Render/RND-F02_MODERN_RHI_DESIGN.md` (Part A 教案 + Part B 设计 + R0–R3 slices).
  Branch `render` for subsequent implementation.
- Validation done:
  Docs only; `mkdocs build` N/A for ai/Render path.
- Next step:
  F02-R0: CommandList draw + remove `gl*` from RenderPasses.

### 2026-06-01 - WF-F02 handbook site skeleton (MkDocs + GitHub Pages CI)
- Goal:
  Public docs site aligned with `src/Runtime` top-level layers; placeholders only.
- Main changes:
  `docs/handbook/` (index, getting-started, core/function/platform/resource overview stubs).
  Root `mkdocs.yml`, `requirements-docs.txt`, `.github/workflows/docs.yml`.
  README link to https://max1122chen.github.io/minEngine/
- Validation done:
  `mkdocs build --strict` from repo root succeeded.
- Next step:
  Merge to `main`; enable Pages source GitHub Actions; fill handbook content per layer.

### 2026-05-26 - Roadmap sync (P3 Undo + E2 + P6.1)
- **Docs:** `PLATFORM_ROADMAP.md`、`EDITOR_PLATFORM_PLAN.md`、`PROJECT_CONTEXT.md` aligned with repo state.
- **Done (marked):** P6.1 CB UI; P3 Undo E1.1–E1.4 + S1–S2; E2.1–E2.3a Inspector/Material viewport preview (`6ccd9bf`).
- **Deferred (consolidated):** E1 Inspector unification; E2.2b Texture preview; E2.3b CB thumbnails; E2.4; E1.5 Material Undo; Command E2 TryMerge; P7; P0/P1; P4/P5.

### 2026-06-05 - RND-F03 M1 tail (`render`, WIP)
- Goal:
  Complete M1 D/E/F tail after golden-scene visual sign-off (dir/point/spot + shadows OK).
- Main changes:
  SkyBoxPass + EnvMapCapture → `RHICreate*` + `SetBindingSet` + `BeginRenderPass` (no `FrameBuffer`/`WrapLegacy`).
  Material `BindForDraw` → Set2 `RHIBindingSet` (textures via `GetRHITexture()`); scalars still legacy uniform upload.
  EnvMap shaders → `#version 420` + `layout(binding=0)`.
  Deleted all `WrapLegacy*` (zero production callers).
  `TextureCubeLoader::CreateRenderTargetCube` / `WrapTextureCube`.
- Validation done:
  `cmake --build` minEngine + Editor; `.\scripts\verify.ps1` smoke + material-ir PASS.
  grep: Pass path `WrapLegacy` / `CreateUniformBuffer` / `CreateFrameBuffer` / `CreateVertexBuffer` = 0.
- Remaining (M2):
  Delete `RHI.h` Legacy API block; remove `Shader` Asset path; drop `m_RHITexture` dual-track on Texture2D/Cube; merge `OpenGLRHIModern` into `OpenGLRHI`.
- Next step:
  User final acceptance → commit; then M2 or RND-F04 prep.

### 2026-06-08 - RND-F03 M2 partial (`render`, WIP)
- Goal:
  Continue M2 after M1 user sign-off: engine passes off Shader Asset, texture single-track, merge OpenGLRHIModern, delete Legacy RHI resource API.
- Main changes:
  `EngineShaderUtils` — engine fixed shaders via `RHICreateShader` (Present/Shadow/Sky/Post/EnvMap/FXAA/Sharpen).
  `Texture2D`/`TextureCube` — single `RHITextureRef`; loaders use `RHICreateTexture2D` only.
  Deleted `OpenGLRHIModern.*`; added `OpenGLRHIResources.*`.
  `RHI.h` — removed `CreateVertexBuffer`/`CreateRHITexture*`/`CreateRHIShader` (Legacy); kept GL state toggles.
  Removed `EngineIBLEnvironment::BindForPBRDraw`; Editor thumbnail + MaterialIRTest use `RHITexture::GetNativeHandle()`.
  `TextureCubeLoader` — `CreateRenderTargetCube` / `WrapTextureCube`; dropped legacy cube factories.
- Validation done:
  grep (Render/): `OpenGLRHIModern`/`CreateRHITexture`/`GetRHITextureModern`/`Shader::CreateFromFiles` in Pass path = 0.
  Full build not re-run this session (long compile); prior blocker was EnvMapCapture missing `OpenGLShader.h` (fixed).
- Remaining (M2 tail):
  Material compile still uses `Shader` Asset + `RHIShaderLegacy`; scalar uniforms still `UploadUniformFloat`.
  Dead legacy impl files (`OpenGLBuffers`, legacy `RHITexture2D` types) still in tree.
- Next step:
  User local `cmake --build` + golden scene → commit; optional Material GPU program migration.

### 2026-06-01 - RND-F03 M1 complete + M2 sign-off (`render`)
- Goal:
  Finish F03 steps 2–5: Material GPU path, engine Pass UBO/BindingSet, legacy file deletion, grep gate, maintainer docs.
- Main changes:
  Material: `RHIShader` + Set2 scalar UBO (`binding=8`) + per-material PSO; removed `Shader` Asset (`Shader.*`, `ShaderLoader.*`, asset registration).
  Engine passes: Shadow/Post/Sky/EnvMap/Present → `EnginePassUniforms` UBO + `SetBindingSet`; shaders `#version 420` + fixed bindings.
  Legacy cleanup: deleted `RHIShaderLegacy`, `OpenGLBuffers.*`, `OpenGLVertexArrayObject.*`; trimmed `OpenGLHeaders.h`; `OpenGLRHIResources` upload texture ownership fix.
  Docs: `ACTIVE_WORK` / `FEATURE_REGISTRY` F03 → Done; `AssetManager.h` drops stale `LoadAsset_Impl<Shader>`.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor` PASS.
  `.\scripts\verify.ps1` (smoke + material-ir) PASS.
  grep `Render/`: `WrapLegacy`, `RHIShaderLegacy`, `UploadUniform*`, `CreateVertexBuffer`, `OpenGLRHIModern` = 0.
  Golden scene visual OK (prior session user sign-off).
- Next step:
  User commit on `render`; promote F04 when ready.

### 2026-06-02 - RND-F03 M4 P0–P2 pipeline refactor (`render`)
- Goal:
  Execute M4 adopted plan §9: detach runtime IBL/EnvMap, unify draw submission, move mesh PSO authority from Material to Pass + MeshDrawCommand; M3 backend type inline alongside.
- Main changes:
  **P0:** CMake excludes `EnvMapCapture` / `EngineIBLEnvironment` / `BrdfLutGenerator` from engine link; `RenderPipeline` drops IBL init; `SkyBoxPass` self-loads `environment_*` cubemap; `BuildSceneSet1` leaves IBL slots null; PBR template + assembler use direct light + simple ambient (`AO * 0.03`); MaterialIR IBL GPU tests skipped.
  **P1:** `RHICommandList::SubmitDraw` / `SubmitDrawMesh` — sole four-step draw path (PSO → bindings → VB/IB → Draw); all passes migrated (Shadow/Present/Post/Sky/mesh).
  **P2:** `RenderPassBase::PrepareMeshDrawCommands` builds per-draw PSO with `material shader + cmd.m_VertexInputLayout + pass fixed depth/blend`; `MeshDrawCommand::m_PipelineState`; `Material` no longer owns `m_PipelineState`.
  **M3 (partial):** Delete `OpenGLShader.*` / `OpenGLTexture.*`; logic inlined into `OpenGLRHIResources`; trim legacy `RHI.h` surface.
  **Docs:** `RND-F03-M4_PIPELINE_REFACTOR_DESIGN.md` (§9 adopted plan); `FEATURE_REGISTRY` / `ACTIVE_WORK` / F03 §16 pointers updated.
- Risks or caveats:
  Per-frame PSO `Create` per mesh draw (no cache yet); `Material::BindForDraw` still creates BindingSet each draw (P3); `RHICreateSRV` direct `new` unchanged (P0′).
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor` PASS.
  `.\scripts\verify.ps1` (smoke + material-ir) PASS.
  Editor golden scene visual OK (mesh layout / lighting / sky — user sign-off).
- Next step:
  P3 material BindingSet cache; optional PSO map per pass; P0′ SRV factory + remaining M3 cleanup.

### 2026-08-03 - RND-F10：EnvMapCapture 去 GL 旁路 + 退役全局 IBL（`render`）
- Goal:
	用现代 RHI 表达 mip/filter；删除 `EngineIBLEnvironment` / `BrdfLutGenerator`；S06 挂 TD。
- Main changes:
	`RHICmdGenerateMips` + CommandList；OpenGL cube 按 `NumMips` 分配；Capture 去掉 glad/`GetOpenGLTextureId`；删死代码与 CMake exclude；登记 **TD-021**（Editor Bake UX）。
- Validation done:
	`mingw32-make minEngine/Editor/minEngineTests`；`.\scripts\verify.ps1` smoke PASS；Editor bake 日志仍通（irradiance/prefilter + baked HDR）。
- Risks or caveats:
	`EnvMapCapture.cpp` 仍有匿名命名空间静态 bake helpers（与全 static Baker API 并存；未再引入引擎层 GL）；BRDF 仍依赖项目 `brdf_lut.png`。
- Next step:
	用户确认是否将 F10 标 Done；准备 commit。

### 2026-08-03 - RND-F10 S05：项目 HDR → 天空/IBL bake（`render`）
- Goal:
	无 face PNG 时从项目 `m_SourceHdrPath` GPU bake 真实天空（付清 TD-015 主路径）。
- Main changes:
	现代路径重开 `EnvMapCapture`（PSO/BindingSet/`CreateShaderResourceView`）；`EnvironmentMap::TryBakeFromSourceHdr`；`DefaultEnvironment.meenv` 指向 `citrus_orchard_puresky_1k.hdr`。
- Validation done:
	`mingw32-make minEngine/Editor`；Editor 日志：`baked sky/IBL from project HDR` + irradiance/prefilter；`.\scripts\verify.ps1` smoke PASS。
- Risks or caveats:
	仍含 `glGenerateMipmap`/glad（RHI 无 GenerateMips）；S04 全局 IBL 入口未删；S06 Editor 显式 Bake 未做。
- Next step:
	用户目视 Viewport 天空；可选 S04 / 去 glad / S06。

### 2026-08-03 - RND-F10 S01–S03：EnvironmentMap 项目资产接线（`render`）
- Goal:
	EnvironmentMap 仅项目 Content；SkyBoxComponent ref → SkyPass / Set1；EngineDefault 只作复制种子。
- Main changes:
	`EnvironmentMap` + `.meenv` loader/registry；Sky proxy/pass 跟 Asset；Set1 IBL 从场景 EnvironmentMap；`MyMEProject` 种子 IBL + `DefaultEnvironment.meenv`。
- Validation done:
	`mingw32-make minEngine/Editor/minEngineTests`；`.\scripts\verify.ps1` smoke PASS；`test render-graph` PASS。
- Risks or caveats:
	无 face PNG 时 validation cube；BRDF 需在 Inspector 指到项目 `brdf_lut`；Bake（TD-015）未做。
- Next step:
	S04 清全局 IBL 入口；S05 现代 Baker；用户目视：给 SkyBox 指定 DefaultEnvironment。

### 2026-08-03 - RND-F10 Draft：EnvironmentMap Asset + Sky/IBL（`render`）
- Goal:
	场景 Asset 引用驱动天空与 IBL；GPU Bake 后置并用现代 RHI 付清 TD-015。
- Main changes:
	登记 `RND-F10`；Design + Impl 草稿；ACTIVE_WORK / TECH_DEBT 指向 F10。
- Next step:
	用户确认 Draft → Planned；建议先 S01–S03 磁盘接线。

### 2026-08-03 - RND-F09 Done：RHI / Binding hygiene（`render`）
- Goal:
	付清 TD-013/014/016/017/018/019（不含 TD-015）。
- Main changes:
	S01 Set0 脏缓存；S02 Material `RHITextureViewCache`；S03 `RHISetBackbufferClearColor`/`RHIClearBackbuffer`；S04 Apply 补 blend 因子；S05 删除 `ShaderResource`+CB 过滤；S06 unit → `EngineShaderBindings`。
- Validation done:
	`cmake --build` minEngine/Editor/minEngineTests；`.\scripts\verify.ps1` smoke PASS；`test render-graph` PASS。
- Risks or caveats:
	Blend 因子尚未 desc 驱动；Editor 黄金场景待用户目视。
- Next step:
	用户目视后准备 commit；TD-015 EnvMap 专题另议。

### 2026-08-03 - RND-F09 Planned：RHI / Binding hygiene sweep（`render`）
- Goal:
	打包付清 TD-013/014/016/017/018/019；明确排除 TD-015 EnvMap。
- Main changes:
	登记 `RND-F09`；Design + Impl；TECH_DEBT / ACTIVE_WORK 指向 F09 切片。
- Next step:
	用户确认方案后从 S01（Set0 脏标记）开工。

### 2026-08-03 - RND-F08 follow-up：阴影 Slot 语义瘦身（`render`）
- Goal:
	删除 `ShadowResourceManager`；用显式 `SlotIndex` 对齐 Set1 / LightUBO。
- Main changes:
	`Make*ShadowBinding` 内联进 `ForwardRenderer`；Bind/UBO 按 `SlotIndex`；删 Manager 源文件。
	短设计 `RND-F08_SHADOW_SLOT_SEMANTICS.md`。
- Validation done:
	`test render-graph` / `test smoke`（见本会话）。
- Next step:
	用户目视阴影；准备 commit。

### 2026-08-03 - RND-F08 follow-up：阴影 Slot 语义瘦身（`render`）
- Goal:
	删除 `ShadowResourceManager`；用显式 `SlotIndex` 对齐 Set1 / LightUBO。
- Main changes:
	`Make*ShadowBinding` 内联进 `ForwardRenderer`；Bind/UBO 按 `SlotIndex`；删 Manager 源文件。
	短设计 `RND-F08_SHADOW_SLOT_SEMANTICS.md`。
- Validation done:
	`test render-graph` / `test smoke` PASSED。
- Next step:
	用户目视阴影；准备 commit。

### 2026-08-02 - RND-F08：阴影贴图图所有权（`render`）
- Goal:
	Directional/Spot/Point depth 由图拥有；关掉 TD-020；Manager 不再 Create 纹理。
- Main changes:
	`SetupFrameRenderGraph` → `BindGraphShadowTextures` → `BuildSceneSet1` → `EnqueueFrameRenderGraph`。
	`ShadowGraphPass` Absolute 声明（Dir=`DirShadowAtlas` 2DArray；Spot/Point 共享 `GraphDepthResourceName`）。
	`ShadowResourceHandle::IsValid` 与 `HasBoundTexture` 分离；Manager 仅 unit/metadata。
	`RenderGraph::InvalidateBake` 在阴影尺寸指纹变化时失效。
- Risks or caveats:
	Manager 已在 2026-08-03 follow-up 删除。
- Validation done:
	`test render-graph` 4 PASSED；`test smoke` PASSED。
- Next step:
	Slot 语义瘦身（已做）。

### 2026-08-02 - RND-F07：Editor 视口只显示 ImGui Image 占位（`render`）
- Goal:
	接回后视口无正常场景，只见 ImGui Image 占位；修帧纹理寿命与采样/清屏链路。
- Main changes:
	`Bake` 不再 `assign(nullptr)` 清空物理纹理；rebake 前清 pass IO / resource usage。
	Color RT 强制 `ShaderResource`；`SetupAttachments` 按 flags 不匹配则重建。
	`SceneRenderTarget::Resize` 同尺寸不 `reset` publish。
	`SkyBoxPass` 恒跑并 clear；Opaque LoadStore；Post 链 `NeedRenderPass` + predecessor，FXAA 失败时 Sharpen 不覆盖 SceneColor。
- Risks or caveats:
	阴影仍 TD-020。
- Validation done:
	`cmake --build … --target Editor minEngineTests`；`test render-graph` 4 PASSED；`test smoke` PASSED；用户目视黄金场景 OK。
- Next step:
	准备 commit；可选付清 TD-020。

### 2026-08-02 - RND-F07 Phase1 S01–S03：真渲染停工 + 帧 RT 不分配（`render`）
- Goal:
	Phase1 中间态：无人分配 SceneColor/Depth/shadow/post；`ForwardRenderer` 不跑真图。
- Main changes:
	`ForwardRenderer::Execute` / `EnsurePostBufferTexture` 早退；`SceneRenderTarget::Resize` 只记尺寸不建纹理；`ShadowResourceManager::Ensure*` 恒 false。
	`render-graph` 移出 smoke；Design 定名图节点 `RenderPass` / 钩子 `IRenderPass`；Status In Progress。
- Risks or caveats:
	Editor 视口黑屏（已有 null texture 提示）；旧 Manual 图代码暂留待 S04 替换。
- Validation done:
	`cmake --build minEngine/build --target minEngineTests`
	`minEngine\bin\minEngineTests.exe test smoke` → PASSED（`render-graph` 已不在 smoke）
- Next step:
	S04 Granite 式图核心（§3.6）。

### 2026-08-02 - RND-F07-S06–S09：接回场景/Post + 收口（`render`）
- Goal:
	完成 F07：主路径走 Granite 式图；Scene/Post 图拥有；删 Manual external。
- Main changes:
	`RenderGraphFrameContext` / `ForceIncludePass` / `Prepare(graph)`。
	Sky/Opaque/Translucent/Post/Present 新生命周期；`ForwardRenderer::Execute` 全路径恢复。
	SceneRT Publish color+depth；Shadow ForceInclude + Manager Ensure 恢复（**TD-020**）。
	Registry F07 Done；无 `RegisterExternal`。
- Risks or caveats:
	阴影 atlas 尚未迁入图；Bake 每尺寸变化重建。
- Validation done:
	`test render-graph` 4 PASSED；`test smoke` PASSED。
- Next step:
	用户 Editor 黄金场景目视；可选付清 TD-020；准备 commit。

### 2026-08-02 - RND-F07-S05：图拥有资源 GPU 竖切（`render`）
- Goal:
	证明 Bake/SetupAttachments 产生的物理纹理可上 GPU，并有可观察输出。
- Main changes:
	`GraphClearPass`：声明 `SceneColor` + ClearStore。
	`ForwardRenderer::Execute` 跑竖切图；`PublishGraphColorTexture` 把图 `shared_ptr` 交给 SceneRT 显示。
	`GetPhysicalTextureShared`；单测 `glGetTexImage` 校验 clear 色。
- Risks or caveats:
	仅 clear，无场景几何；S07 前视口固定 slate-blue。
- Validation done:
	`minEngineTests.exe test render-graph` → 4 PASSED（含 clear 读回）
	`minEngineTests.exe test smoke` → PASSED
- Next step:
	S06 Pass 生命周期收紧，或直接进 S07 接回场景。

### 2026-08-02 - RND-F07-S04：Granite 式 RenderGraph Bake 核心（`render`）
- Goal:
	落地 Design §3.6：声明 → Bake → SetupAttachments → GetPhysicalTexture；替换 Manual builder。
- Main changes:
	新增/重写 `RDGTypes` / `RDGResource` / `IRenderPass` / `RenderPass` / `RenderGraph`（Bake 依赖回溯 + 物理表；transient/merge Deferred）。
	删除 `RenderPassBuilder` / `RenderGraphFrameResources` / `RDGTexture`；场景 Pass 改为新 `IRenderPass` stub；`ForwardRenderer` 旧构图路径 idle。
	`RenderGraphTest`：headless GL + Bake/物理纹理/依赖序/缺 writer。
- Risks or caveats:
	主路径仍黑屏；InputRelative / swapchain 非拥有视图 / barrier 未做；S05 才上 GPU 竖切。
- Validation done:
	`cmake --build minEngine/build --target minEngine minEngineTests`
	`minEngineTests.exe test render-graph` → 3 PASSED
	`minEngineTests.exe test smoke` → PASSED
- Next step:
	S05 最小 GPU 竖切（图拥有 color → clear/present）。

### 2026-08-02 - RND-F07 Draft：Granite-style RDG + 帧资源所有权大重构（`render`）
- Goal:
	拍板破坏性两阶段：Phase1 无人分配帧 RT、真渲染停工；Phase2 化用本机 Granite `render_graph` 再接回。取代 F01 S06+ 实验 Bake 产品方向。
- Main changes:
	新增 `RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md` / `_IMPLEMENTATION.md`；Registry / ACTIVE_WORK；F01 Meta 标注 Superseded by F07。
- Risks or caveats:
	中间态黑屏；Phase2 须对照 Granite `bake()` 防再发明。
- Validation done:
	Phase1 code landed；见同日 Phase1 条目。
- Next step:
	S04 Granite 式图核心（§3.6）。

### 2026-07-24 - RND-F01 S05 RDG implementation hygiene (`render`)
- Goal:
  收敛 Manual RDG 过碎/空壳实现，名实相符；不扩 Bake/transient。
- Main changes:
  删除 `RenderGraphExecute.cpp`、`RDGBuffer.h` 占位、`PassParameters.h` / `RenderGraphFrameContext.h` / `RenderGraphTransition.*`（并入 `IRenderPass` / `RenderGraphFrameResources`）；`RenderGraphScenePass.h` → `SceneRenderPassUtils.h`。
- Risks or caveats:
  `RDGBuffer` 待真有 buffer 资源时再引入；目录仍含 `RenderPipeline/`（F06-S03 可选）。
- Validation done:
  `cmake --build` minEngine + minEngineTests PASS；`minEngineTests.exe test render-graph` PASS。
- Next step:
  F01-S06 Bake。

### 2026-07-24 - RND-F06 S01–S02：ForwardRenderer 替换 RenderPipeline（`render`）
- Goal:
  删除 `RenderPipeline`；`SceneRenderer` 薄基类 + `ForwardRenderer` 实现；`RenderSystem` 只依赖基类。
- Main changes:
  `SceneRenderer.h`、`EngineRenderLimits.h`；`RenderPipeline.h/.cpp` → `ForwardRenderer.h/.cpp`；Pass/utils/Context 指针改名；`FrameRenderGraphContext::Renderer`；目录名暂留 `RenderPipeline/`。
- Risks or caveats:
  目录与类名短期不一致（S03）；黄金场景目视待维护者确认。
- Validation done:
  `cmake --build` minEngine + minEngineTests + Editor PASS。
  `minEngineTests.exe test render-graph` + `test smoke`（from `bin/`）PASS。
- Next step:
  维护者 Editor 黄金场景目视；F06-S03 目录/注释收尾；然后 F01 S05 RDG 卫生。

### 2026-07-24 - RND-F06 Design：ForwardRenderer / Graph 职责分离（文档）
- Goal:
  钉死 Renderer vs RenderGraph 心智模型；登记 Feature；调整 F01「下一步 Bake」口径，避免在 `RenderPipeline` 上帝对象上继续堆机制。
- Main changes:
  新增 `docs/ai/Render/RND-F06_FORWARD_RENDERER_DESIGN.md`；Registry / ACTIVE_WORK / PROJECT_CONTEXT 更新；F01 §6 切片改为 **F06 闸门 → S05 卫生 → S06 Bake → S08 调图形态**。
- Risks or caveats:
  尚未写 C++；须按 ACTIVE_WORK 接力，勿跳过 F06 直接 Bake。
- Validation done:
  Registry 已登记 `RND-F06`；RND next free → F07。
- Next step:
  F06-S01 抽出 `ForwardRenderer`；或先准备 commit 本批文档。

### 2026-06-12 - RND-F01 S04 Shadow passes RenderGraph migration (`render`)
- Goal:
  Split monolithic ShadowPass into per-command graph passes; declare DirShadowAtlas scene input edge.
- Main changes:
  `ShadowGraphPass` (IRenderPass per `ShadowDrawCommand`); `ShadowPass::RenderSingleDrawCommand` + `PrepareShadowPass`.
  Dynamic shadow pass pool in `m_FrameRenderGraph` (rebuild when command count changes); execution order Shadow.* → Scene → Post.
  `kRDGDirShadowAtlas`; Base/Translucent `AddTextureInput(DirShadowAtlas)`; removed legacy `m_ShadowPass.Execute` from main path.
- Risks or caveats:
  Shadow pass names use index slots (`Shadow.N` / `ShadowDepth.N`); spot/point logical names deferred; visual shadow sign-off pending.
- Validation done:
  `cmake --build` + `minEngineTests.exe test render-graph` PASS.
- Next step:
  ~~F01-S05 Bake~~ → **已改口径（2026-07-24）：先 RND-F06，再 F01 S05 卫生 → S06 Bake。**

### 2026-06-12 - RND-F01 S03 Scene passes RenderGraph migration (`render`)
- Goal:
  Migrate Sky / Opaque / Translucent into unified frame RenderGraph; extract mesh draw helpers from RenderPassBase.
- Main changes:
  `SkyBoxPass`, `BasePass`, `TranslucencyPass` implement `IRenderPass`; `SceneMeshDrawUtils` + `FrameRenderGraphContext`.
  `m_FrameRenderGraph` merges scene + post + present; per-pass `BeginRenderPass` on SceneColor/SceneDepth (Sky/Opaque clear, Translucent load).
  Removed monolithic scene `RHICmdBeginRenderPass` block from `RenderPipeline::Execute`.
- Risks or caveats:
  Visual golden-scene sign-off pending user; Shadow still Legacy outside graph (S04).
- Validation done:
  `cmake --build` + `minEngineTests.exe test render-graph` PASS.
- Next step:
  F01-S04 Shadow pass graph migration.

### 2026-06-12 - RND-F01 S02 Post chain RenderGraph migration (`render`)
- Goal:
  Migrate FXAA → Sharpen → Present to Manual RenderGraph; remove scene-pass post loop and `m_SceneColorTexture` injection.
- Main changes:
  `PostProcessPass` / `PresentPass` implement `IRenderPass` (Setup / PreparePass / BuildRenderPass); `RenderPipeline` owns `m_PostRenderGraph`, `m_PostBufferTexture`, `ExecutePostRenderGraph` after scene `EndRenderPass`.
  `RenderGraphFrameResources::BeginFrame`; `RegisterExternalTexture` re-register; `kRDGPostBufferA`.
  Binding sets created in `PreparePass`, not `BuildRenderPass`.
- Risks or caveats:
  Visual golden-scene sign-off pending user; transient PostBufferA owned by pipeline (not graph pool).
- Validation done:
  `cmake --build` + `minEngineTests.exe test render-graph` PASS.
- Next step:
  F01-S03 Scene passes (Sky / Opaque / Translucent) graph migration.

### 2026-06-12 - RND-F01 S01 Manual RenderGraph skeleton (`render`)
- Goal:
  Deliver compile-ready RenderGraph types + two-phase executor without touching main pipeline path.
- Main changes:
  New `Render/RenderGraph/`: `RenderGraph`, `RenderPass`, `RenderPassBuilder`, `RenderGraphFrameResources`, `IRenderPass`, `RDGTexture` (string names), `AddTransition`; deleted empty `RenderPipeline/RenderGraph.h` stub.
  `ExecuteGraph`: all `PreparePass` then all `BuildRenderPass`; `render-graph` smoke tests (setup IO + execution order).
- Risks or caveats:
  No `Bake()` / no main-path wiring; internal RT creation deferred to S02.
- Validation done:
  `cmake --build` clean + `minEngineTests.exe test render-graph` PASS (2 cases, 7 asserts).
- Next step:
  F01-S02 Post/Present graph migration.

### 2026-06-12 - RND-F01 S0 Binding vocabulary unification (`render`)
- Goal:
  Align RHI binding types/APIs with Vulkan descriptor mental model before RenderGraph S01.
- Main changes:
  `RHIBinding.h` → `RHIShaderBinding.h`; `RHIShaderBindingSetLayout` / `RHIShaderBindingSetLayoutEntry` / `RHIShaderBindingSet` / `RHIShaderBinding` / `RHIShaderBindingType`; `CreateShaderBindingSetLayout` / `CreateShaderBindingSet` / `SetShaderBindingSet`; `GetShaderBindingSetLayout` / `kMaxShaderBindingSets`; `MeshDrawPacket::ShaderBindingSets`; OpenGL impl classes renamed; all Pass/Material/Engine call sites updated.
- Risks or caveats:
  `EngineSceneBindingSets` class name unchanged (engine layer); Tier-B design docs still cite old `RHIBinding*` in places.
- Validation done:
  `cmake --build minEngine/build --target minEngine` PASS.
  `minEngineTests.exe test smoke` + `material-ir` PASS (existing binary; test exe relink blocked by file lock).
- Next step:
  F01-S01 Manual RenderGraph skeleton; optional commit S0.

### 2026-06-11 - RND-F04 S04 PSO/SRV cache + RHI contract cleanup (`render`)
- Goal:
  Close F04 hot-path caching and RHI contract gaps; remove legacy draw submit API.
- Main changes:
  **PSO cache:** `EnginePipelineLayouts::GetOrCreateSceneMeshGraphicsPipelineState` keyed by layout + VIL + shader + pass kind.
  **SRV flyweight:** `RHITextureViewCache`; Scene Set1 dirty rebuild; Present/Post texture-keyed BindingSet cache; Sky SRV/set at init.
  **RHI:** `RHICmdTransition` (GL no-op); `OpenGLRHI` tracks bound descriptor sets with setIndex validation; removed `SubmitDraw*` from `RHICommandList`.
  Docs: F04 Done; TECH_DEBT TD-013–TD-019.
- Risks or caveats:
  `BuildSceneSet0` still rebuilds each frame; Material SRV not flyweighted; `verify.ps1` not recorded this session.
- Validation done:
  Maintainer local cmake build PASS; golden scene visual OK.
- Next step:
  Commit S04; run `verify.ps1`; start F03-M3 tail inventory (EnvMap bypass, asset ShaderResource).

### 2026-06-11 - RND-F04 S01–S03 PipelineLayout + MeshDrawPacket (`render`)
- Goal:
  Modern RHI semantic evolution: glue PSO to binding via PipelineLayout; complete draw packet; unify all Pass submit paths.
- Main changes:
  **S01:** `RHIPipelineLayout`, `RHICreatePipelineLayout`, GL `OpenGLRHIPipelineLayout`; `RHIGraphicsPSODesc::PipelineLayout`; `EnginePipelineLayouts` (shadow / scene mesh / pass-local).
  **S02:** `MeshDrawPacket`, `RHICommandList::SubmitMeshDrawPacket`; Present / Post / Sky migrated.
  **S03:** `PrepareMeshDrawPackets` + `SubmitSceneMeshDrawPackets`; Base / Translucent / Shadow on full packet (Set0/1/2); `MeshDrawCommand` pass-agnostic; removed `BindSceneDrawResources`.
  Docs: F04 design §12–§13 slice status; `ACTIVE_WORK` / `FEATURE_REGISTRY`.
- Risks or caveats:
  Per-draw PSO create still uncached; SRV per-frame create unchanged; `setIndex` still ignored; `SubmitDraw*` legacy API retained on CommandList.
- Validation done:
  User local cmake build PASS; golden scene visual OK.
- Next step:
  S04: PSO cache, SRV flyweight, `setIndex`, `RHICmdTransition` no-op; remove legacy SubmitDraw API.

### 2026-06-02 - RND-F03 M4 P3 material BindingSet cache (`render`)
- Goal:
  Stop per-draw `CreateBindingSet` in `Material::BindForDraw`; cache Set2 at compile / texture parameter change.
- Main changes:
  `Material::m_MaterialBindingSet` + `RebuildMaterialBindingSet` (compile + `SetTextureParameter`).
  `BindForDraw` only uploads scalar UBO; material Set2 bound via `SubmitDrawMesh` in `DrawMeshCommand`.
  `PrepareMeshDrawCommands` fills `MeshDrawCommand::m_MaterialBindingSet` from `Material::GetMaterialBindingSet`.
- Risks or caveats:
  Texture SRVs still `make_shared<OpenGLRHIShaderResourceView>` (P0′); per-frame PSO create unchanged; fullscreen passes still create binding sets per frame.
- Validation done:
  User local cmake build + Editor visual OK.
- Next step:
  P0′ `RHICreateShaderResourceView`; optional Pass PSO cache; PROGRESS_LOG commit on `render`.

### 2026-08-17 - ED-F01 Vulkan Editor Parity registered (`feat/render`)
- Goal:
  After RND-F05 S07d, restore full Vulkan Editor (ImGui-Vulkan, embedded viewport, navigation) and continue shadow/sky/IBL in real Editor — not smoke fork.
- Main changes:
  **Registry:** `ED-F01` Planned; `RND-F05` → Done (RHI vertical slice S01–S07d).
  **Docs:** [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md), [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md).
  **Handoff:** F05 S07e/f → ED-F01-S06/S07; smoke mode slated for removal at ED-F01-S03.
  **Source:** `imgui_impl_vulkan` from sibling `../imgui` clone (1.92.7 aligned).
- Risks or caveats:
  Frame sync (TD-024), swapchain/ImGui render pass alignment, dynamic RT ImGui descriptors.
- Validation done:
  Design/Impl draft only; no code yet.
- Next step:
  ED-F01-S01: copy `imgui_impl_vulkan` + CMake; then S02 ImGui empty frame.
