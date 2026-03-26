---
name: git-commit-mentor
description: "Prepare high-quality commit messages for this repository. Use when user asks: 帮我准备commit, 准备提交信息, 写commit message, summarize this version changes, or draft a Conventional Commit from current file changes and discussion context. Always request user approval before running git commit."
---

# Git Commit Mentor

## Purpose

Help the user prepare accurate, reviewable commit messages that match repository style and common best practices.

## Trigger phrases

This skill should be used when user says things like:
- 帮我准备commit
- 帮我写提交信息
- 写一下commit message
- summarize this version and draft commit

## User style baseline in this repository

Observed style from recent history:
- Conventional Commit style in subject line, commonly using feat/fix/refactor with optional scope.
- Subject often followed by a detailed bullet-like body describing:
  - new features,
  - bug fixes,
  - refactors,
  - caveats and known limitations.
- Tone is practical and explanatory, sometimes includes development notes.

## Required workflow

1. Collect evidence
- Inspect current file changes and staged/unstaged diff summary.
- Group changes into: Feature, Fix, Refactor, Docs, Chore.
- Consider relevant discussion context from current conversation.

2. Draft message options
- Provide 1-2 subject line options in Conventional Commit form.
- Provide one full commit message draft matching repository style.
- Ensure message reflects only actual changes.

3. Approval gate (mandatory)
- Ask user to approve or request edits.
- Do not run git commit automatically.
- If user approves and asks to commit, then execute commit.

4. Final safety check
- Re-verify changed files before commit in case edits occurred after draft.

## Commit message format

Preferred default format:

<type>(<scope>): <short summary>
- <change 1>
- <change 2>
- <change 3>

Optional notes:
- <limitations or follow-up>

Type guidance:
- feat: new behavior or capability
- fix: bug fix or correctness issue
- refactor: structural/internal improvement without intended behavior change
- docs: documentation changes
- chore: maintenance or tooling

## Language policy

- Default: English-only commit message unless user explicitly requests otherwise.
- Use bilingual (Chinese + English) commit message only when the version has broad scope and large change volume across multiple subsystems.
- For bilingual commits, keep the same factual content in both languages and avoid adding mismatched claims.
- If change scope is small or localized, do not use bilingual by default.

## Quality rules

- Subject line should be specific, not vague.
- Body should map to concrete code changes.
- Mention important bug root cause when known.
- Avoid claiming unverified outcomes.
- Keep language concise and readable.
- Apply language policy consistently before drafting options.

## Approval prompt template

Use this prompt after drafting:
- "这是我准备的 commit message 草稿。请确认是否通过；如果你同意，我再执行 git commit。"

## Common pitfalls

- Mixing unrelated topics into one commit message.
- Using feat when dominant change is a fix.
- Omitting critical fix details (root cause and effect).
- Committing before explicit approval.
