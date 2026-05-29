# Unified Test Runner — Design Spec

## Meta
- **ID:** `TEST-F01` (+ follow-on `TEST-F02`)
- **Type:** Feature
- **Status:** Done (F01 S01–S05; F02 doctest + `minEngineTests.exe`)
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [TEST_F01_IMPLEMENTATION.md](./TEST_F01_IMPLEMENTATION.md), [TEST_F02_LAYOUT_MIGRATION.md](./TEST_F02_LAYOUT_MIGRATION.md), [CLI_UNIFIED_DESIGN.md](../CLI/CLI_UNIFIED_DESIGN.md), [INFRASTRUCTURE_ROADMAP.md](../INFRASTRUCTURE_ROADMAP.md), [TECH_DEBT.md](../../TECH_DEBT.md) TD-001–002

## TL;DR

**Self-built TestRunner** (registry + `TestContext` + smoke/full tables) + **doctest** for assertions. **`minEngineTests.exe`** is the primary entry; `Editor.exe test …` forwards. Legacy `--*-test` argv flags removed (2026-05-28). Follow-on **TEST-F03**: slim per-suite `TEST_CASE` bodies.

## Scope

### TEST-F01 (this feature — implement first)
- **In:** `Runtime/Test/` runner + registry + `TestContext`; wire `main.h`; migrate all five suites to registry; legacy flag aliases with warnings; `test smoke` / `test full`; `scripts/verify.ps1`; DOC_GOVERNANCE §7.2 smoke default.
- **Out:** doctest vendoring; moving `*Test.cpp` out of `Runtime/`; rewriting test bodies; `minEngineTests.exe`; remote CI (TD-010).

### TEST-F02 (follow-on — design stub only until F01 Done)
- **In:** doctest v2.x vendored; `minEngine/Tests/` layout; `minEngineTests` CMake target; suite-by-suite migration + selective rewrite; CLI alias policy (`Editor.exe test` vs `minEngineTests`).
- **Out:** gmock; pixel-diff visual tests; multi-process isolation.

## Reader quick start
1. This file — architecture, split with doctest, suite tables.
2. [TEST_F01_IMPLEMENTATION.md](./TEST_F01_IMPLEMENTATION.md) — slices S01–S05.
3. [TEST_F02_LAYOUT_MIGRATION.md](./TEST_F02_LAYOUT_MIGRATION.md) — Phase 2 after F01.
4. Code (planned F01): `Runtime/Test/`, `minEngine/src/main.h`.

---

## 1) 背景与目标

### Pain (resolved by F01/F02)
- ~~Scattered `ShouldRun*` / `--*-test` in `main.h`~~ → unified `test` subcommand + registry.
- ~~Per-suite LogSystem / PathRegistry duplication~~ → `TestContext` once per run.
- ~~No smoke table / verify script~~ → `test smoke` + `scripts/verify.ps1`.

### Remaining (TEST-F03 — planned)
- See [TEST_F03_SUITE_SLIM_PLAN.md](./TEST_F03_SUITE_SLIM_PLAN.md): split `TEST_CASE`s, **fixture B** (reflection in `EngineTestFixture`), reorder smoke table, slim Material IR smoke.
- Historical docs may still mention `--material-ir-test`; F03-S06 updates active docs only.

### Non-goals (F01)
- Replace `bool Run*()` with doctest macros (→ F02).
- Split test executable (→ F02).

---

## 2) 现状

| Location | Behavior |
|----------|----------|
| `main.h` | Parse → Editor or forward `test` to `minEngineTests.exe` |
| `minEngine/Tests/Suites/*.cpp` | doctest `TEST_CASE` + `Run*Tests()` wrapper |
| `Runtime/Test/` | `TestRunner`, registry, adapters |
| CLI | `minEngineTests test smoke\|full\|<suite-id>`; `--suite=` for reflection |
| Assertion style | doctest `CHECK` in suite TU; legacy `bool` inside `Run*Tests` until F03 |
| Third-party test lib | doctest 2.4.11 (implement only in `TestMain.cpp`) |

---

## 3) 架构

### 3.1 Layering (binding)

```text
┌─────────────────────────────────────────────────────────────┐
│ ApplicationCommandLine  →  CommandLineResult                │
└───────────────────────────────┬─────────────────────────────┘
                                │ mode == Test
                                ▼
┌─────────────────────────────────────────────────────────────┐
│ TestRunner::Run(result)                                     │
│   resolve target: smoke | full | <suite-id>                 │
│   TestContext (fixture) once per run                          │
│   for each suite: ITestSuite::Run(context) → bool             │
│   aggregate → exit 0 | 1 | 2                                  │
└───────────────────────────────┬─────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
 MaterialIRSuite         AssetManagerSuite      … (5 total)
 (wraps RunMaterialIR…)  (wraps RunAssetManager…)

 ─── TEST-F02 adds below ───
        ▼
 doctest TEST_CASE + CHECK  inside minEngine/Tests/Suites/
```

| Layer | Owner | F01 | F02 |
|-------|-------|-----|-----|
| CLI / mode | `ApplicationCommandLine` | Done (CLI-F01) | Optional forward to `minEngineTests.exe` |
| Orchestration | **Self-built** `TestRunner` | **Implement** | Same runner; second entry in `TestMain.cpp` |
| Fixture | **Self-built** `TestContext` | **Implement** | Shared; doctest `TEST_CASE_FIXTURE` wraps context |
| Assertions | **doctest** | Not used | **Implement** |
| Test sources | `Runtime/**/**Test.cpp` | Wrap in place | Move to `minEngine/Tests/` |

**Why not use doctest’s main for everything?** doctest discovers `TEST_CASE` symbols; it does not know engine smoke tables, GPU metadata, or `CommandLineResult`. We keep a thin **engine runner** and call doctest only inside F02 suites (or run `minEngineTests` with doctest’s runner for that executable).

### 3.2 Modules (F01 — under `Runtime/Test/`)

| File | Responsibility |
|------|----------------|
| `TestSuiteMetadata.h` | `Id`, `DisplayName`, `InSmoke`, `InFull`, `RequiresGpu`, optional `EstimatedSeconds` |
| `ITestSuite.h` | `GetMetadata()`, `Run(TestContext&)` |
| `TestContext.h/.cpp` | Holds `CommandLineResult`, legacy `argc/argv` (transition), log init guard, PathRegistry load helper |
| `TestSuiteRegistry.h/.cpp` | Register suites; query by id; lists for smoke/full |
| `TestRunner.h/.cpp` | `Run(CommandLineResult)` → `CommandLineExitCode` |
| `TestSuites/*.cpp` (or co-located adapters) | One adapter per suite wrapping existing `Run*Tests` |

**CLI11 rule:** no doctest includes outside test translation units (F02: only under `minEngine/Tests/`).

### 3.3 TestRunner dispatch (pseudocode)

```cpp
CommandLineExitCode TestRunner::Run(const CommandLineResult& cli, int argc, char** argv)
{
    TestContext ctx(cli, argc, argv);
    if (!ctx.InitializeEnginePaths()) return CommandLineExitCode::Failure;

    std::vector<ITestSuite*> suites = ResolveSuites(cli); // smoke | full | single id
    if (suites.empty()) return CommandLineExitCode::UsageError;

    bool allPassed = true;
    for (ITestSuite* suite : suites)
    {
        if (!suite->Run(ctx)) allPassed = false;
    }
    return allPassed ? Success : Failure;
}
```

`ResolveSuites` rules:

| CLI target | Suites run |
|------------|------------|
| `smoke` | All registered with `InSmoke == true` (fixed order, see §3.5) |
| `full` | All with `InFull == true` |
| `<suite-id>` | Single suite; unknown id → usage error |
| `reflection-function` + `--suite=meta,invoke` | Single suite; `ReflectionSuiteFilter` passed in `TestContext` |

### 3.4 TestContext (fixture contract)

**Initialize once per `TestRunner::Run`:**

1. `LogSystem::Initialize()` (ref-count or guard so nested suite calls do not double-shutdown incorrectly).
2. `PathRegistry::LoadEngineConfiguration(cli.CommandLine, …)` — **no** `ParsePrefixedArg` in suites for globals.
3. Store `ReflectionSuiteFilter` from CLI for reflection adapter.

**Suites receive** `TestContext&` and call legacy `Run*Tests(argc, argv)` until F02; adapters may ignore argv except reflection filter mapping.

**Headless GL / GPU:** Material IR suite sets `RequiresGpu = true`; smoke table still includes it (document local time budget). F02 may add `[gpu]` doctest tag.

### 3.5 Suite registry (v1 — single source of truth)

Stable IDs must match [CLI_UNIFIED_DESIGN](../CLI/CLI_UNIFIED_DESIGN.md) §3.2.

| Suite ID | InSmoke | InFull | RequiresGpu | Entry |
|----------|---------|--------|-------------|-------|
| `object-manager` | yes | yes | no | `RunObjectManagerTests` |
| `serialization-archive` | yes | yes | no | `RunSerializationArchiveTests` |
| `asset-manager` | yes | yes | no | `RunAssetManagerTests` |
| `reflection-function` | yes (meta+invoke+ref) | yes (all phases) | no | `RunReflectionFunctionTests` |
| `material-ir` | yes | yes | yes | `RunMaterialIRSmokeTests` |

**Smoke run order (current):** reflection-function → object-manager → serialization-archive → asset-manager → material-ir (reflection first — legacy ordering).

**Smoke run order (TEST-F03 / fixture B):** object-manager → serialization-archive → asset-manager → reflection-function → material-ir. See [TEST_F03_SUITE_SLIM_PLAN.md](./TEST_F03_SUITE_SLIM_PLAN.md).

**Reflection smoke vs full:** Adapter maps `TestContext`:

- smoke / no filter: run meta + invoke + ref (same as today’s default `--reflection-function-test` without `=`)
- full: all phases (meta, invoke, ref, types, static)
- `--suite=` on CLI: pass through to existing reflection parser logic

### 3.6 Entry points (current)

| Executable | Role |
|------------|------|
| `minEngineTests.exe` | **Primary** — `TestMain.cpp` + doctest + `TestRunner::Run` |
| `Editor.exe test …` | Forwards to `minEngineTests.exe` (`TestExecutableForward`) |

Legacy `--*-test` flags removed 2026-05-28 (no stderr alias).

### 3.8 verify integration (F01-S05)

```powershell
# scripts/verify.ps1 (sketch)
cmake --build minEngine/build --target Editor
Push-Location minEngine/bin
./Editor.exe test smoke
Pop-Location
```

Exit code propagates. Document in [DOC_GOVERNANCE](../../templates/DOC_GOVERNANCE.md) §7.2 as default Slice engineering check.

---

## 4) doctest (TEST-F02 — decision record)

**Chosen:** [doctest](https://github.com/doctest/doctest) (v2.4.x, vendored under `Third-Party/doctest/`, pin in F02 commit).

| Criterion | doctest |
|-----------|---------|
| Compile time | Fast — important while tests still link much of runtime |
| Vendoring | Header-friendly — matches CLI11 pattern |
| API | `TEST_CASE`, `SUBCASE`, tags `[smoke]` |
| Runner | Own `main()` in `minEngineTests`; engine tables can filter by tag or stay in `TestSuiteRegistry` |

**Split:**

- **doctest:** `CHECK`, `CHECK_EQ`, failure messages, `TEST_CASE` structure.
- **Self-built:** smoke/full membership (until tags fully replace tables), `TestContext`, PathRegistry bootstrap, suite ordering, CLI integration.

See [TEST_F02_LAYOUT_MIGRATION.md](./TEST_F02_LAYOUT_MIGRATION.md).

---

## 5) 备选方案

| Option | Pros | Cons | Verdict |
|--------|------|------|---------|
| **A. Self-built runner + doctest (F02)** | Clear split; minimal F01 churn | Two-phase delivery | **选用** |
| B. Full doctest in F01 inside Editor | One phase | doctest `main` vs Editor `main` awkward; large bang | 拒绝 |
| C. GoogleTest | Industry default | Heavier; slower builds | 拒绝（F02 不引入） |
| D. Self-built assert + runner | No deps | Reinvents wheel; poor ergonomics for rewrite | 拒绝 |

---

## 6) 风险与缓解

| Risk | Impact | Mitigation |
|------|--------|------------|
| Double LogSystem init | Crash / noisy logs | `TestContext` owns init/shutdown once per run |
| Suite order flaky failures | Hard to bisect | Fixed smoke order; log suite name boundaries |
| Material IR slow smoke | verify > 2 min | Document budget; F02 split `[smoke]` vs `[full]` cases |
| Global singleton pollution | Order-dependent failures | F01 keep sequential run; F02 fixture reset per TEST_CASE where needed |
| Link size / compile time (F02) | Slow iteration | Separate `minEngineTests` target; doctest only in Tests/ |

---

## 7) 验收标准 (TEST-F01)

- [x] `Editor.exe test smoke` runs all InSmoke suites; exit 0 on clean tree from `minEngine/bin`.
- [x] `Editor.exe test full` runs all InFull suites; exit 0.
- [x] Each suite ID works: `Editor.exe test material-ir`, etc.
- [x] Legacy flags warn and pass/fail same as new commands; **no** `ShouldRun*` in `main.h`.
- [x] `TestContext` loads engine config from `CommandLineResult` (warn-and-continue if load fails).
- [x] `scripts/verify.ps1` exists; documented in BOOTSTRAP + DOC_GOVERNANCE §7.2.
- [x] `reflection-function` + `--suite=` behavior preserved.

---

## 8) Status note

Planned — user approved doctest + two-phase exe strategy (2026-05-28). Set to **In Progress** when TEST-F01-S01 coding starts.

---

## 变更记录

| Date | Note |
|------|------|
| 2026-05-28 | Initial design; TEST-F01 runner + TEST-F02 doctest/layout split |
