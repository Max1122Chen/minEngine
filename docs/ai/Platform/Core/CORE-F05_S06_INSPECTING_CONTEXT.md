# CORE-F05-S06 — Inspecting Context (Design Slice)

> Canonical decisions also live in [CORE-F05_PLAY_MODE_DESIGN.md](./CORE-F05_PLAY_MODE_DESIGN.md) §9.
> Naming: **Inspecting** (aligns with Inspector), not Observing.

## Meta
- **ID:** `CORE-F05-S06`
- **Status:** Done (2026-09-03 visual verify)
- **Last updated:** 2026-09-03

## TL;DR

Do **not** switch Document Active (`m_CurrentActiveScene`). Editor maintains **Inspecting Scene**. Play → auto-inspect PIE; Stop → Editor. Hierarchy / Inspector / Console follow Inspecting. Play may `set` PIE; mutate must never silently hit Editor.

## Three Scene semantics

| Semantic | API | Play points to | Use |
|----------|-----|----------------|-----|
| Document | `GetEditorScene()` / `GetDocumentScene()` | Always Editor | Save, Dirty |
| Tick | `GetTickTargetScene()` | PIE | Runtime |
| Inspecting | `IEditorContext::GetInspectingScene()` | PIE (default) | Hierarchy, Selection, Inspector, Console |

## Decisions

| ID | Decision |
|----|----------|
| D11 | Editor owns Inspecting Context; modules do not read Document Active for inspect UI |
| D12 | EnterPlay → Inspecting=PIE; Stop → Inspecting=Editor |
| D13 | Play mutates (e.g. `set`) allowed on PIE |
| D14 | Mutate target must be Inspecting; never silently fall back to Editor |
| D15 | `PlayObjectMapping` is boundary-only (optional selection transfer), not inspect primary path |

## Rejected

- Switch `m_CurrentActiveScene` to PIE while playing
- Inspect via Editor GO + Mapping as primary path

## DoD

- [x] Play: Hierarchy / Inspector / Command use PIE
- [x] Play: `set` changes PIE; Stop leaves Editor document unpolluted
- [x] Play: mutate of non-Inspecting target fails explicitly
- [x] Stop: Inspecting returns to Editor
- [x] Save/Dirty remain Document-only
- [x] UI shows `Inspecting: PIE` as in-panel status (stable dock IDs)
