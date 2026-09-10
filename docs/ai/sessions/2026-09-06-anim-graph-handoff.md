# Session handoff - ANIM-F03 (after CORE-F08 complete)

Last updated: 2026-09-06
Status: Active handoff for next agent
Branch: feat/animation
HEAD: 99d05b9 - feat(core): serialize parameter schema via reflection
Worktree: d:/Dev/GitRepo/minEngine-animation

## One-line goal

Implement ANIM-F03 Animation Graph MVP: Unity-style FSM + transition Pose blend, consuming CORE-F08 ParameterStore. Start at S01 Pose::Blend.

## Done (do not re-litigate)

- ANIM-F01 / ANIM-F02 / ASSET-F02 Done.
- CORE-F08 **S00–S02 Done** (`9c3dcc5` + `99d05b9`): `Runtime/Function/Framework/Parameters/`
  - Schema -> Layout -> Store; KeyId = declaration order; Bool=1B; DefaultBytes; CopyFrom.
  - S02: ParameterValueType ME_ENUM; Schema/Entry ME_STRUCT; JSON round-trip.
  - Verify: `minEngineTests.exe test parameter-store` — 7 cases / 95 asserts PASS.
- ANIM-F03 Design + Impl **Planned** (product lock-in 2026-09-06). Docs on disk; Graph **code not started**.

## Next (priority)

1. Read Design + Impl; Pre-flight; implement **S01 Pose::Blend**.
2. S02 AnimationGraph asset (embed ParameterSchema — no ParamDef bypass) -> S03 Instance -> S04 SMC -> S05 Demo.
3. Demo: Idle↔Walk same skeleton; SMC embeds Graph Instance (AnimatorComponent Deferred).
4. AnimGraphWindow = S08 Deferred (EditorGraph projection; truth = StateMachine).

## Locked decisions

- Params under Function/Framework/Parameters/ - not Core, not Animation.
- Anim Trigger = Bool Raise/Consume policy, not a F08 value type.
- Unity Mecanim-lite FSM — not UE AnimBP VM; no Event / Blend Tree / Retarget in F03 MVP.
- Player || GraphInstance (SMC chooses); Graph does not feed Player OutPose.
- Asset is the graph (EditorPos); EditorGraph+Pin = presentation only.
- Multi-clip may share one .meskeleton via Import Skeleton picker (ASSET-F02).

## Build / test

- CMake build dir: repo-root build/ (not minEngine/build).
- cmake --build build --target minEngineTests
- minEngine/bin/minEngineTests.exe test parameter-store
- Smoke: .\scripts\verify.ps1 from repo root.

## Do NOT commit

- MyMEProject/Assets/Animations/**, Assets/Sources/**
- scene / project / DefaultMaterial local tweaks
- build_*.log

## Skills / rules

- Prefer engine-learning-mentor; cpp-style for C++.
- Trust ACTIVE_WORK + Registry In Progress/Planned; code over stale roadmaps.
- Commits: draft message + explicit user approval before git commit (git-commit-mentor).
- Write tool may emit UTF-16 here; use Python UTF-8 if build sees null bytes.

## Out of scope unless user asks

Full Blackboard; Import Settings framework; Event/IK/Root Motion; Lua ParameterStore binding.
