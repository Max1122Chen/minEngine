# Technical Debt Register

Last updated: 2026-05-28  
Purpose: explicit queue of **deferred or risky work** for Pre-flight and roadmap planning. Not a bug list — use [bugs/](./bugs/) for defects.

**Rules:** add a row when deferring non-trivial work; link Feature ID when known; do not delete rows — set Status `Done` or `Cancelled`.

| ID | Title | Module | Severity | Status | Feature / doc | Notes |
|----|-------|--------|----------|--------|---------------|-------|
| TD-001 | Scattered argv test flags, no unified CLI | TEST / Runtime | **High** | Open | Planned `CLI-F01` | Each `*Test.cpp` parses its own `--*-test`; `main.h` chains ShouldRun*; hard for CI/agent to run one smoke |
| TD-002 | No shared test runner / fixtures | TEST | **High** | Open | Planned `TEST-F01` | No `tests/` convention; headless GL / temp project setup duplicated |
| TD-003 | No `verify` script (build + smoke one command) | WF | **Medium** | Open | Roadmap infra | Slice DoD still ad hoc per task |
| TD-004 | Content Browser full tree rebuild on registry change | ED / ASSET | **Medium** | Open | [CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md](./Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md) | Incremental registry OK; UI model rebuilds whole tree |
| TD-005 | P4 reflection docs lag implementation | CORE | **Medium** | Open | [REFLECTION_FUNCTIONS_CURRENT_STATE.md](./Platform/Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md) | Code has invoke MVP; state doc still “no UFunction” — reconcile |
| TD-006 | Delegates + Lua not designed for implementation | CORE | **Low** | Deferred | P5/P6 placeholders | After CLI/Test stable |
| TD-007 | Render viewport refactor (multi-viewport) | RND | **Medium** | Deferred | [RENDER_REFACTOR_PLAN.md](./Render/RENDER_REFACTOR_PLAN.md) | Large; true refactor only with plan |
| TD-008 | Playground unmaintained / path hardcoding | Platform | **Low** | Deferred | PLATFORM_ROADMAP P0 tail | BUILD_PLAYGROUND off |
| TD-009 | Editor E1 Inspector unification | ED | **Medium** | Deferred | EDITOR_PLATFORM_PLAN E1 | Product; queue behind infra |
| TD-010 | GitHub Actions / remote CI | WF | **Low** | Deferred | After CLI+verify local | **CI = Continuous Integration**; separate from CLI |
| TD-011 | Post-commit context hook | WF | **Low** | Deferred | — | Optional; digest reduces need |
| TD-012 | Legacy doc IDs (Phase/M/E/P) vs F/S | WF | **Low** | Open | DOC_GOVERNANCE §10 | Migrate gradually; new docs use F/S only |

---

## Adding a row

```text
| TD-0nn | Short title | DOMAIN | High/Med/Low | Open/Deferred/Done | Feature or link | One line why deferred |
```

When paid down: set Status `Done`, add date in Notes, optional Progress log entry.
