# Working With AI in minEngine

Last updated: 2026-03-26

## Why this exists

This project has no formal in-repo dev diary yet.
To avoid repeating background in every chat, keep concise AI-readable context in docs/ai.

## Minimal workflow

1) Before coding discussion
- Mention one line only: continue previous task in this repo.

2) During discussion
- Ask AI to read docs/ai/PROJECT_CONTEXT.md and docs/ai/PROGRESS_LOG.md first.

3) At the end of each meaningful session
- Ask AI: record this session into progress log with next action.

## Suggested user prompts

- Start prompt:
  Continue last minEngine task. First read docs/ai/PROJECT_CONTEXT.md and docs/ai/PROGRESS_LOG.md, then summarize current context and propose next step.

- End prompt:
  Please append today progress to docs/ai/PROGRESS_LOG.md using the template, and list the first action for next session.

## Optional: session notes folder

If tasks become complex, create short session notes under:
- docs/ai/sessions/YYYY-MM-DD-topic.md

Recommended sections:
- Background
- Decisions
- Open issues
- Next immediate action
