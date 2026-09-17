# Bootstrap Digest (read in under 2 min)

Last updated: 2026-09-10  
Purpose: **one-page** context for humans and AI when starting or recovering a session. Details live in linked docs.  
**Doc trust:** `.cursor/rules/docs-trust-tiers.mdc` ‚Ä?do not treat old roadmaps as backlog; use [ACTIVE_WORK.md](./ACTIVE_WORK.md).

**Snapshot:** M1 Anim + M2 2D + M3 UI MVP on `master`. **0.1.0 window (Draft):** [ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md). Next Primary unset until that roadmap is approved. IDs: [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md).

---

## Project in one line

**minEngine** ‚Ä?personal C++ game engine; you learn by building it; **engineering bar is professional** on foundations (platform, reflection, assets, render core). **Capabilities, not opinions** ‚Ä?see [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md).

---

## Read order (new session)

1. [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) ‚Ä?long-term design constraints (skim principles)  
2. [PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md) ‚Ä?architecture snapshot  
3. [PROGRESS_LOG.md](./PROGRESS_LOG.md) ‚Ä?what changed recently (recent entries only)  
4. [ACTIVE_WORK.md](./ACTIVE_WORK.md) ‚Ä?**current backlog** (human-edited; agents prefer this over old roadmaps)  
5. **This file** ‚Ä?rules + commands + habits  
6. [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) ‚Ä?multi-track stage map (not a linear TODO)  
7. [TECH_DEBT.md](./TECH_DEBT.md) ‚Ä?Open rows only (what not to rush)  
8. [playbooks/README.md](./playbooks/README.md) ‚Ä?typical bug patterns & debugging (Tier B)  
9. Task-specific design ‚Ä?**only** if the user names it or ACTIVE_WORK links it; check Meta **Status** first ([doc trust tiers](../.cursor/rules/docs-trust-tiers.mdc))  

Do **not** scan [README.md](./README.md) roadmap lists to infer mandatory work.

**Active handoff (VK shadow quality, 2026-08-31):** [sessions/2026-08-31-vk-shadow-self-shadow-handoff.md](./sessions/2026-08-31-vk-shadow-self-shadow-handoff.md) ¬∑ [playbooks/Render/VK_SHADOW_DEBUGGING.md](./playbooks/Render/VK_SHADOW_DEBUGGING.md) ¬ß7

---

## IDs and docs

| Item | Rule |
|------|------|
| Feature | `<DOMAIN>-Fnn` ‚Ä?register in [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) first |
| Slice | `<FeatureID>-Snn` |
| Bug | `BUG-<DOMAIN>-nnn` ‚Ä?[bug template](./templates/bug-record.template.md) |
| New design | [DOC_GOVERNANCE.md](./templates/DOC_GOVERNANCE.md) + templates under [templates/](./templates/) |

**Domains (examples):** `WF`, `CORE`, `ASSET`, `ED`, `RND`, `MAT`, `TEST` ‚Ä?not a closed list.

---

## Agent partner (short)

- **Partner, not servant:** Pre-flight before new module / Feature / **refactor**; challenge weak plans; user decides after risks are stated.  
- **Philosophy:** Capabilities not opinions; mechanism over policy; minimal Core; Agent-friendly via shared APIs ‚Ä?remind when plans drift ([philosophy](./ENGINE_DESIGN_PHILOSOPHY.md)).  
- **Learning ‚â?sloppy:** simplify scope, not ownership or foundation quality.  
- **Refactor:** target state + delete old paths ‚Ä?no band-aid wrappers.  
- **Defects:** fixing module A ‚Ä?don‚Äôt drive-by fix large bugs in B/C/D; file `BUG-*` first.  
- **Work boundary:** after a finished batch ‚Ü?**offer ÂáÜÂ§á commit** before starting unrelated work.  
- **Stage mode:** one Primary track + parallel side tracks ‚Ä?not strict linear TODOs.

Skills: `.agents/skills/engine-learning-mentor/SKILL.md`, `.agents/skills/git-commit-mentor/SKILL.md`, `.agents/skills/engine-system-review/SKILL.md`  
Triggers: `.cursor/rules/docs-workflow-triggers.mdc` ¬∑ `.cursor/rules/engine-design-philosophy.mdc`

---

## Slice Done (DoD summary)

**Docs:** Progress entry; Design/Implementation updated; Registry status.  
**Engineering:** build or test command run (record which).  
**Commit message:** plain language what changed ‚Ä?not `ED-F03-S02` as the subject.  
**Prepare commit ‚â?execute** until you approve.

Full checklist: [DOC_GOVERNANCE.md](./templates/DOC_GOVERNANCE.md) ¬ß7.

---

## Build and run (current)

| What | Typical command |
|------|-----------------|
| Build Editor | `cmake --build minEngine/build --target Editor` ‚Ü?`bin/MaximumEditor.exe` |
| Build tests | `cmake --build minEngine/build --target minEngineTests` |
| **Verify (smoke)** | `.\scripts\verify.ps1` from repo root |
| Run tests | `minEngineTests.exe test smoke` from `minEngine/bin` |
| Engine config | `--engine-config=`, `--engine-root=` (or space form); env `MINENGINE_ENGINE_*` |

**Unified CLI (`CLI-F01`, from `minEngine/bin`):**

| Command | Purpose |
|---------|---------|
| `MaximumEditor.exe --help` | Global options + `test` subcommand |
| `MaximumEditor.exe test --help` | smoke / full / suite-id |
| `MaximumEditor.exe test material-ir` | Material IR headless smoke (preferred) |
| `MaximumEditor.exe --project <path.meproject>` | Open project (default editor mode) |

Entry: `minEngineTests.exe` (`Tests/TestMain.cpp`) or `MaximumEditor.exe test ‚Ä¶` (forwards to minEngineTests). No `--*-test` legacy flags.

**Verify:** `.\scripts\verify.ps1` ‚Ü?`minEngineTests.exe test smoke`.

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
| ÂáÜÂ§á commit / ÊàëË¶Å commit | Draft message + DoD; **no** `git commit` until you approve execution |
| handoff / ‰∫§Êé• | session note + Progress; Block incomplete slice |
| Êñ∞Ê®°Âù?/ ÈáçÊûÑ | Pre-flight + Registry |
| re-bootstrap / ÂØπÈΩêÁ∫¶Êùü | Re-apply hard constraints + this digest |

See [WORKING_WITH_AI.md](./WORKING_WITH_AI.md).
