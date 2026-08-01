# Active work (agent backlog)

Last updated: 2026-07-31  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### CORE-F02 Lua Script binding（当前会话 / `luaScript` 分支）

- **Design:** [LUA_SCRIPT_BINDING_DESIGN.md](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md)（**In Progress**）
- **已完成：** S01–S04、**S06**
- **下一刀：** S05 值类型策略，或 S07 `sol::bases` / ScriptPure
- **分支约定：** 继续 **`luaScript`**；勿与 `render` 混交。

### CORE-F01 Lua runtime（已收口）

- **Design:** [LUA_SCRIPTING_DESIGN.md](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md)（**Done**）
- 绑定 codegen 已移交 F02（原 S06 取消）。

### RND-F02 Modern RHI（并行轨 / `render` 分支）

- GPU 工作模型抽象；OpenGL 首个适配后端。
- **RND-F03** Vulkan — **Planned**；依赖 F02。
- 设计：[RND-F02_MODERN_RHI_DESIGN.md](./Render/RND-F02_MODERN_RHI_DESIGN.md)

### RND-F02 边界（本阶段）

| In | Out |
|----|-----|
| GPU 模型驱动的 RHI 契约（Device、Resource/View、PSO、Binding、RenderPass、CommandList、Transition、Upload） | RenderGraph / FrameGraph |
| OpenGL backend **实现** 新契约（非 RHI 迁就 GL） | 第一版 Vulkan 全管线（CSM/PBR/Material IR 等为 F03 后续里程碑） |
| Pass 经 CommandList 提交；去掉 Pass 内 `gl*` / GL 强转 | 以某一 API（含 Vulkan）**定义** RHI 形状 |
| 设计案 = **现代 RHI 理解教案 + 设计**（normative 是 GPU 语义） | 业务层 `#if VULKAN` 分叉 |

**验证：** `.\scripts\verify.ps1`；F02 各切片后 Editor 主视口与改前一致（GL 单后端）。

---

## Maintenance (not blocking CORE / render tracks)

- **WF-F02** handbook / Pages：骨架已上；正文按需补。

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## Explicitly not backlog (unless you promote them)

These are **optional future feats**, not debt owed by old docs:

- **RND-F01** RenderGraph（已登记 **Deferred**；F02/F03 完成后再议）。
- Editor: unified Inspector target model, Material graph Undo, texture preview in Inspector.
- Core: **delegates**（Lua 事件可后置；不挡 Tick 驱动竖切）。
- Content Browser: further registry/watcher optimizations beyond R1 incremental `AssetTreeModel` patch.
- Infra: GitHub Actions (see `TECH_DEBT.md` TD-010 when you want it).

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [LUA_SCRIPTING_DESIGN.md](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md) | `CORE-F01`（`luaScript` 分支） |
| [RND-F02_MODERN_RHI_DESIGN.md](./Render/RND-F02_MODERN_RHI_DESIGN.md) | Modern RHI（`render` 分支） |
| [TECH_DEBT.md](./TECH_DEBT.md) | Deferred problems worth tracking |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [RENDER_REFACTOR_PLAN.md](./Render/RENDER_REFACTOR_PLAN.md) | Viewport/SceneDraw（与 F02 正交；Tier B 参考） |
| Old roadmaps under `Platform/`, `Editor/` | Architecture reference only — see `.cursor/rules/docs-trust-tiers.mdc` |
