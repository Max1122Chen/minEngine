# Active work (agent backlog)

Last updated: 2026-08-02  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### 渲染主线（`render`）— 空闲

- **RND-F07** Done（帧 RT 图所有权）。
- **RND-F08** Done（阴影图所有权；TD-020 关闭）；用户已确认阴影效果正常。
- 下一候选：RND-F05 Vulkan / RND-F03 尾 / TD-013 Set0 BindingSet（按需开）。

### 相关（非紧急）

- **RND-F05** Vulkan — Planned。
- **RND-F03** Legacy 清零尾。
- **RND-F06-S03** 目录改名可选。

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

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [RND-F08 Design](./Render/RND-F08_SHADOW_GRAPH_OWNERSHIP_DESIGN.md) | 阴影图所有权（Done） |
| [RND-F07](./Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) | 帧 RT 图所有权（Done） |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
