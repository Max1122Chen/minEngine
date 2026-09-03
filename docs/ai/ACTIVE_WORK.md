# Active work (agent backlog)

Last updated: 2026-09-03（`master`：**CORE-F05** S03 Done；下一步 Observing / rollback）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## 当前焦点（`master`）

### **CORE-F05 — Play Mode** ← 下一步

| 项 | 链接 / 说明 |
|----|-------------|
| Design | [CORE-F05_PLAY_MODE_DESIGN.md](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · [Impl Plan](./Platform/Core/CORE-F05_PLAY_MODE_IMPLEMENTATION.md) |
| Registry | `CORE-F05` **In Progress** |
| 前置 | ~~`CORE-F06` Component Activate~~ **Done** |
| 目标 | Edit/Play；**UE 式双 Scene 共存**（Editor + PIE）；Stop 仅销毁 PIE |
| 分支 | `master` |

**建议起步：** EnterPlay 失败 rollback；或 **S06 Observing Context**（Inspector/Command 仍绑 Editor，Play 时为预期错位）。S03/S04 **Done**。

**最近提交（2026-09-03）：** S03 收尾（隐藏 gizmo + 输入门控）；S06 登记为后续切片。

---

## 当前策略（2026-09-02）

| 轨 | 分支 | 合入目标 | 说明 |
|----|------|----------|------|
| **内核** | `master` | `master` | **CORE-F05** Play Mode；小修复收尾 |
| **编辑器** | ~~`feat/editor`~~ | **已合入 `master`** | ED-F02 + **CORE-F07** + ED-F04 Console |
| **动画** | `feat/animation` | — | 合并检查点之后再规划 |

**明确 Defer：** ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` Contact 派发 · ED-F04 S10b `activate`/`deactivate`（CORE-F06 已 Done，可开切片）

---

## Worktrees

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | **CORE-F05** + 内核 + 已合入 editor 轨 |
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
| **CORE-F05** Play Mode | **In Progress** ← 当前 |
| ~~CORE-F07~~ 反射展示名 | **Done** — 已合入 `master` |

### C. `master` — ED-F02 Editor Workflow

[Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md)

| 切片 | 内容 | 优先级 |
|------|------|--------|
| S00 | Content Browser 双击 → `TryOpenAsset` | 高（接线） |
| S01 | 打开 Scene（File/Open、切换、dirty） | 高 |
| S02 | 创建资产（Scene、Material、…） | 高 |
| S03 | Material Editor SkyBox 修复 | 中 |
| S04 | Viewport 鼠标约束 | 中 |
| S05 | Abstract Component 过滤 + Component 下拉图标 | 低 |

**Defer（不占带宽）：** S03 Material SkyBox（体验项，merge 后可补）

### D. `master` — ED-F03 Viewport Play Toolbar

| ID | 内容 | 状态 |
|----|------|------|
| **ED-F03** | Viewport 三行：Tab / Toolbar / 主体 | **Done** — [Design](./Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md) |

### E. `master` — ED-F04 Debug Console（MVP 已收口，**非 Done**）

| ID | 内容 | 状态 |
|----|------|------|
| **ED-F04** | Debug Console & Unified Command System | **In Progress** — MVP S00–S10a **Done**；[Design](./Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md)（文件名保留 feat/editor 编号） |

**已交付（2026-09-02）：** Command Tab REPL、get/set/inspect/find、补全/校验、`undo`/`redo`、`@` PropertyPath、`rename`；`command-system` **25 cases / 120 asserts PASS**。

**Deferred：** S10b `activate`/`deactivate`；S07 ExportSchema；极矮布局；Command Palette。

**下一优先：** **ED-F02** Editor Workflow（§C）。

**边界：** 不扩展 PropertyPath 覆盖 `m_Name` 等引擎字段；对象级操作用专用命令。`activate`/`deactivate` **仅** `GOName@Component`（`@` 必填）。

### F. `master` — CORE-F07（已完成）

| ID | 内容 | 状态 |
|----|------|------|
| **CORE-F07** | 反射展示名去 `m_`/`x_`/`b_` 前缀 | **Done** — [Design](./Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) |

---

## Done / 维护

- ~~RND-F05 / RND-F11 / AUD-F01 / LAUN-F01 / CORE-F06 / PHYS-F04 / BUG-PHYS-003/004 / CORE-F07 / feat/editor merge~~
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
