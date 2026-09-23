# CORE-F31 — BeginPlay / EndPlay — Implementation Plan

## Meta
- **ID:** CORE-F31
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-23
- **Related:** [Design](./CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md)
- **Design readiness:** Ready with deferred

## Goal

Ship GameObject→Component BeginPlay/EndPlay (UE order), Scene Dispatch, PIE Enter/Stop hooks, simulation query, and egin-play tests.

## Design Decisions Being Implemented

| Decision | Notes |
|----------|-------|
| UE Actor routing | Comp BeginPlay before GO OnBeginPlay; EndPlay GO then comps |
| Activate orthogonal | Late Activate while simulating → Component BeginPlay |
| PIE order | Systems OnBeginPIE then DispatchBeginPlay; Stop reverses |
| Standalone | Explicit Scene::DispatchBeginPlay (host/tests call); no Editor auto |

## Slices

| Slice | Summary | Verify | Status |
|-------|---------|--------|--------|
| S01 | EEndPlayReason + Component/GO BeginPlay/EndPlay | build | Done |
| S02 | Scene Dispatch + CreateGameObject dynamic + query | build | Done |
| S03 | PIE EnterPlay/Stop wiring | build | Done |
| S04 | suite egin-play | minEngineTests.exe test begin-play | Done |

## Risks

- Destroy during BeginPlay: best-effort EndPlay in GO dtor
