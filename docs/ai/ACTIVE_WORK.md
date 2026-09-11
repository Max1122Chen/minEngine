# Active work (agent backlog)

Last updated: 2026-09-10（Draft：[ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md) 待评审）  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.  
> **0.1.0 窗口：** 方向草案见 [ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md)（**Draft** — 批准前勿当强制排期；长期轨仍看 Capability Roadmap）。  
> **IDs:** 正式号以 [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) 为准；历史文件名 remap 见 Registry §Merge-wave。

---

## 当前焦点

**无锁定 Primary。** 维护者正在评审 **Maximum 0.1.0** 迭代草案（FPS Demo + Prefab + Agent-friendly + Schema…）。

批准 Roadmap 后建议第一刀：**Phase F 地基**（Logger Channel → Schema/引擎版本 → Maximum 品牌名），**全部合入 `master` 后再** fan-out editor/prefab/lua/mcp/anim/ui。

### 0.1.0 执行模型（详见 Roadmap §3）

| Phase | 主题 |
|-------|------|
| **F** Foundation | Log channels · Schema/引擎版本 · Maximum 显示名 |
| **P** Parallel | editor（Query/Verify）· prefab A · lua · mcp（跟 API）· anim/ui · core Profiler ·（可选）隔离 RT |
| **D** Demo | FPS 竖切 + 0.1.0 打标；Prefab B 不挡门 |

**Prefab：** A=Instantiate 必达倾向；B=Unity 式子编辑器依赖隔离 RT（防单 RDG 同尺寸串图），可后置。

### 原候选（仍可作支线，不与 0.1.0 抢 Primary）

| 选项 | 内容 | 说明 |
|------|------|------|
| **A. 动画加深** | ANIM-F03 收口；ANIM-F04/F05 | 服务 Demo 再拉 |
| **B. Editor DX** | ED-F02 S03/S05 | 并入 Wave 5 按需 |
| **C. Infra** | Schema / Prefab / Lifetime | **并入 0.1.0 Wave 1–2** |
| **D. Render 债** | RND-F06 / Sort-Batch | 不挡 0.1.0 |

### 并行支线（不升主线）

| ID | 状态 | 说明 |
|----|------|------|
| `WF-F02` | In Progress | handbook 子系统文档按需 |
| `ED-F01` | In Progress | VK 阴影质量 **Deferred**；勿当主航道 |
| `RND-F06` | In Progress | S01–S02 Done |
| Open TD | Medium | TD-004/005/023/024/027/030 — 见 [TECH_DEBT.md](./TECH_DEBT.md) |

---

## 本波已合入（master）— 勿再当 WIP

| 轨 | 内容 | Status |
|----|------|--------|
| core | CORE-F08–F12 序列化/反射 + TEST-F04 | Done |
| editor | ED-F05 Inspector / Component UX | Done（S05 Deferred） |
| gameplay | GP-F01 Tag + GP-F02 Event | Done |
| animation | ANIM-F01–F02 Done；ANIM-F03 代码已合入；CORE-F13 Parameter；ED-F06–F07 SM Canvas | F03 收口中；F04 Review |
| ui | UI-F01–F03；RND-F16；CORE-F14–F16；ED-F08 Hierarchy Tree | Done |
| build fallout | MinGW `--exclude-libs`、ME_GENERATED_BODY()、loaders、ProjectRoot | Done（`54b228e`） |

---

## 动画后续 backlog（未升 Primary 前仅记账）

| 项 | 备注 |
|----|------|
| **ANIM-F03** 收口 | 人型 Idle→Walk 闭环 smoke → 可标 Done |
| **ANIM-F04** | Blend Tree 1D — Status **Review**，待审批 |
| **ANIM-F05** | Nested SM — **Planned**，Design 未开 |
| Preview 窗 | AnimGraph 预览；另 Feature |
| Play 时活跃 State/边高亮 | Editor 小切片 |
| Inspector 条件编辑 UX | ED-F07 后可选 |
| Undo/Redo 图编辑 | 痛感够再开 |
| Exit Time | Deferred |
| Animation Event / IK / Root Motion / Retarget / 完整 AnimBP | **明确不排期** |

---

## `master` DX 余量（ED-F02）

[Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md)

| 切片 | 内容 | 状态 |
|------|------|------|
| S00 | Content Browser 双击 → `TryOpenAsset` | Done |
| S01 | 打开 Scene | Done |
| S02 | 创建资产 | Done |
| S03 | Material Editor SkyBox 修复 | **Remaining** |
| S04 | Viewport 鼠标约束 | Done |
| S05 | Abstract Component 过滤 + Component 下拉图标 | **Remaining**（低优） |

**ED-F04：** MVP S00–S10a Done；S10b / S07 Deferred — 可不升主线，择机标 Done。

---

## Worktrees

| 路径 | 分支 | 用途 |
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 主仓；merge 后默认工作区 |
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | 可归档或留给下一动画切片 |
| `D:/Dev/GitRepo/minEngine-ui` | `feat/ui` | 可归档 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可归档；注意旧场景 `m_LightColor` 数组格式与主仓 LinearColor 不兼容 |
| `D:/Dev/GitRepo/minEngine-gameplay` | `feat/gameplay-framework` | 可归档 |

`MyMEProject.meproject` 的 `ProjectRoot` 须指向**当前工作树**的 `MyMEProject`（主仓：`…/minEngine/minEngine/MyMEProject`）。

---

## Explicitly deferred

`.memesh` · Animation Event · IK / Root Motion / Retarget · Import Settings 框架（F02 之后）· ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` · ED-F04 S10b · CORE-F05-S05 Pause/Step · ANIM Shadow skinned · Prefab / GC / Net / 完整 Gameplay Framework

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` |
| Full tests | `minEngineTests.exe test full` |
| Parameter store | `minEngineTests.exe test parameter-store` |
| Animation | `minEngineTests.exe test animation-graph` / `animation-clip` / `skeleton-pose` |
| GL Editor | `Editor.exe --rhi opengl --project ..\MyMEProject\MyMEProject.meproject`（从 `minEngine/bin`） |

Record in `PROGRESS_LOG.md` after meaningful slices.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
| [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) | Multi-track stage direction（非线性 TODO） |
