# Active work (agent backlog)

Last updated: 2026-09-06（CORE-F14 Done；下一刀待选）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.  
> **Philosophy / stage map:** [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) · [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) — long-term constraints + multi-track direction; **this file** still wins for “what to cut next”.

---

## 当前焦点（`feat/ui`）

### ~~CORE-F14 — LinearColor 作者颜色~~ **Done**

| 项 | 链接 / 说明 |
|----|-------------|
| Design | [CORE-F14](./Platform/Core/CORE-F14_LINEAR_COLOR_AUTHORING_DESIGN.md) · **Done** |

### 下一刀（待选）

| 候选 | 说明 |
|------|------|
| **UI-F02**（建议本分支） | Hit-test / 指针命中（UI-F01 Out）；再开 Text/Button |
| **ANIM-F01** | Capability Primary；其他 worktree `feat/animation` |
| **ED-F02 余量** | `master`：Material Preview / SkyBox 等 |
| **RND-F06** | ForwardRenderer 收口（非阻塞） |

### ~~UI-F01 — UI System~~ **Done**（目视含 Image alpha）

| 项 | 链接 / 说明 |
|----|-------------|
| Design | [UI-F01](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md) · **Done** |

### ~~Hierarchy~~ design docs CORE-F08 / CORE-F09 **Done**（Registry remap → CORE-F12 / CORE-F13）

| 项 | 链接 / 说明 |
|----|-------------|
| Hierarchy (docs `CORE-F08_*`) | [Design](./Platform/Core/CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md) · Registry **CORE-F12** · GUID 父指针 |
| ED-F05 | [Design](./Editor/ED-F05_HIERARCHY_TREE_DESIGN.md) · 树 + Sticky 拖拽改父 |
| KeepWorld (docs `CORE-F09_*`) | [Design](./Platform/Core/CORE-F09_PARALLEL_HIERARCHY_KEEPWORLD_DESIGN.md) · Registry **CORE-F13** · Root↔Root + world 同步 |

### ~~RND-F16 — 2D Rendering Foundation~~ Path A/B **代码+目视 Done**

| 项 | 链接 / 说明 |
|----|-------------|
| Design / Impl | [Design](./Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md) · [Impl](./Render/RND-F16_2D_RENDERING_FOUNDATION_IMPLEMENTATION.md) |
| 状态 | Sprite + ScreenUI/`WidgetComponent` 已落地 |

**明确后置：** UI Hit-test / Text / Button；UVRect GPU；World Canvas；完整 Flex。

### ~~feat/core CORE-F08–F11~~ Serialization / Reflection **Done**（已合入本分支）

| 项 | 说明 |
|----|------|
| **CORE-F08** | StaticClass API + 删死代码 + P1 — **Done** |
| **CORE-F09** | Binary Transient v2 — **Done**（关 TD-028/029；PIE Binary） |
| **CORE-F10** | JSON 存盘宽松 + `$schemaVersion` — **Done** |
| **CORE-F11** | Getter/Setter + Assign — **Done**（含 S06 Inspector live Assign；关 TD-026） |

[CORE-F11 Design](./Platform/Reflection/CORE-F11_PROPERTY_ACCESSOR_THUNKS_DESIGN.md) · [CORE-F10 Design](./Platform/Serialization/CORE-F10_JSON_DISK_COMPAT_DESIGN.md)

### ~~CORE-F05 — Play Mode~~ **Done（MVP）**（`master`）

| 项 | 链接 / 说明 |
|----|-------------|
| Design / Impl | [Design](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · [Impl](./Platform/Core/CORE-F05_PLAY_MODE_IMPLEMENTATION.md) · [S06](./Platform/Core/CORE-F05_S06_INSPECTING_CONTEXT.md) |
| Deferred / 债 | S05 Pause/Step；**TD-030** EnterPlay rollback |

### 并行候选（非本 worktree 焦点）

| 项 | 说明 |
|----|------|
| **ANIM-F01** | Primary 候选（其他 worktree `feat/animation`）；本仓焦点仍 UI-F01 |
| **ED-F02** | S03/S05 余量；不挡 UI |
| **ED-F04** | Console MVP 已收；S10b / S07 **Deferred** |
| **RND-F06** | ForwardRenderer 收尾 |

---

## 已收口（近期）

| 轨 | 分支 | 合入目标 | 说明 |
|----|------|----------|------|
| **内核** | `master` | `master` | CORE-F05 MVP Done；小修复 |
| **序列化/反射** | `feat/core` | **已合入 `feat/ui`** | CORE-F08–F11 Done；TD-026/028/029 Done |
| **UI / 2D / Hierarchy** | `feat/ui` | — | RND-F16 Done → Hierarchy（docs F08/F09 → Registry F12/F13）→ ED-F05 → **UI-F01** |
| **编辑器** | ~~`feat/editor`~~ | **已合入 `master`** | ED-F02 + **CORE-F07** + ED-F04 Console |
| **动画** | `feat/animation` | — | 并行候选；非本 worktree 焦点 |

| 项 | 状态 |
|----|------|
| **CORE-F11** | Done — S01–S06；live Assign；TD-026；删 PhysicsEditorSideEffects |
| **CORE-F10** | Done — JSON 盘路径宽松 + `$schemaVersion` |
| **CORE-F09** | Done — Binary v2；PIE Binary；TD-028/029 Done |
| **CORE-F08** | Done — StaticClass Serializer API；死代码清理 |
| **CORE-F12 / F13** | Done — Hierarchy / KeepWorld（design filenames CORE-F08_/F09_） |
| **CORE-F05** Play Mode MVP | **Done** — S00–S04 + S06；S05 Deferred；TD-030 Open |
| **CORE-F06 / F07** | Done |
| **ED-F03** Viewport Play Toolbar | Done |
| **ED-F02** S00–S02 / S04 | Done on `master` |
| **PHYS-F04** / BUG-PHYS-003/004 | Done / Fixed |
| **feat/editor** merge | 已合入 `master` |

---

## 明确 Defer

ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` · ED-F04 S10b · CORE-F05-S05 · Prefab / GC / Gameplay Framework 大包 / Networking（Capability Roadmap §6）

---

## Worktrees

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 主开发（常 checkout `feat/core` 等） |
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | Animation（并行候选） |
| `D:/Dev/GitRepo/minEngine-ui` | `feat/ui` | **本 worktree 焦点 — UI-F01** |
| `D:/Dev/GitRepo/minEngine-gameplay` | `feat/gameplay-framework` | Future — Gameplay Framework（插件化） |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可归档 |
| `D:/Dev/GitRepo/minEngine-physics` / `-audio` / `-launcher` / `-asset-workflow` | 历史轨 | 按需保留或删除 |

**新建 worktree：** `.agents/skills/create-worktree` + `scripts/create-worktree.ps1`

### Placeholder branches（无 worktree）

`feat/asset-pipeline` · `feat/network` · `feat/ai` — 仅占位。  
**`feat/core`** — 序列化轨 CORE-F08–F11 **Done**（已合入 `feat/ui`）。

---

## Vision placeholders（Registry；不排期）

| ID | 说明 |
|----|------|
| `ANIM-F01` | 并行候选 — Design 待写（`feat/animation`） |
| Gameplay 插件化 / 网络 / AI | Future；见哲学 |

---

## Done / 维护

- ~~RND-F05 / RND-F11 / AUD-F01 / LAUN-F01 / CORE-F06 / PHYS-F04 / BUG-PHYS-003/004 / CORE-F07 / feat/editor merge / **CORE-F05 MVP** / CORE-F08–F11 / Hierarchy F12/F13~~
- **ED-F01** — 代码在 master；VK 阴影质量 defer
- **WF-F02** handbook — 骨架 Done，正文按需

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
| [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) | 长期设计约束 |
| [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) | 多轨里程碑与并行关系 |
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
