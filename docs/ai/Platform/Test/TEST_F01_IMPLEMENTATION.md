# TEST-F01 — Implementation Plan

## Meta
- **ID:** `TEST-F01`
- **Status:** Done (S01–S05)
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [TEST_UNIFIED_DESIGN.md](./TEST_UNIFIED_DESIGN.md)

## TL;DR

Five slices: runner skeleton + empty smoke; `TestContext` + first adapter; migrate all suites + delete legacy `main.h` chain; smoke/full tables + reflection filter; `verify.ps1` + governance. **No doctest yet** (TEST-F02). **User approval** on Design → set Design to `Planned` before S01 (already approved 2026-05-28).

## Scope
- **In:** S01–S05 below.
- **Out:** doctest; `minEngine/Tests/`; rewriting test bodies (TEST-F02).

## Reader quick start
1. [TEST_UNIFIED_DESIGN.md](./TEST_UNIFIED_DESIGN.md)
2. This table
3. [INFRASTRUCTURE_ROADMAP.md](../INFRASTRUCTURE_ROADMAP.md) M3–M4

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `TEST-F01-S01` | `Runtime/Test/` core: `ITestSuite`, `TestSuiteRegistry`, `TestRunner`; wire `main.h` for `test smoke` with **zero** real suites (or one no-op) | Done | Build Editor; `test smoke` exit 0 |
| `TEST-F01-S02` | `TestContext` (log + PathRegistry from `CommandLineResult`); register `material-ir` adapter; remove special-case in `main.h` | Done | `Editor.exe test material-ir` exit 0 |
| `TEST-F01-S03` | Register remaining four suites; legacy flag warnings; **delete** all `ShouldRun*` + legacy chain in `main.h` | Done | Each `test <id>` + legacy alias; `main.h` clean |
| `TEST-F01-S04` | Smoke/full tables + reflection `--suite=` via `TestContext`; document run order and time budget | Done | `test smoke` all pass; `test full` all pass |
| `TEST-F01-S05` | `scripts/verify.ps1`; update DOC_GOVERNANCE §7.2 + BOOTSTRAP_DIGEST | Done | `.\scripts\verify.ps1` exit 0 |

---

## 2) 切片详情

### TEST-F01-S01 — Runner skeleton
- **Goal:** Single dispatch path for `ApplicationMode::Test`.
- **Touch:** `Runtime/Test/*`, `main.h`, `CMakeLists.txt` (minEngine).
- **DoD:** `TestRunner::Run` returns UsageError for unknown suite; smoke with empty registry exits 0 (or one `NoOpSuite` documented).
- **Verify:** `cmake --build minEngine/build --target Editor`; `Editor.exe test smoke`.

### TEST-F01-S02 — TestContext + first suite
- **Goal:** Shared fixture; material-ir no longer special-cased in `main.h`.
- **Touch:** `TestContext.cpp`, `MaterialIRTestSuite` adapter, `MaterialIRTest` (stop duplicate log init if moved to context — optional cleanup).
- **DoD:** `--engine-config=` on CLI path used via `TestContext`.
- **Verify:** `Editor.exe test material-ir` from `minEngine/bin`.

### TEST-F01-S03 — Full registry + legacy removal
- **Goal:** All five suites on unified path; true refactor of entry.
- **Touch:** `main.h`, `*Test.h` remove `ShouldRun*`, adapters in `Runtime/Test/TestSuites/`.
- **DoD:** No legacy test `if` chain in `main.h`; warnings on old flags.
- **Verify:** All five suite IDs + five legacy flags (warn).

### TEST-F01-S04 — Smoke / full + reflection filter
- **Goal:** Complete §3.5 tables; reflection phase mapping.
- **Touch:** `TestSuiteRegistry`, reflection adapter, design §7 checkboxes.
- **DoD:** Smoke order documented in registry source comment.
- **Verify:** `Editor.exe test smoke`; `Editor.exe test full`; `test reflection-function --suite=meta`.

### TEST-F01-S05 — verify.ps1 + DoD docs
- **Goal:** One local verify command for slices.
- **Touch:** `scripts/verify.ps1`, `DOC_GOVERNANCE.md`, `BOOTSTRAP_DIGEST.md`, `INFRASTRUCTURE_ROADMAP.md` milestone status.
- **DoD:** Script fails non-zero if build or smoke fails.
- **Verify:** `.\scripts\verify.ps1` from repo root.

---

## 3) 依赖顺序

```text
S01 → S02 → S03 → S04 → S05
```

Depends on **CLI-F01** Done. **TEST-F02** depends on F01-S05 (verify hook exists).

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-28 | Initial plan |
