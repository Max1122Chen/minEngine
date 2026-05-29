# TEST-F03 — Suite Slim-Down (doctest Cases + Fixture B)

## Meta
- **ID:** `TEST-F03`
- **Type:** Feature (follow-on)
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [TEST_UNIFIED_DESIGN.md](./TEST_UNIFIED_DESIGN.md), [TEST_F02_LAYOUT_MIGRATION.md](./TEST_F02_LAYOUT_MIGRATION.md), [INFRASTRUCTURE_ROADMAP.md](../INFRASTRUCTURE_ROADMAP.md)

## TL;DR

Replace monolithic `Run*Tests()` wrappers with **small doctest `TEST_CASE`s** that each prove **one invariant**. Centralize **reflection readiness in `EngineTestFixture` (option B)** so smoke order no longer depends on `reflection-function` running first. Slim **smoke** tables and **Material IR** GPU scope so `verify.ps1` stays fast and failures are easy to locate.

## Scope

### In
- `EngineTestFixture` / `EngineTestContextScope`: one-time `ReflectionSystem::FinalizeReflection()` per test process (or per fixture instance — see §2).
- Reorder `TestSuiteRegistry::GetSmokeSuites()` to **dependency order** (§3).
- Per-suite migration: split `TEST_CASE`, `CHECK`/`REQUIRE`, delete redundant paths.
- doctest tags `[smoke]` / `[full]` aligned with registry metadata.
- Update **active** docs that still say `--material-ir-test` → `minEngineTests test material-ir` (checklist in §6).

### Out
- gmock; screenshot tests; remote CI (TD-010).
- Rewriting production code except test-only helpers.
- Changing CLI surface (`test smoke` / suite IDs stay stable).

## Prerequisites
- **TEST-F01 / TEST-F02 Done** — `minEngineTests.exe`, registry, `verify.ps1` green.
- Legacy `--*-test` argv removed (2026-05-28).

---

## 1) Fixture strategy (option B — binding)

### Problem today
- Smoke table runs **`reflection-function` first** so global reflection state exists before other suites.
- `ObjectManagerTest`, `SerializationArchiveTest`, etc. each call **`EnsureReflectionReady()`** (duplicate `FinalizeReflection`).

### Target
| Layer | Responsibility |
|-------|----------------|
| `TestContext` | Paths, log guard, `CommandLineResult` (unchanged) |
| `EngineTestContextScope` | Active context thread-local (unchanged) |
| **`EngineTestFixture`** | On construction: ensure reflection **ready once** (idempotent `FinalizeReflection` if `!IsReady()`); expose `GetArgc`/`GetArgv` |
| Each `TEST_CASE` | Own `EngineTestFixture`; suite-local scopes (`ObjectManagerTestScope`, temp project dirs) |

### Idempotency rule
```text
if (!ReflectionSystem::Get().IsReady())
    FinalizeReflection() or fail fast with ME_CORE_ERROR + doctest CHECK
```
Safe to call from every `TEST_CASE`; no ordering requirement between suites.

### Validation gate (before slice 1 code)
Run from `minEngine/bin` with **new** smoke order (§3) but **old** monolithic suites:
1. `minEngineTests.exe test object-manager` (alone)
2. `minEngineTests.exe test reflection-function` (alone)
3. `minEngineTests.exe test smoke`

All exit 0 → proceed with splitting object-manager.

---

## 2) Smoke / full registry order (after B)

Update `TestSuiteRegistry.cpp` `kSmokeOrder[]`:

| Order | Suite ID | Rationale |
|-------|----------|-----------|
| 1 | `object-manager` | Core handles; no reflection ordering hack |
| 2 | `serialization-archive` | Uses OM + archive |
| 3 | `asset-manager` | PathRegistry + temp project |
| 4 | `reflection-function` | Exercises meta/invoke in isolation |
| 5 | `material-ir` | GPU + assets; slowest |

**Full** runs: same five + any future `InFull`-only suites (none today).

Remove “reflection must be first” language from [TEST_UNIFIED_DESIGN.md](./TEST_UNIFIED_DESIGN.md) §3.5 when implementing F03-S00.

---

## 3) Per-suite invariants and migration

### 3.1 `object-manager` (F03-S01)

**Invariants (keep):**
| ID | Invariant | Suggested `TEST_CASE` tag |
|----|-----------|---------------------------|
| OM-1 | `FindObject(guid)` live after `NewObject`; null after `shared_ptr` release + GC → 0 tracked | `[smoke]` |
| OM-2 | Scene/GO/component hierarchy teardown clears all GUIDs without explicit `RemoveObject` | `[smoke]` |
| OM-3 | `RegisterGarbageRootSource` + `CollectGarbageWithEngineRoots` keeps engine-rooted scene alive | `[full]` |

**Delete / merge:**
- Remove `EnsureReflectionReady()` from suite (fixture owns it).
- Remove single wrapper `TEST_CASE("object-manager suite …")` calling `RunObjectManagerTests` once all cases migrated.

**DoD:** `test object-manager` + `test smoke`; 2 smoke + 1 full cases.

---

### 3.2 `serialization-archive` (F03-S02)

**Invariants:**
| ID | Invariant | Tag |
|----|-----------|-----|
| SA-1 | Round-trip serialize/deserialize a minimal object graph (existing golden path) | `[smoke]` |
| SA-2 | Archive version / property iteration edge (only if today’s test already covers; else defer) | `[full]` |

**Delete:** duplicate OM bootstrap; any second round-trip that only differs by type name.

**DoD:** `test serialization-archive` + smoke green.

---

### 3.3 `asset-manager` (F03-S03)

**Invariants:**
| ID | Invariant | Tag |
|----|-----------|-----|
| AM-1 | Register + lookup asset by path/GUID in temp project | `[smoke]` |
| AM-2 | Delete asset removes registry entry | `[smoke]` |
| AM-3 | Move/rename or unregister (pick **one** extra path today; drop redundant P2 variants) | `[full]` |

**Delete:** multiple near-duplicate `_P2UnitTest/*` scenarios that assert the same registry invariant.

**DoD:** `test asset-manager` + smoke; temp dir cleanup unchanged.

---

### 3.4 `reflection-function` (F03-S04)

**Invariants:**
| ID | Invariant | Tag |
|----|-----------|-----|
| RF-1 | Meta: class/property lookup for known test type | `[smoke]` |
| RF-2 | Invoke: `Add` success + expected failure paths (null fn, bad parms, IsA) — **one** negative case | `[smoke]` |
| RF-3 | Ref / UProperty-style binding (if still in smoke profile) | `[smoke]` |
| RF-4 | `types`, `static` phases | `[full]` only |

**CLI:** keep `test reflection-function --suite=meta,invoke` mapping via `TestContext` / `ConfigureReflectionProfile`.

**Delete:** duplicate phase runners; legacy argv `--reflection-function-test=` **internal** builders can stay until cases are native doctest filters.

**DoD:** smoke logs show only intended phases; `test full` runs all phases.

---

### 3.5 `material-ir` (F03-S05)

**Smoke invariants (keep):**
| ID | Invariant | Tag |
|----|-----------|-----|
| MIR-1 | Golden `MaterialIRSmoke.memtl` on-disk fields + pin type rules | `[smoke]` |
| MIR-2 | One **Unlit** graph: compile + GPU link | `[smoke]` |
| MIR-3 | One **BlinnPhong** path (e.g. Constant3→Normal or golden BlinnPhong smoke) | `[smoke]` |

**Move to `[full]` (not smoke / not verify default):**
- IBL environment capture + convolution + missing-assets fallback chain
- Translucent Unlit, PBR IBL GPU convolution, verbose GLSL dump to `Saved/Materials/`
- TextureCube RHI-only unless needed for a single compile regression

**DoD:** `test material-ir` smoke subset under ~15s local; `test smoke` still exit 0; `test full` or tagged `[full]` runs heavy paths.

---

## 4) Implementation slices

| Slice | Deliverable | Verify |
|-------|-------------|--------|
| **F03-S00** | Fixture B + smoke order change; remove per-suite `EnsureReflectionReady` where redundant | §1 validation gate |
| **F03-S01** | object-manager split | `test object-manager`, smoke |
| **F03-S02** | serialization-archive split | `test serialization-archive`, smoke |
| **F03-S03** | asset-manager split | `test asset-manager`, smoke |
| **F03-S04** | reflection-function split + smoke/full tags | `test reflection-function`, `test full` |
| **F03-S05** | material-ir smoke vs full split | `test material-ir`, smoke timing check |
| **F03-S06** | Active doc string pass (`--material-ir-test` → CLI) | grep active docs only |

**Rule:** one slice per PR/commit batch; user review between slices if desired.

---

## 5) doctest ↔ runner integration

Today: `DoctestSuiteRunner::RunTestCaseSubstring("… suite")` runs one fat case per suite.

**Target:**
- Multiple `TEST_CASE` per file with tags.
- `ITestSuite::Run` for smoke: either
  - **A)** `DoctestSuiteRunner::RunTag("[smoke]")` filtered by suite prefix, or
  - **B)** drop substring runner; registry smoke invokes doctest filter per suite id.

Prefer **B** long-term: `minEngineTests test smoke` = TestRunner runs suites in order; each suite adapter runs doctest with filter `suite-id` + `[smoke]`.

**Full:** `[full]` cases run when `test full` or `test <suite-id>` with full profile.

---

## 6) Active doc cleanup (F03-S06)

Replace command examples in (non-historical) files:

| File | Change |
|------|--------|
| [MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md](../../Render/Material/MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md) | `minEngineTests test material-ir` |
| [MEMORY_MANAGEMENT_DESIGN.md](../MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) | object-manager via `test object-manager` |
| [PLATFORM_ROADMAP.md](../PLATFORM_ROADMAP.md) § acceptance | unified `test` subcommand |

**Do not** rewrite `PROGRESS_LOG.md` historical entries.

---

## 7) Risks

| Risk | Mitigation |
|------|------------|
| Reflection init order | Fixture B + gate in S00 |
| Material IR smoke too thin | Keep MIR-1..3; full tag for IBL |
| Flaky GPU | No new GPU cases in smoke |
| Deleted test hid bug | PR lists invariant ID coverage |

---

## 8) Acceptance (feature Done)

- [ ] All five suites: multiple `TEST_CASE`s; no monolithic `Run*Tests` required for smoke.
- [ ] `EngineTestFixture` owns reflection readiness.
- [ ] Smoke order §2; reflection not required first.
- [ ] `.\scripts\verify.ps1` exit 0 from repo root.
- [ ] `FEATURE_REGISTRY` `TEST-F03` → Done; PROGRESS_LOG entry.
