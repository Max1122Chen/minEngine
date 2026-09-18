# Active work (agent backlog)

Last updated: 2026-09-18（MAT-F02 代码落地；待手验）  
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog.  
> **0.1.0 窗口：** [ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md)（**In Progress**）。  
> **IDs:** [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md)。

---

## 当前焦点

**Primary（`feat/render`）：MAT-F02 MaterialRenderProxy — 手验验收**

| 序 | ID | 标题 | Status | Design |
|----|-----|------|--------|--------|
| — | **`MAT-F01`** | Material Runtime Productization **路线图** | **In Progress** | [Roadmap](./Render/Material/MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md) |
| 1 | **`MAT-F02`** | **MaterialRenderProxy**（Draw 绑定真源） | **In Progress** | [Design](./Render/Material/MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) |
| — | **`MAT-F03`–`F06`** | Interface → MI → MID → MIC | **Planned/Review** | 见路线图 §3 |
| — | **`RND-F17`** | Isolated preview 调度 | **Done** | [Design](./Render/RND-F17_ISOLATED_PREVIEW_DRAW_SCHEDULING_DESIGN.md) |
| — | **`ED-F11`** | Multi-document Tab Host | **In Progress**（余量） | [Design](./Editor/ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md) |

**下一刀：** 手验 F02（场景可画、改材质参数）→ 标 Done → 再开 F03。

**已收口（近期）：** 设计文档 commit `b7469df`；CORE-F19/F21；Prefab；**RND-F06**；**RND-F17**。

**并行**

| 轨 | ID | 说明 |
|----|-----|------|
| `feat/render` | MAT-F02… | **Proxy（手验）→ Interface → MI → MID** |
| `minEngine-editor` | `ED-F11` | Tab Host 余量（可选） |
| — | Prefab 后置 | Nested / Apply→Prefab；并排预览可消费 F17 |

### 0.1.0 执行模型（详见 Roadmap §3.0 拓扑图）

| Phase | 主题 |
|-------|------|
| **F** Foundation | **Done** — Log → Schema → Brand |
| **P** Parallel | **进行中** — fan-out（editor 余量；Prefab 已合入） |
| **D** Demo | FPS + 0.1.0 tag |

**Prefab：** F23=资产+Instantiate；F24=Override；F16=编辑器（单 Viewport；并排预览依赖隔离 RT）。

### 并行支线（不升主线）

| ID | 状态 | 说明 |
|----|------|------|
| `WF-F02` | In Progress | handbook 子系统文档按需 |
| `ED-F01` | In Progress | VK 阴影质量 **Deferred**；勿当主航道 |
| `RND-F17` | **Done** | ViewId+SRT；TD-027 Done |
| `MAT-F01` | **In Progress** | 运行时产品化路线图（Proxy 先行） |
| `MAT-F02` | **In Progress** | MaterialRenderProxy 代码已合入工作区；待手验 |
| Open TD | Medium | TD-004/005/023/024/030 — 见 [TECH_DEBT.md](./TECH_DEBT.md)（TD-027 Done） |

---

## 本波已合入（master）— 勿再当 WIP

| 轨 | 内容 | Status |
|----|------|--------|
| scripting | CORE-F19 Call + CORE-F21 Lua Dynamic subscribe | Done |
| prefab | CORE-F23–F26 + ED-F16 + ASSET-F03 + Review 全包 | Done |
| core | CORE-F08–F12 序列化/反射 + TEST-F04；CORE-F20/F22 | Done |
| editor | ED-F05 Inspector；ED-F11 W3 大部 + ED-F12–F15 | Done（F11 余量） |
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
| `D:/Dev/GitRepo/minEngine` | **`master`**（合入后） | lua + prefab + CORE-F20/F22 + editor ED-F11–F15 |
| `D:/Dev/GitRepo/minEngine-lua-script` | **`feat/lua-script`** | 可对齐 master 后归档 |
| `D:/Dev/GitRepo/minEngine-prefab` | **`feat/prefab`** | 可对齐 master 后归档 |
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | 可归档或留给下一动画切片 |
| `D:/Dev/GitRepo/minEngine-ui` | `feat/ui` | 可归档 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可与 master 对齐；`ProjectRoot` 本地指向本树 |
| `D:/Dev/GitRepo/minEngine-gameplay` | `feat/gameplay-framework` | 可归档 |

`MyMEProject.meproject` 的 `ProjectRoot` 须指向**当前工作树**的 `MyMEProject`（主仓：`…/minEngine/minEngine/MyMEProject`）。

---

## Explicitly deferred

`.memesh` · Animation Event · IK / Root Motion / Retarget · Import Settings 框架（F02 之后）· ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` · ED-F04 S10b · CORE-F05-S05 Pause/Step · ANIM Shadow skinned · Prefab Nested / Apply→Prefab / 并排预览（隔离 RT）· GC / Net / 完整 Gameplay Framework

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` |
| Full tests | `minEngineTests.exe test full` |
| Prefab | `minEngineTests.exe test prefab` / `prefab-overrides` |
| Lua | `minEngineTests.exe test lua-script-mvp` |
| Parameter store | `minEngineTests.exe test parameter-store` |
| Animation | `minEngineTests.exe test animation-graph` / `animation-clip` / `skeleton-pose` |
| GL Editor | `Maximum.exe --rhi opengl --project ..\MyMEProject\MyMEProject.meproject`（从 `minEngine/bin`） |

Record in `PROGRESS_LOG.md` after meaningful slices.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
| [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md) | Multi-track stage direction（非线性 TODO） |
