# Active work (agent backlog)

Last updated: 2026-06-12  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Physics track（`physics` 分支 · worktree `D:/Dev/GitRepo/minEngine-physics`）

**当前主线：`PHYS-F01-S02`（碰撞层 + Contact）— S01 bootstrap 已完成，待 commit**

#### PHYS-F01 — Jolt physics bootstrap（**In Progress**，S01 Done）

- **定位：** 物理子系统**启动计划** — Jolt + 最薄抽象 + `RigidBodyComponent` + `BoxColliderComponent`
- **设计：** [PHYS-F01_JOLT_INTEGRATION_DESIGN.md](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md)
- **实施：** [PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md](./Physics/PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md)
- **S01 Done（2026-06-12）：** Jolt vendor、PhysicsSystem/World、P10 代理组件、LogicalTick、`physics-smoke` ✅
- **下一 slice：** **S02** — 碰撞通道 + Contact Begin/End
- **S03：** `LineTrace`
- **刻意不碰：** RHI、RenderPipeline、SceneProxy、Material、Editor 物理 Gizmo

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
| Physics | `minEngine\bin\minEngineTests.exe test physics-smoke`（从 `minEngine/bin`） |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
