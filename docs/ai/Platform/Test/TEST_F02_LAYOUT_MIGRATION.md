# TEST-F02 — Test Layout, doctest, and Separate Executable

## Meta
- **ID:** `TEST-F02`
- **Type:** Feature (follow-on)
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [TEST_UNIFIED_DESIGN.md](./TEST_UNIFIED_DESIGN.md), [TEST_F01_IMPLEMENTATION.md](./TEST_F01_IMPLEMENTATION.md)

## TL;DR

After **TEST-F01** stabilizes the runner: vendor **doctest**, add **`minEngineTests.exe`**, move tests from `Runtime/**/**Test.cpp` to **`minEngine/Tests/`**, and **selectively rewrite** suites as `TEST_CASE` + `CHECK`. Keep **`TestRunner` + `TestContext`**; doctest replaces manual assert loops only.

## Scope
- **In:** doctest vendoring; CMake `minEngineTests` target; directory layout; per-suite migration; update `verify.ps1` to prefer `minEngineTests`; remove old `*Test.cpp` from runtime GLOB.
- **Out:** gmock; screenshot tests; deleting `Editor.exe test` until alias period ends (deprecation warning first).

## Prerequisites
- **TEST-F01 Done** — registry, smoke/full, verify.ps1 green.

---

## 1) 目标目录 (target)

```text
minEngine/
  Tests/
    CMakeLists.txt           → add_executable(minEngineTests …)
    TestMain.cpp             → doctest RUN_ALL_TESTS or custom main calling TestRunner
    TestContextBridge.cpp    → shares Runtime/Test/TestContext with doctest fixtures
    Suites/
      ObjectManagerTest.cpp
      SerializationArchiveTest.cpp
      AssetManagerTest.cpp
      ReflectionFunctionTest.cpp
      MaterialIRTest.cpp
  minEngine/
    Third-Party/doctest/     → doctest.h (pinned version)
    src/Runtime/Test/        → runner (unchanged from F01)
```

**Runtime `src/`:** no `*Test.cpp` after migration complete.

---

## 2) doctest 与 Runner 切分

| Concern | Owner |
|---------|--------|
| `TEST_CASE("…")`, `CHECK`, `SUBCASE` | doctest |
| `[smoke]` / `[full]` tags | doctest (may mirror registry during transition) |
| PathRegistry bootstrap, cwd = `bin/` | `TestContext` |
| `test smoke` CLI semantics | `TestRunner` (F01) or `minEngineTests --smoke` flag (F02 detail) |
| GPU / long tests | Tags + registry metadata |

**Fixture pattern (sketch):**

```cpp
struct EngineTestFixture {
    EngineTestFixture() { m_Ctx.InitializeEnginePaths(); }
    ~EngineTestFixture() { /* ctx shutdown */ }
    minEngine::TestContext m_Ctx;
};

TEST_CASE_FIXTURE(EngineTestFixture, "ObjectManager GC smoke") {
    CHECK(/* … */);
}
```

---

## 3) Executable 策略

| Entry | F02 behavior |
|-------|----------------|
| `minEngineTests.exe` | Primary for dev and `verify.ps1` |
| `Editor.exe test …` | Optional: forward to same runner code path **or** print deprecation + exit 2 with message |

Recommend **shared static lib or linked objects** for `TestRunner` so Editor and Tests exe do not diverge.

---

## 4) 迁移顺序 (suite-by-suite)

Migrate **smallest / no GPU first** to prove layout:

1. `object-manager`
2. `serialization-archive`
3. `asset-manager`
4. `reflection-function` (use `SUBCASE` per phase)
5. `material-ir` (last — largest, GPU)

Each slice: new doctest file → green → delete old `Runtime/.../XTest.cpp` → one commit.

---

## 5) 切片草案 (implementation TBD)

| Slice ID | Goal |
|----------|------|
| `TEST-F02-S01` | Vendor doctest; `minEngineTests` empty `TEST_CASE`; CMake |
| `TEST-F02-S02` | `TestContext` bridge + migrate object-manager |
| `TEST-F02-S03` | Migrate serialization + asset-manager |
| `TEST-F02-S04` | Migrate reflection-function |
| `TEST-F02-S05` | Migrate material-ir; remove Runtime test sources |
| `TEST-F02-S06` | `verify.ps1` → `minEngineTests`; Editor test alias policy |

Full Implementation plan to be written when F01 completes.

---

## 6) 验收标准 (outline)

- [ ] `cmake --build … --target minEngineTests` succeeds.
- [ ] `minEngineTests` smoke equivalent exit 0 from `minEngine/bin`.
- [ ] No `*Test.cpp` under `Runtime/` in engine library sources.
- [ ] `verify.ps1` uses `minEngineTests`.

---

## 变更记录

| Date | Note |
|------|------|
| 2026-05-28 | Initial stub; doctest + layout decision |
