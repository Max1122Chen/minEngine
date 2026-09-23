# CORE-F29 — Play-gated gameplay Input — Design Spec

## Meta
- **ID:** `CORE-F29`
- **Type:** Feature
- **Complexity:** L1–L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-23
- **Related:**
  - **Depends:** [CORE-F31](./CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md)
  - Sibling: [CORE-F30](./CORE-F30_PLAY_MAIN_CAMERA_BINDING_DESIGN.md)
  - Play Mode: [CORE-F05](./CORE-F05_PLAY_MODE_DESIGN.md)
  - Catalyst: minCraft TD-007
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F31` (≥ Planned); existing Enhanced Input
- **Blocks:** Edit-safe gameplay IMC/IA for all game projects

## TL;DR

With **F31**, gameplay input gating is a **switch**: while not simulating, `InputSystem` does **not** update/dispatch gameplay IMC→IA. Play / standalone: normal. Stop/EndPlay: stop cleanly, no residual Triggered.

## Scope

### In
- Gate in `InputSystem` driven by F31 simulation / Play state
- Edit: gameplay mappings do not drive IA
- Play/PIE + standalone: normal dispatch
- Stop/EndPlay: no residual Triggered
- Document coexistence with Editor IMC / Consume / priority
- Verify: Edit WASD ≠ gameplay binds; Play works; Stop stops

### Out
- Local multiplayer PlayerInput split
- Forcing per-frame KeyDown sampling
- Owning BeginPlay itself (F31)
- Second input stack

## Reader quick start
1. Dependency on F31
2. Gate policy
3. Acceptance

---

## Problem

`InputSystem::Tick` has no Play filter. Editor hub/viewport gates ≠ Runtime IA dispatch. Register+AddIMC at Host/Activate still runs in Edit.

Root fix: **F31 simulation lifecycle**; F29 applies the **enable switch**.

## Requirements

- Non-simulating editor world: gameplay IA off
- Simulating: on
- Stop: immediate off, no stale Triggered
- Standalone: simulating after world BeginPlay
- Do not break Editor-owned IMCs

## Constraints

- Runtime Input must not depend on Editor types; use F31 simulation query
- Prefer one gate site in InputSystem

## Gate policy (default)

```text
if (!IsGameplayInputEnabled())
  skip updating / dispatching gameplay IMC → IA
else
  existing Tick path
```

`IsGameplayInputEnabled()` comes from F31 Scene/SceneManager simulation state.

Optional: mark IMC / InputComponent as Gameplay vs Editor — only Gameplay gated. Inventory in Impl.

Complementary (allowed): comps Register in OnBeginPlay / unregister OnEndPlay **and** system gate.

## Acceptance

1. Edit: gameplay binds silent
2. Play: binds work
3. Stop: silent; no residual Triggered
4. Editor shortcuts/viewport still usable in Edit
5. Progress records test or hand steps

## Open questions

1. Inventory Editor-owned IMCs (Impl)
2. Pause: gate = simulating/IsPlaying only (no EndPlay on Pause) — aligned with F31

## Design Review note

Non-blocking on F31. Implement **after** F31 exposes simulation query + Enter/Stop dispatch.

## 变更记录

| Date | Change |
|------|--------|
| 2026-09-23 | Draft from minCraft TD-007 |
| 2026-09-23 | Reframe: Depend F31; gate-switch model |
| 2026-09-23 | Implementation Done: IsGameplayInputEnabled gate + ClearGameplayActionEvents; suite gameplay-input-gate |
