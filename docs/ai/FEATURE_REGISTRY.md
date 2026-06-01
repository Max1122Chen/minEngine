# Feature Registry

Last updated: 2026-06-01  
Purpose: **single source of truth** for `<DOMAIN>-F<nn>` IDs. Avoid duplicate or conflicting Feature IDs between you and AI.

**Rules (mandatory for new work):**

1. **Register a row before** creating Design Spec or assigning `<DOMAIN>-Fnn` in docs.
2. Pick the next free number for that `DOMAIN` (see [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) §3).
3. Set Status: `Planned` → `In Progress` → `Done` | `Deferred` | `Cancelled`.
4. Link the Design (or Implementation) path in **Design** column.
5. Do not reuse IDs; deprecate by setting Status `Cancelled` and a note — do not recycle numbers.

---

## Active & recent features

| Feature ID | Title | Status | Owner | Design / plan |
|------------|-------|--------|-------|----------------|
| `CLI-F01` | Unified command-line interface | Done | — | [CLI_UNIFIED_DESIGN](./Platform/CLI/CLI_UNIFIED_DESIGN.md) |
| `TEST-F01` | Test runner, suite registry, verify integration | Done | — | [TEST_UNIFIED_DESIGN](./Platform/Test/TEST_UNIFIED_DESIGN.md) |
| `TEST-F02` | Test layout migration, doctest, minEngineTests exe | Done | — | [TEST_F02_LAYOUT_MIGRATION](./Platform/Test/TEST_F02_LAYOUT_MIGRATION.md) |
| `TEST-F03` | Suite slim-down, doctest cases, fixture B reflection | Done | — | [TEST_F03_SUITE_SLIM_PLAN](./Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md) |
| `WF-F01` | Documentation templates and collaboration governance | Done | — | [templates/](./templates/), [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) |
| `WF-F02` | 协作者文档站（MkDocs 公开手册 + GitHub Pages） | In Progress | — | [Design](./Platform/Docs/HANDBOOK_SITE_DESIGN.md) · [Impl](./Platform/Docs/HANDBOOK_SITE_IMPLEMENTATION.md) — 骨架 Done，子系统文档待补 |

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | Command-line / tools entry |
| `TEST` | F04 | Automated tests (follow-on) |
| `WF` | F03 | Workflow / docs |
| `CORE` | F01 | Reflection, object, startup |
| `ASSET` | F01 | Asset pipeline extensions |
| `ED` | F01 | Editor productization (new IDs only) |
| `RND` | F01 | Render (new IDs only) |
| `MAT` | F01 | Material (new IDs only; legacy Phase docs keep old names) |

Update **Next Feature #** when you register a new row.
