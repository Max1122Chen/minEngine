# CORE-F32 Enhanced Input Hold / InputTriggerDown Events — Design Spec

## Meta
- **ID:** CORE-F32
- **Type:** Feature
- **Complexity:** L1
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-24
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · CORE-F29 · minCraft feedback ENGINE_FEEDBACK_PROMPT_ENHANCED_INPUT_HOLD_AND_DOWN

## TL;DR

Fix Enhanced Input so InputTriggerDown Boolean actions fire Started on press and Canceled on release. Root causes: finalize used polluted tracker state as prev (not LastState); release frames skipped evaluation. Keep no-trigger Triggered path for Jump-style binds.

## Problem

Verified in InputSystem::Tick (2026-09-24):
- **A:** finalize `prevState = TriggerStateTracker.GetState()` after mapping wrote NoTrigger=Triggered; Down evaluates to Ongoing → event Ongoing, never Started.
- **B:** mapping only queues Press|Down; Release/Idle skips finalize → Ongoing→None (Canceled) never computed.
- Comment claimed NoTrigger == InputTriggerDown but NoTrigger uses Triggered (correct for Jump).

## Requirements

1. InputTriggerDown: press → Started; release → Canceled; hold may emit None between.
2. prevState = LastState (stable end-of-prior-frame).
3. Instances with LastState Ongoing/Triggered re-evaluated with value 0 when no key drive this frame.
4. NoTrigger stays Triggered-while-held (Jump); comment must not claim Down.
5. Regression: no-trigger Boolean Triggered still works.
6. Automated suite.

## Non-Goals

- Game-side KeyDown bypass
- Remodel IMC priority
- Mapping-only triggers redesign (pre-existing; out of this slice)

## Proposed fix

1. Reset TriggerStateTracker before finalize evaluate; prevState = LastState.
2. After mapping: enqueue release re-eval for LastState Ongoing|Triggered not updated this frame (Value=0).
3. Fix NoTrigger comment.
4. Suite `input-trigger-down`.

## Acceptance

- [x] Down+Started/Canceled automated (`input-trigger-down`)
- [x] Jump-style no-trigger Triggered regression
- [x] Comment fixed (NoTrigger ≠ InputTriggerDown)

## Changelog

| Date | Note |
|------|------|
| 2026-09-24 | Done: LastState prev + release re-eval; suite PASS |
| 2026-09-24 | Open from minCraft GP feedback; bugs A/B verified |
