# Active work (agent backlog)

Last updated: 2026-06-04  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

- **RND-F02** Modern RHI — **S1 Done**；设计 **§B.8 S2 规格已落盘（待你审批 docs commit）** → 代码 S2：`RHIShaderLegacy` 重命名、Modern 词汇、`RHIGraphicsPSODesc`、统一 `CreateDesc`。**S3** 契约 → **S4** GL+Pass。见 [RND-F02](./Render/RND-F02_MODERN_RHI_DESIGN.md) §B.8。
- **RND-F03** Vulkan backend — **Planned**；依赖 F02 契约稳定；与 GL 分里程碑行为对齐（Present → Shadow → 简化 Base → …）。
- **分支约定：** `master` 上仅 planning/registry 分隔；实现与 [RND-F02 设计](./Render/RND-F02_MODERN_RHI_DESIGN.md) 在 **`render`** 分支。

### RND-F02 边界（本阶段）

| In | Out |
|----|-----|
| GPU 模型驱动的 RHI 契约（Device、Resource/View、PSO、Binding、RenderPass、CommandList、Transition、Upload） | RenderGraph / FrameGraph |
| OpenGL backend **实现** 新契约（非 RHI 迁就 GL） | 第一版 Vulkan 全管线（CSM/PBR/Material IR 等为 F03 后续里程碑） |
| Pass 经 CommandList 提交；去掉 Pass 内 `gl*` / GL 强转 | 以某一 API（含 Vulkan）**定义** RHI 形状 |
| 设计案 = **现代 RHI 理解教案 + 设计**（normative 是 GPU 语义） | 业务层 `#if VULKAN` 分叉 |

**验证：** `.\scripts\verify.ps1`；F02 各切片后 Editor 主视口与改前一致（GL 单后端）。

---

## Maintenance (not blocking render track)

- **WF-F02** handbook / Pages：骨架已上；正文按需补，不与 RND 抢 `render` 分支。

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
- Core: delegates, Lua scripting bindings.
- Content Browser: further registry/watcher optimizations beyond R1 incremental `AssetTreeModel` patch.
- Infra: GitHub Actions (see `TECH_DEBT.md` TD-010 when you want it).

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [RND-F02_MODERN_RHI_DESIGN.md](./Render/RND-F02_MODERN_RHI_DESIGN.md) | Modern RHI 教案 + 设计（`render` 分支） |
| [TECH_DEBT.md](./TECH_DEBT.md) | Deferred problems worth tracking |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [RENDER_REFACTOR_PLAN.md](./Render/RENDER_REFACTOR_PLAN.md) | Viewport/SceneDraw（与 F02 正交；Tier B 参考） |
| Old roadmaps under `Platform/`, `Editor/` | Architecture reference only — see `.cursor/rules/docs-trust-tiers.mdc` |
