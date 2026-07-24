# Active work (agent backlog)

Last updated: 2026-07-24  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

- **RND-F01** RenderGraph — **Draft**；**S0–S05 Done**。**下一步：S06 Bake** → S08 调图形态。见 [F01](./Render/RND-F01_RENDER_GRAPH_DESIGN.md)。
- **RND-F06** ForwardRenderer — **S01–S02 Done**（闸门已过；S03 目录改名可选）。见 [F06](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md)。
- **RND-F02** Modern RHI — **Done**。
- **RND-F03** Legacy RHI removal — **In Progress（M3/M4 尾）**；不抢 F01 主线。
- **RND-F04** Modern RHI further evolution — **Done**。
- **RND-F05** Vulkan — **Planned**（依赖 F03 Done + F04 Done）。
- **分支约定：** `render` 分支继续承载渲染实现；planning/registry 可合 `master`。

### 渲染主线接力（2026-07-24 拍板）

```text
1. F06  ForwardRenderer + 删除 RenderPipeline     ← S01–S02 Done
2. F01 S05  RDG 实现卫生                          ← Done
3. F01 S06+ Bake 等，补全 Graph 机制              ← 下一步
4. F01 S08  Renderer 侧调图形态整理
```

真源：[F06 §1.3](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md)。

---

## Maintenance (not blocking render track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补，不与 RND 抢 `render` 分支。

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

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [RND-F06](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md) | Renderer / Graph 分离（当前主线） |
| [RND-F01](./Render/RND-F01_RENDER_GRAPH_DESIGN.md) | RenderGraph 机制（S05+ 等 F06） |
| [RND-F02](./Render/RND-F02_MODERN_RHI_DESIGN.md) | Modern RHI 教案 + 契约（Done） |
| [RND-F03](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) | Legacy 清零 |
| [RND-F04](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) | 现代 RHI 语义终态 |
| [RND-F05](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) | Vulkan + 补全 |
