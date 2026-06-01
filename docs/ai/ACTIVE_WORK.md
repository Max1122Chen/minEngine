# Active work (agent backlog)

Last updated: 2026-06-01  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

- **WF-F02** 二期 S04–S07 已实施；push `main` 验 Pages。handbook 正文由维护者按需补充。

Example entries (delete or replace):

- Remote CI: run `scripts/verify.ps1` on push (smoke only).
- Reconnect reflection `types` / `static` tests in `test full` after sample functions are aligned.
- Asset Inspector: show selected asset meta with PropertyWidgets instead of plain text.

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## Explicitly not backlog (unless you promote them)

These are **optional future feats**, not debt owed by old docs:

- Editor: unified Inspector target model, Material graph Undo, texture preview in Inspector.
- Core: delegates, Lua scripting bindings.
- Content Browser: further registry/watcher optimizations beyond R1 incremental `AssetTreeModel` patch.
- Infra: GitHub Actions (see `TECH_DEBT.md` TD-010 when you want it).

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [TECH_DEBT.md](./TECH_DEBT.md) | Deferred problems worth tracking |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| Old roadmaps under `Platform/`, `Editor/` | Architecture reference only — see `.cursor/rules/docs-trust-tiers.mdc` |
