# CORE-F31 — GameObject / Component BeginPlay · EndPlay — Design Spec

## Meta
- **ID:** `CORE-F31`
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-23 (Done)
- **Related:**
  - Consumers: [CORE-F29](./CORE-F29_PLAY_GATED_GAMEPLAY_INPUT_DESIGN.md) · [CORE-F30](./CORE-F30_PLAY_MAIN_CAMERA_BINDING_DESIGN.md)
  - Activate axis (orthogonal): [CORE-F06](./CORE-F06_COMPONENT_ENABLE_DESIGN.md)
  - Play Mode: [CORE-F05](./CORE-F05_PLAY_MODE_DESIGN.md)
  - UE anchors (read-only): `AActor::DispatchBeginPlay` / `BeginPlay` / `EndPlay` · `UActorComponent::BeginPlay` / `EndPlay`
  - Catalyst: minCraft PIE Input/Camera feedback (TD-007/008) — missing sim lifecycle
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F05` · `CORE-F06` (Activate remains separate)
- **Blocks:** principled Play-only behavior for Input, Camera, and future gameplay comps

## TL;DR

minEngine has **Activate** (`OnActivate`/`OnDeactivate`) but no **simulation lifecycle**. Edit-loaded components are already Active, so Register-IMC / camera setup at Activate leaks into the editor. Add **BeginPlay / EndPlay** on `GameObject` that **forwards to Components** (UE Actor pattern), dispatched by Scene when simulation starts/stops. F29/F30 become thin consumers.

## Scope

### In
- `GameObject::BeginPlay` / `EndPlay` (+ `HasBegunPlay`)
- `Component::BeginPlay` / `EndPlay` (+ `HasBegunPlay`); override hooks for subclasses
- **Routing order (UE-aligned):**
  - BeginPlay: GO enters Beginning → **each Active+eligible Component BeginPlay** → GO virtual hook → GO HasBegunPlay
  - EndPlay: only if HasBegunPlay → GO EndPlay hook → **each Component that HasBegunPlay** → clear flags
- `Scene::DispatchBeginPlay` / `DispatchEndPlay` iterating GOs
- Triggers:
  - **Editor PIE:** after successful `EnterPlay` clone+register → DispatchBeginPlay on PIE Scene; `Stop` → DispatchEndPlay before teardown
  - **Standalone / game host:** when gameplay Scene becomes the running world (first ready-to-tick), DispatchBeginPlay once
- **Editor document Scene:** never DispatchBeginPlay while in Edit
- Dynamic spawn: if Scene has already begun play, newly added GO gets BeginPlay when activated into that Scene
- Minimal `EEndPlayReason`: `Stopped` / `Destroyed`
- Query: Scene or SceneManager simulation-has-begun for systems (Input gate)

### Out
- Full UE stack: `OnRegister`, `InitializeComponent`, `PostInitializeComponents`, net deferral, Blueprint `Receive*`
- Replacing Activate with BeginPlay
- GameObject-level `bActive` hierarchy
- Pause/Step (CORE-F05-S05 Deferred) — Pause does **not** call EndPlay
- Forcing all existing comps to override BeginPlay (base + dispatch only; F29/F30 consume)

## Reader quick start
1. Mental model (Activate vs BeginPlay)
2. UE mapping + order
3. Dispatch triggers
4. Acceptance

---

## Problem

| Axis | Exists today | Gap |
|------|--------------|-----|
| Intent in world | `m_bActive` → OnActivate/OnDeactivate | — |
| Simulation running | PIE clone + System OnBeginPIE | **No GO/Component BeginPlay/EndPlay** |

Activate fires on Edit load and on PIE duplicate. Play-only setup cannot hang there without Edit side effects.

## Requirements

- Once-per-sim BeginPlay / EndPlay for GOs in a simulating Scene
- GO owns routing to Components (UE-like)
- Activate remains independent
- Standalone and PIE share the same Component API
- Safe if no overrides (no-op base)
- Double-dispatch guarded by HasBegunPlay

## Constraints

- Runtime must not include Editor types; Editor calls Scene dispatch APIs
- Forward-only: one path
- Philosophy: mechanism in Core; Camera/Input policies stay on those comps

## Current Architecture

| Piece | Today |
|-------|-------|
| `Component` | Tick, OnActivate/OnDeactivate — **no BeginPlay** |
| `GameObject::Tick` | Forwards Tick to components |
| `Scene::Tick` | Ticks GOs when Gameplay tick selected |
| `PlayInEditorSession::EnterPlay` | DuplicateForPIE + System OnBeginPIE — **no GO BeginPlay** |
| Editor Scene during Play | TickPolicy **None** |

## Proposed Architecture

### Mental model

```text
Activate   = in a Scene and intended to participate in systems (Edit + Play)
BeginPlay  = this simulating world has started gameplay (Play / standalone only)
EndPlay    = this simulating world is ending for this GO (Stop / destroy-after-begun)
```

### UE mapping (source-backed)

| UE | minEngine MVP |
|----|---------------|
| `AActor::DispatchBeginPlay` → `BeginPlay` | `Scene::DispatchBeginPlay` → each `GameObject::BeginPlay` |
| `AActor::BeginPlay`: **Components first**, then `ReceiveBeginPlay` | Same: Components then GO virtual `OnBeginPlay` |
| `AActor::EndPlay`: Actor notify, then **Components** | Same: GO `OnEndPlay` then Components |
| `UActorComponent::BeginPlay` / `EndPlay` + `bHasBegunPlay` | `Component::BeginPlay` / `EndPlay` + `HasBegunPlay()` |
| World has begun play | Scene flag `HasBegunPlay()` |

UE refs:
- `Engine/Source/Runtime/Engine/Private/Actor.cpp` — `DispatchBeginPlay` (~4681), `BeginPlay` (~4737), `EndPlay` (~3207)
- `Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp` — `BeginPlay` (~1541), `EndPlay` (~1557)

### API sketch (illustrative)

```text
GameObject:
  void BeginPlay();
  void EndPlay(EEndPlayReason);
  bool HasBegunPlay() const;
protected:
  virtual void OnBeginPlay() {}
  virtual void OnEndPlay(EEndPlayReason) {}

Component:
  void BeginPlay();
  void EndPlay(EEndPlayReason);
  bool HasBegunPlay() const;
protected:
  virtual void OnBeginPlay() {}
  virtual void OnEndPlay(EEndPlayReason) {}

Scene:
  void DispatchBeginPlay();
  void DispatchEndPlay(EEndPlayReason);
  bool HasBegunPlay() const;
```

Prefer `OnBeginPlay` / `OnEndPlay` to match existing `OnActivate` (not UE `Receive*` unless Script needs it later).

### Eligibility

Component receives BeginPlay only if owner is dispatching, `IsActive()`, and `!HasBegunPlay()`.

If a component becomes Active later while Scene `HasBegunPlay()` and owner GO already `HasBegunPlay()`, call that Component's `BeginPlay()` only.

### Dispatch triggers

| Host | When BeginPlay | When EndPlay |
|------|----------------|--------------|
| PIE | After PIE Scene registered + systems OnBeginPIE | Stop: GOs EndPlay **before** destroying PIE Scene / system OnEndPIE |
| Standalone | After gameplay Scene loaded and ready to tick (once) | Scene unload / shutdown if HasBegunPlay |
| Edit document | **Never** | n/a |

Default order: **System OnBeginPIE first, then Scene::DispatchBeginPlay**. Stop: **DispatchEndPlay first, then System OnEndPIE**.

### Dynamic spawn during Play

If `Scene::HasBegunPlay()` and a new GO is added (or pending activation fulfilled): call that GO's `BeginPlay()`.

### Relationship to Tick

MVP: **do not** block Tick before BeginPlay.

## Responsibilities

| Unit | Responsible for | Not for |
|------|-----------------|--------|
| `Scene` | Dispatch begin/end; scene HasBegunPlay | Per-comp gameplay |
| `GameObject` | Route to comps; GO hooks | Editor viewport |
| `Component` | Override OnBeginPlay/OnEndPlay | Knowing Editor vs PIE |
| `PlayInEditorSession` | Call Scene dispatch at Enter/Stop | Per-comp logic |
| Input/Camera Features | Consume hooks / gates | Owning lifecycle |

## Failure / edge cases

- Double DispatchBeginPlay: no-op if Scene already HasBegunPlay
- EndPlay without BeginPlay: no-op
- Destroy during BeginPlay: best-effort EndPlay if begun; log
- Prefab Stage: Play blocked today — no special BeginPlay until Stage Play exists

## Alternatives considered

| Option | Why not default |
|--------|-----------------|
| Only Component BeginPlay, no GO | Diverges from UE; games lose GO-level hook |
| Piggyback OnActivate as BeginPlay | Breaks Edit; F06 already separated axes |
| Editor-only Input gate without lifecycle | Fixes one symptom; Camera/future comps stay ad hoc |

## Acceptance

1. PIE EnterPlay → every Active Component in PIE Scene receives OnBeginPlay exactly once; Stop → OnEndPlay; Edit document comps never receive BeginPlay for that Edit session
2. Subclass override works (test double or F29/F30 consumer)
3. Activate still works in Edit without BeginPlay
4. Dynamic add during PIE gets BeginPlay
5. Automated suite preferred (`begin-play`); else documented hand steps
6. F29/F30 Depend on F31

## Open questions

| # | Question | Default | Blocking? |
|---|----------|---------|-----------|
| 1 | Order vs System OnBeginPIE | Systems then GOs | Soft |
| 2 | EndPlayReason width | `Stopped` · `Destroyed` | Soft |
| 3 | Gate Tick until BeginPlay | No | Soft |
| 4 | Script/Lua OnBeginPlay | Out of F31 | Soft |

## Design Review (2026-09-23 Mode A)

**Verdict:** Ready with deferred (soft opens only).

Absorbed defaults:
- System OnBeginPIE → then DispatchBeginPlay; Stop reverses
- Inactive comps skip BeginPlay; late Activate while simulating → Component BeginPlay only
- Tick-before-BeginPlay allowed in MVP
- Standalone trigger pinned in Impl at first ready-to-tick gameplay Scene

Findings (non-blocking):
- D01 Standalone call site must be named in Implementation Plan
- D02 Editor IMC vs Gameplay IMC inventory belongs to F29 Impl
- D03 Camera bool exact property name belongs to F30 Impl

## 变更记录

| Date | Change |
|------|--------|
| 2026-09-23 | Draft: UE Actor→Component routing; Activate≠BeginPlay; F29/F30 consumers |
| 2026-09-23 | Design-review absorb: dispatch order; late-Activate; Ready-with-deferred |
| 2026-09-23 | Implementation Done: API + PIE hooks + suite begin-play |
