# Active work (agent backlog)

Last updated: 2026-06-11  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Physics track（`physics` 分支 · worktree `D:/Dev/GitRepo/minEngine-physics`）

**当前优先：`CORE-F01`（Transform 四元数）→ 完成后 `PHYS-F01`（Jolt）**

#### CORE-F01 — Transform quaternion storage（**Planned**，可开 S01）

- **设计：** [CORE-F01_TRANSFORM_QUATERNION_DESIGN.md](./Platform/Core/CORE-F01_TRANSFORM_QUATERNION_DESIGN.md)
- **实施：** [CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md](./Platform/Core/CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md)
- **分支：** `physics`（完成后 PR → `master`）
- **拍板摘要：** D1 XYZ 保持；D2 `q≡-q`；D3 struct 四字段；D4 **不**自动读旧 Euler；D5 纳入 `RenderCamera`；D6 标签 `Rotation`；D7 rebase 自行安排
- **切片（逻辑 6 片，建议 3 PR）：** S01+S02 内核、Scene API、RenderCamera → S03+S04 序列化与 Inspector → S05+S06 清扫与合入
- **Inspector 约定：** `Rotation` 反射为 `Quaternion`，`TransformWidget` 仍显示 vec3 欧拉（度），写入映射四元数

#### PHYS-F01 — Jolt physics integration（**Blocked by CORE-F01**）

- **S01 目标：** Jolt CMake → `PhysicsSystem` → `RigidBodyComponent` + `BoxColliderComponent` → 落体 + `verify.ps1` / headless 测试
- **S02：** 碰撞通道（Default/World/Trigger）+ Contact 双缓冲 Begin/End
- **S03：** `LineTrace` 查询 API
- **分支基线：** `master`；**不**基于 `render`；与 RND-F02/F03 并行，定期 merge `master`
- **刻意不碰（本 track）：** RHI、RenderPipeline、SceneProxy、Material

### Render track（`render` 分支 · 主 worktree `minEngine`）

- 见 `render` 分支上最新的 `ACTIVE_WORK.md`（本 worktree 的 render 条目可能滞后于 `render` 分支）
- **`CORE-F01` 合入 `master` 后：** render worktree 与 master 对齐由维护者安排（D7）

---

## Maintenance (not blocking physics track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
