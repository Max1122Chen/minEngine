# Active work (agent backlog)

Last updated: 2026-09-03（ANIM-F01 目视竖切通过；**下一焦点 `ASSET-F01` FBX/Import Pipeline**）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## 当前焦点（`feat/animation`）

### ASSET-F01 — External Import Pipeline ← **下一焦点（FBX 等为 Source）**

| 项 | 链接 / 说明 |
|----|-------------|
| Design | [Placeholder](./Asset/ASSET-F01_IMPORT_PIPELINE_DESIGN.md)（**Planned** → 开工前升 Draft/Design） |
| 目标 | FBX/glTF 不当成引擎 AssetType；显式 Import → 原生 `StaticMesh` / `SkeletalMesh`(+`Skeleton`) 等 |
| Next | Pre-flight + Design 扩写 → Implementation Plan → 切片开工 |

### ANIM-F01 — Skeletal Mesh Pipeline ← **竖切目视通过（收尾中）**

| 项 | 链接 / 说明 |
|----|-------------|
| Design / Impl | [Design](./Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md) · [Impl](./Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_IMPLEMENTATION.md) |
| 验证 | `MinSkinnedStick.glb` + `DefaultMaterial_Skinned`；Editor 可见竖棍；Shadow skinned **仍 Deferred** |
| 残留 | Feature 可标 Done/Review；扭骨交互 UX 可后补；完整 FBX 人型走 ASSET-F01 |

**明确不排期（动画扩展）：** Animation Event、IK、Root Motion、Retarget、完整 AnimBP。

### `master` 旁路（非本 worktree 焦点）

| 项 | 状态 |
|----|------|
| CORE-F05 Play Mode MVP | **Done** |
| ED-F02 Editor Workflow | Planned（候选；与动画轨并行不抢） |
| ED-F04 Console | In Progress（MVP Done） |

---

## 当前策略（2026-09-03）

| 轨 | 分支 | 合入目标 | 说明 |
|----|------|----------|------|
| **动画** | `feat/animation` | 竖切后再论 | **当前焦点** ANIM-F01 → F02 → F03 |
| **内核 / 编辑器** | `master` | `master` | CORE-F05 Done；ED-F02 等可并行 |

**明确 Defer：** Animation Event（暂不登记）· IK / Root Motion / Retarget · ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` · ED-F04 S10b · CORE-F05-S05 Pause/Step  
**下一开干：** [`ASSET-F01`](./Asset/ASSET-F01_IMPORT_PIPELINE_DESIGN.md)（FBX/glTF Import → 原生资产）

---

## Worktrees

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 内核 + 已合入 editor 轨 |
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | **动画轨**（当前） |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可归档或用于下一 editor 切片 |

旧 `minEngine-physics` / `minEngine-audio` / `minEngine-launcher` worktree 可按需保留或删除。

---

## In focus

> 本 worktree（`minEngine-animation` / `feat/animation`）以文首 **ANIM-F01** 为准。下列 A–F 为 `master` 轨历史与旁路 backlog。

### A. `master` — 小修复（收尾）

| 项 | 状态 |
|----|------|
| ~~BUG-RENDER-014~~ 点光半径/衰减 | Done（`f3c8200`） |
| ~~PHYS-F04~~ Collider 与 Scale 解耦 | **Done** — [Design](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) · `c2c0893` |
| ~~BUG-PHYS-003~~ Add BoxCollider 间歇崩溃 | **Fixed** — [Record](./bugs/BUG-PHYS-003.md)（未再复现） |
| ~~BUG-PHYS-004~~ Collider 禁/删形体刷新 | **Fixed** — `c0a51ce` |

### B. `master` — 内核

| 项 | 状态 |
|----|------|
| ~~CORE-F06~~ Component Activate | **Done** — `b07009e` |
| ~~CORE-F05~~ Play Mode MVP | **Done** — S00–S04 + S06；S05 Deferred |
| ~~CORE-F07~~ 反射展示名 | **Done** — 已合入 `master` |

### C. `master` — ED-F02 Editor Workflow（旁路候选）

[Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md)

| 切片 | 内容 | 优先级 |
|------|------|--------|
| S00 | Content Browser 双击 → `TryOpenAsset` | 高（接线） |
| S01 | 打开 Scene（File/Open、切换、dirty） | 高 |
| S02 | 创建资产（Scene、Material、…） | 高 |
| S03 | Material Editor SkyBox 修复 | 中 |
| S04 | Viewport 鼠标约束 | 中 |
| S05 | Abstract Component 过滤 + Component 下拉图标 | 低 |

### D. `master` — ED-F03 Viewport Play Toolbar

| ID | 内容 | 状态 |
|----|------|------|
| **ED-F03** | Viewport 三行：Tab / Toolbar / 主体 | **Done** — [Design](./Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md) |

### E. `master` — ED-F04 Debug Console（MVP 已收口，**非 Done**）

| ID | 内容 | 状态 |
|----|------|------|
| **ED-F04** | Debug Console & Unified Command System | **In Progress** — MVP S00–S10a **Done**；[Design](./Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md) |

**Deferred：** S10b `activate`/`deactivate`；S07 ExportSchema；极矮布局；Command Palette。

### F. `master` — CORE-F07（已完成）

| ID | 内容 | 状态 |
|----|------|------|
| **CORE-F07** | 反射展示名去 `m_`/`x_`/`b_` 前缀 | **Done** |

---

## Done / 维护

- ~~RND-F05 / RND-F11 / AUD-F01 / LAUN-F01 / CORE-F06 / PHYS-F04 / BUG-PHYS-003/004 / CORE-F07 / feat/editor merge / **CORE-F05 MVP**~~
- **ED-F01** — 代码在 master；VK 阴影质量 defer
- **WF-F02** handbook — 骨架 Done，正文按需

---

## 愿景占位（Registry only，不排期）

| ID | 分支（将来） | 前置 |
|----|--------------|------|
| `ANIM-F02` / `F03` | `feat/animation` | F01 Done 后依次 |
| Animation Event / IK / Root Motion / Retarget | — | 未登记；Graph MVP 后再评估 |
| `UI-F01` | `feat/ui` | `RND-F16` Sprite 2D |
| `RND-F16` | `feat/sprite`（未建） | — |
| Gameplay 插件化 / 网络 | — | 仅文档占位，见 REGISTRY 备注 |

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` |
| Physics | `minEngineTests.exe test physics-shapes` / `physics-smoke` |
| GL Editor | `Editor.exe --rhi opengl --project …` |

Record in `PROGRESS_LOG.md` after meaningful slices.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
