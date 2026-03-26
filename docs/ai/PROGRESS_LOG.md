# minEngine Progress Log (for AI)

Last updated: 2026-03-26

## Purpose

This file is an AI-oriented progress digest converted from commit messages.
It is not a full changelog. It focuses on architecture moves, rendering milestones, and known pitfalls.

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

## Current Technical Focus (Inferred)

- Improve render pipeline correctness and lifetime management.
- Keep pass orchestration predictable when adding new features.
- Continue validating memory/resource safety under long runs.

## Known Pitfalls to Recheck During Refactors

- Any per-frame vector/list/map used in pipeline build path must be cleared or reused safely.
- Any GL resource wrapper must own and release native handles in destructor.
- Scene proxies or render entries must not be recreated indefinitely without ownership policy.
- Shadow pass changes (viewport/state/targets) must be restored before later passes.

## Entry Template (Append for each meaningful task)

### YYYY-MM-DD - Task title
- Goal:
- Main changes:
- Risks or caveats:
- Validation done:
- Next step:
