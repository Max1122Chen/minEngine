# Bootstrap Digest (read in under 2 min)

Last updated: 2026-09-03  
Purpose: **one-page** context for humans and AI when starting or recovering a session. Details live in linked docs.  
**Doc trust:** `.cursor/rules/docs-trust-tiers.mdc` — do not treat old roadmaps as backlog; use [ACTIVE_WORK.md](./ACTIVE_WORK.md).

---

## Project in one line

**minEngine** — personal C++ game engine; you learn by building it; **engineering bar is professional** on foundations (platform, reflection, assets, render core). **Capabilities, not opinions** — see [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md).

---

## Read order (new session)

1. [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) — long-term design constraints (skim principles)  
2. [PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md) — architecture snapshot  
3. [PROGRESS_LOG.md](./PROGRESS_LOG.md) — what changed recently (recent entries only)  
4. [ACTIVE_WORK.md](./ACTIVE_WORK.md) — **current backlog** (human-edited; agents prefer this over old roadmaps)  
5. **This file** — rules + commands + habits  
6. [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) — multi-track stage map (not a linear TODO)  
7. [TECH_DEBT.md](./TECH_DEBT.md) — Open rows only (what not to rush)  
8. [playbooks/README.md](./playbooks/README.md) — typical bug patterns & debugging (Tier B)  
9. Task-specific design — **only** if the user names it or ACTIVE_WORK links it; check Meta **Status** first ([doc trust tiers](../.cursor/rules/docs-trust-tiers.mdc))  

Do **not** scan [README.md](./README.md) roadmap lists to infer mandatory work.

**Active handoff (VK shadow quality, 2026-08-31):** [sessions/2026-08-31-vk-shadow-self-shadow-handoff.md](./sessions/2026-08-31-vk-shadow-self-shadow-handoff.md) · [playbooks/Render/VK_SHADOW_DEBUGGING.md](./playbooks/Render/VK_SHADOW_DEBUGGING.md) §7

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
- **Philosophy:** Capabilities not opinions; mechanism over policy; minimal Core; Agent-friendly via shared APIs — remind when plans drift ([philosophy](./ENGINE_DESIGN_PHILOSOPHY.md)).  
- **Learning ≠ sloppy:** simplify scope, not ownership or foundation quality.  
- **Refactor:** target state + delete old paths — no band-aid wrappers.  
- **Defects:** fixing module A — don’t drive-by fix large bugs in B/C/D; file `BUG-*` first.  
- **Work boundary:** after a finished batch → **offer 准备 commit** before starting unrelated work.  
- **Stage mode:** one Primary track + parallel side tracks — not strict linear TODOs.

Skills: `.agents/skills/engine-learning-mentor/SKILL.md`, `.agents/skills/git-commit-mentor/SKILL.md`  
Triggers: `.cursor/rules/docs-workflow-triggers.mdc` · `.cursor/rules/engine-design-philosophy.mdc`

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
| Build tests | `cmake --build minEngine/build --target minEngineTests` |
| **Verify (smoke)** | `.\scripts\verify.ps1` from repo root |
| Run tests | `minEngineTests.exe test smoke` from `minEngine/bin` |
| Engine config | `--engine-config=`, `--engine-root=` (or space form); env `MINENGINE_ENGINE_*` |

**Unified CLI (`CLI-F01`, from `minEngine/bin`):**

| Command | Purpose |
|---------|---------|
| `Editor.exe --help` | Global options + `test` subcommand |
| `Editor.exe test --help` | smoke / full / suite-id |
| `Editor.exe test material-ir` | Material IR headless smoke (preferred) |
| `Editor.exe --project <path.meproject>` | Open project (default editor mode) |

Entry: `minEngineTests.exe` (`Tests/TestMain.cpp`) or `Editor.exe test …` (forwards to minEngineTests). No `--*-test` legacy flags.

**Verify:** `.\scripts\verify.ps1` → `minEngineTests.exe test smoke`.

---

## Planning vs reference docs

| Use for **what to do next** | Reference only (no automatic backlog) |
|-----------------------------|----------------------------------------|
| [ACTIVE_WORK.md](./ACTIVE_WORK.md) | `Platform/*_ROADMAP.md`, `Editor/*_PLAN.md`, `*_CURRENT_STATE.md`, Archived issue docs |
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) (In Progress / Planned) | Done / Snapshot / Archived / Reference Meta |
| Code + `verify.ps1` / `minEngineTests` | Unchecked boxes in old designs |

Infra slice **CLI + unified tests + TEST-F03** is **done**; [INFRASTRUCTURE_ROADMAP.md](./Platform/INFRASTRUCTURE_ROADMAP.md) is maintenance/history (Meta: Done).

---

## User phrases

| You say | Agent should |
|---------|----------------|
| 准备 commit / 我要 commit | Draft message + DoD; **no** `git commit` until you approve execution |
| handoff / 交接 | session note + Progress; Block incomplete slice |
| 新模块 / 重构 | Pre-flight + Registry |
| re-bootstrap / 对齐约束 | Re-apply hard constraints + this digest |

See [WORKING_WITH_AI.md](./WORKING_WITH_AI.md).
