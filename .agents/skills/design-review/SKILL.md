---
name: design-review
description: "Design / pre-implementation review for minEngine. Review design docs for clarity, consistency, completeness, testability, scope, architecture fit, shell-product red flags, and concrete design-principle findings—before coding or after multi-round design revisions. Use when user asks: design review, 设计评审, pre-implementation review, 开工前评审, 方案评审, review this Design Spec, check design readiness. Read-only on design by default; distinct from engine-system-review (code/subsystem audit)."
---

# Design Review (Pre-Implementation & Iterative)

## Purpose

Review a **design document and its surrounding context** to decide whether it is:

- Clear, internally consistent, and sufficiently complete
- Executable by an engineer or AI agent
- Testable and verifiable
- Appropriately scoped
- Compatible with existing minEngine architecture
- Explicit about important decisions and assumptions

Behave as a combination of **software architect**, **technical product manager**, **test/validation engineer**, and **careful design reviewer**.

**Not** an elegance contest.  
**Not** rubber-stamping a design because this agent wrote it, the user agreed earlier, it sounds UE-like, or the prose is polished.  
**Not** an implementation / subsystem code audit — use **`engine-system-review`** for that.

Learning / 0.1.0 context: keep foundations coherent; keep report **proportional** to design maturity. Do not demand every class signature on a high-level architecture draft.

## When to use

- New Feature / system Design Spec drafted or revised
- About to start implementation (“开工前看一眼设计”)
- After **multiple discussion rounds** — check contradictions, lost requirements, scope creep
- Architecture change, refactor plan, Editor/Runtime/Asset/Serialization/Render/UI/Anim/Tooling/MCP designs
- User asks: design review / 设计评审 / pre-implementation review / 方案评审 / design readiness
- Flag **shell-product red flags** (DOC_GOVERNANCE §7.3) and concrete principle issues (`DESIGN_PRINCIPLES.md`) — not checkbox theater

## When NOT to use

- Day-to-day Pre-flight only → **`engine-learning-mentor`**
- Code / 0.1.0 subsystem audit → **`engine-system-review`**
- Commit message prep → **`git-commit-mentor`**
- One-line typo in a Design heading
- “Implement Feature X now” with no review ask (unless user ties review to gate)

## Inputs

| Input | Required? | Notes |
|-------|-----------|--------|
| **Design doc path** (or paste) | **Yes** | Prefer `docs/ai/**/*_DESIGN.md` / `*_REFACTOR_PLAN.md` |
| **Review mode** | Preferred | **A** Pre-Implementation / **B** Iterative (auto-detect if omitted) |
| **Feature ID** | Preferred | e.g. `ED-F11`, `CORE-F23` |
| **User focus** | Optional | Specific worries to prioritize |
| **Discussion context** | Optional | Prior decisions; Mode B uses chat/Progress when available |
| **Save report?** | Optional | **Default: chat only** — do **not** write a report file unless the user explicitly asks to save under `docs/ai/` |

If the target Design is ambiguous, ask **one** clarifying question; otherwise discover from `ACTIVE_WORK` / `FEATURE_REGISTRY`.

## Relationship to other skills

| Skill | Role |
|-------|------|
| `engineering-design` | Produce / revise Design Spec (discovery, boundaries, ownership, failure, alternatives) |
| `engine-learning-mentor` | Partner stance, Pre-flight, docs collaboration, philosophy |
| `engine-system-review` | **Code** / subsystem engineering review after (or instead of) design |
| `git-commit-mentor` | After design clarifications or follow-up doc edits land |
| This skill | Design readiness / consistency → structured report **before** coding |

**Default gate:** substantial new Feature (L2+) → Design Status ≥ Planned → **design-review (Mode A)** with readiness verdict → then implement. Mentors Pre-flight still applies for Go/Defer. L3 always requires Implementation Readiness.

---

## Core review philosophy

### 1. Review the design, not the author's confidence

Do **not** treat a design as correct merely because:

- This agent proposed it
- It was discussed for many rounds
- The user previously agreed
- It sounds elegant or resembles a known engine
- The document is polished

Independently examine coherence and actionability.

### 2. Do not confuse missing detail with a defect

Distinguish:

1. Intentional abstraction  
2. Implementation detail that can be deferred  
3. Missing information that **blocks** implementation  
4. Missing information that creates **correctness risk**  
5. Ambiguity that can yield **inconsistent** implementations  
6. Genuine **architectural contradiction**  
7. **Future enhancement**

Do not demand unnecessary detail for completeness theater.

### 3. Prefer explicit contracts over vague descriptions

Watch for words like: automatically, stable, reliable, efficient, seamless, flexible, extensible, thread-safe, asynchronous, persistent, real-time, lightweight, support, handle, manage, optimize, correctly, transparently.

These are not automatically wrong. Ask:

> What **observable** behavior, constraint, or contract does this term imply?

Example — “Support asynchronous loading” → blocking? completion notify? failure? cancel? threading? lifetime? ordering? duplicate requests? resource visibility?

Flag the **ambiguity that matters**, not a word list dump. See `reference.md` for a checklist.

### 4. Review at the appropriate level

Avoid:

- Too shallow: only checking whether headings exist  
- Too deep: designing every class/function prematurely  

A high-level architecture proposal may omit exact signatures but must still define important **contracts, responsibilities, dependencies, lifecycle expectations, and acceptance criteria**.

---

## Review modes

### Mode A — Pre-Implementation Review

**When:** Design drafted; implementation about to begin.

**Primary question:**  
Is this design clear, complete, and testable enough to start implementation **without** substantial undocumented assumptions?

**Focus:** scope, requirements, completeness, contracts, workflows, dependencies, edge cases, acceptance criteria, test plan, implementation sequencing, **blocking** open decisions.

### Mode B — Iterative / Post-Discussion Design Review

**When:** Design evolved through multiple discussion/revision rounds.

**Primary question:**  
Is the design still internally consistent? Did revisions introduce contradictions, gaps, lost requirements, or unresolved assumptions?

**Focus:** changes vs earlier decisions, cross-section contradictions, lost requirements, new deps, unresolved alternatives, discussed-but-not-finalized decisions, outdated assumptions, scope creep, incomplete migration/compat.

Full discussion history is not required; use available chat, Progress, Design changelog, and ADRs when present.

**Auto-detect:** if Design Meta shows recent revision / Status Review / user says “改了几轮后再看” → prefer Mode B; if “准备开工” → Mode A. Can run both focuses in one report when useful (label sections).

---

## Hard rules

1. Inspect repo + Design Meta Status (`docs-trust-tiers`) before architectural claims; **code + tests** win over stale docs for “what exists today.”
2. Review independently — even if this agent authored the Design.
3. Evidence from Design sections and/or repo; mark uncertainty explicitly.
4. Do **not** invent requirements.
5. Do **not** treat every omission as a defect.
6. Ask focused questions only when they materially affect correctness or implementation.
7. Prefer explicit assumptions / decision records over vague prose.
8. Distinguish **blocking** vs **deferrable** decisions.
9. Avoid proposing large rewrites unless there is a fundamental contradiction.
10. **Do not modify the Design** during the initial review unless the user explicitly asks.
11. Support follow-up review after revisions (Mode B emphasis on change-induced contradictions).
12. Keep **design review** distinct from **implementation review**.
13. Keep report proportional to scope and maturity.
14. Do **not** declare Ready merely because headings exist.
15. Do **not** declare Not Ready merely because intentional implementation details are deferred.
16. Apply `ENGINE_DESIGN_PHILOSOPHY.md` when judging Core vs Plugin, opinions vs mechanisms, Agent-friendly shared APIs (not a separate Agent Framework).

---

## Workflow (mandatory order)

### Phase 1 — Establish review context

Identify:

- What is being designed? Problem? Consumers?
- Intended scope / explicit out of scope
- Design stage (Draft / Planned / Review / …); implementation already started?
- Decisions already made; repo constraints; user-specific concerns

Do **not** assume the Design file alone is complete context. Inspect related code, architecture docs, conventions, and sibling systems when needed.

### Phase 2 — Reconstruct the intended design

Summarize **what the design says** before criticizing:

Goals, non-goals, concepts, responsibilities, public interfaces, data model, ownership/lifetime, dependencies, state transitions, important workflows, error behavior, persistence/compat, extension points, constraints.

Output as report **§ Design Understanding**.

### Phase 3 — Requirement completeness

Missing / unclear / conflicting requirements; non-evaluable requirements; missing important scenarios; hidden assumptions about existing systems.

Do not invent requirements — explain **why** a gap may matter.

### Phase 4 — Design consistency

Contradictions (ownership both ways, sync vs async, immutable vs mutable, failure API without failure path, optional-but-required deps, lifecycle conflicts, editor-only vs runtime assumptions, “versioned” format without compat behavior, …).

Cite sections/statements for every contradiction.

### Phase 5 — Architecture and boundaries

Responsibility allocation, dependency direction, coupling, ownership/lifecycle boundaries, API stability, data ownership, extension points, cross-system contracts, temporary details leaking into public design.

Do not apply principles mechanically — ask whether the boundary is **coherent and justified**.

### Phase 6 — Workflows and state transitions

Apply **relevant** workflows only: create, init, normal op, modify, save/load, failure/partial failure, cancel, retry, reload, destroy, shutdown, reinit, Undo/Redo, Editor↔Runtime sync, …

State which apply and whether they are sufficiently defined.

### Phase 7 — Edge cases and failure behavior

Invalid input, missing resources, duplicates, null/empty, partial init, dependency failure, destroy-during-op, unload, reentrancy, concurrency (if claimed), cancel, version mismatch, corrupt data, unsupported ops, shutdown during pending work, recovery — **focus on what matters for this design**.

### Phase 8 — Acceptance criteria

For important behaviors: what must be true? Human-observable? Automatable? Failure paths? Measurable perf/compat when relevant?

Flag vague, non-observable, untied, unreproducible, or overly implementation-specific criteria that skip user-visible behavior.

### Phase 9 — Test and validation plan

Which of unit / integration / round-trip / lifecycle / regression / runtime / editor workflow / manual / perf / stress / failure injection / compat tests are **relevant**? Identify gaps — do not require every category.

### Phase 10 — Implementation readiness

Can an engineer or agent begin without repeatedly asking for fundamental decisions?

List: must-decide-before-coding vs safely deferrable; missing contracts/interfaces/formats/workflows/tests/migration/acceptance; unclear sequencing.

Goal: separate **healthy implementation freedom** from **dangerous ambiguity**.

### Phase 11 — Actionable report

Prioritized findings + how to resolve. No generic checklist dump.

**Output default:** present the full report **in the chat reply only**. Do **not** create/update a `*_DESIGN_REVIEW.md` (or similar) unless the user explicitly asks to save it.

**Templates (structure guide / optional save):**  
- Skill-local: `.agents/skills/design-review/report.template.md`  
- Docs copy: `docs/ai/templates/design-review.template.md`

**Save location (only if user asks):** same domain folder as the Design, e.g. `docs/ai/Editor/<ID>_DESIGN_REVIEW.md`.

---

## Review dimensions (apply when relevant)

| ID | Dimension |
|----|-----------|
| A | Requirements and Scope |
| B | Terminology and Concept Clarity |
| C | Architecture and Boundaries |
| D | Data Model and State |
| E | Workflow Completeness |
| F | API and Behavioral Contracts |
| G | Error Handling and Recoverability |
| H | Compatibility and Migration |
| I | Acceptance and Testing |
| J | Implementation Readiness |
| K | Agent Friendliness (shared Engine surface; not Agent-only APIs) |

Details and prompts: `reference.md`.

### Finding kind (pick one primary)

- Confirmed design defect  
- Ambiguity requiring clarification  
- Missing decision  
- Architectural risk  
- Optional improvement  
- Open question  

### Severity

`Critical` | `High` | `Medium` | `Low` | `Informational`

### Confidence

`Confirmed` | `Likely` | `Possible` | `Unknown`

**No numerical scores. No overall quality ranking.**

### Finding fields (required)

ID (`D01`, `D02`, …), Title, Category (dimension A–K or kind), Severity, Confidence, Evidence / affected section, Problem, Why it matters, Example misinterpretation/failure, Recommended clarification or design change, **Blocks implementation?** (Y/N), **Resolve before implementation?** (Y/N/Defer OK).

---

## Required report structure

1. **Review Context** — design, mode, docs/files, related systems, scope, limitations  
2. **Design Understanding** — goals, non-goals, concepts, responsibilities, deps, workflows, contracts, invariants  
3. **Findings** — prioritized, required fields  
4. **Ambiguities and Open Decisions** — Blocking / Important-deferrable / Optional  
5. **Missing Workflows / Edge Cases**  
6. **Acceptance and Test Coverage Gaps**  
7. **Positive Design Decisions** — **do not leave empty by default**  
8. **Implementation Readiness Assessment** — one of (DOC_GOVERNANCE §5.6 / Design Spec Review field):
   - `Ready` (synonym: Ready to implement)
   - `Ready with deferred items` (synonym: Ready after targeted clarifications) — deferred items must be explicit and non-blocking
   - `Not ready` — block large coding; list concrete gaps
   (with reasoning; no score)
9. **Recommended Next Steps** — ordered, practical  
10. **Suggested Acceptance Checklist** — short list for later implementation review  

---

## minEngine-specific notes

Discover actual lifecycle/ownership/threading from **this** repo — do not assume uniformity.

Relevant surfaces when reviewing: Engine DLL vs Editor/app executables; Reflection/codegen; JSON/binary serialization; Object/GUID/UID; Asset/resource; Scene/GameObject/Component; OpenGL/Vulkan RHI; Lua; Editor Undo/Redo; future MCP via **shared** Engine APIs; preference for stable Service/Interface boundaries and data-driven workflows where practical; 0.1.0 foundation transition.

Philosophy checklist (major Features): `ENGINE_DESIGN_PHILOSOPHY.md` decision checklist — use when Core vs Plugin / opinions / premature complexity appear.

---

## Interaction behavior

1. Read Design Meta Status before deep review.  
2. Reconstruct understanding before findings.  
3. Partner stance: challenge weak readiness claims; recommend a default path (clarify X/Y before coding).  
4. After review, if user asks to revise Design: edit Design + changelog; optional Mode B re-review.  
5. Do not start large implementation in the same turn as “Not ready” unless user overrides.  
6. When Ready / Ready-after-clarifications and user proceeds: mentor Pre-flight still applies for Go scope.

## Pitfalls (self-check)

- [ ] Did I reconstruct the design before criticizing?  
- [ ] Did I rubber-stamp because I wrote it?  
- [ ] Did I flag omissions that are intentional/deferrable as defects?  
- [ ] Are contradictions cited with sections?  
- [ ] Blocking vs deferrable clearly split?  
- [ ] Positive decisions present?  
- [ ] Readiness label justified without a score?  
- [ ] Stayed in design review (not code bug hunt)?  

## Sample invocations

```text
用 design-review（Mode A）评审 docs/ai/Editor/ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md，
准备开工 W3 余量前：重点看 Save As / 多 Scene 打开的契约与验收标准；先出报告，不要改 Design。
```

```text
Mode B：CORE-F23 Prefab 设计改了几轮，对照 Design + 最近 Progress，查是否有互相矛盾的 Override / Stage 决策；标出开工阻塞项。
```

```text
设计评审：docs/ai/Platform/Core/CORE-F22_PROFILER_HARNESS_DESIGN.md — Pre-implementation readiness，关注 Agent/Editor 共享查询面。
```
