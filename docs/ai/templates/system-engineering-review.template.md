# {{SUBSYSTEM}} — System Engineering Review

> Copy from this template when saving a durable review under `docs/ai/`.  
> Agent workflow: `.agents/skills/engine-system-review/SKILL.md`.

## Meta
- **Type:** System Engineering Review
- **Status:** Draft | Review | Done
- **Owner:** project maintainer
- **Last updated:** YYYY-MM-DD
- **Branch / tip:** `branch` @ `sha`
- **Related:** Design / Feature IDs / TECH_DEBT rows
- **Skill:** `engine-system-review`

---

## 1) Review Scope

| Item | Content |
|------|---------|
| **Target subsystem** | |
| **Review objective** | e.g. 0.1.0 readiness / pre-merge / ownership audit |
| **Explicit focus** | |
| **Out of scope** | |
| **Directories / files inspected** | |
| **Related systems inspected** | (only if needed for correctness) |
| **Scenarios traced** | |
| **Limitations** | unread generated code, no runtime repro, … |

---

## 2) System Understanding

### Responsibilities
-

### Non-responsibilities
-

### Main abstractions / public surface
-

### Ownership and lifetime
-

### Important dependencies
-

### Key invariants
-

### Important state transitions / data flow
-

### Editor vs Runtime (if applicable)
-

### Error / persistence / threading assumptions (if applicable)
-

---

## 3) Findings

> **Do not** present Future Enhancement as a current defect.  
> Every finding needs Evidence **or** must move to §7 Open Questions.

### R01 — {{Title}}

| Item | Content |
|------|---------|
| **Category** | Critical Bug / Correctness / Lifecycle·Ownership / Data Integrity / Error·Recoverability / Concurrency / Architectural Risk / Maintainability / Observability / Future Enhancement |
| **Severity** | Critical / High / Medium / Low / Informational |
| **Confidence** | Confirmed / Likely / Possible / Unknown |
| **Evidence** | file paths, class/function names, call chains |
| **Why it is a problem** | |
| **Trigger / scenario** | |
| **Impact** | |
| **Recommended action** | proportional; avoid mega-rewrite by default |
| **0.1.0 bucket** | Must Fix Before 0.1.0 / Should Fix Soon / Known Limitation·Acceptable / Future Enhancement |

---

## 4) Positive Findings

-
-

---

## 5) Release Assessment (0.1.0)

| Bucket | Finding IDs |
|--------|-------------|
| **Must Fix Before 0.1.0** | |
| **Should Fix Soon** | |
| **Known Limitation / Acceptable for 0.1.0** | |
| **Future Enhancement** | |

**Readiness judgment:**

---

## 6) Recommended Next Steps

1.
2.
3.

---

## 7) Open Questions

| ID | Question | Blocked on |
|----|----------|------------|
| Q1 | | |

---

## Placement hint

| Area | Path |
|------|------|
| Core / Platform | `docs/ai/Platform/<Topic>/<NAME>_SYSTEM_REVIEW.md` |
| Editor | `docs/ai/Editor/<NAME>_SYSTEM_REVIEW.md` |
| Render | `docs/ai/Render/<NAME>_SYSTEM_REVIEW.md` |
