# Active work (agent backlog)

Last updated: 2026-09-02（**CORE-F05** 当前焦点；`master`）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## 当前焦点（`master`）

### **CORE-F05 — Play Mode** ← 下一步

| 项 | 链接 / 说明 |
|----|-------------|
| Design | [CORE-F05_PLAY_MODE_DESIGN.md](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md)（Placeholder → 待展开正式 Design） |
| Registry | `CORE-F05` **In Progress** |
| 前置 | ~~`CORE-F06` Component Activate~~ **Done** |
| 目标 | Edit/Play 切换；Play 时运行时 Tick；Stop 回 Edit（不丢未保存 Scene 上下文） |
| 分支 | `master` |

**建议起步：** Pre-flight + Design Meta 展开 → Implementation Plan S01（Editor Play/Stop + 运行时域 gate）。

---

## 当前策略（2026-09-02）

| 轨 | 分支 | 合入目标 | 说明 |
|----|------|----------|------|
| **内核** | `master` | `master` | **CORE-F05** Play Mode；小修复收尾 |
| **编辑器** | `feat/editor` | → `master`（合并检查点） | ED-F02 + **CORE-F07**（反射展示名） |
| **动画** | `feat/animation` | — | 合并检查点之后再规划 |

**明确 Defer：** ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` Contact 派发

---

## Worktrees

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | **CORE-F05** + 内核 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | ED-F02 · **CORE-F07** |

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
| ~~CORE-F07~~ 反射展示名 | **`feat/editor` 轨** — master 不处理 |

### C. `feat/editor` — ED-F02 + CORE-F07

[ED-F02](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · **开干前 / 定期 `git merge master`**

| 切片 | 内容 |
|------|------|
| S01–S02 | Open Scene、创建资产 |
| S03–S05 | SkyBox、Viewport 约束、Component 下拉 |
| **CORE-F07** | Inspector 去 `m_`/`x_` 展示前缀 — [Placeholder](./Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) |

### D. 合并检查点（Gate）

**CORE-F05 首 slice + ED-F02 S01–S02 Done 后：** `git checkout master && git merge feat/editor`

---

## Done / 维护

- ~~RND-F05 / RND-F11 / AUD-F01 / LAUN-F01 / CORE-F06 / PHYS-F04 / BUG-PHYS-003/004~~
- **ED-F01** — 代码在 master；VK 阴影质量 defer
- **WF-F02** handbook — 骨架 Done

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
