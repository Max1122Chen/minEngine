# CLI-F01 — Implementation Plan

## Meta
- **ID:** `CLI-F01`
- **Status:** Done (S01–S03)
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [CLI_UNIFIED_DESIGN.md](./CLI_UNIFIED_DESIGN.md)

## TL;DR

Three slices: vendor CLI11 + parse skeleton; wire `main.h` + help; migrate first test + legacy alias. PathRegistry/Editor argv refactor split across S02–S03. **User approval** on Design → set Design to `Planned` before S01 code.

## Scope
- **In:** S01–S03 below.
- **Out:** Full suite migration (TEST-F01); `verify.ps1`.

## Reader quick start
1. [CLI_UNIFIED_DESIGN.md](./CLI_UNIFIED_DESIGN.md)
2. This table
3. [INFRASTRUCTURE_ROADMAP.md](../INFRASTRUCTURE_ROADMAP.md) M1–M2

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `CLI-F01-S01` | Vendor CLI11; `ApplicationCommandLine` + `CommandLineResult`; parse globals + `test`/`editor` tree; unit smoke | Done | Build minEngine; optional parse-only test |
| `CLI-F01-S02` | `main.h` dispatch; PathRegistry from result; Editor project from result; `--help` | Done | `Editor.exe --help`; `--project` opens project |
| `CLI-F01-S03` | `test material-ir` + legacy `--material-ir-test` alias; remove `ShouldRunMaterialIR*` from main | Done | `Editor.exe test material-ir` exit 0 |

---

## 2) 切片详情

### CLI-F01-S01 — CLI11 + parse core
- **Goal:** Parse without running app; fill `CommandLineResult`.
- **Touch:** `Third-Party/CLI11/`, `Runtime/Core/CLI/*`, `CMakeLists.txt` (minEngine).
- **DoD:** No CLI11 includes outside `ApplicationCommandLine.cpp`; globals parse correctly.
- **Verify:** `cmake --build … --target minEngine`; dev-only parse harness or gtest later (optional).

### CLI-F01-S02 — Entry + Editor + PathRegistry
- **Goal:** Single dispatch path; stop duplicate argv scans for engine + project.
- **Touch:** `main.h`, `Editor.cpp`, `PathRegistry.cpp/.h`, `Engine::Initialize` if needed.
- **DoD:** `ParseProjectDescriptorPathFromArgs` removed; `LoadEngineConfiguration(argc,argv)` delegates to parsed result or thin wrapper.
- **Verify:** `Editor.exe --help`; launch with `--project`; log shows same path resolution.

### CLI-F01-S03 — First test migration
- **Goal:** Prove `test <suite>` path; one legacy alias.
- **Touch:** `main.h`, `MaterialIRTest` (remove ShouldRun only; Run kept), help text.
- **DoD:** Legacy flag warns once; old code path deleted from main chain.
- **Verify:** `Editor.exe test material-ir` from `minEngine/bin`; compare with prior `--material-ir-test`.

---

## 3) 依赖顺序

```text
S01 → S02 → S03
```

`TEST-F01` depends on S03 (dispatch hook exists).

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-28 | S01–S03 implemented; build + `test material-ir` / legacy alias verified |
