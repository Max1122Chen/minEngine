# ED-F23 Viewport Pointer Capture UX — Design Spec

## Meta
- **ID:** ED-F23
- **Type:** Feature
- **Complexity:** L1
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-24
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · BUG-EDITOR-004 · CORE-F29 · CORE-F05-S03

## TL;DR

Unify Edit navigate and PIE pointer handling under one viewport policy (clip + Raw intent + optional hide). PIE defaults to Captured (hidden+clip); Esc stops Play; Left Alt releases to Editor (gameplay IMC gated off); click viewport to recapture. Fix navigate cursor flicker vs ImGui.

## Problem

- Navigate used GLFW_CURSOR_DISABLED; PIE used ClipCursor — not the same path; flicker when ImGui restores cursor.
- PIE had no Esc-stop / Alt-release capture modes; Released while Playing still fed gameplay IMC (F29 only checks BeginPlay).

## Requirements

- Single ViewportPointerPolicy for Navigate / PieCaptured / PieReleased / Off.
- Navigate: hide + clip to Scene image + Raw intent (same as Captured physically).
- PIE Enter to Captured; Esc to Stop; Left Alt to Released; LMB on viewport image to Captured.
- Released: show cursor, no clip, EditorGameplayCaptureActive=false so IMC gated.
- No flicker while hidden (ImGui must not override cursor).

## Constraints

- Win32-first (existing ClipCursor + Raw Input); other platforms degrade.
- Esc must not steal Console / inline rename text focus (WantTextInput).

## Non-Goals

- minCraft yaw euler round-trip
- Game-side Pointer Lock API
- Multi-viewport capture
- Configurable release key (defer ED-F10)

## Proposed Architecture

SceneEditingViewportClient owns mode + ApplyPointerPolicy each EndFrame.
InputSystem::SetEditorGameplayCaptureActive(bool) — when Editor viewport exists and world has begun play, gate requires capture true.

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| SceneEditingViewportClient | Mode, Esc/Alt/click, clip/hide/ImGui flag | Gameplay look math |
| InputSystem | Capture bool in F29 gate | Key binding policy |
| PlayInEditorSession | Enter/Stop lifecycle hooks (clear on Stop) | Cursor drawing |

## State

Off | EditNavigate | PieCaptured | PieReleased

## Failure / Cleanup

Stop / dtor / leave Play to Off; clear clip; show cursor; capture flag reset true for next Play.

## Verification

- Navigate: no flicker; clip; look at edge works.
- PIE Captured to Esc Stop; Alt to editor usable, no look; click to Captured.
- gameplay-input-gate: Playing + capture false to disabled.

## Acceptance

### S01
- [x] Navigate uses clip+hide; no ImGui flicker
### S02
- [x] PIE Captured/Released + Esc/Alt + capture gate + test
### S03
- [x] Viewport hint Esc/Alt

## Slices

| Slice | Goal |
|-------|------|
| S00 | Registry + this Design |
| S01 | Policy + Navigate |
| S02 | PIE modes + gate |
| S03 | Hint overlay |

## Deferred

Pause/console auto-release; status bar; click-to-capture-on-Play; remappable Alt.

## Changelog

| Date | Note |
|------|------|
| 2026-09-24 | In Progress from plan |

