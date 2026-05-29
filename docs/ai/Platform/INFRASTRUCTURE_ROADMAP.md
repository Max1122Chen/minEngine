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
| 1 | `CLI-F01` | Unified command-line interface | Done | [CLI_UNIFIED_DESIGN](./CLI/CLI_UNIFIED_DESIGN.md) |
| 2 | `TEST-F01` | Test runner, suite registry, verify integration | Planned | [TEST_UNIFIED_DESIGN](./Test/TEST_UNIFIED_DESIGN.md) |
| 3 | `TEST-F02` | doctest, `minEngine/Tests/`, `minEngineTests.exe` | Planned | [TEST_F02_LAYOUT_MIGRATION](./Test/TEST_F02_LAYOUT_MIGRATION.md) |

**Existing headless flags to migrate** (today in `minEngine/minEngine/src/main.h`):

| Current flag | Module |
|--------------|--------|
| `--material-ir-test` | Material IR |
| `--asset-manager-test` | AssetManager |
| `--object-manager-test` | ObjectManager |
| `--serialization-archive-test` | Serialization archive |
| `--reflection-function-test` | Reflection functions |

**Target UX (end state after TEST-F01 / F02):**

```text
Editor.exe test smoke          # F01: primary until F02
Editor.exe test material-ir
minEngineTests.exe             # F02: primary for verify
scripts/verify.ps1             # F01-S05: build + smoke
```

---

## 3) Milestones and slices

| Milestone | Delivers | Status | Acceptance |
|-----------|----------|--------|------------|
| **M1** | CLI core + dispatch hook | Done | `help` works; `test material-ir` |
| **M2** | First real test on new CLI | Done | `--material-ir-test` migrated |
| **M3** | All tests + smoke/full | Done | All five suites via `test`; legacy flags warn |
| **M4** | Verify + DoD | Done | `scripts/verify.ps1`; governance §7.2 cites smoke |

### CLI-F01 slices

| Slice ID | Goal | Verify |
|----------|------|--------|
| `CLI-F01-S01` | `CommandLine` parse + `CommandRegistry` (register subcommand, help text, handler) | Unit or minimal executable |
| `CLI-F01-S02` | Wire dispatch at app entry (`main.h` / Editor startup); global `--help` / `--version` | `Editor.exe --help` |
| `CLI-F01-S03` | Migrate `--material-ir-test` to registered command | `Editor.exe --run-tests material-ir` or agreed equivalent |

### TEST-F01 slices

See [Test/TEST_F01_IMPLEMENTATION.md](./Test/TEST_F01_IMPLEMENTATION.md).

| Slice ID | Goal | Verify |
|----------|------|--------|
| `TEST-F01-S01` | `TestRunner` + registry skeleton | `Editor.exe test smoke` |
| `TEST-F01-S02` | `TestContext` + material-ir adapter | `Editor.exe test material-ir` |
| `TEST-F01-S03` | All suites; delete legacy `main.h` chain | `test <id>` + legacy warn |
| `TEST-F01-S04` | smoke/full tables + reflection `--suite=` | `test smoke` / `test full` |
| `TEST-F01-S05` | `scripts/verify.ps1` + DoD docs | verify exit 0 |

### TEST-F02 slices

See [Test/TEST_F02_LAYOUT_MIGRATION.md](./Test/TEST_F02_LAYOUT_MIGRATION.md) (doctest + `minEngine/Tests/` + `minEngineTests.exe`).

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
| 2026-05-28 | TEST-F01/F02 design; M1–M2 Done; CLI test subcommand UX |
