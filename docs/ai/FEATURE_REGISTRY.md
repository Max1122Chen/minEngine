# Feature Registry

Last updated: 2026-09-10锛坢erge wave: core+editor+gameplay锛?
Purpose: **single source of truth** for `<DOMAIN>-F<nn>` IDs. Avoid duplicate or conflicting Feature IDs between you and AI.

**Rules (mandatory for new work):**

1. **Register a row before** creating Design Spec or assigning `<DOMAIN>-Fnn` in docs.
2. Pick the next free number for that `DOMAIN` (see [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) 搂3).
3. Set Status: `Planned` 鈫?`In Progress` 鈫?`Done` | `Deferred` | `Cancelled`.
4. Link the Design (or Implementation) path in **Design** column.
5. Do not reuse IDs; deprecate by setting Status `Cancelled` and a note 鈥?do not recycle numbers.
6. Architecture / Core vs Plugin choices: [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md). Stage tracks: [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md).

---

## Active & recent features

| Feature ID | Title | Status | Owner | Design / plan |
|------------|-------|--------|-------|----------------|
| `CLI-F01` | Unified command-line interface | Done | 鈥?| [CLI_UNIFIED_DESIGN](./Platform/CLI/CLI_UNIFIED_DESIGN.md) |
| `TEST-F01` | Test runner, suite registry, verify integration | Done | 鈥?| [TEST_UNIFIED_DESIGN](./Platform/Test/TEST_UNIFIED_DESIGN.md) |
| `TEST-F02` | Test layout migration, doctest, minEngineTests exe | Done | 鈥?| [TEST_F02_LAYOUT_MIGRATION](./Platform/Test/TEST_F02_LAYOUT_MIGRATION.md) |
| `TEST-F03` | Suite slim-down, doctest cases, fixture B reflection | Done | 鈥?| [TEST_F03_SUITE_SLIM_PLAN](./Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md) |
| `TEST-F04` | `Testing::TestAccess<T>` 鈥?Core 鍓嶅悜澹版槑 + Tests 鐗瑰寲锛屾浛浠?N 涓?TestScope friend | **Done** | 鈥?| [Design](./Platform/Test/TEST-F04_TEST_ACCESS_DESIGN.md) 路 S00鈥揝04 路 **`feat/core`** |
| `WF-F01` | Documentation templates and collaboration governance | Done | 鈥?| [templates/](./templates/), [DOC_GOVERNANCE](./templates/DOC_GOVERNANCE.md) |
| `WF-F02` | 鍗忎綔鑰呮枃妗ｇ珯锛圡kDocs 鍏紑鎵嬪唽 + GitHub Pages锛?| In Progress | 鈥?| [Design](./Platform/Docs/HANDBOOK_SITE_DESIGN.md) 路 [Impl](./Platform/Docs/HANDBOOK_SITE_IMPLEMENTATION.md) 鈥?楠ㄦ灦 Done锛屽瓙绯荤粺鏂囨。寰呰ˉ |
| `CORE-F01` | Lua scripting runtime锛坰ol2 + System + LuaScript asset + LuaComponent锛?| Done | 鈥?| [LUA_SCRIPTING_DESIGN](./Platform/Scripting/LUA_SCRIPTING_DESIGN.md) |
| `CORE-F02` | Lua Script binding codegen锛圫cript\* specifier 鈫?sol2锛?| Done | 鈥?| [LUA_SCRIPT_BINDING_DESIGN](./Platform/Scripting/LUA_SCRIPT_BINDING_DESIGN.md) |
| `CORE-F03` | Transform 鍥涘厓鏁板瓨鍌紙Quaternion 绫诲瀷銆佸簭鍒楀寲銆両nspector 娆ф媺 Widget锛?| Done | 鈥?| [Design](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_DESIGN.md) 路 [Impl](./Platform/Core/CORE-F03_TRANSFORM_QUATERNION_IMPLEMENTATION.md) |
| `CORE-F04` | Multicast Delegates锛圢ative 澶氭挱锛涜В閿?PHYS-F03锛?| **Done** | 鈥?| [Design](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md) 路 [Impl](./Platform/Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_IMPLEMENTATION.md) |
| `CORE-F05` | Play Mode锛圗dit/Play銆佸弻 Scene銆両nspecting Context锛?| **Done**锛圡VP锛?| 鈥?| [Design](./Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) 路 [Impl](./Platform/Core/CORE-F05_PLAY_MODE_IMPLEMENTATION.md) 路 [S06](./Platform/Core/CORE-F05_S06_INSPECTING_CONTEXT.md) 路 **`master`** 路 S05 Pause/Step Deferred锛汿D-030 Open锛汿D-028/029 鈫?**CORE-F09 Done** |
| `CORE-F06` | Component Activate锛坄m_bActive`銆乣ApplyActivation`銆丼ystem 璺宠繃 inactive锛?| **Done** | 鈥?| [Design](./Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md) 路 [Impl](./Platform/Core/CORE-F06_COMPONENT_ENABLE_IMPLEMENTATION.md) 路 **`master`** |
| `CORE-F07` | 鍙嶅皠/Inspector 灞曠ず鍚嶏紙鍘?`m_`/`x_`/`b_` 鍓嶇紑 + 椹煎嘲鍒嗚瘝锛?| **Done** | 鈥?| [Design](./Platform/Core/CORE-F07_REFLECTION_DISPLAY_NAMES_DESIGN.md) 路 **`master`** |
| `CORE-F08` | 搴忓垪鍖栫郴缁熸暣鐞嗭紙StaticClass API銆佸垹姝讳唬鐮併€丳1 鍐呴儴鏁寸悊锛涗笉鏀?Binary wire锛汿D-026 Deferred锛?| **Done** | 鈥?| [Design](./Platform/Serialization/CORE-F08_SERIALIZATION_CLEANUP_DESIGN.md) 路 [Impl](./Platform/Serialization/CORE-F08_SERIALIZATION_CLEANUP_IMPLEMENTATION.md) 路 **`feat/core`** |
| `CORE-F09` | Binary wire 鍗忚 v2锛圱ransient Ids锛汿D-028/029锛汸ersistent 濂戠害锛?| **Done** | 鈥?| [Design](./Platform/Serialization/CORE-F09_BINARY_WIRE_PROTOCOL_DESIGN.md) 路 [Impl](./Platform/Serialization/CORE-F09_BINARY_WIRE_PROTOCOL_IMPLEMENTATION.md) 路 **`feat/core`** 路 Transient only锛涘瓨鐩?Binary 鏈仛 |
| `CORE-F10` | JSON 瀛樼洏鍏煎锛堝鏉炬湭鐭ュ瓧娈?+ `$schemaVersion` meta锛?| **Done** | 鈥?| [Design](./Platform/Serialization/CORE-F10_JSON_DISK_COMPAT_DESIGN.md) 路 [Impl](./Platform/Serialization/CORE-F10_JSON_DISK_COMPAT_IMPLEMENTATION.md) 路 **`feat/core`** |
| `CORE-F11` | 灞炴€?Getter/Setter native thunk + `AssignProperty`锛圱D-026 / BUG-CORE-001锛?| **Done** | 鈥?| [Design](./Platform/Reflection/CORE-F11_PROPERTY_ACCESSOR_THUNKS_DESIGN.md) 路 [Impl](./Platform/Reflection/CORE-F11_PROPERTY_ACCESSOR_THUNKS_IMPLEMENTATION.md) 路 **`feat/core`** |
| `CORE-F12` | `ME_GENERATED_BODY()` 鍘绘棤鐢ㄧ被鍨嬪疄鍙?+ header-tool marker 褰掑睘鏀剁揣 | **Done** | 鈥?| [Design](./Platform/Reflection/CORE-F12_GENERATED_BODY_NO_ARG_DESIGN.md) 路 **`feat/core`** |
| `RND-F01` | RenderGraph锛圡anual 鍥撅紱S0鈥揝05 Done锛?| **Draft / Superseded direction** | 鈥?| [RND-F01_RENDER_GRAPH_DESIGN](./Render/RND-F01_RENDER_GRAPH_DESIGN.md) |
| `RND-F02` | Modern RHI | Done | 鈥?| [RND-F02_MODERN_RHI_DESIGN](./Render/RND-F02_MODERN_RHI_DESIGN.md) |
| `RND-F03` | Legacy RHI removal | **Done** | 鈥?| [Design](./Render/RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) |
| `RND-F04` | Modern RHI further evolution | **Done** | 鈥?| [Design](./Render/RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) |
| `RND-F05` | Vulkan backend + SPIR-V | **Done** | 鈥?| [Design](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) 路 [Impl](./Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md) |
| `RND-F06` | ForwardRenderer | **In Progress** | 鈥?| [RND-F06_FORWARD_RENDERER_DESIGN](./Render/RND-F06_FORWARD_RENDERER_DESIGN.md) 路 S01鈥揝02 Done |
| `RND-F07` | Granite-style RDG + 甯ц祫婧愭墍鏈夋潈 | **Done** *(shell)* | 鈥?| [Design](./Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) |
| `RND-F08`鈥揱F11` | Shadow 鎵€鏈夋潈 / RHI hygiene / EnvMap / DebugDrawing | **Done** | 鈥?| 瑙佸悇 Design |
| `RND-F12` | Granite RDG 璇箟鍏ㄥ鍒?| **Deferred** *(鍗敓椤?* | 鈥?| [Design](./Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md) 路 涓嶆尅褰撳墠 backlog |
| `RND-F13` | ManualRenderer锛圧eference锛?| **Done** | 鈥?| [Design](./Render/RND-F13_MANUAL_RENDERER_DESIGN.md) |
| `RND-F14` | ShadowPass UBO 瀵垮懡 | **Done** | 鈥?| [Design](./Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) |
| `RND-F16` | Sprite 2D 娓叉煋锛圲I 鍓嶇疆锛?| **Planned** | 鈥?| [Placeholder](./Render/RND-F16_SPRITE_2D_DESIGN.md) 路 鎰挎櫙锛?*涓嶉樆濉?* |
| `ED-F01` | Vulkan Editor Parity | **In Progress** *(VK 闃村奖璐ㄩ噺 **Deferred**)* | 鈥?| [Design](./Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) 路 [Impl](./Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) |
| `ED-F02` | Editor Workflow锛堟墦寮€/鍒涘缓 Scene路Material銆丼kyBox銆乂iewport銆丆omponent UI锛?| **In Progress** *(S00鈥揝02/S04 Done锛汼03/S05 浣欓噺)* | 鈥?| [Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) 路 [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md) 路 **`master`** |
| `ED-F03` | Editor Play Toolbar锛圴iewport 涓夎锛歍ab / Toolbar / 涓讳綋锛?| **Done** | 鈥?| [Design](./Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md) |
| `ED-F04` | Debug Console & Unified Command System锛圧untime 鎺у埗闈?+ Agent-friendly锛?| **In Progress** *(MVP Done)* | 鈥?| [Design](./Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md) 路 S00鈥揝10a Done 路 **S10b Deferred** 路 S07 Deferred |
| `ED-F05` | Inspector / Component UX锛堟敼鍚嶉€€鍑恒€丄dd 鍥炬爣/鎼滅储銆佺粍浠跺ご銆侀噸鎺掋€丆B 鍐呰仈閲嶅懡鍚嶏紱瑙嗗彛鍥炬爣 Deferred锛?| **Done**锛圫05 Deferred锛?| 鈥?| [Design](./Editor/ED-F05_INSPECTOR_COMPONENT_UX_DESIGN.md) 路 [Impl](./Editor/ED-F05_INSPECTOR_COMPONENT_UX_IMPLEMENTATION.md) 路 **`master`** |
| `LAUN-F01` | Engine Launcher | **Done** | 鈥?| [Design](./Platform/Launcher/LAUN-F01_ENGINE_LAUNCHER_DESIGN.md) |
| `AUD-F01` | Audio system | **Done** | 鈥?| [Design](./Platform/Audio/AUD-F01_AUDIO_SYSTEM_DESIGN.md) |
| `ANIM-F01` | Animation system | **Planned** | 鈥?| [Placeholder](./Animation/ANIM-F01_ANIMATION_SYSTEM_DESIGN.md) 路 `feat/animation` 路 worktree `minEngine-animation` |
| `UI-F01` | UI system | **Planned** | 鈥?| [Placeholder](./Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md) 路 `feat/ui` 路 worktree `minEngine-ui` 路 渚濊禆 `RND-F16` |
| `PHYS-F01` | Jolt physics bootstrap | Done | 鈥?| [Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md) |
| `PHYS-F02` | Collision + query shapes | Done | 鈥?| [Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) |
| `PHYS-F03` | Contact gameplay dispatch | Deferred | 鈥?| [Placeholder](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) |
| `PHYS-F04` | Collider 灏哄涓?Scale 瑙ｈ€?| **Done** | 鈥?| [Design](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) 路 **`master`** |
| `GP-F01` | GameplayTag锛圡anager + Container锛汦ngine 瀛愮郴缁燂紱Native 瀹忥級 | **Done** | 鈥?| [Design](./Gameplay/GP-F01_GAMEPLAY_TAG_DESIGN.md) 路 [Impl](./Gameplay/GP-F01_GAMEPLAY_TAG_IMPLEMENTATION.md) 路 **`master`** 路 `test gameplay-tags` |
| `GP-F02` | GameplayEventSystem锛圫cene 浣滅敤鍩?Component锛涙棤 Payload锛?| **Done** | 鈥?| [Design](./Gameplay/GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md) 路 [Impl](./Gameplay/GP-F02_GAMEPLAY_EVENT_SYSTEM_IMPLEMENTATION.md) 路 渚濊禆 GP-F01 路 **`master`** 路 `test gameplay-events` |

---


---

## Merge-wave ID remaps（feat/animation → master）

| 分支原 ID | master 正式 ID | 原因 |
|-----------|----------------|------|
| anim `CORE-F08` Parameter Store | **`CORE-F13`** | master `CORE-F08`–`F12` 已占用（序列化/反射/TestAccess） |
| anim `ED-F05` SM Canvas | **`ED-F06`** | master `ED-F05` = Inspector Component UX |
| anim `ED-F06` Canvas Polish | **`ED-F07`** | 顺延 |
| 设计文件名 | 暂不改 | 路径仍含历史 `CORE-F08_PARAMETER_*` / `ED-F05_STATE_MACHINE_*` / `ED-F06_ANIM_SM_*` |
| ui Hierarchy（docs CORE-F08 / Reg F12） | **`CORE-F14`** | F12–F13 已被 GENERATED_BODY / Parameter 占用 |
| ui KeepWorld（docs CORE-F09 / Reg F13） | **`CORE-F15`** | 同上 |
| ui LinearColor（`CORE-F14`） | **`CORE-F16`** | 顺延 |
| ui Hierarchy Tree（`ED-F05`） | **`ED-F08`** | master ED-F05=Inspector；F06–F07=Anim canvas |


## Vision placeholders锛堟棤鐙珛 Feature ID锛屼笉鎺掓湡锛?
鐧昏浜?[ACTIVE_WORK.md](./ACTIVE_WORK.md) 涓?[ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md)锛氬畬鏁?Gameplay **Plugins / ASC / GAS 涓婂眰**銆丯etworking / Net Game銆丳refab銆丱bject Lifetime/GC銆丷ender Sort/Batch锛堝緟鐧昏锛夈€丄gent-friendly 浣滀负**璁捐鍘熷垯**锛堣 [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md)锛夈€? 
**渚嬪锛?* `GP-F01`/`GP-F02` 涓烘彁鍓嶈惤鍦扮殑**杞婚噺鏈哄埗鍩哄簳**锛圱ag + Scene Event bus锛夛紝涓嶆槸瀹屾暣 Framework銆?
---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | |
| `TEST` | **F05** | F04 Done锛圱estAccess锛?|
| `WF` | F03 | |
| `CORE` | **F13** | F12 Done锛圙ENERATED_BODY no-arg锛夛紱F08鈥揊11 Done锛沗feat/core` |
| `ASSET` | F01 | Async / Lifetime 鎰挎櫙瑙?Capability Roadmap锛涘皻鏈櫥璁?Feature |
| `ED` | **F06** | F02 浣欓噺锛汧03 Done锛汧04 Console MVP锛?*F05 Done**锛圫05 Deferred锛?|
| `RND` | **F17** | F16 Sprite 鍗犱綅锛汧12 Deferred |
| `LAUN` | F02 | F01 Done |
| `AUD` | F02 | F01 Done |
| `ANIM` | F02 | F01 Planned锛汸rimary track 鍊欓€?|
| `UI` | F02 | F01 鍗犱綅锛沗feat/ui` |
| `PHYS` | F05 | F04 on `master` |
| `GP` | **F03** | F01 Tag / F02 Event **Done**锛堝凡鍚堝叆 `master`锛?|
| `MAT` | F01 | |

Update **Next Feature #** when you register a new row.
