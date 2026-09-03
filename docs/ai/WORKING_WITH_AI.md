# Working With AI in minEngine

Last updated: 2026-09-03

## Why this exists

This project has no formal in-repo dev diary yet.
To avoid repeating background in every chat, keep concise AI-readable context in docs/ai.

**Design philosophy:** [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) — agents must apply it on architecture and Feature planning, and remind you when plans drift.

## Minimal workflow

1) Before coding discussion
- Mention one line only: continue previous task in this repo.

2) During discussion
- Ask AI to read docs/ai/ENGINE_DESIGN_PHILOSOPHY.md, PROJECT_CONTEXT.md, PROGRESS_LOG.md, and ACTIVE_WORK.md first.
- Stage direction: ENGINE_CAPABILITY_ROADMAP.md (multi-track; not a linear TODO).
- Design docs live under docs/ai/Platform/, Render/, Editor/ — see docs/ai/README.md.
- New design/work: copy from docs/ai/templates/ and follow docs/ai/templates/DOC_GOVERNANCE.md (Feature ID, Slice ID, status, DoD).
- Triggers (plan/design/bug/wrap-up/commit prep): `.cursor/rules/docs-workflow-triggers.mdc` → mentor skill for execution; **准备 commit** = draft message + your approval (not auto-commit).
- New feature: register in `docs/ai/FEATURE_REGISTRY.md` before Design.
- Handoff end prompt: `请按 handoff 流程写 session 笔记、更新 Progress，未完成 slice 标 Blocked，并给我下一会话首句。`
- New module / big feature / **refactor**: expect **Pre-flight**; refactors need a plan and **deletion of old paths**, not band-aid layers; learning project ≠ sloppy engineering.
- After a finished batch (slice / workflow docs): agent should **offer 准备 commit** before suggesting the next Feature — say `先不提交` if you want to keep going without committing.

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
