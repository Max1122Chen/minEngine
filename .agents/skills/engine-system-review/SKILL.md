---
name: engine-system-review
description: "System-level engineering review for minEngine subsystems (0.1.0 readiness). Reconstruct intended design before judging code; evidence-based findings; separate Critical/Correctness from Future Enhancement. Use when user asks: 系统复盘, 工程 review, system review, subsystem review, 0.1.0 readiness review, architecture review of Serialization/Reflection/AssetManager/Scene/RHI/etc. Read-only by default; do not treat as generic bug hunt."
---

# Engine System Review

## Purpose

Perform a **system-level engineering review** of one minEngine subsystem for correctness, ownership/lifetime, contracts, and 0.1.0 foundation readiness.

**Not** a generic “find bugs in this file” pass.  
**Yes** reconstruct intended design → compare to implementation → produce an actionable, prioritized report.

Learning / portfolio context: engineering bar on foundations stays high; scope stays proportional to 0.1.0 (usable foundation, not commercial production grade everywhere).

## When to use

Use this skill when the user asks to:

- Review / 复盘 / audit a **subsystem** (Serialization, Reflection, AssetManager, Scene, RHI, Lua, Editor, …)
- Assess **0.1.0 readiness** of a system
- Produce a Prefab-style **engineering review report** before merge or release
- Distinguish **must-fix** vs **known limitation** vs **future enhancement**

Prefer **`engine-learning-mentor`** for day-to-day design/implement Pre-flight.  
Prefer **`git-commit-mentor`** for commit prep after review fixes land.

## When NOT to use

- Single-line typo / local compile fix
- “Write Feature X” implementation without a review ask
- Whole-engine “score the quality” requests — refuse overall numerical scores; review **one** subsystem (or a named cluster) per run

## Inputs (what the user should provide)

Minimum useful kickoff:

| Input | Required? | Notes |
|-------|-----------|--------|
| **Target subsystem** | **Yes** | Name or path cluster (e.g. AssetManager, `Runtime/Core/Serialization`) |
| **Review objective** | Strongly preferred | e.g. 0.1.0 readiness, pre-merge, ownership audit |
| **Explicit focus** | Optional | e.g. “lifetime on reload”, “Editor vs Runtime divergence” |
| **Out of scope** | Optional | Systems to ignore |
| **Write report to docs?** | Optional | Default: chat report; offer to save under `docs/ai/` when useful |

If the target is ambiguous, ask **one** clarifying question; otherwise discover from the repo.

## Hard rules

1. **Design before judgment** — reconstruct intended model (Phase 2) before listing defects.
2. **Evidence or uncertainty** — every finding needs paths/symbols/call chains **or** an explicit Open Question.
3. **No preference-as-bug** — Future Enhancement ≠ Correctness Issue.
4. **No drive-by rewrites** — recommendations proportional to stage; prefer small verifiable fixes.
5. **Read-only by default** — do **not** modify code during the initial review unless the user explicitly asks for fixes.
6. **No overall quality score** — use severity + release buckets only.
7. **Trust tiers** — Design Meta Status matters (`docs-trust-tiers`); code + tests win over stale docs.
8. **Philosophy** — apply `ENGINE_DESIGN_PHILOSOPHY.md` when judging coupling / Core vs Plugin / Agent-friendly surface (shared APIs, not a separate Agent Framework).

## Relationship to other skills

| Skill | Role |
|-------|------|
| `engine-learning-mentor` | Partner stance, Pre-flight, true-refactor discipline, docs collaboration |
| `git-commit-mentor` | After fix packs: prepare commit |
| This skill | Deep subsystem audit → structured report → release assessment |

After review, if the user asks to fix: treat findings as a backlog; use mentor Pre-flight before large refactors; file `BUG-*` for cross-module defects.

---

## Workflow (mandatory order)

### Phase 1 — Establish scope

Identify and state:

- Target subsystem and review objective
- Primary directories / file clusters
- Related systems **only if needed for correctness**
- Public API surface and major usage sites
- User-supplied focus / exclusions
- Review limitations (time, no runtime repro, generated code unread, …)

Do not wander into unrelated systems.

### Phase 2 — Reconstruct the system model

**Before** hunting issues, build an evidence-backed model:

- Responsibilities and **non**-responsibilities
- Core abstractions and public interfaces
- Ownership and lifetime
- Dependencies (who calls whom; Core vs Editor)
- State transitions and data flow
- Error-handling expectations
- Serialization / persistence contracts (if any)
- Editor vs Runtime boundaries
- Threading assumptions (default: assume main-thread unless proven otherwise)
- Extension points and invariants

Sources (prefer in order):

1. Code (`*.h` / `*.cpp`, generated Reflection/ScriptBinding if relevant)
2. Tests under `minEngine/Tests/`
3. In-Progress / recent Design docs named by user or linked from `ACTIVE_WORK` / `FEATURE_REGISTRY` (check Meta Status)
4. `TECH_DEBT.md` Open rows touching the system
5. Philosophy / Capability Roadmap only for boundary judgment — not as a bug checklist

Output in the report as **§ System Understanding**. Keep it concise (human-readable).

### Phase 3 — Inspect implementation vs model

Compare code to the reconstructed model. Investigate only **relevant** dimensions (see below).

Look for: contract violations, inconsistent state, missing validation, lifetime hazards, error-path bugs, resource leaks, invalidation / reentrancy issues, serialization inconsistency, Editor/Runtime divergence, hidden coupling, poor failure diagnosability.

Do **not** mechanically tick every dimension.

### Phase 4 — Trace important scenarios

Pick scenarios that matter for **this** subsystem. Examples:

- Init / normal op / partial failure / shutdown / reinit
- Reload, object destroy, resource unload, scene switch
- Editor edit, Undo/Redo, save/load
- Invalid input, duplicate registration, missing dependency
- Concurrent access **only if** the system claims or uses threads/async

Record which scenarios were traced and which were skipped (and why).

### Phase 5 — Validate findings

For each suspected issue:

1. Locate implementation
2. Trace call path
3. Confirm whether it is reachable
4. Note triggering conditions
5. Assess impact
6. Label **Confirmed** vs **Risk** vs **Uncertain**

Use tests, build config, or related systems when useful.  
Do not invent runtime behavior.

### Phase 6 — Produce the report

Write the structured report (chat and/or saved doc). Prefer signal over volume; omit low-value nitpicks.

**Template:** copy `docs/ai/templates/system-engineering-review.template.md`  
**Skill-local copy:** `.agents/skills/engine-system-review/report.template.md`

**Save location (when user wants a durable artifact):**

| Subsystem area | Suggested path |
|----------------|----------------|
| Core / Platform | `docs/ai/Platform/<Topic>/<NAME>_SYSTEM_REVIEW.md` |
| Editor | `docs/ai/Editor/<NAME>_SYSTEM_REVIEW.md` |
| Render | `docs/ai/Render/<NAME>_SYSTEM_REVIEW.md` |
| Cross-cutting | `docs/ai/Platform/` or nearest owning domain |

Include Meta (Status Draft/Review/Done, date, related Feature IDs). Optionally append a short `PROGRESS_LOG` note when the review is a meaningful release gate.

---

## Review dimensions (apply when relevant)

| ID | Dimension | Typical questions |
|----|-----------|-------------------|
| A | Correctness | Intended behavior? Invariants? Edge cases? Invalid states? |
| B | Ownership / Lifetime | Who owns? Create/destroy? Dangling refs? Shutdown/reinit? |
| C | State / Consistency | Transitions? Cache invalidation? Stale/duplicate entries? |
| D | Error / Recoverability | Partial failure? Silent ignore? Corrupt after fail? |
| E | API / Boundaries | Responsibility split? Editor leak into Runtime? Stable surface? |
| F | Data integrity / Persistence | Serialize consistently? ID stability? Load of bad data? |
| G | Concurrency / Async | Explicit assumptions? Callback lifetime? |
| H | Performance | Only with evidence or clear complexity/resource argument |
| I | Observability | Logs/asserts? Diagnosable failures? |
| J | Extensibility / Agent-friendly | Shared query/command/reflection/serialization surface — **not** a separate Agent Framework |

### Finding categories (required taxonomy)

Use one primary category per finding:

- Critical Bug
- Correctness Issue
- Lifecycle / Ownership Issue
- Data Integrity Issue
- Error Handling / Recoverability Issue
- Concurrency / Thread-Safety Issue
- Architectural Risk
- Maintainability Issue
- Observability / Debuggability Issue
- Future Enhancement

**Future Enhancement must never be phrased as a current defect.**

### Severity

`Critical` | `High` | `Medium` | `Low` | `Informational`

### Confidence

`Confirmed` | `Likely` | `Possible` | `Unknown` — with what would raise confidence.

### Release buckets (0.1.0)

- **Must Fix Before 0.1.0**
- **Should Fix Soon**
- **Known Limitation / Acceptable for 0.1.0**
- **Future Enhancement**

Evaluate “sufficiently reliable and coherent for its intended role in the 0.1.0 foundation,” not “production-grade everywhere.”

---

## Required report structure

1. **Review Scope** — target, paths, related systems, focus, limitations  
2. **System Understanding** — responsibilities, abstractions, ownership, deps, invariants, transitions  
3. **Findings** — each with ID, Title, Category, Severity, Confidence, Evidence, Why, Trigger, Impact, Recommended action, 0.1.0 bucket  
4. **Positive Findings** — what works and should be preserved (**do not leave empty by default**)  
5. **Release Assessment** — classify all findings into the four buckets  
6. **Recommended Next Steps** — small, ordered, verifiable actions (not a mega-rewrite)  
7. **Open Questions** — unknowns needing more code, tests, repro, design decision, or user input  

Finding ID scheme: `R01`, `R02`, … (or `A1`/`B1` style if matching an existing review convention the user prefers).

---

## Interaction behavior

1. Inspect the repository before concluding.
2. Ask focused clarifications only when necessary.
3. State assumptions explicitly.
4. Prefer repo evidence over generic engine advice.
5. Avoid modifying code on the initial review unless asked.
6. If asked to fix: separate **review findings** from **implementation work**; fix in slices; re-verify.
7. Keep report size proportional to subsystem scope.
8. Avoid flooding with low-value observations.
9. Align recommendations with `ENGINE_DESIGN_PHILOSOPHY.md` and current `ACTIVE_WORK` / 0.1.0 window.
10. Partner stance: challenge “rewrite everything” urges; offer a default minimal path.

## Pitfalls (self-check before sending)

- [ ] Did I reconstruct the system model before listing issues?
- [ ] Is every finding evidenced or marked uncertain?
- [ ] Did I smuggle style preferences into Correctness?
- [ ] Are Future Enhancements clearly bucketed as such?
- [ ] Is the Must-Fix-before-0.1.0 list short and justified?
- [ ] Did I note positive findings worth preserving?
- [ ] Did I stay inside scope?

## Sample invocation

```text
用 engine-system-review 复盘 AssetManager（含 Create/Load/Save 与 meta Guid），
目标是 0.1.0 readiness。重点看身份稳定性与失败路径；先出报告，不要改代码。
```

Other examples:

- `System-review Serialization JSON/Binary contracts for 0.1.0 — Known Limitations OK if documented.`
- `工程 review：Scene / GameObject / Component lifetime 与 Editor PIE 路径是否一致。`
