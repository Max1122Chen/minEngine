# Active work (agent backlog)

Last updated: 2026-08-04  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Render 轨（`feat/render`）— **当前主线**

1. ~~RND-F03 关账~~ — **Done**  
2. **RND-F05** — [Design](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · [Impl](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md)  
   - **S01–S03 Done**（ShaderCompiler + GL Present SPIR-V + `--rhi` Vulkan clear/present）；下一刀 **S04** VK 最小图形 + SPIR-V  
3. **RND-F11 DebugDrawing** — F05 可演示后再设计  

合入前：定期把 **master** rebase/merge 进 `feat/render`。

### Master / 平台

- **CORE-F04** Delegates **Done**（解锁 PHYS-F03 依赖，但不抢渲染主线）。

### Physics（`feat/physics`）— **冷冻**

- F01/F02 Done；**PHYS-F03 Deferred** 直至 **RND-F11** 成熟后再开正式 Design。  
- worktree 可闲置；重开前再 rebase master。

### 更远（先不占带宽）

- Sprite / 骨骼网格 / 动画 — 等 RHI/Vulkan 竖切更稳后再登记 Viewer。

---

## Maintenance (not blocking active tracks)

- **WF-F02** handbook / Pages：骨架已上；正文按需补。  
- **RND-F06-S03** 目录改名可选。  
- **TD-021** EnvMap Editor Bake UX（低优）。

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Tests only | `minEngine\bin\minEngineTests.exe test smoke` |
| Delegates | `minEngineTests.exe test delegates` |
| Material | `minEngineTests.exe test material-ir` |
| RenderGraph | `minEngineTests.exe test render-graph` |
| ShaderCompiler（含 GL SPIR-V load） | `minEngineTests.exe test shader-compiler` |
| Lua MVP | `test lua-script-mvp` |
| Physics | `test physics-smoke` / `physics-sync` / `physics-load` / `physics-contact` / `physics-linetrace` / `physics-shapes` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## Explicitly not backlog (unless you promote them)

- Editor: unified Inspector target model, Material graph Undo, texture preview in Inspector.
- Content Browser: further registry/watcher optimizations beyond R1 incremental `AssetTreeModel` patch.
- Infra: GitHub Actions (see `TECH_DEBT.md` TD-010 when you want it).
- Deferred GBuffer Renderer（另开 Feature；非 F06）.
- F01 实验 Bake 产品化（已拒绝）.
- **TD-021** EnvironmentMap Editor Bake UX — 低优，不挡 F10 收口.
- Sprite / Skeletal mesh / Animation — 愿景；先不注册 Feature ID.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [RND-F05](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) | 下一渲染主线（Vulkan） |
| [TECH_DEBT.md](./TECH_DEBT.md) | Deferred problems worth tracking |
| [BOOTSTRAP_DIGEST.md](./BOOTSTRAP_DIGEST.md) | Commands, DoD, agent habits |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What already landed |
