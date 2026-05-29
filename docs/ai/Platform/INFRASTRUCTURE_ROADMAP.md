# Infrastructure Roadmap (CLI · Test · Verify)

## Meta
- **ID:** N/A（协调 `CLI-F01` + `TEST-F01`）
- **Status:** **Active**
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [TECH_DEBT.md](../TECH_DEBT.md), [BOOTSTRAP_DIGEST.md](../BOOTSTRAP_DIGEST.md), [PLATFORM_ROADMAP.md](./PLATFORM_ROADMAP.md) §12

## TL;DR

Near-term work is **engineering infrastructure only**: unify **CLI** (command-line), centralize **tests** behind one runner, add local **`verify`** (build + smoke), and bind **Slice DoD** to that command. **No new Editor/product features** until milestone M4. Digest and tech-debt register are already done (not repeated here).

## Scope
- **In:** CLI parsing/registry; test runner + fixtures; migrate existing `--*-test` flags; `smoke` vs `full` suites; `scripts/verify`; governance DoD tweak.
- **Out:** `BOOTSTRAP_DIGEST` / `TECH_DEBT` content (landed); GitHub Actions (TD-010); Editor E1/P7; P4/P5/Lua new slices; render multi-viewport refactor (TD-007).

## Reader quick start
1. This file — order and slices.
2. [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md) — `CLI-F01`, `TEST-F01`.
3. [TECH_DEBT.md](../TECH_DEBT.md) TD-001–003 — motivation.

---

## 1) Direction and principles

- **CLI** = unified **C**ommand **L**ine (not Continuous Integration). One parser, subcommands, consistent `--help` and exit codes.
- **Professional bar:** agents and humans run the same commands; Slice Done means **verify smoke** passed, not “I clicked around.”
- **True migration:** old flags become subcommands; remove duplicate argv scans when a module is migrated (no shims forever).
- **Partner workflow:** Pre-flight before `CLI-F01` / `TEST-F01` implementation; commit after each slice (see [DOC_GOVERNANCE](../templates/DOC_GOVERNANCE.md) §7.4).

---

## 2) Feature priority

| Order | Feature ID | Title | Status | Design (next) |
|-------|------------|-------|--------|----------------|
| 1 | `CLI-F01` | Unified command-line interface | Planned | Design Spec → Implementation Plan |
| 2 | `TEST-F01` | Test runner, suite registry, verify integration | Planned | After CLI-F01-S02 minimum |

**Existing headless flags to migrate** (today in `minEngine/minEngine/src/main.h`):

| Current flag | Module |
|--------------|--------|
| `--material-ir-test` | Material IR |
| `--asset-manager-test` | AssetManager |
| `--object-manager-test` | ObjectManager |
| `--serialization-archive-test` | Serialization archive |
| `--reflection-function-test` | Reflection functions |

**Target UX (end state):**

```text
Editor.exe --help
Editor.exe --run-tests smoke
Editor.exe --run-tests full
# optional: Editor.exe test material-ir   (if subcommand style chosen in CLI-F01 design)
```

Local verify (end state):

```text
scripts/verify.ps1          # cmake build Editor + --run-tests smoke
```

---

## 3) Milestones and slices

| Milestone | Delivers | Status | Acceptance |
|-----------|----------|--------|------------|
| **M1** | CLI core + dispatch hook | Planned | `help` works; one no-op subcommand registered |
| **M2** | First real test on new CLI | Planned | `--material-ir-test` migrated; smoke green |
| **M3** | All tests + smoke/full | Planned | All five suites via `--run-tests`; legacy flags removed or alias one release |
| **M4** | Verify + DoD | Planned | `scripts/verify.ps1`; governance §7.2 cites smoke for slices |

### CLI-F01 slices

| Slice ID | Goal | Verify |
|----------|------|--------|
| `CLI-F01-S01` | `CommandLine` parse + `CommandRegistry` (register subcommand, help text, handler) | Unit or minimal executable |
| `CLI-F01-S02` | Wire dispatch at app entry (`main.h` / Editor startup); global `--help` / `--version` | `Editor.exe --help` |
| `CLI-F01-S03` | Migrate `--material-ir-test` to registered command | `Editor.exe --run-tests material-ir` or agreed equivalent |

### TEST-F01 slices

| Slice ID | Goal | Verify |
|----------|------|--------|
| `TEST-F01-S01` | `tests/` or `Runtime/Test/` layout + `TestRunner` + suite interface | Empty suite exits 0 |
| `TEST-F01-S02` | Shared fixtures (PathRegistry / engine config, headless GL policy) | One suite uses fixture |
| `TEST-F01-S03` | Migrate remaining four `*Test` modules; drop duplicate argv loops | `--run-tests smoke` all pass |
| `TEST-F01-S04` | `smoke` vs `full` suite tables documented | smoke runs in under 2 min locally |
| `TEST-F01-S05` | `scripts/verify.ps1` + update [DOC_GOVERNANCE](../templates/DOC_GOVERNANCE.md) §7.2 default | verify from clean build |

---

## 4) Dependencies and risks

| Item | Notes |
|------|--------|
| **Depends on** | `PathRegistry` / engine config (existing); Editor links runtime tests |
| **Risk: dual parsers** | Mitigation: migrate one flag per slice; delete old `ShouldRun*` per module |
| **Risk: scope creep** | Mitigation: no Editor UI in this roadmap; defer format/CI to TD-010 |
| **Risk: P4 parallel work** | Mitigation: §12 PLATFORM_ROADMAP — no new P4 slices until M4 |

---

## 5) Deferred (functional and other)

| Item | See |
|------|-----|
| Editor E1 / P7 / CB polish | [TECH_DEBT](../TECH_DEBT.md) TD-009; [EDITOR_PLATFORM_PLAN](../Editor/EDITOR_PLATFORM_PLAN.md) |
| P4 reflection expansion, P5/P6 Lua | [PLATFORM_ROADMAP](./PLATFORM_ROADMAP.md) §11 — after M4 |
| Remote CI (GitHub Actions) | TD-010 — same commands as `verify.ps1` |
| Render viewport refactor | TD-007, [RENDER_REFACTOR_PLAN](../Render/RENDER_REFACTOR_PLAN.md) |

---

## 6) Change log

| Date | Note |
|------|------|
| 2026-05-28 | Initial roadmap; CLI-F01 + TEST-F01 registered |
