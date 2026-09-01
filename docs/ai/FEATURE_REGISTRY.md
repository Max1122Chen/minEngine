# Feature Registry

Last updated: 2026-09-01（render + audio merged to `master`）

Purpose: **single source of truth** for `<DOMAIN>-F<nn>` IDs. Avoid duplicate or conflicting Feature IDs between you and AI.

**Rules (mandatory for new work):**

1. **Register a row before** creating Design Spec or assigning `<DOMAIN>-Fnn` in docs.
2. Pick the next free number for that `DOMAIN` (see [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) §3).
3. Set Status: `Planned` → `In Progress` → `Done` | `Deferred` | `Cancelled`.
4. Link the Design (or Implementation) path in **Design** column.
5. Do not reuse IDs; deprecate by setting Status `Cancelled` and a note — do not recycle numbers.

---

## Active & recent features

| Feature ID | Title | Status | Owner | Design / plan |
|------------|-------|--------|-------|----------------|
| `CLI-F01` | Unified command-line interface | Done | — | [CLI_UNIFIED_DESIGN](./Platform/CLI/CLI_UNIFIED_DESIGN.md) |
| `TEST-F01` | Test runner, suite registry, verify integration | Done | — | [TEST_UNIFIED_DESIGN](./Platform/Test/TEST_UNIFIED_DESIGN.md) |
| `TEST-F02` | Test layout migration, doctest, minEngineTests exe | Done | — | [TEST_F02_LAYOUT_MIGRATION](./Platform/Test/TEST_F02_LAYOUT_MIGRATION.md) |
| `TEST-F03` | Suite slim-down, doctest cases, fixture B reflection | Done | — | [TEST_F03_SUITE_SLIM_PLAN](./Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md) |
| `WF-F01` | Documentation templates and collaboration governance | Done | — | [templates/](./templates/), [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) |
| `WF-F02` | 协作者文档站（MkDocs 公开手册 + GitHub Pages） | In Progress | — | [Design](./Platform/Docs/HANDBOOK_SITE_DESIGN.md) · [Impl](./Platform/Docs/HANDBOOK_SITE_IMPLEMENTATION.md) — 骨架 Done，子系统文档待补 |
| `CORE-F01` | Lua scripting runtime（sol2 + System + LuaScript asset + LuaComponent） | Done | — | [LUA_SCRIPTING_DESIGN](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md) |
| `CORE-F02` | Lua Script binding codegen（Script\* specifier → sol2） | Done | — | [LUA_SCRIPT_BINDING_DESIGN](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md) |
| `CORE-F03` | Transform 四元数存储（Quaternion 类型、序列化、Inspector 欧拉 Widget） | Done | — | [Design](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_DESIGN.md) · [Impl](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_IMPLEMENTATION.md) · 原 physics 分支 `CORE-F01`，合入 master 时改号以免与 Lua 冲突 |
| `CORE-F04` | Multicast Delegates（Native 多播；解锁 PHYS-F03） | **Done** | — | [Design](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) · [Impl](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_IMPLEMENTATION.md) · **TD-006** Done；Dynamic/Lua 后置 |
| `RND-F01` | RenderGraph（Manual 图；S0–S05 Done） | **Draft / Superseded direction** | — | [RND-F01_RENDER_GRAPH_DESIGN](./Render/RND-F01_RENDER_GRAPH_DESIGN.md) · **S06+ 产品方向由 `RND-F07` 取代**（实验 Bake 非终态） |
| `RND-F02` | Modern RHI（GPU 工作模型抽象；GL 首适配 + Pass CommandList） | Done | — | [RND-F02_MODERN_RHI_DESIGN](./Render/RND-F02_MODERN_RHI_DESIGN.md) · S0–S5 |
| `RND-F03` | Legacy RHI removal（调用面 + 管线重构 M4 + 后端绞杀 M3） | **Done** | — | [Design](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · 2026-08-04 关账；残留债经 F09/F10 付清 |
| `RND-F04` | Modern RHI further evolution（语义终态：PipelineLayout、Packet、Setup/Execute、缓存） | **Done** | — | [RND-F04_MODERN_RHI_EVOLUTION_DESIGN](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) · S01–S04 |
| `RND-F05` | Vulkan backend + SPIR-V（GL+VK）+ modern RHI completion | **Done** | — | [Design](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · [Impl](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md) · S01–S07d Done；S07e/f **迁至 ED-F01** |
| `ED-F01` | Vulkan Editor Parity（ImGui-Vulkan + viewport + navigation + 承接 shadow/sky） | **In Progress** | S05+ | [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) · S01–S04 代码已落地，待人工验收 |
| `RND-F06` | ForwardRenderer（Renderer / RenderGraph 职责分离；删除 `RenderPipeline`） | **In Progress** | — | [RND-F06_FORWARD_RENDERER_DESIGN](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md) · S01–S02 Done；S03 可选 |
| `RND-F07` | Granite-style RDG + 帧资源所有权大重构 | **Done** *(shell)* | — | [Design](./Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) · [Impl](./Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_IMPLEMENTATION.md) · S01–S09；**bake 语义续作 → RND-F12** |
| `RND-F12` | Granite RDG **语义全复刻**（Phase A–D；非粘贴代码）| **In Progress** *(降级：BUG-013 卫生项)* | — | [Design](./Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md) · [Impl](./Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_IMPLEMENTATION.md) · BUG-013 主因已降级 |
| `RND-F13` | ManualRenderer（对照→主诊场地） | **Done** *(Reference: `--renderer manual`)* | — | [Design](./Render/RND-F13_MANUAL_RENDERER_DESIGN.md) · [Impl](./Render/RND-F13_MANUAL_RENDERER_IMPLEMENTATION.md) · 坐实非 RDG 主因 |
| `RND-F14` | ShadowPass UBO 寿命 + 矩阵单源 | **Done** | — | [Design](./Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) · [Impl](./Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_IMPLEMENTATION.md) |
| `RND-F08` | Shadow map 图所有权（Directional/Spot/Point） | **Done** | — | [Design](./Render/RND-F08_SHADOW_GRAPH_OWNERSHIP_DESIGN.md) · [Impl](./Render/RND-F08_SHADOW_GRAPH_OWNERSHIP_IMPLEMENTATION.md) · [Slot slim](./Render/RND-F08_SHADOW_SLOT_SEMANTICS.md) · 付清 **TD-020** |
| `RND-F09` | Render Binding / RHI hygiene sweep（Set0/Material cache、PSO Apply、Clear、残留） | **Done** | — | [Design](./Render/RND-F09_RHI_HYGIENE_SWEEP_DESIGN.md) · [Impl](./Render/RND-F09_RHI_HYGIENE_SWEEP_IMPLEMENTATION.md) · TD-013/014/016/017/018/019（RND 号段）；**不含** EnvMap Bake |
| `RND-F10` | EnvironmentMap Asset + Sky/IBL 接线；现代 Bake | **Done** | — | [Design](./Render/RND-F10_ENVIRONMENT_MAP_ASSET_DESIGN.md) · [Impl](./Render/RND-F10_ENVIRONMENT_MAP_ASSET_IMPLEMENTATION.md) · TD-015 Done；S06→TD-021 |
| `RND-F11` | DebugDrawing（线/点/盒 wireframe 通道；Editor collider 示范） | **Done** | — | [Design](./Render/RND-F11_DEBUG_DRAWING_DESIGN.md) · [Impl](./Render/RND-F11_DEBUG_DRAWING_IMPLEMENTATION.md) · MVP S01–S02 |
| `LAUN-F01` | Engine Launcher（工程选择、最近项目、启动 Editor） | **Planned** | — | [Placeholder](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md) · `feat/launcher` · worktree `minEngine-launcher` |
| `AUD-F01` | Audio system（资产、播放、Scene 集成 MVP） | **Done** | — | [Design](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_DESIGN.md) · [Impl](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_IMPLEMENTATION.md) · MVP S01–S09 + S11 |
| `ANIM-F01` | Animation system | **Planned** | — | [Placeholder](./Animation/ANIM-F01_ANIMATION_SYSTEM_DESIGN.md) · `feat/ui-anim` |
| `UI-F01` | UI system | **Planned** | — | [Placeholder](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md) · `feat/ui-anim` |
| `PHYS-F01` | Jolt physics bootstrap（PhysicsSystem、RigidBody/BoxCollider、固定步长写回、Channel/Contact、Scene::LineTrace） | Done | — | [Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md) · [Impl](./Physics/PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md) |
| `PHYS-F02` | Collision + query shapes（Sphere/Capsule collider；Scene SphereTrace/CapsuleTrace） | Done | — | [Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) · [Impl](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_IMPLEMENTATION.md) |
| `PHYS-F03` | Contact gameplay dispatch（玩法接触通知） | Deferred | — | [Placeholder](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) · Delegates（CORE-F04）已满足；**RND-F11 Debug 通道已 Done，可独立评估** |
| `PHYS-F04` | Collider fixes & hygiene | **Planned** | — | [Placeholder](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) · `feat/physics` |

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | Command-line / tools entry |
| `TEST` | F04 | Automated tests (follow-on) |
| `WF` | F03 | Workflow / docs |
| `CORE` | F05 | F01–F04 Done |
| `ASSET` | F01 | Asset pipeline extensions |
| `ED` | F02 | F01 Vulkan Editor Parity In Progress |
| `RND` | F15 | F11 Done；**Persistent debug / Editor toggle 待新 Feature 登记** |
| `LAUN` | F02 | F01 Launcher 登记 |
| `AUD` | F02 | F01 Done |
| `ANIM` | F02 | F01 占位 |
| `UI` | F02 | F01 占位 |
| `PHYS` | F05 | F03 Deferred；F04 Planned |
| `MAT` | F01 | Material (new IDs only; legacy Phase docs keep old names) |

Update **Next Feature #** when you register a new row.
