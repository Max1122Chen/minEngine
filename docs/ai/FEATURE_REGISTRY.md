# Feature Registry

Last updated: 2026-09-14（ED-F13/F14 Design 首稿）  
Purpose: **single source of truth** for `<DOMAIN>-F<nn>` IDs. Avoid duplicate or conflicting Feature IDs between you and AI.

**Rules (mandatory for new work):**

1. **Register a row before** creating Design Spec or assigning `<DOMAIN>-Fnn` in docs.
2. Pick the next free number for that `DOMAIN` (see [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) §3).
3. Set Status: `Planned` → `In Progress` → `Done` | `Deferred` | `Cancelled`.
4. Link the Design (or Implementation) path in **Design** column.
5. Do not reuse IDs; deprecate by setting Status `Cancelled` and a note — do not recycle numbers.
6. Architecture / Core vs Plugin choices: [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md). Stage tracks: [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md).

---

## Active & recent features

| Feature ID | Title | Status | Owner | Design / plan |
|------------|-------|--------|-------|----------------|
| `CLI-F01` | Unified command-line interface | Done | — | [CLI_UNIFIED_DESIGN](./Platform/CLI/CLI_UNIFIED_DESIGN.md) |
| `TEST-F01` | Test runner, suite registry, verify integration | Done | — | [TEST_UNIFIED_DESIGN](./Platform/Test/TEST_UNIFIED_DESIGN.md) |
| `TEST-F02` | Test layout migration, doctest, minEngineTests exe | Done | — | [TEST_F02_LAYOUT_MIGRATION](./Platform/Test/TEST_F02_LAYOUT_MIGRATION.md) |
| `TEST-F03` | Suite slim-down, doctest cases, fixture B reflection | Done | — | [TEST_F03_SUITE_SLIM_PLAN](./Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md) |
| `TEST-F04` | `Testing::TestAccess<T>` — Core 前向声明 + Tests 特化，替代 N 个 TestScope friend | **Done** | — | [Design](./Platform/Test/TEST-F04_TEST_ACCESS_DESIGN.md) · **`master`** |
| `WF-F01` | Documentation templates and collaboration governance | Done | — | [templates/](./templates/), [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) |
| `WF-F02` | 协作者文档站（MkDocs 公开手册 + GitHub Pages） | **In Progress** | — | [Design](./Platform/Docs/HANDBOOK_SITE_DESIGN.md) · [Impl](./Platform/Docs/HANDBOOK_SITE_IMPLEMENTATION.md) — 骨架 Done，子系统文档待补 |
| `CORE-F01` | Lua scripting runtime（sol2 + System + LuaScript asset + LuaComponent） | Done | — | [LUA_SCRIPTING_DESIGN](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md) |
| `CORE-F02` | Lua Script binding codegen（Script\* specifier → sol2） | Done | — | [LUA_SCRIPT_BINDING_DESIGN](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md) |
| `CORE-F03` | Transform 四元数存储（Quaternion 类型、序列化、Inspector 欧拉 Widget） | Done | — | [Design](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_DESIGN.md) · [Impl](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_IMPLEMENTATION.md) |
| `CORE-F04` | Multicast Delegates（Native 多播；解锁 PHYS-F03） | **Done** | — | [Design](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) · [Impl](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_IMPLEMENTATION.md) |
| `CORE-F05` | Play Mode（Edit/Play、双 Scene、Inspecting Context） | **Done**（MVP） | — | [Design](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · [Impl](./Platform/Core/CORE-F05_PLAY_MODE_IMPLEMENTATION.md) · [S06](./Platform/Core/CORE-F05_S06_INSPECTING_CONTEXT.md) · S05 Pause/Step Deferred；TD-030 Open |
| `CORE-F06` | Component Activate（`m_bActive`、`ApplyActivation`、System 跳过 inactive） | **Done** | — | [Design](./Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md) · [Impl](./Platform/Core/CORE-F06_COMPONENT_ENABLE_IMPLEMENTATION.md) |
| `CORE-F07` | 反射/Inspector 展示名（去 `m_`/`x_`/`b_` 前缀 + 驼峰分词） | **Done** | — | [Design](./Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) |
| `CORE-F08` | 序列化系统整理（StaticClass API、删死代码、P1 内部整理；不改 Binary wire） | **Done** | — | [Design](./Platform/Serialization/CORE-F08_SERIALIZATION_CLEANUP_DESIGN.md) · [Impl](./Platform/Serialization/CORE-F08_SERIALIZATION_CLEANUP_IMPLEMENTATION.md) |
| `CORE-F09` | Binary wire 协议 v2（Transient Ids；TD-028/029；Persistent 契约） | **Done** | — | [Design](./Platform/Serialization/CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md) · [Impl](./Platform/Serialization/CORE-F09_BINARY_WIRE_PROTOCOL_IMPLEMENTATION.md) · Transient only；存盘 Binary 未做 |
| `CORE-F10` | JSON 存盘兼容（宽松未知字段 + `$schemaVersion` meta） | **Done** | — | [Design](./Platform/Serialization/CORE-F10_JSON_DISK_COMPAT_DESIGN.md) · [Impl](./Platform/Serialization/CORE-F10_JSON_DISK_COMPAT_IMPLEMENTATION.md) |
| `CORE-F11` | 属性 Getter/Setter native thunk + `AssignProperty`（TD-026 / BUG-CORE-001） | **Done** | — | [Design](./Platform/Reflection/CORE-F11_PROPERTY_ACCESSOR_THUNKS_DESIGN.md) · [Impl](./Platform/Reflection/CORE-F11_PROPERTY_ACCESSOR_THUNKS_IMPLEMENTATION.md) |
| `CORE-F12` | `ME_GENERATED_BODY()` 去无用类型实参 + header-tool marker 归属收紧 | **Done** | — | [Design](./Platform/Reflection/CORE-F12_GENERATED_BODY_NO_ARG_DESIGN.md) |
| `CORE-F13` | Parameter Schema / Layout / Store（Anim Graph 等共享参数层） | **Done** | — | [Design](./Platform/Core/CORE-F08_PARAMETER_STORAGE_DESIGN.md) · [Impl](./Platform/Core/CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md) · 文件名保留历史 `CORE-F08_PARAMETER_*` |
| `CORE-F14` | GameObject Hierarchy（父子 Transform） | **Done** | — | [Design](./Platform/Core/CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md) · [Impl](./Platform/Core/CORE-F08_GAMEOBJECT_HIERARCHY_IMPLEMENTATION.md) · 文件名保留历史 `CORE-F08_GAMEOBJECT_*` |
| `CORE-F15` | Parallel Hierarchy KeepWorld | **Done** | — | [Design](./Platform/Core/CORE-F09_PARALLEL_HIERARCHY_KEEPWORLD_DESIGN.md) · 文件名保留历史 `CORE-F09_*` |
| `CORE-F16` | LinearColor 作者颜色（灯光等） | **Done** | — | [Design](./Platform/Core/CORE-F14_LINEAR_COLOR_AUTHORING_DESIGN.md) · 文件名保留历史 `CORE-F14_*` |
| `CORE-F17` | LogChannel + structured LogRecord（spdlog Backend；非 UE Category） | **Done** | — | [Design](./Platform/Core/CORE-F17_LOGGING_CHANNELS_DESIGN.md) · [Impl](./Platform/Core/CORE-F17_LOGGING_CHANNELS_IMPLEMENTATION.md) · Phase F1 |
| `CORE-F18` | EngineVersion（一等公民）+ 磁盘 `$schemaVersion` 门闸 / `$engineVersion` 戳 | **Done** | — | [Design](./Platform/Serialization/CORE-F18_SCHEMA_ENGINE_VERSION_DESIGN.md) · Phase F2 · engine **0.0.9** / schema **1** |
| `WF-F03` | Maximum 产品显示名与版本展示 | **Done** | — | [Design](./Platform/Docs/WF-F03_MAXIMUM_PRODUCT_BRANDING_DESIGN.md) · Phase F3 · 显示名 Maximum；版本数字 CORE-F18（现 0.0.9） |
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
| `RND-F16` | Sprite / 2D Rendering Foundation（ScreenUI 绘制前置） | **Done** | — | [Design](./Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md) · [Impl](./Render/RND-F16_2D_RENDERING_FOUNDATION_IMPLEMENTATION.md) · 已合入 `master`（feat/ui） |
| `ED-F01` | Vulkan Editor Parity | **In Progress** *(VK 阴影质量 **Deferred**)* | — | [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) |
| `ED-F02` | Editor Workflow（打开/创建 Scene·Material、SkyBox、Viewport、Component UI） | **In Progress** *(S00–S02/S04 Done；S03/S05 余量)* | — | [Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md) |
| `ED-F03` | Editor Play Toolbar（Viewport 三行：Tab / Toolbar / 主体） | **Done** | — | [Design](./Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md) |
| `ED-F04` | Debug Console & Unified Command System（Runtime 控制面 + Agent-friendly） | **In Progress** *(MVP Done)* | — | [Design](./Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md) · S00–S10a Done · **S10b Deferred** · S07 Deferred |
| `ED-F05` | Inspector / Component UX（改名逃逸、Add 图标/搜索、组件头、重排、CB 内联重命名；视口图标 Deferred） | **Done**（S05 Deferred） | — | [Design](./Editor/ED-F05_INSPECTOR_COMPONENT_UX_DESIGN.md) · [Impl](./Editor/ED-F05_INSPECTOR_COMPONENT_UX_IMPLEMENTATION.md) |
| `ED-F06` | State Machine Graph Canvas（Anim Graph 真·SM 画布） | **Done** | — | [Design](./Editor/ED-F05_STATE_MACHINE_CANVAS_DESIGN.md) · [Impl](./Editor/ED-F05_STATE_MACHINE_CANVAS_IMPLEMENTATION.md) · 文件名保留历史 `ED-F05_STATE_MACHINE_*` |
| `ED-F07` | Anim SM Canvas Polish（Entry / AnyState / 边菜单 / 空 Clip） | **Done** | — | [Design](./Editor/ED-F06_ANIM_SM_CANVAS_POLISH_DESIGN.md) · [Impl](./Editor/ED-F06_ANIM_SM_CANVAS_POLISH_IMPLEMENTATION.md) · 文件名保留历史 `ED-F06_ANIM_SM_*` |
| `ED-F08` | Hierarchy Tree（拖拽改父） | **Done** | — | [Design](./Editor/ED-F05_HIERARCHY_TREE_DESIGN.md) · 文件名保留历史 `ED-F05_HIERARCHY_*` |
| `ED-F09` | Editor Log Console（LogRecord 展示/按 Channel·Severity 过滤） | **Done**（W0–W2） | — | [Design](./Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md) · Channel/时间窗/Collapse/Clear on Play |
| `ED-F10` | EditorSettings（布局/过滤等持久化） | **Planned** | — | Design 未开 · 0.1.0 Roadmap 占位；**勿抢号** |
| `ED-F11` | Multi-document Tab Host（Session + 文档 Tab；共享 Dock MVP） | **In Progress**（W0–W2；**W3 缓**） | — | [Design](./Editor/ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md) · Host/注册类型/Tab/per-Session 栈 |
| `ED-F12` | Agent Edit Protocol（资产 Session × Debug/EditorCommand 命名收敛） | **Done** | — | [Design](./Editor/ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md) · `edit`/`verify`/`invoke` → 后续 Feature |
| `ED-F13` | EditorCommand-first：去掉 Select/Apply 套壳，Editor 变薄、逻辑进 Command | **Planned** | — | [Design](./Editor/ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md) · Scene 限定；真重构删 Apply* |
| `ED-F14` | 各 SubEditor 命令面完备（按 Session/资产类型补齐 EditorCommand） | **Planned** | — | [Design](./Editor/ED-F14_EDITOR_COMMAND_COVERAGE_DESIGN.md) · **Blocked on F13**；含 edit/verify |
| `LAUN-F01` | Engine Launcher | **Done** | — | [Design](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md) |
| `AUD-F01` | Audio system | **Done** | — | [Design](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_DESIGN.md) |
| `ASSET-F01` | External Import Pipeline（Assimp → 引擎资产；MVP） | **Done**（MVP） | — | [Design](./Asset/ASSET-F01_IMPORT_PIPELINE_DESIGN.md) · [Impl](./Asset/ASSET-F01_IMPORT_PIPELINE_IMPLEMENTATION.md) · S04 / `.memesh` Deferred |
| `ASSET-F02` | Formal Import/Load 注册式管线 + ImportDialog | **Done** | — | [Design](./Asset/ASSET-F02_IMPORT_SERVICE_DESIGN.md) · [Impl](./Asset/ASSET-F02_IMPORT_SERVICE_IMPLEMENTATION.md) |
| `ANIM-F01` | Skeletal Mesh Pipeline（Pose → palette → GPU） | **Done** | — | [Design](./Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md) · [Impl](./Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_IMPLEMENTATION.md) · 旧 Placeholder `ANIM-F01_ANIMATION_SYSTEM_DESIGN.md` 已废弃勿用 |
| `ANIM-F02` | Clip Playback（Track / Player / SMC / `.meaclip`） | **Done** | — | [Design](./Animation/ANIM-F02_CLIP_PLAYBACK_DESIGN.md) · [Impl](./Animation/ANIM-F02_CLIP_PLAYBACK_IMPLEMENTATION.md) |
| `ANIM-F03` | Animation Graph MVP（FSM + Params + Pose Blend + Editor） | **In Progress** *(runtime+Editor 已合入；人型闭环 smoke 收口)* | — | [Design](./Animation/ANIM-F03_ANIMATION_GRAPH_DESIGN.md) · [Impl](./Animation/ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md) |
| `ANIM-F04` | Blend Tree 1D | **Review** | — | [Design](./Animation/ANIM-F04_BLEND_TREE_DESIGN.md) · [Impl](./Animation/ANIM-F04_BLEND_TREE_IMPLEMENTATION.md) · 待审批后再开实现 |
| `ANIM-F05` | Nested State Machine | **Planned** | — | Design 未开；依赖 ANIM-F04 收口后再立 |
| `UI-F01` | ScreenUI Canvas / Widget / Image / Layout MVP | **Done** | — | [Design](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md) |
| `UI-F02` | ScreenUI Hit-test + Play pointer routing | **Done** | — | [Design](./Platform/UI/UI-F02_SCREENUI_HITTEST_DESIGN.md) |
| `UI-F03` | ScreenUI Button（OnClicked + tint） | **Done** | — | [Design](./Platform/UI/UI-F03_SCREENUI_BUTTON_DESIGN.md) |
| `PHYS-F01` | Jolt physics bootstrap | Done | — | [Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md) |
| `PHYS-F02` | Collision + query shapes | Done | — | [Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) |
| `PHYS-F03` | Contact gameplay dispatch | Deferred | — | [Placeholder](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) |
| `PHYS-F04` | Collider 尺寸与 Scale 解耦 | **Done** | — | [Design](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) |
| `GP-F01` | GameplayTag（Manager + Container；Engine 子系统；Native 宽） | **Done** | — | [Design](./Gameplay/GP-F01_GAMEPLAY_TAG_DESIGN.md) · [Impl](./Gameplay/GP-F01_GAMEPLAY_TAG_IMPLEMENTATION.md) · `test gameplay-tags` |
| `GP-F02` | GameplayEventSystem（Scene 作用域 Component；无 Payload） | **Done** | — | [Design](./Gameplay/GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md) · [Impl](./Gameplay/GP-F02_GAMEPLAY_EVENT_SYSTEM_IMPLEMENTATION.md) · 依赖 GP-F01 · `test gameplay-events` |

---

## Merge-wave ID remaps（feat/animation · feat/ui → master）

| 分支原 ID | master 正式 ID | 原因 |
|-----------|----------------|------|
| anim `CORE-F08` Parameter Store | **`CORE-F13`** | master `CORE-F08`–`F12` 已占用（序列化/反射） |
| anim `ED-F05` SM Canvas | **`ED-F06`** | master `ED-F05` = Inspector Component UX |
| anim `ED-F06` Canvas Polish | **`ED-F07`** | 顺延 |
| ui Hierarchy（docs CORE-F08） | **`CORE-F14`** | F12–F13 已被 GENERATED_BODY / Parameter 占用 |
| ui KeepWorld（docs CORE-F09） | **`CORE-F15`** | 同上 |
| ui LinearColor（docs CORE-F14） | **`CORE-F16`** | 顺延 |
| ui Hierarchy Tree（docs ED-F05） | **`ED-F08`** | master ED-F05=Inspector；F06–F07=Anim canvas |
| 设计文件名 | 暂不改 | 路径仍含历史前缀；以本表正式 ID 为准 |

---

## Vision placeholders（无独立 Feature ID，不排期）

登记在 [ACTIVE_WORK.md](./ACTIVE_WORK.md) 与 [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md)：完整 Gameplay **Plugins / ASC / GAS 上层**、Networking / Net Game、Prefab、Object Lifetime/GC、Render Sort/Batch（待登记）、Agent-friendly 作为**设计原则**（见 [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md)）。

**例外：** `GP-F01`/`GP-F02` 为提前落地的**轻量机制底座**（Tag + Scene Event bus），不是完整 Framework。

**明确不排期（动画扩展）：** Animation Event、IK、Root Motion、Retarget、完整 AnimBP。

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | |
| `TEST` | **F05** | F04 Done（TestAccess） |
| `WF` | **F04** | F02 handbook In Progress；**F03** Maximum branding **Done** |
| `CORE` | **F19** | F17 Logging / F18 Schema **Done**；F13–F16 Done |
| `ASSET` | **F03** | F01–F02 Done；Async Lifetime 愿景见 Capability Roadmap |
| `ED` | **F15** | F12 Done；**F13** Command-first Editor Planned；**F14** 命令面完备 Planned；F10 占位；F11 W3 缓 |
| `RND` | **F17** | F16 Done；F06 In Progress；F12 Deferred |
| `LAUN` | F02 | F01 Done |
| `AUD` | F02 | F01 Done |
| `ANIM` | **F06** | F01–F02 Done；F03 closeout；F04 Review；F05 Planned |
| `UI` | **F04** | F01–F03 Done |
| `PHYS` | F05 | F04 Done；F03 Deferred |
| `GP` | **F03** | F01–F02 Done |
| `MAT` | F01 | |

Update **Next Feature #** when you register a new row.
