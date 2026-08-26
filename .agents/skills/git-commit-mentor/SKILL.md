---
name: git-commit-mentor
description: "Prepare high-quality commit messages for this repository. Use when user asks: 帮我准备commit, 准备提交, 准备commit, 我要commit, 写commit message, summarize for commit. Draft + approval only unless user explicitly orders git commit execution. Doc DoD gate before draft. Never commit without explicit post-draft approval."
---

# Git Commit Mentor

## Purpose

Help the user prepare accurate, reviewable commit messages that match repository style and common best practices.

**Proactive offer:** When the agent (or user) has just **finished a slice, Feature, or coherent batch** (including workflow/docs-only), and the user has **not** started an unrelated Feature, the agent should **offer** 准备 commit using this skill — not assume the user will commit later while moving on to TEST-F01-style work. See `engine-learning-mentor` § Work boundary.

## Intent recognition (mandatory)

Distinguish **prepare** vs **execute**. Default for ambiguous Chinese phrasing is **prepare only**.

### Prepare commit (draft + approval — do NOT run `git commit`)

Treat as **prepare** when user says (including variants):

- 帮我准备 commit / 帮我准备提交 / 准备 commit / 准备提交
- 写 commit message / 写提交信息 / 帮我写提交信息
- summarize for commit / draft commit
- **我要 commit** / **我要提交** (wants to commit soon — still **draft first**, then wait for approval)
- 准备一下 commit / 整理提交信息

**Behavior:** run workflow below through **approval gate**; stop after presenting draft unless user then clearly orders execution.

### Execute commit (only after explicit approval)

Run `git add` / `git commit` only when user **clearly approves the draft and asks to execute**, e.g.:

- 用这个 message 提交 / 按这个 commit / 批准，提交吧
- go ahead and commit / execute commit / 执行提交
- 可以 commit 了（指**已看过草稿并同意执行**）

If user only said 准备 / 我要 commit / 写 message — **never** interpret that as permission to run `git commit`.

### Ambiguity

If unclear whether they want draft-only or execution, ask one line:  
「要先看 commit message 草稿并确认后再提交吗？」Default: **draft only**.

## Trigger phrases

This skill should be used when user intent matches **Prepare commit** above.  
Also applies when user explicitly requests commit execution (after draft exists or they waive draft).

## User style baseline in this repository

Observed style from recent history:

- Conventional Commit style in subject line, commonly using feat/fix/refactor with optional scope.
- Subject often followed by a detailed bullet-like body describing changes, fixes, refactors, caveats.
- Tone is practical and explanatory.

## Doc DoD gate (before drafting message)

For non-trivial work (feature, bug fix with doc, design-driven slice), **report checklist** before or with the commit draft. Aligns with `DOC_GOVERNANCE.md` §7. If items missing, list gaps; user may waive.

| Check | Question |
|-------|----------|
| Registry | `FEATURE_REGISTRY.md` row exists and Status fits (or N/A chore)? |
| Feature/Slice | Maps to `<DOMAIN>-Fnn` / `-Snn` (internal traceability — not for commit subject text)? |
| Design / Plan | Status and §验收 checkboxes updated if design exists? |
| Progress | `PROGRESS_LOG.md` entry for this slice/task? |
| Engineering | Build / test command / manual steps recorded as done? |
| ADR | Architecture trade-off documented if behavior/contract changed? |
| Bug | Bug record Fixed/Verified + regression noted if fixing `BUG-*`? |

Output: **DoD: pass** or **DoD: gaps —** bullet list (doc + engineering). Do not block draft unless user asked for strict gate; always surface gaps.

## Required workflow

1. **Doc DoD gate** (table above) for applicable work.

2. **Collect evidence**
   - Inspect staged/unstaged changes; draft from **actual** diffs only.
   - Group: Feature, Fix, Refactor, Docs, Chore.
   - Use conversation context for Feature/Slice IDs and doc paths.

3. **Draft message options**
   - 1–2 Conventional Commit subject options.
   - One full body draft matching repository style.
   - Optional: mention related doc paths updated.

4. **Approval gate (mandatory)**
   - Present full draft; use approval prompt below.
   - **Do not** run `git commit` (or push) until user explicitly approves execution in the same task.
   - If user only wanted a message draft, **stop** after draft.

5. **Execute (only if explicitly approved)**
   - Re-verify diff; then `git add` / `git commit` as user requested.
   - Re-run Doc DoD mentally; if new gaps appeared since draft, note before committing.

## Commit message format

Preferred default:

```text
<type>(<scope>): <short summary>
- <change 1>
- <change 2>
```

Type guidance: feat | fix | refactor | docs | chore

## Human-readable summary (not internal IDs)

Commit messages are read in `git log` by humans who may not have design docs open.

- **Subject and body must describe concrete work** (what changed, for whom, why it matters in one glance).
- **Do not use Feature/Slice/Bug IDs as the primary label** in subject or bullets — e.g. avoid `feat(test): TEST-F01-S02` or bullets that only say “complete WF-F01-S01”.
- **Translate IDs to plain language** using Design/Plan titles or diff evidence — e.g. `TEST-F01-S02` → “add unified CLI argument parser for headless tests” / “CI 基础框架：统一命令行解析”.
- **IDs belong in process docs**, not in the commit headline: use `PROGRESS_LOG`, Design Meta, or PR description to link `<DOMAIN>-Fnn-Snn` / `BUG-*`.
- **Optional footnote only:** one trailing line like `Refs: TEST-F01-S02` if traceability helps — never as the only description.

**Bad:** `feat(wf): WF-F01-S03` / `- implement slice S02`  
**Good:** `docs(wf): add doc templates and collaboration rules` / `- add docs/ai/templates and governance`

## Language policy

- Default: English-only unless user requests otherwise.
- Bilingual only for large cross-cutting changes when user asks.

## Quality rules

- Subject specific; body maps to concrete changes in **plain language** (see Human-readable summary).
- Mention important root cause for fixes.
- Avoid unverified claims.
- Conventional Commit **type** (`feat`, `fix`, …) is the change category — not a substitute for saying what was built.

## Approval prompt template

After drafting (prepare flow):

「这是 commit message 草稿 + Doc DoD 检查结果。请确认文案；**只有你明确说可以提交/执行 commit 后**，我才会运行 `git commit`。」

## Common pitfalls

- Treating 准备 commit / 我要 commit as execute permission.
- Committing before explicit post-draft approval.
- Skipping Doc DoD when design/docs were part of the task.
- Mixing unrelated topics in one message.
- **Using `TEST-F01-S02` / `BUG-ED-014` in subject or bullets without explaining the actual change.**
