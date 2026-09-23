# CORE-F30 — Play main Camera (BeginPlay policy) — Design Spec

## Meta
- **ID:** `CORE-F30`
- **Type:** Feature
- **Complexity:** L1–L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-23
- **Related:**
  - **Depends:** [CORE-F31](./CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md)
  - Sibling: [CORE-F29](./CORE-F29_PLAY_GATED_GAMEPLAY_INPUT_DESIGN.md)
  - Play Mode: [CORE-F05](./CORE-F05_PLAY_MODE_DESIGN.md)
  - Catalyst: minCraft TD-008
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F31`; `CameraComponent`; existing `SyncPlayModeCamera`
- **Blocks:** place Main Camera → Play sees world without manual viewport hacks

## TL;DR

Camera Play behavior is a **Component policy on F31**: `bSetAsMainCameraOnBeginPlay` → `OnBeginPlay` calls `SetSelfAsMainCamera()`. Other comps may still call `SetSelfAsMainCamera()` anytime. Clarify Main mark vs Edit viewport preview; PIE follows Main via `SyncPlayModeCamera` after BeginPlay.

## Scope

### In
- Bool such as `bSetAsMainCameraOnBeginPlay` (final name in Impl)
- `CameraComponent::OnBeginPlay`: if bool set → `SetSelfAsMainCamera()`
- Keep/clarify `SetSelfAsMainCamera()` for runtime Main mark (+ optional Edit preview, documented)
- After BeginPlay, PIE view follows Main
- Stop/EndPlay: Edit viewport navigation restored
- Docs: Main arbitration; Edit vs Play
- No-camera scenes safe

### Out
- Full cine / CameraRig
- Owning BeginPlay dispatch (F31)
- Requiring games to call Editor viewport APIs to see Play

## Reader quick start
1. F31 dependency + bool-on-BeginPlay
2. SetSelfAsMainCamera still public
3. Acceptance

---

## Problem

`SetSelfAsMainCamera` targets Editor viewport; PIE sync is separate. Games workaround at startup → Edit/Play confusion.

With F31, the natural hook is **OnBeginPlay**.

## Requirements

- BeginPlay auto-main when bool set
- Manual `SetSelfAsMainCamera` still works during Play
- Play view follows Main Transform
- Stop restores Edit camera behavior
- Unique-camera auto-Main optional (default: **yes if exactly one** CameraComponent)

## Constraints

- CameraComponent stays Runtime; no Editor includes
- Prefer Main mark + existing Sync over a parallel camera bus

## Proposed behavior

```text
OnBeginPlay:
  if bSetAsMainCameraOnBeginPlay:
    SetSelfAsMainCamera()

Play view:
  SyncPlayModeCamera / FindViewCameraInScene uses IsMainCamera

OnEndPlay / Stop:
  Editor viewport returns to edit navigation camera
  PIE clone destroyed; Edit doc objects were never BegunPlay
```

### API split

| API | Meaning |
|-----|---------|
| `IsMainCamera` / `m_bIsMainCamera` | Scene gameplay main view camera |
| `SetSelfAsMainCamera()` | Set Main mark; may also push Edit viewport preview — document; Play must not require that push |
| `bSetAsMainCameraOnBeginPlay` | Authoring: apply Main at BeginPlay |

## Responsibilities

| Unit | Responsible for | Not for |
|------|-----------------|--------|
| `CameraComponent` | Bool + OnBeginPlay policy; Main mark API | Scene DispatchBeginPlay |
| `SceneEditingViewportClient` | Follow Main while Playing; restore Edit on Stop | Game camera switching rules |
| F31 | Calling OnBeginPlay | Knowing which camera is main |

## Acceptance

1. Scene with Camera + bool: Play follows; Stop clean
2. Other comp can SetSelfAsMainCamera during Play; view updates
3. No camera: no crash
4. Progress verification notes

## Open questions

1. Copy-to-viewport vs SetActiveCamera — default **keep copy**
2. Auto-Main when exactly one camera — default **yes**
3. EndPlay clear Main on Edit objects — default **no** (Edit never BeginPlay)

## Design Review note

Non-blocking on F31. Implement after F31 BeginPlay reaches CameraComponent.

## 变更记录

| Date | Change |
|------|--------|
| 2026-09-23 | Draft from minCraft TD-008 |
| 2026-09-23 | Reframe: F31 consumer; bSetAsMainCameraOnBeginPlay; manual Set allowed |
| 2026-09-23 | Implement Done: OnBeginPlay + sole-camera auto-Main; suite \play-main-camera\ |
