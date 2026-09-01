# Active work (agent backlog)

Last updated: 2026-09-01 (render + debug-drawing merged to `master`)
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## Worktrees（本机并行）

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 集成 / backlog / 合入目标 |
| `D:/Dev/GitRepo/minEngine-launcher` | `feat/launcher` | **LAUN-F01** 启动器 |
| `D:/Dev/GitRepo/minEngine-audio` | `feat/audio` | **AUD-F01** 音效 |
| `D:/Dev/GitRepo/minEngine-physics` | `feat/physics` | **PHYS-F04 / F03**（重开前 merge `master`） |

各 worktree 内 `MyMEProject.meproject` → `ProjectRoot` 指向**该 worktree** 下的 `minEngine/MyMEProject`。

---

## In focus (edit as you go)

### Render / Editor 轨（`master`）— **当前主线**

1. ~~RND-F03 关账~~ — **Done**  
2. ~~**RND-F05** RHI 竖切~~ — **Done**（S01–S07d；VK Forward Base + smoke 验收）  
3. **ED-F01 Vulkan Editor Parity** — [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md)  
   - **S01–S05 Done**；**S07 HDR sky bake Done**（citrus HDR cubemap on VK；IBL convolution still deferred）
   - **Bugfix batch Done / Verified**：[visual parity design](./Editor/ED-F01_VULKAN_VISUAL_PARITY_BUGFIX_DESIGN.md)；`BUG-RENDER-005`…`009`
   - **S06 Done / Verified** — VK shadows + post + sky；RND-F14 修复多光源 shadow UBO 寿命
   - ~~**BUG-RENDER-010 / TD-025**~~ — **Fixed / Verified** 2026-08-31
   - ~~**BUG-RENDER-011**~~ — **Fixed / Verified** 2026-08-31
   - ~~**BUG-RENDER-013**~~ — **Fixed / Verified** 2026-08-31（RND-F14 Phase A）
   - ~~**RND-F14**~~ — **Done**（[Design](./Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) · [Impl](./Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_IMPLEMENTATION.md)）
   - ~~**RND-F13**~~ — **Done**（ManualRenderer 对照场地；`--renderer manual` 保留 Reference）
   - **RND-F12** — RDG 语义：降级为卫生项（不挡 shadow 正确性）
   - **Next（shadow 质量）:** VK **Dir + Spot** 接收体假自阴影（Point 暂无明显问题）；**CSM 级联已排除**（E3）；主因 **写路径 cull/winding**（非 cascade 选择、非全局 depthBias 未开）。Handoff → [session note](./sessions/2026-08-31-vk-shadow-self-shadow-handoff.md) · [playbook §4.5–7](./playbooks/Render/VK_SHADOW_DEBUGGING.md)
     - 待做：Debug 5/6、RenderDoc、scheme B / `VK_FRONT_FACE_CLOCKWISE` A/B
     - TEMP 实验值已恢复（`MAX_CASCADES=4`、`DIR_SHADOW_FORCE_CASCADE=-1`）
   - 收口债：`TD-023`（scene pass / clear），`TD-024`（VK frame sync）

4. ~~BUG-RENDER-004~~ CSM 地面自阴影痤疮 — **Fixed 2026-08-04**  
5. ~~**RND-F11** DebugDrawing~~ — **Done（MVP S01–S02）** — [Design](./Render/RND-F11_DEBUG_DRAWING_DESIGN.md)
   - **Delivered:** `DebugDraw` 通道 + `DebugDrawPass` + Editor collider wireframe（`PhysicsDebugDraw` 示范）
   - **Deferred:** contact/trace 可视化 → Physics 或后续消费 Feature；toggle / Persistent → **新 Feature**
   - **Open:** [BUG-PHYS-003](./bugs/BUG-PHYS-003.md) — intermittent Add `BoxColliderComponent` crash（Physics/Editor，非 RND-F11 阻塞）

### Launcher 轨（`feat/launcher`）— **待 merge**

1. **LAUN-F01** — [Placeholder](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md)
2. worktree：`minEngine-launcher`

### Audio 轨（`feat/audio`）— **待 merge**

1. **AUD-F01** — [Placeholder](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_DESIGN.md)
2. worktree：`minEngine-audio`

### Physics 轨（`feat/physics`）— **冷冻**

- F01/F02 Done；**PHYS-F03 Deferred** — RND-F11 Debug 通道已 Done，可独立评估
- worktree：`minEngine-physics` — **开干前** `git merge master`

### UI / Animation 轨（`feat/ui-anim`）— **占位**

1. **UI-F01** — [Placeholder](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md)
2. **ANIM-F01** — [Placeholder](./Animation/ANIM-F01_ANIMATION_SYSTEM_DESIGN.md)
3. 分支已建；**无独立 worktree**

### 更远（新 Feature 候选，未登记 ID）

- **Debug Persistent lifetime** — `DebugDrawService` Phase 2
- **Debug Editor toggle** — category / CLI；各子系统自行决定如何响应
- Sprite / 骨骼网格 / 动画 — 等 ED-F01 / RHI 更稳后再登记 Viewer。

### Master / 平台

- **CORE-F04** Delegates **Done**。

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
- **TD-021** EnvironmentMap Editor Bake UX — 低优.
- Sprite — 愿景；未登记 Feature ID.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
