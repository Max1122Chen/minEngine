# Working With AI in minEngine

Last updated: 2026-10-15

## Why this exists

This project has no formal in-repo dev diary yet.
To avoid repeating background in every chat, keep concise AI-readable context in docs/ai.

**Design philosophy:** [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) — agents must apply it on architecture and Feature planning, and remind you when plans drift.

**Workflow posture:** [WORKFLOW_PROFILE.md](./WORKFLOW_PROFILE.md) — preset/autonomy/role; say `reconfigure workflow` to change. Presets: [templates/WORKFLOW_PRESETS.md](./templates/WORKFLOW_PRESETS.md).

## Minimal workflow

1) Before coding discussion
- Mention one line only: continue previous task in this repo.

2) During discussion
- Ask AI to read docs/ai/WORKFLOW_PROFILE.md, ENGINE_DESIGN_PHILOSOPHY.md, PROJECT_CONTEXT.md, PROGRESS_LOG.md, and ACTIVE_WORK.md first.
- Stage direction: ENGINE_CAPABILITY_ROADMAP.md (multi-track; not a linear TODO).
- Design docs live under docs/ai/Platform/, Render/, Editor/ — see docs/ai/README.md.
- New design/work: assess **L0–L3**; copy from docs/ai/templates/ and follow docs/ai/templates/DOC_GOVERNANCE.md (Feature ID, Slice ID, status, DoD, readiness).
- Triggers (plan/design/bug/wrap-up/commit prep): `.cursor/rules/docs-workflow-triggers.mdc` → mentor skill for execution; **准备 commit** = draft message + your approval (not auto-commit).
- New feature: register in `docs/ai/FEATURE_REGISTRY.md` before Design; L2+ use **`engineering-design`** then **design-review** readiness before large coding.
- Handoff end prompt: `请按 handoff 流程写 session 笔记、更新 Progress，未完成 slice 标 Blocked，并给我下一会话首句。`
- New module / big feature / **refactor**: expect **Pre-flight**; refactors need a plan and **deletion of old paths**, not band-aid layers; learning project ≠ sloppy engineering.
- After a finished batch (slice / workflow docs): agent should **proactively draft the commit message** (DoD + draft) for your approval — you do **not** need to say 准备 commit first. Say `先不提交` if you want to keep going without a draft.
- UE / Godot comparisons: agent should read local trees `D:/Dev/GitRepo/UnrealEngine` and `D:/Dev/GitRepo/Godot` (read-only) instead of relying on model memory for concrete APIs.
- Design Spec readiness (before coding or after multi-round revisions): ask for **design review** → `.agents/skills/design-review/SKILL.md` (distinct from code **system review**). Reject shell products (empty critical TBD, unverifiable acceptance).
- Structure lenses: [templates/DESIGN_PRINCIPLES.md](./templates/DESIGN_PRINCIPLES.md) — no SOLID matrix ceremony.
- No Feature/Slice/Bug/ADR IDs in source comments; branches `feat/<topic>[/<subtopic>]`, never ID-based.
- Ambiguous L1+ requests: short intent playback before large design/coding (you may skip).

3) At the end of each meaningful session
- Ask AI: record this session into progress log with next action.

## Suggested user prompts

- Start prompt:
  Continue last minEngine task. First read docs/ai philosophy + PROJECT_CONTEXT + PROGRESS_LOG + ACTIVE_WORK, then summarize current context and propose next step.

- End prompt:
  Please append today progress to docs/ai/PROGRESS_LOG.md using docs/ai/templates/progress-log-entry.template.md, and list the first action for next session.

## Optional: session notes folder

If tasks become complex, create short session notes under:
- docs/ai/sessions/YYYY-MM-DD-topic.md

Recommended sections:
- Background
- Decisions
- Open issues
- Next immediate action
