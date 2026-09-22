# <FEATURE_ID>_<SLUG> Implementation Plan

> **Filename (required):** `docs/ai/<DOMAIN>/<FEATURE_ID>_<SLUG>_IMPLEMENTATION.md` — ALL CAPS.
> Example: `docs/ai/CORE/CORE-F01_EVENT_BUS_IMPLEMENTATION.md`
>
> This plan must trace back to a Design Spec. Avoid becoming a bare TODO list.

## Meta
- **ID:** `<DOMAIN>-F<nn>`
- **Status:** `Draft | Planned | In Progress | Review | Done | Blocked | Deferred | Cancelled`
- **Owner:** `<name>`
- **Last updated:** `YYYY-MM-DD`
- **Related:** `[Design Spec](./<FEATURE_ID>_<SLUG>_DESIGN.md)`
- **Design readiness:** `Ready | Ready with deferred items` (must not be `Not ready` / `Draft-only`)

## Goal

Define independently verifiable slices that implement an already-reviewed design.

## Design Decisions Being Implemented

| Decision ID / title | Design section | Notes |
|---------------------|----------------|-------|
| `<D1>` | `<section>` | `<brief>` |

## Implementation Slices

| Slice ID | Summary | Design decision | Dependencies | Verification | Status |
|----------|---------|-----------------|--------------|--------------|--------|
| `<FeatureID>-S01` | `<summary>` | `<D1>` | `<dep>` | `<command/check>` | Planned |
| `<FeatureID>-S02` | `<summary>` | `<D2>` | `<dep>` | `<command/check>` | Planned |

## Slice → Design Traceability

For each slice (or group closely related slices):

### `<FeatureID>-S01`

- **Design decision:**
- **Implementation:**
- **Verification:**

### `<FeatureID>-S02`

- **Design decision:**
- **Implementation:**
- **Verification:**

## Integration Points

- Modules / APIs touched:
- Data contracts:
- Feature flags / rollout hooks (if any):

## Risks / Mitigations

- Risk:
- Mitigation:

## Verification per Slice

Reuse profile `verification_bar`. Record commands/results in Progress Log when slices complete.

## Migration / Compatibility

- Steps required during rollout:
- Backward-compat checks:
- Removal plan for transitional paths:

## Deferred Work

| Item | Why deferred | Unblock condition |
|------|--------------|-------------------|
| `<item>` | `<reason>` | `<condition>` |

## Completion Criteria

- [ ] All in-scope slices Done or explicitly Deferred with note
- [ ] Design acceptance criteria covered or mapped to deferred work
- [ ] Docs DoD + engineering DoD satisfied
- [ ] Ready to propose prepare commit

## Blocked/Deferred note (required if applicable)

- Reason:
- Impact:
- Unblock condition:
- Next check date:
