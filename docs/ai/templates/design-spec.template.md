# <FEATURE_ID>_<SLUG> Design Spec

> **Filename (required):** `docs/ai/<DOMAIN>/<FEATURE_ID>_<SLUG>_DESIGN.md` — ALL CAPS.
> Example: `docs/ai/CORE/CORE-F01_EVENT_BUS_DESIGN.md`
>
> Fill only relevant sections. If a section does not apply, write `Not applicable because ...`.
> Behavior guide: `.agents/skills/engineering-design/SKILL.md`
> Review guide: `.agents/skills/design-review/SKILL.md`

## Meta
- **ID:** `<DOMAIN>-F<nn>`
- **Type:** `Feature | Refactor`
- **Complexity:** `L0 | L1 | L2 | L3`
- **Status:** `Draft | Planned | In Progress | Review | Done | Blocked | Deferred | Cancelled | Snapshot | Archived | Reference`
- **Owner:** `<name>`
- **Last updated:** `YYYY-MM-DD`
- **Related:** `[Feature Registry](../FEATURE_REGISTRY.md)`

## TL;DR

3-5 lines: problem, proposed approach, current status.

## Problem

What is wrong or missing today?

## Requirements

- `<must-have>`

## Constraints

- `<hard limit>`

## Preferences

- `<soft preference>`

## Assumptions

- `<unverified belief>`

## Non-Goals

- `<explicitly out of scope>`

## Current Architecture

What exists today that this change must respect or extend?
(modules, APIs, data/control flow, constraints)

## Proposed Architecture

Describe the intended structure and how it fits the current system.
Use domain-neutral terms (`OrderService`, `Cache`, `EventBus`, …) unless the project domain requires otherwise.

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `<module/type>` | `<...>` | `<...>` |

## Boundaries

- Allowed interactions:
- Forbidden interactions / dependency rules:

## Dependencies

- Depends on:
- Depended on by:
- New dependency direction justified because:

## Ownership & Lifetime

- Who owns key state?
- Who creates / destroys / invalidates?
- Sharing / reference semantics:
- `Not applicable because ...` if irrelevant

## Data Flow

`Input → Processing → Storage → Output` (adapt as needed)

## Control Flow

Who calls whom? Note async/events/callbacks/messages explicitly.

## State & Invariants

- States / transitions:
- Illegal combinations:
- Invariants and who maintains them:
- `Not applicable because ...` if irrelevant

## API / Interface Semantics

For each important API:
- Responsibility:
- Inputs / outputs:
- Ownership / mutation:
- Errors:
- Sync/async:
- Thread-safety (if relevant):

## Failure Model

Relevant failure modes and expected behavior (only those that matter here).

## Alternatives & Trade-offs

### Option A (recommended)

- Summary:
- Pros:
- Cons:

### Option B

- Summary:
- Why not selected:

## Integration Impact

Impact on existing modules, callers, data, and ops.

## Migration / Compatibility

- Compatibility promises:
- Migration / rollout:
- Transitional code and exit condition:

## Verification Strategy

- Core behavior checks:
- Failure-path checks (if relevant):
- Integration / compatibility / performance (if relevant):

## Open Questions

- `<question>` — blocking? yes/no

## Design Review

- **Verdict:** `Pending | Ready | Ready with deferred items | Not ready`
- **Reviewer / date:**
- **Link or summary:**

## Implementation Readiness

- [ ] Requirements understood
- [ ] Constraints identified
- [ ] Relevant existing architecture inspected
- [ ] Responsibilities and boundaries defined
- [ ] Key dependencies understood
- [ ] Ownership/lifetime resolved or N/A
- [ ] Major state/invariants resolved or N/A
- [ ] API semantics sufficiently defined
- [ ] Important failure modes considered
- [ ] Alternatives considered where meaningful
- [ ] Integration impact understood
- [ ] Verification strategy exists
- [ ] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes | no` — if no, list blockers.

## Acceptance Checklist

- [ ] Design review complete when required by complexity/profile
- [ ] Implementation plan traces to design decisions (L2+)
- [ ] Progress log updated when status changes
- [ ] Feature registry status synced
