# UI-F08 ScreenUI Widget Pointer Interaction — Implementation Plan

## Meta
- **ID:** `UI-F08`
- **Status:** `Review`
- **Owner:** project maintainer
- **Last updated:** `2026-10-18`
- **Related:** [Design](./UI-F08_SCREENUI_WIDGET_POINTER_INTERACTION_DESIGN.md)
- **Design readiness:** `Ready with deferred items`

## Goal

Multi-button pointer state + modifiers; Widget Enter/Leave/Down/Up/Click; Button sugar; Drag `InitiatingButton`.

## Slices

| Slice | Summary | Verify | Status |
|-------|---------|--------|--------|
| S01 | Types + multi-button UISystem state machine + modifiers | unit via UpdatePointerFrame | Done |
| S02 | Widget pointer delegates | screen-ui-pointer | Done |
| S03 | Button OnPointerClick + OnClicked Left sugar | screen-ui-button + pointer | Done |
| S04 | Drag InitiatingButton + per-button click suppress | screen-ui-drag + pointer | Done |

## Verification

```text
minEngineTests.exe test screen-ui-pointer
minEngineTests.exe test screen-ui-button
minEngineTests.exe test screen-ui-drag
```
