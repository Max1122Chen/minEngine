# minEngine

Native C++ game engine and editor. Workflow adapted from `min-agent-workflows`;
collaboration truth lives in `docs/ai/`.

## Project Structure

- Repo root: `D:/Dev/GitRepo/minEngine`
- CMake source root: `minEngine/` (contains `minEngine/`, `Editor/`, `Tests/`, `Examples/`, `Launcher/`, `cmake/`)
- Runtime sources: `minEngine/minEngine/src`
- Editor sources: `minEngine/Editor/src`
- Tests: `minEngine/Tests`
- Contract build: `minEngine/build` -> binaries in `minEngine/bin`
- `docs/ai/` - collaboration truth and workflow records
- `.cursor/rules/` - enforcement layer (always-applied rules)
- `AGENTS.md` - this file (general agent entry point)

## Verification

- Single gate: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1` from the repo root.
- It builds `minEngineTests`, `Editor`, `SampleEditor`, then runs export audits, consumer coverage,
  editor/module boundary checks and `minEngineTests.exe test smoke`.
- Only the contract build (`minEngine/build`, `minEngine/bin`) may be used to declare a change verified.
  A side build directory must prove export parity (`--report` shows the same export count) before its
  results mean anything; otherwise it is for compile-speed only.

## Export contract

- New public API is annotated with `MINENGINE_API`. Do **not** add new lines to
  `minEngine/cmake/minEngine_exports.def`; the whitelist is a legacy carrier that should only shrink.
- `minEngine_export_baseline.json` records the symbols consumers depend on. Refreshing it is the
  review step for an intentional removal, so a deletion shows up in the diff; never refresh it to
  silence a failure.
- These assertions are part of the gate and must stay green: export budget, forbidden third-party
  buckets, removal baseline, consumer coverage, GL function-table load guard.

## Consumer repositories

- `D:/Dev/GitRepo/minCraft*` are consumers. Treat them as **read-only** unless the user explicitly
  authorizes an edit. They are the source of truth for what the engine must keep exporting.

## Editor noise

- Starting the editor rewrites project `*.meta` files once (schema field fill-in). Restore them after
  a run, but keep diffs that existed before you started.

## Doc Layout

- `docs/ai/PROJECT_CONTEXT.md` - stable project snapshot
- `docs/ai/WORKFLOW_PROFILE.md` - collaboration posture (presets)
- `docs/ai/BOOTSTRAP_DIGEST.md` - short session recovery
- `docs/ai/ACTIVE_WORK.md` - active queue
- `docs/ai/FEATURE_REGISTRY.md` - feature registration
- `docs/ai/PROGRESS_LOG.md` - append-only factual history
- `docs/ai/TECH_DEBT.md` - explicit debt
- `docs/ai/bugs/` - defect records
- `docs/ai/templates/` - reusable templates; `DOC_GOVERNANCE.md` holds the rules

## Workflow Profile Gate

On bootstrap or first substantial work, read `docs/ai/WORKFLOW_PROFILE.md`. If `status` is
`unconfigured`, ask the meta-question once (configure now / skip / later) per
`docs/ai/templates/WORKFLOW_PRESETS.md`, then write the result back. Do not re-ask once configured,
default-applied or deferred, unless the user says `reconfigure workflow`.

## Naming

- Feature: `<DOMAIN>-F<nn>` · Slice: `<FeatureID>-S<nn>` · Bug: `BUG-<DOMAIN>-<nnn>`
- Long docs: `docs/ai/<DOMAIN>/<FEATURE_ID>_<SLUG>_<DOC_TYPE>.md`
  where DOC_TYPE is `DESIGN` | `IMPLEMENTATION` | `ROADMAP` | `REFACTOR_PLAN` | `SURVEY`
- Branches carry topics, not registry IDs: `feat/<topic>`, `fix/<topic>`.

## Hard Constraints

Enforced in `.cursor/rules/hard-constraints.mdc`; summarised here.

### Commit preparation gate

- Preparing a commit means drafting the message and running readiness checks. Never run `git commit`
  or push without an explicit instruction to execute.
- Draft from actual diffs, not from remembered intent.

### Planning source discipline

- Plan from `ACTIVE_WORK.md`, `FEATURE_REGISTRY.md`, `TECH_DEBT.md`, recent `PROGRESS_LOG.md`, code
  and tests. Do not infer backlog from snapshot or archive documents.
- When docs and runtime disagree, code, tests and verification output win.

### Comment hygiene

- Source comments, log text and test names carry no document section numbers, Feature/Slice/Bug IDs,
  requirement labels or commit hashes. State the reason in place instead.
- Rule: `.cursor/rules/comment-hygiene.mdc`.
- When editing comments in bulk, never read a whole file, transform and write it back; replace lines
  in place and confirm the diff is a line-for-line substitution.

### Refactor discipline

- Target a clear end state; do not leave silent dual-path layers. Remove them or register debt.

## Workflow Triggers

- New feature or refactor: assess complexity first. L2+ needs Design Spec, design review, readiness,
  then implementation; register the Feature ID before the design.
- Bug or regression: write the defect record with reproduction and the regression check.
- End of a meaningful batch: complete process and quality DoD, then propose commit preparation.
- Session switch or handoff: session note plus progress entry, with deferred slices marked.
- Ambiguous non-trivial request: short intent playback before designing.

## Engineering Design Loop

```
Request -> Discovery -> Complexity -> Design -> Design Review
  -> Implementation Readiness -> Implementation -> Verification -> Done
```

- Separate requirements from proposed solutions.
- Inspect existing architecture before inventing a parallel abstraction.
- L0 may skip design stages; L3 needs the readiness gate before large coding.