# Bootstrap Digest (read in under 2 min)

Last updated: 2026-10-16  
Purpose: **one-page** context for humans and AI when starting or recovering a session. Details live in linked docs.  
**Doc trust:** `.cursor/rules/docs-trust-tiers.mdc` -- do not treat old roadmaps as backlog; use [ACTIVE_WORK.md](./ACTIVE_WORK.md).

**Snapshot:** Workflow **`learning-mentor`**. Branch **`master`**. **Primary：** `ASSET-F10`（S01/S02 Done）。并行候选：`CORE-F38` TaskSystem（**仅** TaskFlow + 合成证明；**不含** Asset 异步）。`ASSET-F09` **Deferred**。详见 [ACTIVE_WORK.md](./ACTIVE_WORK.md)。

---

## Project in one line

**minEngine** -- personal C++ game engine; you learn by building it; **engineering bar is professional** on foundations (platform, reflection, assets, render core). **Capabilities, not opinions** -- see [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md).

---

## Read order (new session)

1. [WORKFLOW_PROFILE.md](./WORKFLOW_PROFILE.md) -- collaboration posture (if `unconfigured`, ask meta-question once)  
2. [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) -- long-term design constraints (skim principles)  
3. [PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md) -- architecture snapshot  
4. [PROGRESS_LOG.md](./PROGRESS_LOG.md) -- what changed recently (recent entries only)  
5. [ACTIVE_WORK.md](./ACTIVE_WORK.md) -- **current backlog** (human-edited; agents prefer this over old roadmaps)  
6. **This file** -- rules + commands + habits  
7. [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) -- multi-track stage map (not a linear TODO)  
8. [TECH_DEBT.md](./TECH_DEBT.md) -- Open rows only (what not to rush)  
9. [playbooks/README.md](./playbooks/README.md) -- typical bug patterns & debugging (Tier B)  
10. Task-specific design -- **only** if the user names it or ACTIVE_WORK links it; check Meta **Status** first ([doc trust tiers](../../.cursor/rules/docs-trust-tiers.mdc))  

Do **not** scan [README.md](./README.md) roadmap lists to infer mandatory work.

**Active handoff (VK shadow quality, 2026-08-31):** [sessions/2026-08-31-vk-shadow-self-shadow-handoff.md](./sessions/2026-08-31-vk-shadow-self-shadow-handoff.md) / [playbooks/Render/VK_SHADOW_DEBUGGING.md](./playbooks/Render/VK_SHADOW_DEBUGGING.md) section 7

---

## IDs and docs

| Item | Rule |
|------|------|
| Feature | `<DOMAIN>-Fnn` -- register in [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) first |
| Slice | `<FeatureID>-Snn` |
| Bug | `BUG-<DOMAIN>-nnn` -- [bug template](./templates/bug-record.template.md) |
| New design | [DOC_GOVERNANCE.md](./templates/DOC_GOVERNANCE.md) + templates under [templates/](./templates/) |

**Domains (examples):** `WF`, `CORE`, `ASSET`, `ED`, `RND`, `MAT`, `TEST` -- not a closed list.

---

## Agent partner (short)

- **Partner, not servant:** Pre-flight before new module / Feature / **refactor**; challenge weak plans; user decides after risks are stated.  
- **Philosophy:** Capabilities not opinions; mechanism over policy; minimal Core; Agent-friendly via shared APIs -- remind when plans drift ([philosophy](./ENGINE_DESIGN_PHILOSOPHY.md)).  
- **Learning != sloppy:** simplify scope, not ownership or foundation quality.  
- **Refactor:** target state + delete old paths -- no band-aid wrappers.  
- **Defects:** fixing module A -- do not drive-by fix large bugs in B/C/D; file `BUG-*` first.  
- **Work boundary:** after a finished deliverable batch -> **proactively draft commit** (DoD + message); wait for approval -- user need not say prepare commit first.  
- **Stage mode:** one Primary track + parallel side tracks -- not strict linear TODOs.
- No Feature/Slice/Bug/ADR IDs in source comments; branches use `feat/<topic>[/<subtopic>]`, never ID-based names.
- Profile may tune strictness/autonomy/role/language; it cannot disable the rules above (including comment/branch ID bans). Structure lenses: [DESIGN_PRINCIPLES.md](./templates/DESIGN_PRINCIPLES.md).

Skills: `.agents/skills/engine-learning-mentor/SKILL.md`, `.agents/skills/engineering-design/SKILL.md`, `.agents/skills/design-review/SKILL.md`, `.agents/skills/git-commit-mentor/SKILL.md`, `.agents/skills/engine-system-review/SKILL.md`  
Triggers: `.cursor/rules/docs-workflow-triggers.mdc` / `.cursor/rules/engine-design-philosophy.mdc`  
Design loop (L2+): complexity → Design Spec → Design Review / readiness → Implementation Plan → verify (DOC_GOVERNANCE §5.5–5.6)

---

## External engine source (read-only)

| Engine | Local path | When |
|--------|------------|------|
| Unreal Engine | `D:/Dev/GitRepo/UnrealEngine` | reference / align / how UE does X / architecture mapping |
| Godot | `D:/Dev/GitRepo/Godot` | same for Godot |

**Rule:** open source and cite class/function/path anchors; do **not** invent concrete APIs from model memory. If path missing, say so and fall back. Details: mentor skill section External source bootstrap.

---

## Slice Done (DoD summary)

**Docs:** Progress entry; Design/Implementation updated; Registry status.  
**Engineering:** build or test command run (record which).  
**Commit message:** plain language what changed -- not `ED-F03-S02` as the subject.  
**Prepare commit != execute** until you approve. Agent should **draft** after a deliverable batch without waiting for you to ask.

Full checklist: [DOC_GOVERNANCE.md](./templates/DOC_GOVERNANCE.md) section 7.

---

## Build and run (current)

| What | Typical command |
|------|-----------------|
| Build Editor | `cmake --build minEngine/build --target Editor` -> `bin/MaximumEditor.exe` |
| Build tests | `cmake --build minEngine/build --target minEngineTests` |
| **Verify (smoke)** | `.\scripts\verify.ps1` from repo root |
| Run tests | `minEngineTests.exe test smoke` from `minEngine/bin` |
| Engine config | `--engine-config=`, `--engine-root=` (or space form); env `MINENGINE_ENGINE_*` |

**Product / sample hosts:** project roots own **Editor + Game** executables and link the game SHARED explicitly. In-tree sample (default ON): `BUILD_GAME_MODULE_SAMPLE` → `SampleGame` / `SampleEditor` + `package-SampleGame`. Launcher template: `GameProject` (`minlauncher create <Name> --template GameProject`). Design: [BUILD-F01](./Platform/ProductBuild/BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md).

Prove external `ME_CLASS` + `AddComponent<T>()` (with sample enabled):

```powershell
.\minEngine\bin\minEngineTests.exe test game-module-stub
```

Do **not** copy engine math types or maintain a no-engine dual build; use TEST-F05 for domain suites. World setup = game/default scene (no official C Startup).

**Optional extra test suites (`TEST-F05`, off by default):**

```powershell
cmake -S minEngine -B minEngine/build `
  -DMINENGINE_EXTRA_TEST_DIR=minEngine/Tests/Fixtures/ExtraTestStub
.\minEngine\bin\minEngineTests.exe test game.stub-extra
```

See [TEST-F05 Design](./Platform/Test/TEST-F05_EXTERNAL_TEST_HARNESS_DESIGN.md) and `Tests/Fixtures/ExtraTestStub/README.md`.

**Unified CLI (`CLI-F01`, from `minEngine/bin`):**

| Command | Purpose |
|---------|---------|
| `MaximumEditor.exe --help` | Global options + `test` subcommand |
| `MaximumEditor.exe test --help` | smoke / full / suite-id |
| `MaximumEditor.exe test material-ir` | Material IR headless smoke (preferred) |
| `MaximumEditor.exe --project <path.meproject>` | Open project (default editor mode) |

Entry: `minEngineTests.exe` (`Tests/TestMain.cpp`) or `MaximumEditor.exe test ...` (forwards to minEngineTests). No `--*-test` legacy flags.

**Verify:** `.\scripts\verify.ps1` -> `minEngineTests.exe test smoke`.

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
| prepare commit / 我要 commit | Draft message + DoD; **no** `git commit` until you approve execution |
| (after agent finishes a batch) | Agent **drafts** commit without you asking; say `先不提交` to skip |
| handoff / 交接 | session note + Progress; Block incomplete slice |
| 新模块 / 重构 | Pre-flight + Registry |
| 参考 UE / Godot / 对标 | Read local UE/Godot trees; cite source anchors |
| 设计评审 / design review | `design-review` on Design Spec (readiness / consistency) |
| 系统复盘 / system review | `engine-system-review` on code/subsystem |
| re-bootstrap / 对齐约束 | Re-apply hard constraints + this digest |

See [WORKING_WITH_AI.md](./WORKING_WITH_AI.md).
