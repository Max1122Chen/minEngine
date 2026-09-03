# Feature Registry

Last updated: 2026-09-03（`master`：**CORE-F05** MVP **Done**）

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
| `CORE-F03` | Transform 四元数存储（Quaternion 类型、序列化、Inspector 欧拉 Widget） | Done | — | [Design](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_DESIGN.md) · [Impl](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_IMPLEMENTATION.md) |
| `CORE-F04` | Multicast Delegates（Native 多播；解锁 PHYS-F03） | **Done** | — | [Design](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) · [Impl](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_IMPLEMENTATION.md) |
| `CORE-F05` | Play Mode（Edit/Play、双 Scene、Inspecting Context） | **Done**（MVP） | — | [Design](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · [Impl](./Platform/Core/CORE-F05_PLAY_MODE_IMPLEMENTATION.md) · [S06](./Platform/Core/CORE-F05_S06_INSPECTING_CONTEXT.md) · **`master`** · S05 Pause/Step Deferred；TD-028/029/030 Open |
| `CORE-F06` | Component Activate（`m_bActive`、`ApplyActivation`、System 跳过 inactive） | **Done** | — | [Design](./Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md) · [Impl](./Platform/Core/CORE-F06_COMPONENT_ENABLE_IMPLEMENTATION.md) · **`master`** |
| `CORE-F07` | 反射/Inspector 展示名（去 `m_`/`x_`/`b_` 前缀 + 驼峰分词） | **Done** | — | [Design](./Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) · **`master`** |
| `RND-F01` | RenderGraph（Manual 图；S0–S05 Done） | **Draft / Superseded direction** | — | [RND-F01_RENDER_GRAPH_DESIGN](./Render/RND-F01_RENDER_GRAPH_DESIGN.md) |
| `RND-F02` | Modern RHI | Done | — | [RND-F02_MODERN_RHI_DESIGN](./Render/RND-F02_MODERN_RHI_DESIGN.md) |
| `RND-F03` | Legacy RHI removal | **Done** | — | [Design](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) |
| `RND-F04` | Modern RHI further evolution | **Done** | — | [Design](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) |
| `RND-F05` | Vulkan backend + SPIR-V | **Done** | — | [Design](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · [Impl](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md) |
| `RND-F06` | ForwardRenderer | **In Progress** | — | [RND-F06_FORWARD_RENDERER_DESIGN](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md) · S01–S02 Done |
| `RND-F07` | Granite-style RDG + 帧资源所有权 | **Done** *(shell)* | — | [Design](./Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) |
| `RND-F08`–`F11` | Shadow 所有权 / RHI hygiene / EnvMap / DebugDrawing | **Done** | — | 见各 Design |
| `RND-F12` | Granite RDG 语义全复刻 | **Deferred** *(卫生项)* | — | [Design](./Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md) · 不挡当前 backlog |
| `RND-F13` | ManualRenderer（Reference） | **Done** | — | [Design](./Render/RND-F13_MANUAL_RENDERER_DESIGN.md) |
| `RND-F14` | ShadowPass UBO 寿命 | **Done** | — | [Design](./Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) |
| `RND-F16` | Sprite 2D 渲染（UI 前置） | **Planned** | — | [Placeholder](./Render/RND-F16_SPRITE_2D_DESIGN.md) · 愿景；**不阻塞** |
| `ED-F01` | Vulkan Editor Parity | **In Progress** *(VK 阴影质量 **Deferred**)* | — | [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) |
| `ED-F02` | Editor Workflow（打开/创建 Scene·Material、SkyBox、Viewport、Component UI） | **Planned** | — | [Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md) · **`master`** |
| `ED-F03` | Editor Play Toolbar（Viewport 三行：Tab / Toolbar / 主体） | **Done** | — | [Design](./Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md) |
| `ED-F04` | Debug Console & Unified Command System（Runtime 控制面 + Agent-friendly） | **In Progress** *(MVP Done)* | — | [Design](./Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md) · S00–S10a Done · **S10b Deferred** · S07 Deferred |
| `LAUN-F01` | Engine Launcher | **Done** | — | [Design](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md) |
| `AUD-F01` | Audio system | **Done** | — | [Design](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_DESIGN.md) |
| `ANIM-F01` | Animation system | **Planned** | — | [Placeholder](./Animation/ANIM-F01_ANIMATION_SYSTEM_DESIGN.md) · `feat/animation` · **merge 检查点后** |
| `UI-F01` | UI system | **Planned** | — | [Placeholder](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md) · `feat/ui` · 依赖 `RND-F16` |
| `PHYS-F01` | Jolt physics bootstrap | Done | — | [Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md) |
| `PHYS-F02` | Collision + query shapes | Done | — | [Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) |
| `PHYS-F03` | Contact gameplay dispatch | Deferred | — | [Placeholder](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) |
| `PHYS-F04` | Collider 尺寸与 Scale 解耦 | **Done** | — | [Design](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) · **`master`** |

---

## Vision placeholders（无独立 Feature ID，不排期）

登记于 [ACTIVE_WORK.md](./ACTIVE_WORK.md) §愿景：Gameplay 插件化框架、网络游戏、Editor Debug Console、Agent-friendly 设计规范。

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | |
| `TEST` | F04 | |
| `WF` | F03 | |
| `CORE` | **F08** | F05 In Progress on `master`；F06–F07 Done |
| `ASSET` | F01 | |
| `ED` | **F05** | F02–F04 on `master`；F03 Toolbar Done；F04 Console In Progress |
| `RND` | **F17** | F16 Sprite 占位；F12 Deferred |
| `LAUN` | F02 | F01 Done |
| `AUD` | F02 | F01 Done |
| `ANIM` | F02 | F01 占位；merge 后开 `feat/animation` |
| `UI` | F02 | F01 占位；`feat/ui` |
| `PHYS` | F05 | F04 on `master` |
| `MAT` | F01 | |

Update **Next Feature #** when you register a new row.
