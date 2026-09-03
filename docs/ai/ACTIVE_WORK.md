# Active work (agent backlog)

Last updated: 2026-09-03（`master`：**CORE-F05** MVP **Done** → 下一焦点待定）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## 当前焦点（`master`）

### ~~CORE-F05 — Play Mode~~ **Done（MVP）**

| 项 | 链接 / 说明 |
|----|-------------|
| Design / Impl | [Design](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · [Impl](./Platform/Core/CORE-F05_PLAY_MODE_IMPLEMENTATION.md) · [S06](./Platform/Core/CORE-F05_S06_INSPECTING_CONTEXT.md) |
| Registry | `CORE-F05` **Done**（MVP） |
| 交付 | 双 Scene PIE、Viewport、Per-World Audio/Physics、Inspecting Context |
| Deferred / 债 | S05 Pause/Step；**TD-028/029** Binary/JSON；**TD-030** EnterPlay rollback |

**下一阶段：** 由维护者指定（候选：§C **ED-F02** Editor Workflow；或其它 Registry Planned 项）。

---

## 当前策略（2026-09-02）

| 轨 | 分支 | 合入目标 | 说明 |
|----|------|----------|------|
| **内核** | `master` | `master` | CORE-F05 MVP Done；小修复 / 下一 Feature |
| **编辑器** | ~~`feat/editor`~~ | **已合入 `master`** | ED-F02 + **CORE-F07** + ED-F04 Console |
| **动画** | `feat/animation` | — | 合并检查点之后再规划 |

**明确 Defer：** ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` Contact 派发 · ED-F04 S10b `activate`/`deactivate` · CORE-F05-S05 Pause/Step

---

## Worktrees

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 内核 + 已合入 editor 轨 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可归档或用于下一 editor 切片 |

旧 `minEngine-physics` / `minEngine-audio` / `minEngine-launcher` worktree 可按需保留或删除。

---

## In focus

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

### C. `master` — ED-F02 Editor Workflow ← **下一优先候选**

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
| `ANIM-F01` | `feat/animation` | 合并检查点 + Design |
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
