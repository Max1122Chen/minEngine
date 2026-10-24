# Active work (agent backlog)

Last updated: 2026-10-24（feat/render：RND-F26 引擎 Done；minCraft 已跟进）
Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog.  
> **0.1.0 窗口：** [ENGINE_0_1_0_ROADMAP.md](./ENGINE_0_1_0_ROADMAP.md)（**In Progress**）。  
> **IDs:** [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md)。

---

## 当前焦点

**Primary（本工作树 `feat/render`）：** `RND-F26` **Done**（引擎 + minCraft Chunk 细剔）；待 commit。主仓 Primary 仍可以是 `ASSET-F10`。

| 序 | ID | 标题 | Status | Design |
|----|-----|------|--------|--------|
| 0 | RND-F26 | View-Aware Mesh Collect（Proxy 子剔除钩子） | **Done**（S01–S02 + consumer） | [Design](./Render/RND-F26_VIEW_AWARE_MESH_COLLECT_DESIGN.md) · [Impl](./Render/RND-F26_VIEW_AWARE_MESH_COLLECT_IMPLEMENTATION.md) |
| — | ASSET-F10 | DataAsset 多态 Load/Create（meta.RuntimeClassName） | **In Progress**（S01/S02 Done；S03 Deferred） | [Design](./Asset/ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md) |
| 1 | CORE-F38 | TaskSystem · TaskFlow + 合成证明（**无** Asset 异步） | **Draft** | [Design](./Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md) |
| — | BUILD-F03 刀2 | 烧 `.def`→`explicit`（标宏 + codegen `MODULE_API`） | **Done**（批0–5 + 默认 `explicit`） | [Design](./Platform/ProductBuild/BUILD-F03_SYMBOL_CONTRACT_ROLLOUT_DESIGN.md) · 附录 C |
| — | GP-F03 | GameplayTag 名身份修订（去 Tag 上 Manager* + hash） | **Done** | [Design](./Gameplay/GP-F03_GAMEPLAY_TAG_MESTRUCT_DESIGN.md) · `test gameplay-tags` |
| — | CORE-F39 | 值类型 Codec 注册 | **Cancelled** | 由 GP-F03 取代；勿开工 |
| — | ASSET-F09 | 异步 DataAsset | **Deferred** | 等 F38 站稳后另开 Design |
| — | CORE-F33 | Spawn 即时入世 · Destroy 延迟 · EOF swap | **In Progress**（S01+S02 landed） | [Design](./Platform/Core/CORE-F33_DEFERRED_STRUCTURAL_EDIT_DESIGN.md) |
| — | ASSET-F04 | CB Workflow + Automation | **In Progress** | [Design](./Asset/ASSET-F04_CONTENT_WORKFLOW_DESIGN.md) |
| — | CORE-F36 | FixedTick + 变步 Tick 相位对齐 | **Done**（S00–S04；S05 Stub Deferred） | [Design](./Platform/Core/CORE-F36_GAMEPLAY_FIXED_TICK_DESIGN.md) |
| — | CORE-F37 | RTTI hygiene | **In Progress**（S01/B1-a/MAT-F07/S02 Done；B2/B1-b 后置） | [Survey](./Platform/Core/CORE-F37_RTTI_HYGIENE_SURVEY.md) · [S02](./Render/CORE-F37_RHI_STATIC_DOWNCAST_DESIGN.md) |
| — | MAT-F07 | NodeDef pin value type | **Done** | [Design](./Render/Material/MAT-F07_NODEDEF_PIN_VALUE_TYPE_DESIGN.md) |
| — | RND-F24 | 网格顶点语义属性 Color→Custom | **In Progress**（S01–S03 Done；S04 工厂 Pending） | [Design](./Render/RND-F24_MESH_VERTEX_ATTRIBUTES_DESIGN.md) · [S03](./Render/RND-F24_S03_VERTEX_CUSTOM_DESIGN.md) |
| — | CORE-F35 | GI 会话中枢 · 项目可选派生类 | **Done**（S01–S03；S04 Deferred） | [Design](./Platform/Core/CORE-F35_GAME_INSTANCE_HUB_AND_PROJECT_CLASS_DESIGN.md) |
| — | BUILD-F01 | 产品构建链 · Editor/Game · 模板 · Stage | **Done**（S01–S07） | [Design](./Platform/ProductBuild/BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md) |
| — | CORE-F34 | GameInstance · Subsystem | **Done**（S00–S06） | [Design](./Platform/Core/CORE-F34_GAME_INSTANCE_AND_SUBSYSTEM_LIFECYCLE_DESIGN.md) |
| — | ASSET-F06 | AssetManager Facade 重构 | **Done** | [Design](./Asset/ASSET-F06_ASSET_MANAGER_FACADE_REFACTOR_DESIGN.md) |
| — | ASSET-F05 | Texture2D cook 资产 | **Done** | [Design](./Asset/ASSET-F05_TEXTURE_COOKED_ASSET_DESIGN.md) |
| — | ED-F24 | AES GUI 收敛 | **Done** | [Design](./Editor/ED-F24_ASSET_EDITOR_SERVICES_GUI_CONVERGENCE_DESIGN.md) |
| — | CORE-F32 | EI hold Down Started·Canceled | **Done** | [Design](./Platform/Core/CORE-F32_ENHANCED_INPUT_HOLD_DOWN_EVENTS_DESIGN.md) |
| — | ED-F23 | Viewport Pointer Capture | **Done** | [Design](./Editor/ED-F23_VIEWPORT_POINTER_CAPTURE_DESIGN.md) |
| — | CORE-F31 | GO/Component BeginPlay · EndPlay | **Done** | [Design](./Platform/Core/CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md) |

**下一动作（`feat/render`）：** 审批后 commit RND-F26；可选手验 minCraft 低头 UBO。  
**主仓旁路：** ASSET-F10；`RND-F21` 仍 Deferred。

**已记录目标（多轨，不是线性 TODO）：**

| 轨 | 意图 | 状态 |
|----|------|------|
| 构建 · 导出契约 | 刀2：标注烧 `.def`→`explicit`；反射 `MODULE_API`；单例/常量表单份 | **BUILD-F03 刀2 Done**（默认 `explicit`）；F02/刀1/F04 大部已落地 |
| 多线程 · TaskSystem | 引入 TaskFlow；Drain；合成作业 1 vs N。**本波不做 Asset 异步** | **CORE-F38 Draft（已收窄）** |
| 多线程 · 资产异步 | DataAsset 等真实客户 | **ASSET-F09 Deferred** |
| 多线程 · 帧线程 | 逻辑/渲染分离 | 后置 |
| 光照 | Atmosphere | 后置 |
| 模块边界 | Plugin / Package | 后置 |

**F38 验收：** 合成作业 1 worker 对 N 的 wall-clock；空场景 FPS 不算。

**后置（不升 Primary）：** Skylight / Atmosphere / 高度雾（仅后续研究）；CORE-F36 S05 Stub。

**本波已收口（勿当 WIP）：** CORE-F36；CORE-F35；BUILD-F01；CORE-F34；UI-F05；UI-F06；ASSET-F08；ASSET-F07；CORE-F32；ED-F23；UI-F04；CORE-F28–F31；CORE-F29/F30；SPR-F01–F07；RND-F18–F20/F22/F23、CORE-F27 / ED-F21、WF-F04、**WF-F05**；RND-F21 **Deferred**。
**并行 / 不挡**

| 轨 | ID | 说明 |
|----|-----|------|
| ui | `UI-F05` / `UI-F06` | **Done**；已合入 `master`；解锁 minCraft 拖拽 + 图集图标 |
| ui | `UI-F08` | **Review** — Widget 多键指针 + 修饰键；Button 糖；Drag 发起键已实现；`test screen-ui-pointer`；待验收 · [Design](./Platform/UI/UI-F08_SCREENUI_WIDGET_POINTER_INTERACTION_DESIGN.md) |
| input | `CORE-F40` | **Done** — 全量键表 + 合成修饰 + Gamepad；`test input-key-catalog` · [Design](./Platform/Core/CORE-F40_INPUT_KEY_CATALOG_DESIGN.md) |
| ui | `UI-F07` | FillAmount Planned；F06 后可开 Design |
| asset | `ASSET-F08` / `ASSET-F07` | **Done**；已合入 `master`（去 typed Load 糖衣 + DataAsset） |
| editor | `ED-F11` | Tab Host 余量（可选） |
| editor | `ED-F02` | S03/S05 余量（低优） |
| — | Prefab 后置 | Nested / Apply→Prefab / 并排预览 |
| — | `ED-F10` | EditorSettings 占位 |
| render | `RND-F21` | 光视锥 / CSM — Deferred |

<details>
<summary>历史焦点（MCP / ED-F19 / RND-F20 等，折叠）</summary>

| 序 | ID | 标题 | Status |
|----|-----|------|--------|
| — | RND-F18–F20 | MeshBatch / Cull / Sort | **Done** |
| — | ED-F19 / MCP-F01–F04 / CLI-F02 / ED-F17–F18 | Automation + MCP 波 | **Done** |

</details>

### 本波已合入 master（勿再当 WIP）

| 轨 | 内容 | Status |
|----|------|--------|
| scripting / prefab / editor command | CORE-F19–F26、ED-F12–F16、ASSET-F03 等 | Done — 见下节历史表 |

<details>
<summary>历史焦点表（master 合入后 Phase P 余量，折叠）</summary>

| 序 | ID | 标题 | Status |
|----|-----|------|--------|
| — | CORE-F19–F26 / ED-F16 / ASSET-F03 | Prefab + Lua 波 | **Done** |
| 1 | ED-F09 | Console LogRecord | **Done** |
| 2 | ED-F11 | Tab Host | **In Progress**（W3 大部） |
| 3–6 | ED-F12–F15 | Agent Edit / Command-first | **Done** |
| — | ED-F10 | EditorSettings | **Planned** |

</details>

### 0.1.0 执行模型（详见 Roadmap §3.0 拓扑图）

| Phase | 主题 |
|-------|------|
| **F** Foundation | **Done** — Log → Schema → Brand |
| **P** Parallel | **进行中** — fan-out（editor 余量；Prefab / Sprite MVP 已合入） |
| **D** Demo | FPS + 0.1.0 tag |

**Prefab：** F23=资产+Instantiate；F24=Override；F16=编辑器（单 Viewport；并排预览依赖隔离 RT）。

### 并行支线（不升主线）

| ID | 状态 | 说明 |
|----|------|------|
| `CORE-F41` | **Review** | 会话日志落盘 + `editor.get_log` + Console Copy Visible/Line；待手工点验 Console · [Design](./Platform/Core/CORE-F41_SESSION_LOG_FILE_AND_READBACK_DESIGN.md) |
| `WF-F02` | In Progress | handbook 子系统文档按需 |
| `ED-F01` | In Progress | VK 阴影质量 **Deferred**；勿当主航道 |
| `RND-F17` | **Done** | ViewId+SRT；TD-027 Done |
| `MAT-F01`–`F06` | **Done** | 运行时产品化主链（含 `.memi`） |
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
| `D:/Dev/GitRepo/minEngine` | **`feat/asset-load-api`** | ASSET-F08/F07 已合入 master；可对齐或归档；可能仍有本地 WIP |
| `D:/Dev/GitRepo/minEngine-ui` | **`master`** | 主协作树（UI-F05/F06 + ASSET-F08/F07 已合入） |
| `D:/Dev/GitRepo/minEngine-lua-script` | **`feat/lua-script`** | 可对齐 master 后归档 |
| `D:/Dev/GitRepo/minEngine-prefab` | **`feat/prefab`** | 可对齐 master 后归档 |
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | 可归档或留给下一动画切片 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 可与 master 对齐；`ProjectRoot` 本地指向本树 |
| `D:/Dev/GitRepo/minEngine-gameplay` | `feat/gameplay-framework` | 可归档 |
| `D:/Dev/GitRepo/minEngine-render` | **`feat/render`** | 2026-10-24 已 ff→`master`（`a0626b67`）；当前 **RND-F26** Draft |

`MyMEProject.meproject` 的 `ProjectRoot` 须指向**当前工作树**的 `MyMEProject`（主仓：`…/minEngine/minEngine/MyMEProject`）。

---

## Explicitly deferred

`.memesh` · Animation Event · IK / Root Motion / Retarget · Import Settings 框架（F02 之后）· ED-F01 VK 阴影质量 · `RND-F12` · `PHYS-F03` · ED-F04 S10b · CORE-F05-S05 Pause/Step · ANIM Shadow skinned · Prefab Nested / Apply→Prefab / 并排预览（隔离 RT）· GC / Net / 完整 Gameplay Framework · Sprite Flipbook / 运行时动态 Pack / 体素专用 API

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
