# Active work (agent backlog)

Last updated: 2026-08-31  
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
| `D:/Dev/GitRepo/minEngine`（stash） | `feat/render` | 渲染主线；`git checkout feat/render` + `stash pop` |

各 worktree 内 `MyMEProject.meproject` → `ProjectRoot` 指向**该 worktree** 下的 `minEngine/MyMEProject`。

---

## In focus (edit as you go)

### Render 轨（`feat/render`）— **持续；VK 局部阻塞**

> **master 文档滞后于 `feat/render` 代码**（ED-F01、RND-F12、F13 等在 render 分支 / stash）。合入前以 render 分支为准。

1. **RND-F12** Granite RDG 语义 · **BUG-RENDER-013**（VK 多光源 shadow）
2. **ED-F01** Vulkan Editor parity（GL 正常；VK 目视部分 pending）
3. **RND-F11** DebugDrawing — GL-first MVP；**软依赖** PHYS-F03 视口调试
4. **RND-F13** ManualRenderer — stash 中 S01 WIP

合入前：定期 **merge `master` → `feat/render`**。

### Physics 轨（`feat/physics`）— **解冻**

1. **PHYS-F04** — [Placeholder](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) 碰撞体修复
2. **PHYS-F03** — [Placeholder](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) Contact 回调（CORE-F04 Done；F11 为软依赖）
3. worktree：`minEngine-physics` — **开干前** `git merge master`

### Launcher 轨（`feat/launcher`）— **S01–S04 Done；下一步 S05 GUI**

1. **LAUN-F01** — [Design](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md) · [Impl](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_IMPLEMENTATION.md)
   - CLI **Done**（`open` / `create` / `recent` / `config`）
   - **Next:** **LAUN-F01-S05** Tauri 2 GUI

### Audio 轨（`feat/audio`）— **新开**

1. **AUD-F01** — [Placeholder](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_DESIGN.md)
2. worktree：`minEngine-audio`

### UI / Animation 轨（`feat/ui-anim`）— **占位**

1. **UI-F01** — [Placeholder](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md)
2. **ANIM-F01** — [Placeholder](./Animation/ANIM-F01_ANIMATION_SYSTEM_DESIGN.md)
3. 分支已建；**无独立 worktree**

### Master / 平台

- **CORE-F04** Delegates **Done**

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
| GL Editor | `minEngine\bin\Editor.exe --rhi opengl --project ..\MyMEProject\MyMEProject.meproject` |
| VK Editor（render 轨） | `Editor.exe --rhi vulkan --project …` |
| Delegates | `minEngineTests.exe test delegates` |
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
- **TD-021** EnvironmentMap Editor Bake UX — 低优.
- Sprite — 愿景；未登记 Feature ID.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status when starting a **new** registered feature |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
