# Active work (agent backlog)

Last updated: 2026-08-03  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### RND-F05 — Vulkan backend（`render`）— **讨论 / Planned**

- **Design：** [RND-F05](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md)
- **前置核对：** F03 仍 In Progress；F04 Done；F10 Done（EnvMap）
- **挂起非阻塞：** [TD-021](./TECH_DEBT.md) Editor EnvMap Bake UX

### 已关

- **RND-F10** EnvironmentMap Asset + 现代 Bake（TD-015）
- **RND-F07 / F08 / F09** 帧 RT、阴影图所有权、Binding/RHI hygiene

### 相关（非紧急）

- **RND-F03** 仍 In Progress（F05 依赖口径需对齐）
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
- **TD-021** EnvironmentMap Editor Bake UX — 低优，不挡 F10 收口。

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [RND-F10 Design](./Render/RND-F10_ENVIRONMENT_MAP_ASSET_DESIGN.md) | 当前主线草案 |
| [TECH_DEBT.md](./TECH_DEBT.md) | TD-015 归 F10 Bake |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
