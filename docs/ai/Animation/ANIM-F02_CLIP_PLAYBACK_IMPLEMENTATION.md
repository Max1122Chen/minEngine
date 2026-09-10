# ANIM-F02 — Clip Playback — Implementation Plan

## Meta
- **ID:** `ANIM-F02`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05（人型 Editor 目视通过；OpenGL Int4 bone indices 修复）
- **Related:** [Design](./ANIM-F02_CLIP_PLAYBACK_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Branch:** `feat/animation`

## TL;DR
S00–S03 Done. F02 = MVP transitional (bone `AnimationTrack` + `TryGetNamedFloat` stub). Humanoid Walking clip playback verified in Editor (OpenGL).

## Scope
- **In:** Design In
- **Out:** Design Out; named float Import; Property direct-write

## 1) Slice overview

| Slice ID | Content | Status | Verify |
|----------|---------|--------|--------|
| `ANIM-F02-S00` | Track / Clip::Evaluate / TryGetNamedFloat + tests | Done | `test animation-clip` 4/4 |
| `ANIM-F02-S01` | Loader + `.meaclip` + Registry + ImportAnimationClip | Done | build; Import API |
| `ANIM-F02-S02` | AnimationPlayer + SkeletalMeshComponent | Done | Player tests; Tick; PlayOnAwake |
| `ANIM-F02-S03` | Editor Import AnimationClip + docs | Done | 人型目视 PASS；smoke / animation-clip |

## 2) Dependency

```text
S00 -> S01 -> S02 -> S03
```

## 3) Deferred

| Item | Reason |
|------|--------|
| Named float Import | Design §3.5 |
| Property direct-write | Out of F02 |
| Standalone Player Component | Locked |

## 4) Changelog

| Date | Note |
|------|------|
| 2026-09-04 | Initial plan; S00–S03 code landed |
| 2026-09-05 | Humanoid visual PASS; fix OpenGL glVertexAttribIPointer for bone indices; Skeleton/meta Guid identity; ObjectPtr reload dedupe; PlayOnAwake |