# Active work (agent backlog)

Last updated: 2026-08-02  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### RND-F07 — Granite-style RDG + 帧资源所有权（`render`）— **主线 Done**

- **Design / Impl:** Done（S01–S09）；阴影 atlas 图所有权见 **TD-020**。
- **现状：** SceneColor/Depth/PostBuffer 由图 `Bake`→`SetupAttachments` 拥有；Sky/Opaque/Translucent/Post/Present 已接回；`RegisterExternal` 已清。
- **视口：** 2026-08-02 修复后用户已确认黄金场景正常。
- **尾项（非挡）：** TD-020 把 `ShadowResourceManager` 纹理迁入图。

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
| [RND-F07 Design](./Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) | 当前主线（Done） |
| [RND-F06](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md) | Renderer 宿主 |
| [RND-F01](./Render/RND-F01_RENDER_GRAPH_DESIGN.md) | 旧 Manual 图（Superseded by F07） |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
