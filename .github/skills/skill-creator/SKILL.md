---
name: skill-creator
description: "Create, update, and troubleshoot Copilot skill files in this workspace. Use when user asks to create a new skill, edit an existing skill, improve triggering description, fix SKILL.md frontmatter, or scaffold .github/skills/<name>/ structure."
---

# Skill Creator (Copilot)

## Purpose

Create or maintain workspace skills that run well in VS Code Copilot.

## When to use

- User asks to create a skill from scratch.
- User asks to revise an existing skill.
- User asks why a skill does not trigger.
- User asks to improve description keywords for better triggering.

## Scope and constraints

- Target location: `.github/skills/<skill-name>/`.
- Required file: `SKILL.md`.
- Required frontmatter keys: `name`, `description`.
- Keep instructions practical for this environment.
- Avoid dependencies on external or vendor-specific toolchains unless user explicitly asks.

## Workflow

1. Clarify intent
- Confirm skill name, purpose, and trigger phrases.
- Confirm expected outputs and optional templates.

2. Create or update files
- Ensure folder exists at `.github/skills/<skill-name>/`.
- Create/update `SKILL.md` with valid YAML frontmatter.
- Add optional assets under `templates/` when useful.

3. Validate quality
- `name` must match folder name.
- `description` must include concrete trigger phrases users are likely to type.
- Steps must be explicit and executable.
- Keep wording concise and deterministic.

4. Report result
- Summarize created/edited files.
- Provide 1-2 example prompts that should trigger the skill.

## Recommended SKILL.md structure

1. Frontmatter (`name`, `description`)
2. Purpose
3. When to use
4. Inputs
5. Steps
6. Output format
7. Pitfalls

## Trigger optimization rules

- Include verbs users will type: create, add, build, fix, update, optimize.
- Include artifact names users mention: skill, SKILL.md, frontmatter, templates.
- Mention both create and repair scenarios.
- Prefer one dense sentence over vague wording.

## Minimal completion checklist

- Folder exists in `.github/skills/`.
- `SKILL.md` exists and parses as YAML frontmatter.
- Description is actionable and specific.
- At least one realistic trigger prompt is provided to user.
