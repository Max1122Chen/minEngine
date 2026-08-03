# Active work (agent backlog)

Last updated: 2026-08-03  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Master — TD-013 enum codec（**Done 2026-08-03**）

- Size-aware enum storage via `MEEnum::GetSize()`; wire format remains int64.
- Next on master: **`CORE-F04` Delegates**（付清 TD-006，解锁 PHYS-F03）.

### Render 轨（`render` / 拟 `feat/render`）— 顺序已定

1. **小收尾**（F03 关账口径 / F06-S03 可选）  
2. **RND-F05 Vulkan**（先于 DebugDrawing）  
3. **RND-F11 DebugDrawing**（Vulkan 竖切后再开）

合入前：把 master（含 TD-013 / 后续 Delegates）定期 merge 进 render。

### Master 并行 — CORE-F04 Delegates（**Planned**）

- 付清 **TD-006**；正式 Design 后再实现。  
- 完成后可恢复 **PHYS-F03** Contact gameplay dispatch。

### Physics（`physics` / 拟 `feat/physics`）

- F01/F02 Done；**F03 Deferred** until Delegates.  
- 建议：TD-013 + Delegates 落地后，将分支 base 到 master，再开 F03；DebugDrawing 会显著改善调试体验。

### 更远（先不占带宽）

- Sprite / 骨骼网格 / 动画 — 等 RHI/Vulkan 竖切更稳后再登记 Feature。

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
| Material | `minEngineTests.exe test material-ir` |
| RenderGraph | `minEngineTests.exe test render-graph` |
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
