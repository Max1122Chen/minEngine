# Active work (agent backlog)

Last updated: 2026-08-01  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Physics track（`physics` 分支 · worktree `D:/Dev/GitRepo/minEngine-physics`）

**当前主线：physics 垂直切片暂告一段（F01/F02 Done）；`PHYS-F03` 已 Deferred**

#### PHYS-F03 — Contact gameplay dispatch（**Deferred**）

- [占位](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) · 依赖 **TD-006** Delegates
- 正式 Design / 实现搁置；临时可用 `GetContactEvents()` 轮询
- **下一（physics）：** 由你指定（例如调试绘制、其它查询、或切 CORE Delegate）

#### PHYS-F02 — Collision + query shapes（**Done**）

- [Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) · [Impl](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_IMPLEMENTATION.md)

#### PHYS-F01 — Jolt physics bootstrap（**Done**）

- [Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md) · [Impl](./Physics/PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md)

#### Tech debt

- **TD-006** Delegates（挡 PHYS-F03）· **TD-013** enum codec → [TECH_DEBT.md](./TECH_DEBT.md)

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
| Physics | `test physics-smoke` / `physics-sync` / `physics-load` / `physics-contact` / `physics-linetrace` / `physics-shapes` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
