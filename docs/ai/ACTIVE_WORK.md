# Active work (agent backlog)

Last updated: 2026-08-03  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### 渲染主线 — `render` 分支暂空闲

- **RND-F09** Done（RHI hygiene；TD-013/014/016/017/018/019）
- **明确不做：** **TD-015 EnvMap**（后议用户专题）
- 下一候选：用户选题 / F05 准备前契约 / F03 EnvMap 专题

### 已关

- **RND-F07 / F08 / F09** 帧 RT、阴影图所有权、Binding/RHI hygiene

### 相关（非紧急）

- **RND-F05** Vulkan — Planned（受益于 F09 的 016/017）
- **RND-F03** 仍 In Progress；EnvMap 尾与 F09 解耦
- **RND-F06-S03** 目录改名可选

### 分支约定

- 渲染实现继续在 **`render`**；勿与 `master` 上的 physics/lua 混交除非单独合入计划。

---

## Maintenance (not blocking render track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补。

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |
| Material | `minEngineTests.exe test material-ir` |
| RenderGraph | `minEngineTests.exe test render-graph` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## Explicitly not backlog (unless you promote them)

- Editor: unified Inspector target model, Material graph Undo, texture preview in Inspector.
- Core: delegates, Lua scripting bindings.
- Deferred GBuffer Renderer（另开 Feature；非 F06）。
- F01 实验 Bake 产品化（已拒绝）。
- **TD-015 EnvMap** — 等用户专题，不塞进已完成的 F09。

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [RND-F09 Design](./Render/RND-F09_RHI_HYGIENE_SWEEP_DESIGN.md) | 刚收口的 hygiene |
| [TECH_DEBT.md](./TECH_DEBT.md) | TD-015 仍 Open |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
