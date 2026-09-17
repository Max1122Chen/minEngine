# Design Review — Reference

Companion to `SKILL.md`. Use when needed; do not dump this entire file into every report.

## Vague / soft language — probe list

When these appear, ask what **observable contract** is meant (do not auto-fail on the word alone):

automatically, stable, reliable, efficient, seamless, flexible, extensible, thread-safe, asynchronous, persistent, real-time, lightweight, support, handle, manage, optimize, correctly, transparently, robust, scalable, simple, smart, proper, appropriate, as needed, etc.

### Probe template (async / loading example)

If the design says “async” / “background” / “non-blocking”:

| Topic | Question |
|-------|----------|
| Blocking | Does any API still block the caller? Which thread? |
| Completion | How is success signaled? |
| Failure | How are errors reported and to whom? |
| Cancel | Can work be cancelled? What happens to resources? |
| Lifetime | Who owns in-flight work? What if owner is destroyed? |
| Ordering | Are completions ordered? Can they reorder? |
| Duplicates | Same request twice — coalesce, queue, or error? |
| Visibility | When may consumers observe the resource / result? |

Adapt the same table for “persistent”, “thread-safe”, “real-time”, etc.

## Dimension prompts (A–K)

### A. Requirements and Scope

- Problem clearly defined? Goals / non-goals explicit?
- Scope appropriate? Scope creep?
- Important user/system scenarios missing?
- Requirements objective enough to evaluate?

### B. Terminology and Concept Clarity

- Key terms defined? Similar concepts distinguished?
- Names consistent across sections?
- Overloaded terms? Responsibilities on the right concepts?

### C. Architecture and Boundaries

- Responsibilities coherent? Dependency direction clear?
- Ownership / lifecycle boundaries explicit?
- Public contracts stable enough for the stage?
- Implementation details leaking into architecture?
- Core vs Plugin / mechanism vs policy (philosophy)?

### D. Data Model and State

- Important structures/concepts defined?
- State transitions clear? Valid/invalid states?
- Invariants explicit?
- Persistence / compatibility rules when relevant?

### E. Workflow Completeness

- Normal and exceptional paths covered where relevant?
- Failure / cancel / retry / reload / destroy addressed?
- Could two implementers interpret workflows differently?

### F. API and Behavioral Contracts

- Inputs, outputs, side effects, errors, lifetime clear?
- Sync/async semantics defined?
- Threading / reentrancy expectations where relevant?
- Ownership and invalidation rules explicit?

### G. Error Handling and Recoverability

- What happens on failure? Partial failure → invalid state?
- Recoverable? Errors observable / diagnosable?

### H. Compatibility and Migration

When relevant: existing data/API compat, migration, versioning, old assets/projects, rollback.

### I. Acceptance and Testing

- Acceptance criteria objective and observable?
- Important requirements mapped to a validation method?
- Edge / failure paths covered? Plan practical?

### J. Implementation Readiness

- Can implementation begin?
- Blocking decisions vs safe-to-defer details?
- Implementation sequence clear enough?

### K. Agent Friendliness

- Implicit assumptions? Missing contracts? Ambiguous terms?
- Unclear module/file boundaries? Unclear acceptance?
- Missing validation commands/procedures?
- Steps that need undocumented human knowledge?
- Hard-to-verify decisions?

Do **not** optimize solely for agents — keep human-engineer clarity. Prefer shared Engine query/command/reflection/serialization surfaces over a separate Agent Framework (`ENGINE_DESIGN_PHILOSOPHY.md`).

## Contradiction patterns (examples)

| Pattern | What to check |
|---------|----------------|
| Dual ownership | Same object “owned by A” and “owned by B” |
| Sync vs async | Sections disagree on blocking / thread |
| Immutable vs mutable | Same resource described both ways |
| Error API without path | “Returns error” but workflow has no failure branch |
| Optional-but-required | Dependency marked optional yet required for happy path |
| Lifecycle clash | Init order conflicts with shutdown / reload |
| Editor-only → Runtime | Feature scoped to Editor later assumed in Runtime |
| Versioned without policy | Format “versioned” but no read/compat behavior |
| Forward-only vs migrate | Claims break compat but no migration/out-of-scope note |

## Readiness labels (definitions)

| Label | Meaning |
|-------|---------|
| **Ready to implement** | Blocking decisions resolved or explicitly deferred with safe defaults; acceptance/test strategy adequate for stage; no unresolved contradictions that affect the first slice |
| **Ready after targeted clarifications** | Small set of blocking Qs or section edits; implementation should wait for those answers (or record assumptions in Design) |
| **Not ready** | Fundamental contradictions, missing core contracts, or unresolved alternatives that would force large rework if coding starts now |

## Follow-up review (Mode B extras)

When re-reviewing a revised Design:

1. Diff Meta / changelog / Acceptance checkboxes vs prior review if available.  
2. Re-check every prior **Blocking** finding — resolved, deferred with note, or still open?  
3. Hunt **new** contradictions introduced by edits (especially Scope, Ownership, Workflow, Compat).  
4. Watch for requirements that silently disappeared.  
5. Update readiness label; do not keep an old “Ready” without re-justification.

## vs engine-system-review (quick)

| | `design-review` | `engine-system-review` |
|--|-----------------|------------------------|
| Subject | Design docs + context | Code + subsystem behavior |
| Gate | Before / during design freeze | 0.1.0 / merge / foundation audit |
| Primary output | Implementation readiness | Correctness + release buckets |
| Default mutate | No Design edits | No code edits |
