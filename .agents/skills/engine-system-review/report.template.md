# {{SUBSYSTEM}} — System Engineering Review

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

> Categories: Critical Bug | Correctness | Lifecycle/Ownership | Data Integrity | Error/Recoverability | Concurrency | Architectural Risk | Maintainability | Observability | **Future Enhancement**  
> Severity: Critical | High | Medium | Low | Informational  
> Confidence: Confirmed | Likely | Possible | Unknown  
> 0.1.0 bucket: Must Fix Before 0.1.0 | Should Fix Soon | Known Limitation / Acceptable | Future Enhancement

### R01 — {{Title}}

| Item | Content |
|------|---------|
| **Category** | |
| **Severity** | |
| **Confidence** | |
| **Evidence** | paths, symbols, call chains |
| **Why it is a problem** | |
| **Trigger / scenario** | |
| **Impact** | |
| **Recommended action** | proportional; prefer small verifiable fix |
| **0.1.0 bucket** | |

_(Repeat for R02…)_

---

## 4) Positive Findings

What works and should be preserved:

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

**Readiness judgment (1–3 sentences):** Is this subsystem sufficiently reliable and coherent for its intended role in the 0.1.0 foundation?

---

## 6) Recommended Next Steps

Ordered, small, verifiable:

1.
2.
3.

---

## 7) Open Questions

| ID | Question | Blocked on |
|----|----------|------------|
| Q1 | | more code / test / repro / design decision / user |

---

## Appendix (optional)

- Call graphs, scenario notes, test commands run
