# Active work (agent backlog)

Last updated: 2026-08-30  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## In focus (edit as you go)

### Render / Editor 轨（`feat/render`）— **当前主线**

1. ~~RND-F03 关账~~ — **Done**  
2. ~~**RND-F05** RHI 竖切~~ — **Done**（S01–S07d；VK Forward Base + smoke 验收）  
3. **ED-F01 Vulkan Editor Parity** — [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md)  
   - **S01–S05 Done**；**S07 HDR sky bake Done**（citrus HDR cubemap on VK；IBL convolution still deferred）
   - **Bugfix batch Done / Verified**：[visual parity design](./Editor/ED-F01_VULKAN_VISUAL_PARITY_BUGFIX_DESIGN.md)；`BUG-RENDER-005`…`009`
   - **S06 implemented / pending visual verify** — VK Editor flags: shadows + post + sky；`Texture2DArray` atlas + ShadowPass `set=` + depth-only PSO
   - **BUG-RENDER-010 / TD-025** — VK shadow caps + scheme A implemented；**pending user visual verify**（[Design](./Render/RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md)）
   - **BUG-RENDER-011** — spot/point 关 shadow 崩溃；SRV 槽清空 fix landed
   - **BUG-RENDER-013** — VK 多光源 shadow；根因 → 未完成 Granite RDG **全语义** → **[RND-F12](./Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md)** Phase A
   - **BUG-RENDER-010** — CSM 多影：`MAX_CASCADES=1` + `FORCE_CASCADE=0` → 单影（坐实级联）；浅影/强度待查
   - **RND-F13** — Hand-Pass Probe Renderer（RDG 对照实验）— [Design Draft](./Render/RND-F13_HAND_PASS_PROBE_RENDERER_DESIGN.md) **待审批**
   - **Next**: 审批 RND-F13 设计 → Impl；并行 F12 / BUG-010 CSM 修复
   - 收口债：`TD-023`（scene pass / clear），`TD-024`（VK frame sync）  

4. ~~BUG-RENDER-004~~ CSM 地面自阴影痤疮 — **Fixed 2026-08-04**  
5. **RND-F11 DebugDrawing** — ED-F01 主视口 parity 后再设计  

合入前：定期把 **master** rebase/merge 进 `feat/render`。

### Master / 平台

- **CORE-F04** Delegates **Done**。

### Physics（`feat/physics`）— **冷冻**

- F01/F02 Done；**PHYS-F03 Deferred** 直至 **RND-F11** 成熟后再开正式 Design。

### 更远（先不占带宽）

- Sprite / 骨骼网格 / 动画 — 等 ED-F01 / RHI 更稳后再登记 Viewer。

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
| VK Editor（ED-F01 起） | `minEngine\bin\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` |
| GL Editor 回归 | `Editor.exe --rhi opengl --project …` |
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
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
