# Design Principles Card

Authority for *named lenses* used during engineering design and implementation.  
Behavior: `.agents/skills/engineering-design/SKILL.md` · Review: `design-review` · Coding-time: `implementation-discipline` (when on).

Keep this card short. **Do not** paste a SOLID matrix into every Design Spec.

## Lenses → existing design questions

| Lens | Operational rule | Hook |
|------|------------------|------|
| High cohesion | One clear purpose per unit | Responsibilities |
| Low coupling | Narrow interfaces; limit ripple | Boundaries, Dependencies |
| SRP | One primary reason to change | Responsibilities |
| OCP | Prefer extend via existing points; no speculative hooks | Reuse/extend + simplicity |
| LSP | Implementations honor caller contracts | API semantics |
| ISP | Clients must not depend on unused surface | Boundaries, API |
| DIP | Policy depends on abstractions when churn/tests justify it | Dependencies |
| YAGNI | No speculative generality | Overengineering check |
| DRY (knowledge) | One authoritative place for a rule/fact | implementation-discipline |

## Anti-abuse

- Lenses produce **concrete findings**, not checkbox rows.
- “Followed OCP” with no structural consequence = shell product (Quality DoD fail).
- OCP ≠ add interfaces/plugins everywhere.
- Non-OO code: apply cohesion/coupling/ISP/DIP ideas to modules; skip LSP if no substitution.

## Good / bad micro-examples

**Comment**

- Bad: `// TMPL-F05-S01: add branch rule`
- Good: `// Branch names describe area of change; registry IDs stay in docs.`

**Branch**

- Bad: `feat/core-f01-event-bus`, `feat/TMPL-F05`
- Good: `feat/render`, `feat/render/material`, `feat/agent-workflows`

**Design / plan shell**

- Bad: Acceptance = “more maintainable”; plan step = “add appropriate tests”
- Good: Acceptance = “DoD skill lists Process and Quality sections”; plan step = “Edit `workflow-dod/SKILL.md`; grep for Quality DoD”

## Public references (names only; do not copy long texts)

- Cohesion / coupling — classic modular design (Constantine/Yourdon tradition; SE coursework)
- SOLID — R.C. Martin design principles; OCP also Bertrand Meyer; overview: [Wikipedia SOLID](https://en.wikipedia.org/wiki/SOLID)
- YAGNI / simplest that works — common agile/XP practice
- Coupling via interfaces — e.g. Fowler on reducing coupling

Language-specific style guides remain repo-local and out of scope here.
