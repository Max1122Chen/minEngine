# Active work (agent backlog)

Last updated: 2026-08-01  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Physics track（`physics` 分支 · worktree `D:/Dev/GitRepo/minEngine-physics`）

**当前主线：PHYS-F01 bootstrap 切片已齐（S01–S03 Done）— 可准备 commit / 后续产品化**

#### PHYS-F01 — Jolt physics bootstrap（**In Progress→切片 Done**，S01–S03）

- **定位：** 物理子系统**启动计划** — Jolt + 最薄抽象 + RigidBody/BoxCollider + Channel/Contact + `Scene::LineTrace`
- **设计：** [PHYS-F01_JOLT_INTEGRATION_DESIGN.md](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md)
- **实施：** [PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md](./Physics/PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md)
- **Done：** S01 同步；S02 Channel/Contact；S03 `Scene::LineTrace` + `physics-linetrace`
- **下一（可选）：** Registry 标 Feature Done / PR；或产品化（Preset UI、Sweep、多形状）— **新 Feature ID**
- **刻意不碰（仍延后）：** RHI、RenderPipeline、Editor 物理 Gizmo；System LineTrace 转发

#### CORE-F01 — Transform quaternion（**In Progress**，代码已 land，文档收尾）

- **剩余：** S05 grep 扫尾、S06 Registry Done + PR → `master`

### Render track（`render` 分支 · 主 worktree `minEngine`）

- 见 `render` 分支上最新的 `ACTIVE_WORK.md`

---

## Maintenance (not blocking physics track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |
| Physics | `test physics-smoke` / `physics-sync` / `physics-load` / `physics-contact` / `physics-linetrace` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
