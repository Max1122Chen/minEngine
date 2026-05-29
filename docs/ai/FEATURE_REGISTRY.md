# Feature Registry

Last updated: 2026-05-28  
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
| `CLI-F01` | Unified command-line interface | Planned | — | [INFRASTRUCTURE_ROADMAP](./Platform/INFRASTRUCTURE_ROADMAP.md) |
| `TEST-F01` | Test runner, suite migration, verify integration | Planned | — | [INFRASTRUCTURE_ROADMAP](./Platform/INFRASTRUCTURE_ROADMAP.md) |
| `WF-F01` | Documentation templates and collaboration governance | Done | — | [templates/](./templates/), [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) |

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | Command-line / tools entry |
| `TEST` | F02 | Automated tests (after TEST-F01) |
| `WF` | F02 | Workflow / docs |
| `CORE` | F01 | Reflection, object, startup |
| `ASSET` | F01 | Asset pipeline extensions |
| `ED` | F01 | Editor productization (new IDs only) |
| `RND` | F01 | Render (new IDs only) |
| `MAT` | F01 | Material (new IDs only; legacy Phase docs keep old names) |

Update **Next Feature #** when you register a new row.
