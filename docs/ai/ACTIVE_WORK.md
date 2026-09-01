# Active work (agent backlog)

Last updated: 2026-09-01（双轨：master 内核 + feat/editor；ANIM 待合并检查点）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## 当前策略（2026-09-01，待审批）

| 轨 | 分支 | 合入目标 | 说明 |
|----|------|----------|------|
| **小修复 + 内核** | `master` | `master` | 点光阴影、PHYS-F04、CORE-F05/06/07；**不另开 feat 修复分支** |
| **编辑器工作流** | `feat/editor` | → `master`（**一次 merge 检查点**） | ED-F02；开干前 `git merge master` |
| **动画** | `feat/animation` | — | **合并检查点之后**再规划/开分支 |
| **愿景** | — | — | Registry 占位 only；**不阻塞** |

**明确 Defer（不占当前带宽）：**
- ED-F01 **VK Dir/Spot 自阴影质量**（handoff 保留：[session](./sessions/2026-08-31-vk-shadow-self-shadow-handoff.md) · [playbook](./playbooks/Render/VK_SHADOW_DEBUGGING.md)）
- `RND-F12` RDG 语义卫生项
- `PHYS-F03` Contact 玩法派发

**废弃 / 不再使用：** `feat/ui-anim`（拆为 `feat/animation` / `feat/ui` 占位，无代码）

---

## Worktrees（本机并行）

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 小修复 + CORE 内核 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | **ED-F02**（建议；开干前 merge `master`） |

旧 `minEngine-physics` / `minEngine-audio` / `minEngine-launcher` worktree 可按需保留或删除；**PHYS-F04 改在 master 上做**。

---

## In focus

### A. `master` — 小修复（直接 commit）

1. ~~**[BUG-RENDER-014](./bugs/BUG-RENDER-014.md)**~~ — 点光半径/衰减 + 阴影截止（**待 commit**）；[Design](./Render/BUG-RENDER-014_POINT_LIGHT_RADIUS_ATTENUATION_DESIGN.md)
2. **PHYS-F04** — Collider 尺寸与 Transform Scale **完全独立**；[Design](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md)
3. （可选）[BUG-PHYS-003](./bugs/BUG-PHYS-003.md) — Add `BoxColliderComponent` 间歇崩溃

**验证：** `verify.ps1` + `physics-shapes` / `physics-smoke`；Editor GL 点光场景目视。

### B. `master` — 内核（CORE，ANIM 前置）

1. **CORE-F06** Component Enable — [Placeholder](./Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md) · `bEnabled`、各 System 跳过
2. **CORE-F05** Play Mode — [Placeholder](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · Edit/Play 切换、Stop 回 Edit
3. **CORE-F07** 反射展示去 `m_`/`x_` 前缀 — [Placeholder](./Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) · 低优，Enable/Play 之后

**建议顺序：** F06 → F05 → F07。

### C. `feat/editor` — ED-F02 Editor Workflow

[Placeholder](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · **开干前 `git merge master`**

| 切片 | 内容 | 优先级 |
|------|------|--------|
| S01 | 打开 Scene（File/Open、切换、dirty） | 高 |
| S02 | 创建资产（Scene、Material、…） | 高 |
| S03 | Material Editor SkyBox 修复 | 中 |
| S04 | Viewport 鼠标约束 | 中 |
| S05 | Abstract Component 过滤 + Component 下拉图标 | 低（可 merge 后） |

### D. 合并检查点（Gate）

**当 A+B 核心项 + C 至少 S01–S02 Done：**

```text
git checkout master && git merge feat/editor
```

验证 → `PROGRESS_LOG` → 再启动 **ANIM-F01** 正式 Design。

---

## Done / 维护（不挡当前轨）

- ~~RND-F05 / RND-F11 / AUD-F01 / LAUN-F01~~ — 已合入 `master`
- **ED-F01** — S01–S07 代码在 master；**VK 阴影质量 defer**（见上）
- **WF-F02** handbook — 骨架 Done，正文按需

---

## 愿景占位（Registry only，不排期）

| ID | 分支（将来） | 前置 |
|----|--------------|------|
| `ANIM-F01` | `feat/animation` | 合并检查点 + Design |
| `UI-F01` | `feat/ui` | `RND-F16` Sprite 2D |
| `RND-F16` | `feat/sprite`（未建） | — |
| Gameplay 插件化 / 网络 / Debug Console / Agent-friendly | — | 仅文档占位，见 REGISTRY 备注 |

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` from repo root |
| Physics | `minEngineTests.exe test physics-shapes` / `physics-smoke` |
| GL Editor | `Editor.exe --rhi opengl --project …` |
| VK Editor（非当前重点） | `Editor.exe --rhi vulkan --project …` |

Record which command you ran in `PROGRESS_LOG.md` after a meaningful change.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
