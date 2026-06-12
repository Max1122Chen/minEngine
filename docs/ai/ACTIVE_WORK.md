# Active work (agent backlog)

Last updated: 2026-06-11  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Physics track（`physics` 分支 · worktree `D:/Dev/GitRepo/minEngine-physics`）

**当前主线：`PHYS-F01`（Jolt bootstrap）— 设计已定稿，待 S01-a 开工**

#### PHYS-F01 — Jolt physics bootstrap（**In Progress**）

- **定位：** 物理子系统**启动计划** — Jolt + 最薄抽象 + `RigidBodyComponent` + `BoxColliderComponent`；边界刻意收窄（见 Design Scope）
- **设计：** [PHYS-F01_JOLT_INTEGRATION_DESIGN.md](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md)
- **实施：** [PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md](./Physics/PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md)
- **拍板：** P1–P9 全部默认（2026-06-11）；**P10** 刚体为 Component 物理代理、Transform 在 Root（2026-06-12）
- **下一 slice：** **S01-a** — Jolt submodule + CMake
- **S01 路径：** S01-a CMake → S01-b PhysicsSystem/World → S01-c 组件 → S01-d LogicalTick + `physics-smoke` 测试
- **S02：** 碰撞通道 + Contact Begin/End
- **S03：** `LineTrace`
- **刻意不碰：** RHI、RenderPipeline、SceneProxy、Material、Editor 物理 Gizmo

#### CORE-F01 — Transform quaternion（**In Progress**，代码已 land，文档收尾）

- **设计：** [CORE-F01_TRANSFORM_QUATERNION_DESIGN.md](./Platform/Core/CORE-F01_TRANSFORM_QUATERNION_DESIGN.md)
- **实施：** [CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md](./Platform/Core/CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md)
- **剩余：** S05 grep 扫尾、S06 Registry Done + PR → `master`（可与 PHYS S01-a 并行）

### Render track（`render` 分支 · 主 worktree `minEngine`）

- 见 `render` 分支上最新的 `ACTIVE_WORK.md`
- **`CORE-F01` 合入 `master` 后：** render worktree 与 master 对齐由维护者安排

---

## Maintenance (not blocking physics track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |
| Physics (S01-d+) | `minEngine\bin\minEngineTests.exe test physics-smoke` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
