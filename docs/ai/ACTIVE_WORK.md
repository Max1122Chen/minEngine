# Active work (agent backlog)

Last updated: 2026-06-11  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Physics track（`physics` 分支 · worktree `D:/Dev/GitRepo/minEngine-physics`）

**当前优先：`CORE-F01`（Transform 四元数）→ 完成后 `PHYS-F01`（Jolt）**

#### CORE-F01 — Transform quaternion storage（**In Progress**，S01–S04 已 land 代码，待 commit）

- **设计：** [CORE-F01_TRANSFORM_QUATERNION_DESIGN.md](./Platform/Core/CORE-F01_TRANSFORM_QUATERNION_DESIGN.md)
- **实施：** [CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md](./Platform/Core/CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md)
- **已完成（代码）：** `Quaternion` + `Transform` quat 存储；Scene/GameObject/RenderCamera API；`TransformWidget` 欧拉行；序列化 `Rotation` round-trip 测试
- **验证：** `cmake --build minEngine/build --target minEngineTests Editor` + `minEngineTests.exe test smoke` ✅
- **剩余：** S05 扫尾 grep、S06 合入文档/registry、准备 commit → 再开 PHYS-F01

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
