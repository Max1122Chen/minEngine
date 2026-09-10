# Active work (agent backlog)

Last updated: 2026-09-10（CORE-F12 Done；TEST-F04 Planned）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.  
> **Philosophy / stage map:** [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md) · [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) — long-term constraints + multi-track direction; **this file** still wins for “what to cut next”.

---

## 当前阶段（2026-09-03）

从「单系统能力建设」进入 **整体能力扩展 + 完整开发体验**。模式：**一条主航道 + 多条可并行支线**（非线性 TODO）。

| Track | 方向 | 阻塞 Primary？ |
|-------|------|----------------|
| **Primary（推荐）** | Animation → 2D/Sprite → UI | — |
| **Infrastructure** | Async Asset / Lifetime / Thread · Binary Ser · Prefab · Object Lifetime | 否（真缺口再插入） |
| **Rendering** | Sort / Batch | 否 |
| **DX / Agent** | Editor Workflow 余量 · Reflection UX · Commands | 否 |
| **Future** | Gameplay Plugins · Networking · AI | 刻意延后 |

---

## 当前焦点 — 推荐 Primary = `ANIM-F01`

| 项 | 链接 / 说明 |
|----|-------------|
| Worktree | `D:/Dev/GitRepo/minEngine-animation` · 分支 `feat/animation` |
| Placeholder | [ANIM-F01](./Animation/ANIM-F01_ANIMATION_SYSTEM_DESIGN.md) · Registry **Planned** |
| 下一步 | 正式 Design Spec（机制边界、Core 范围、M1 垂直切片）→ Pre-flight → 开码 |
| 哲学闸门 | 先 Pose/Instance/Skinning；勿一次做成完整 Anim Graph Framework |

### DX 余量（不挡 Primary）：`ED-F02`

| 切片 | 状态 |
|------|------|
| S00–S02 工作流主路径 | **Done**（已合入 `master`） |
| S04 视口局部 RMB | **Done** |
| S03 Material Preview SkyBox 实体 | Remaining |
| S05 Abstract 标注补齐 | Partial |

[Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md)

### Infra 轨（`feat/core`）：序列化 → 属性写入 → 测试访问面

| 项 | 说明 |
|----|------|
| **CORE-F08** | StaticClass API + 删死代码 + P1 — **Done** |
| **CORE-F09** | Binary Transient v2 — **Done**（关 TD-028/029；PIE Binary） |
| **CORE-F10** | JSON 存盘宽松 + `$schemaVersion` — **Done** |
| **CORE-F11** | Getter/Setter + Assign — **Done**（含 S06 Inspector live Assign；关 TD-026） |
| **CORE-F12** | `ME_GENERATED_BODY()` 无参 + marker 归属 — **Done** |
| **TEST-F04** | `Testing::TestAccess<T>` 收敛 friend — **Planned**（下一小切片） |
| GC / Lifetime | 刻意延后 |

[CORE-F12 Design](./Platform/Reflection/CORE-F12_GENERATED_BODY_NO_ARG_DESIGN.md) · [TEST-F04 Design](./Platform/Test/TEST-F04_TEST_ACCESS_DESIGN.md) · [CORE-F11 Design](./Platform/Reflection/CORE-F11_PROPERTY_ACCESSOR_THUNKS_DESIGN.md) · [CORE-F10 Design](./Platform/Serialization/CORE-F10_JSON_DISK_COMPAT_DESIGN.md)

### 可并行（不升主线）

| 项 | 说明 |
|----|------|
| **ED-F04** | Console MVP 已收；S10b / S07 **Deferred** |
| **TD-030** | EnterPlay rollback（CORE-F05 遗留） |
| **RND Sort/Batch** | Rendering track；另开设计时再登记 |
| **RND-F06** | ForwardRenderer 收尾；不挡 Animation |

---

## 已收口（近期）

| 项 | 状态 |
|----|------|
| **CORE-F12** | Done on `feat/core` — GENERATED_BODY no-arg + attached marker |
| **CORE-F11** | Done on `feat/core` — S01–S06；live Assign；TD-026；删 PhysicsEditorSideEffects |
| **CORE-F10** | Done on `feat/core` — JSON 盘路径宽松 + `$schemaVersion` |
| **CORE-F09** | Done on `feat/core` — Binary v2；PIE Binary；TD-028/029 Done |
| **CORE-F08** | Done on `feat/core` — StaticClass Serializer API；死代码清理 |
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
| `D:/Dev/GitRepo/minEngine` | `master` | 主开发（当前常 checkout `feat/core`） |
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | Primary — Animation |
| `D:/Dev/GitRepo/minEngine-ui` | `feat/ui` | Primary 后续 — UI（依赖 `RND-F16`） |
| `D:/Dev/GitRepo/minEngine-gameplay` | `feat/gameplay-framework` | Future — Gameplay Framework（插件化） |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可归档 |
| `D:/Dev/GitRepo/minEngine-physics` / `-audio` / `-launcher` / `-asset-workflow` | 历史轨 | 按需保留或删除 |

**新建 worktree：** `.agents/skills/create-worktree` + `scripts/create-worktree.ps1`

### Placeholder branches（无 worktree）

`feat/asset-pipeline` · `feat/network` · `feat/ai` — 仅占位。  
**`feat/core`** — 已自 `master` 初始化；序列化轨 **CORE-F08–F10**（主仓 checkout，可选 `minEngine-core` worktree）。

---

## Vision placeholders（Registry；不排期）

| ID | 说明 |
|----|------|
| `ANIM-F01` | Primary — Design 待写（`feat/animation`） |
| `RND-F16` / `UI-F01` | Primary 后续（`feat/ui`） |
| Gameplay 插件化 / 网络 / AI | Future；见哲学 |

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
