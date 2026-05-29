# Bootstrap Digest (read in under 2 min)

Last updated: 2026-05-28  
Purpose: **one-page** context for humans and AI when starting or recovering a session. Details live in linked docs.

---

## Project in one line

**minEngine** — personal C++ game engine; you learn by building it; **engineering bar is professional** on foundations (platform, reflection, assets, render core).

---

## Read order (new session)

1. [PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md) — architecture snapshot  
2. [PROGRESS_LOG.md](./PROGRESS_LOG.md) — what changed recently  
3. **This file** — rules + commands + habits  
4. Task-specific: [README.md](./README.md) → `Platform/` / `Editor/` / `Render/`  
5. [TECH_DEBT.md](./TECH_DEBT.md) — what not to rush  

---

## IDs and docs

| Item | Rule |
|------|------|
| Feature | `<DOMAIN>-Fnn` — register in [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) first |
| Slice | `<FeatureID>-Snn` |
| Bug | `BUG-<DOMAIN>-nnn` — [bug template](./templates/bug-record.template.md) |
| New design | [DOC_GOVERNANCE.md](./templates/DOC_GOVERNANCE.md) + templates under [templates/](./templates/) |

**Domains (examples):** `WF`, `CORE`, `ASSET`, `ED`, `RND`, `MAT`, `TEST` — not a closed list.

---

## Agent partner (short)

- **Partner, not servant:** Pre-flight before new module / Feature / **refactor**; challenge weak plans; user decides after risks are stated.  
- **Learning ≠ sloppy:** simplify scope, not ownership or foundation quality.  
- **Refactor:** target state + delete old paths — no band-aid wrappers.  
- **Defects:** fixing module A — don’t drive-by fix large bugs in B/C/D; file `BUG-*` first.  
- **Work boundary:** after a finished batch → **offer 准备 commit** before starting unrelated work.  

Skills: `.cursor/skills/engine-learning-mentor/SKILL.md`, `.cursor/skills/git-commit-mentor/SKILL.md`  
Triggers: `.cursor/rules/docs-workflow-triggers.mdc`

---

## Slice Done (DoD summary)

**Docs:** Progress entry; Design/Implementation updated; Registry status.  
**Engineering:** build or test command run (record which).  
**Commit message:** plain language what changed — not `ED-F03-S02` as the subject.  
**Prepare commit ≠ execute** until you approve.

Full checklist: [DOC_GOVERNANCE.md](./templates/DOC_GOVERNANCE.md) §7.

---

## Build and run (current)

| What | Typical command |
|------|-----------------|
| Build Editor | `cmake --build minEngine/build --target Editor` |
| Run Editor | from `minEngine/bin` (EngineConfig / project paths apply) |
| Engine config | `--engine-config=`, `--engine-root=`; env `MINENGINE_ENGINE_*` |

**Headless tests today (scattered CLI — to be unified under `CLI-F01` / `TEST-F01`):**

| Flag | Area |
|------|------|
| `--material-ir-test` | Material IR / GPU smoke |
| `--asset-manager-test` | AssetManager CRUD |
| `--object-manager-test` | ObjectManager / GC |
| `--serialization-archive-test` | Binary archive |
| `--reflection-function-test` | Reflection functions (optional `=suite`) |

Entry: `minEngine/minEngine/src/main.h` dispatches before normal app startup (Editor links same runtime).

**Planned:** unified CLI + `--run-tests smoke|full` + `scripts/verify` (build + smoke). See [TECH_DEBT.md](./TECH_DEBT.md) TD-001, TD-002.

---

## Current infra focus (not in this file’s scope)

- **Next roadmap (planned):** CLI unification → test framework → migrate flags → verify binds to Slice DoD.  
- **Defer for now:** large Editor features, Lua, drive-by platform refactors — see TECH_DEBT.

---

## User phrases

| You say | Agent should |
|---------|----------------|
| 准备 commit / 我要 commit | Draft message + DoD; **no** `git commit` until you approve execution |
| handoff / 交接 | session note + Progress; Block incomplete slice |
| 新模块 / 重构 | Pre-flight + Registry |
| re-bootstrap / 对齐约束 | Re-apply hard constraints + this digest |

See [WORKING_WITH_AI.md](./WORKING_WITH_AI.md).
