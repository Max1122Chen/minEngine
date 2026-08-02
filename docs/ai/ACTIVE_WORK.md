# Active work (agent backlog)

Last updated: 2026-08-02  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Master integration（`luaScript` + `physics` → `master`）

- Transform 使用物理侧 **Quaternion** 存储；Script\* 绑定保留（`CORE-F02`）。
- Physics 原 Transform Feature 已改号为 **`CORE-F03`**（Lua 保留 `CORE-F01`/`CORE-F02`）。
- **下一：** 设计 Delegate 系统（**TD-006**，挡 `PHYS-F03`）。

### Physics track（已合入 master）

**垂直切片暂告一段（F01/F02 Done）；`PHYS-F03` 已 Deferred**

#### PHYS-F03 — Contact gameplay dispatch（**Deferred**）

- [占位](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) · 依赖 **TD-006** Delegates
- 正式 Design / 实现搁置；临时可用 `GetContactEvents()` 轮询

#### PHYS-F02 / PHYS-F01 — **Done**

- [PHYS-F02 Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) · [PHYS-F01 Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md)

#### Tech debt

- **TD-006** Delegates（挡 PHYS-F03）· **TD-013** enum codec → [TECH_DEBT.md](./TECH_DEBT.md)

#### CORE-F03 — Transform quaternion（**Done**，合入 master）

- [Design](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_DESIGN.md) · [Impl](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_IMPLEMENTATION.md)

### CORE-F01 / CORE-F02 Lua（**Done**，合入 master）

- [LUA_SCRIPTING_DESIGN](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md) · [LUA_SCRIPT_BINDING_DESIGN](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md)
- 后续（非阻塞）：扩 GO/组件白名单、Matrix/quat、弱引用句柄

### RND-F02 Modern RHI（并行轨 / `render` 分支）

- GPU 工作模型抽象；OpenGL 首个适配后端。
- **RND-F03** Vulkan — **Planned**；依赖 F02。
- 设计：[RND-F02_MODERN_RHI_DESIGN.md](./Render/RND-F02_MODERN_RHI_DESIGN.md)

---

## Maintenance (not blocking active tracks)

- **WF-F02** handbook / Pages：骨架已上；正文按需补。

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |
| Lua MVP | `test lua-script-mvp` |
| Physics | `test physics-smoke` / `physics-sync` / `physics-load` / `physics-contact` / `physics-linetrace` / `physics-shapes` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## Explicitly not backlog (unless you promote them)

These are **optional future feats**, not debt owed by old docs:

- **RND-F01** RenderGraph（已登记 **Deferred**；F02/F03 完成后再议）。
- Editor: unified Inspector target model, Material graph Undo, texture preview in Inspector.
- Core: **delegates**（现为 TD-006 / PHYS-F03 前置；可单独开 Feature）。
- Content Browser: further registry/watcher optimizations beyond R1 incremental `AssetTreeModel` patch.
- Infra: GitHub Actions (see `TECH_DEBT.md` TD-010 when you want it).

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [LUA_SCRIPTING_DESIGN.md](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md) | `CORE-F01` Lua runtime |
| [RND-F02_MODERN_RHI_DESIGN.md](./Render/RND-F02_MODERN_RHI_DESIGN.md) | Modern RHI（`render` 分支） |
| [TECH_DEBT.md](./TECH_DEBT.md) | Deferred problems worth tracking |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
