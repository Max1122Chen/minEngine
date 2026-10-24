# RND-F26 — View-Aware Mesh Collect Implementation Plan

## Meta
- **ID:** RND-F26
- **Status:** Done（引擎 S01–S02）
- **Owner:** project maintainer
- **Last updated:** 2026-10-24
- **Related:** [Design Spec](./RND-F26_VIEW_AWARE_MESH_COLLECT_DESIGN.md)
- **Design readiness:** Ready (maintainer approved Option A)
- **Worktree:** `D:/Dev/GitRepo/minEngine-render` · `feat/render`

## Goal

Land Collect Context + signature change; prove sub-cull with an in-engine Fake Proxy test. minCraft `ChunkMeshSceneProxy` consumer follow-up landed in minCraft repo.

## Design Decisions Being Implemented

| Decision | Design | Notes |
|----------|--------|-------|
| D1 MeshCollectContext | §3.2–3.3 | Purpose + optional Frustum* |
| D2 ShadowOnly null frustum | §3.3 / invariants | No main-frustum sub-cull on shadow-only path |
| D3 Default proxies ignore | §3.3 | Signature only; behavior unchanged |
| D4 Engine fake test | Verification | Not minCraft |

## Implementation Slices

| Slice ID | Summary | Verification | Status |
|----------|---------|--------------|--------|
| RND-F26-S01 | Context header; GetMeshBatches(context); Renderer fill; engine proxy overrides | `minEngine` build PASS | **Done** |
| RND-F26-S02 | Fake multi-section Proxy tests in `frustum` suite | `minEngineTests.exe test frustum` 8/8 PASS | **Done** |
| consumer | minCraft ChunkMeshSceneProxy + `AllowsWorldBounds` | game repo | **Done** |

## Slice → Design Traceability

### RND-F26-S01
- **Design:** D1–D3
- **Implementation:** `MeshCollectContext.h`; Proxy + ForwardRenderer
- **Verification:** build Editor/tests

### RND-F26-S02
- **Design:** D4
- **Implementation:** doctest cases under suite `frustum`
- **Verification:** MainView culls off-screen section; ShadowOnly with null frustum emits both

## Risks / Mitigations

- **minCraft override break:** document one-line signature sync; not engine gate.
- **Over-cull:** fail-open + AllowsWorldBounds helper.

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-24 | Plan after Design approval; S01/S02 |
| 2026-10-24 | S01/S02 Done；`test frustum` 8/8；minCraft Chunk 跟进另仓 |
