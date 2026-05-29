# TEST-F02 — Implementation Plan

## Meta
- **ID:** `TEST-F02`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [TEST_F02_LAYOUT_MIGRATION.md](./TEST_F02_LAYOUT_MIGRATION.md)

## TL;DR

Vendored doctest; added `minEngineTests.exe`; moved five suite sources to `minEngine/Tests/Suites/` with `TEST_CASE` wrappers; `Editor.exe test` forwards to `minEngineTests`; `verify.ps1` uses `minEngineTests`.

## Slice status

| Slice ID | Status | Verify |
|----------|--------|--------|
| `TEST-F02-S01` | Done | `cmake --build … --target minEngineTests` |
| `TEST-F02-S02` | Done | doctest `object-manager suite` runs |
| `TEST-F02-S03` | Done | serialization + asset-manager suites |
| `TEST-F02-S04` | Done | reflection-function suite |
| `TEST-F02-S05` | Done | material-ir; no `*Test.cpp` under `Runtime/` |
| `TEST-F02-S06` | Done | `verify.ps1` → `minEngineTests test smoke` |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-28 | Implemented F02 S01–S06 |
