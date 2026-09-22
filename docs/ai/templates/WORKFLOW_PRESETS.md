# Workflow Presets Catalog

Authority for TMPL-F02. Agents use this when configuring or reconfiguring `docs/ai/WORKFLOW_PROFILE.md`.

## 0) Meta-question

Ask only when `WORKFLOW_PROFILE.md` status is `unconfigured`.

| Answer | Agent action |
|--------|--------------|
| `configure now` | Offer named presets first; allow dimension overrides; write `status: configured` |
| `skip` | Apply preset `serious-engineering`; write `status: default-applied` |
| `later` | Set `status: deferred`; do not re-ask unless user says `reconfigure workflow` |

Explain purpose in one sentence:

> These settings control agent strictness, ask-vs-act behavior, role stance, and docs language for later sessions.

## 1) Named presets (prefer these)

### `learning-mentor`

| Field | Value |
|-------|-------|
| `repo_posture` | `learning` |
| `autonomy` | `propose-then-act` |
| `agent_role` | `mentor` |
| `docs_language` | `en` (or user choice) |
| `docs_topology` | `single-track` |
| `verification_bar` | `smoke-required` |
| `implementation_discipline` | `on` |

**Effect**
- Explain why, not only what
- Challenge weak foundations and skipped workflow
- Pre-flight: recommended for new modules/refactors
- Pause for approval on irreversible/destructive ops and broad refactors
- Keep professional engineering bar even in learning repos
- Design ceremony: teach during discovery/design; still require Design Review + readiness for L2+/L3
- Prefer explaining alternatives and overengineering checks explicitly

### `prototype-partner`

| Field | Value |
|-------|-------|
| `repo_posture` | `prototype` |
| `autonomy` | `act-within-slice` |
| `agent_role` | `partner` |
| `docs_language` | `en` (or user choice) |
| `docs_topology` | `single-track` (offer dual-track if product docs exist) |
| `verification_bar` | `smoke-required` |
| `implementation_discipline` | `off` (default; can turn on) |

**Effect**
- Optimize for fast validated slices
- Less ceremony; still register Feature IDs for substantial work
- Pre-flight: optional unless refactor/multi-module
- Challenge lightly when trust tiers are violated
- Prefer partner tone: propose options, then move
- Design ceremony: L1 may use short notes; L2 keeps lean Design + Review; L3 still requires readiness (ownership/public contracts/migration cannot be skipped)

### `serious-engineering` (default on skip)

| Field | Value |
|-------|-------|
| `repo_posture` | `serious-engineering` |
| `autonomy` | `propose-then-act` |
| `agent_role` | `partner` |
| `docs_language` | `en` |
| `docs_topology` | `single-track` |
| `verification_bar` | `tests-required` if tests exist, else `smoke-required` |
| `implementation_discipline` | `on` |

**Effect**
- Strict workflow adherence
- Challenge when Feature registration/DoD/commit gate is skipped
- Pre-flight: required for new Feature/Refactor
- Dual-path temporary layers must be removed or registered as debt
- Partner stance with high willingness to push back on scope/quality
- Design ceremony: L2+ requires Design Spec + Design Review + readiness before large coding; L3 mandatory

### `executor-tight`

| Field | Value |
|-------|-------|
| `repo_posture` | `commercial` |
| `autonomy` | `ask-first` for scope changes; `act-within-slice` once approved |
| `agent_role` | `executor` |
| `docs_language` | `en` |
| `docs_topology` | `single-track` |
| `verification_bar` | `tests-required` if tests exist, else `smoke-required` |
| `implementation_discipline` | `on` |

**Effect**
- Minimal teaching prose
- Ask before expanding scope; execute approved slice efficiently
- High compliance with DoD and verification bar
- Challenge mainly on process/safety violations, not style preferences
- Design ceremony: terse reviews allowed, but cannot skip L2+/L3 architectural safety (ownership, contracts, failure, migration)

## 2) Dimension reference (custom / overrides)

Use when user rejects named presets or wants fine-tuning.

### `repo_posture`

| Value | Challenge level | Pre-flight | Notes |
|-------|-----------------|------------|-------|
| `learning` | medium-high on foundations | recommended | Teaching allowed; quality bar stays high |
| `prototype` | low-medium | optional | Speed with accountable slices |
| `serious-engineering` | high on workflow skips | required for Feature/Refactor | Default professional posture; L2+/L3 design+review+readiness |
| `commercial` | high on risk/process | required for Feature/Refactor | Prefer compliance over exploration; L3 safety non-skippable |

### `autonomy`

| Value | Agent pauses when |
|-------|-------------------|
| `ask-first` | Before non-trivial edits, new files beyond tiny fixes, or scope expansion |
| `propose-then-act` | After stating plan for substantial work; tiny fixes may proceed |
| `act-within-slice` | Only for out-of-slice work, destructive ops, or ambiguous requirements |

### `agent_role`

| Value | Behavior |
|-------|----------|
| `mentor` | Teach rationale; offer exercises/alternatives; challenge fundamentals |
| `partner` | Collaborative; propose options with recommendation; push back on weak plans |
| `advisor` | Prefer analysis and recommendations; wait for explicit implement request |
| `executor` | Minimize lecture; implement approved scope; escalate blockers clearly |

### `docs_language`

| Value | Effect |
|-------|--------|
| `en` | New workflow docs and progress entries in English (default) |
| `zh` | New workflow docs and progress entries in Chinese |
| `bilingual` | English primary; add short Chinese summary for large/meaningful batches |

### `docs_topology`

| Value | Effect |
|-------|--------|
| `single-track` | Product + implementation decisions live under `docs/ai/` (and normal project docs) |
| `dual-track` | Product truth path recorded in profile; agent treats it as read-only unless user delegates edits |

### `verification_bar`

| Value | Slice engineering DoD |
|-------|------------------------|
| `docs-only` | Docs/progress update sufficient for docs-only slices; still record N/A for code |
| `smoke-required` | Run and record smoke/verify command when code changes |
| `tests-required` | Run and record relevant tests (or justify blocker) for code slices |

### `implementation_discipline`

| Value | Effect |
|-------|--------|
| `on` | Reuse-first; remove superseded paths in-change; register transitional debt |
| `off` | Do not force cleanup beyond what user/task requires; still no silent unbounded dual paths without debt if touched |

## 3) Apply algorithm (for agents)

1. Read `docs/ai/WORKFLOW_PROFILE.md`.
2. If `status` is `configured` or `default-applied`: follow Effective behavior; do not re-ask.
3. If `status` is `deferred`: do not re-ask unless user requests reconfigure.
4. If `status` is `unconfigured`:
   - Ask meta-question once.
   - On configure: present the 4 presets; allow custom dimensions.
   - Write all fields + Effective behavior.
   - Confirm with a short “how I will behave” summary.
5. On `reconfigure workflow`: repeat step 4 and diff old vs new fields.

## 4) Adapter expectations

All agent adapters should mention:
- Read `WORKFLOW_PROFILE.md` during bootstrap
- Honor meta-question when `unconfigured`
- Never disable non-negotiable constraints
- For non-trivial work: complexity assessment + engineering-design / design-review when required by level

## 5) Design ceremony by complexity (summary)

| Level | Minimum expectation |
|-------|---------------------|
| L0 | No design review |
| L1 | Short design optional |
| L2 | Design + Design Review + Plan before large coding |
| L3 | Same as L2 + explicit Implementation Readiness gate |

Details: `DOC_GOVERNANCE.md` §5.2–5.3; skills under `.agents/skills/engineering-design/` and `design-review/`.
