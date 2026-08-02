# Feature Registry

Last updated: 2026-08-02 (master: luaScript + physics merge; Transform quat → CORE-F03)  
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
| `RND-F01` | RenderGraph / frame scheduling | Deferred | — | 待 Design；**不在** F02/F03 范围 |
| `RND-F02` | Modern RHI（GPU 工作模型抽象；GL 首适配） | In Progress | — | [RND-F02_MODERN_RHI_DESIGN](./Render/RND-F02_MODERN_RHI_DESIGN.md) · 开发分支 `render` |
| `RND-F03` | Vulkan RHI backend（与 GL 契约一致、分里程碑对齐） | Planned | — | 依赖 F02；Design 随 F02 稳定后补 `RND-F03_*` |
| `PHYS-F01` | Jolt physics bootstrap（PhysicsSystem、RigidBody/BoxCollider、固定步长写回、Channel/Contact、Scene::LineTrace） | Done | — | [Design](./Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md) · [Impl](./Physics/PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md) |
| `PHYS-F02` | Collision + query shapes（Sphere/Capsule collider；Scene SphereTrace/CapsuleTrace） | Done | — | [Design](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md) · [Impl](./Physics/PHYS-F02_COLLISION_QUERY_SHAPES_IMPLEMENTATION.md) |
| `PHYS-F03` | Contact gameplay dispatch（玩法接触通知） | Deferred | — | [Placeholder](./Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) · blocked on **TD-006** Delegates；正式 Design 延后 |

---

## ID allocation by domain (next free)

| DOMAIN | Next Feature # | Notes |
|--------|----------------|-------|
| `CLI` | F02 | Command-line / tools entry |
| `TEST` | F04 | Automated tests (follow-on) |
| `WF` | F03 | Workflow / docs |
| `CORE` | F04 | F01–F02 Lua Done；F03 Transform quaternion Done |
| `ASSET` | F01 | Asset pipeline extensions |
| `ED` | F01 | Editor productization (new IDs only) |
| `RND` | F04 | F01 RenderGraph (Deferred); F02–F03 active render track |
| `PHYS` | F04 | F01–F02 Done；F03 Deferred（TD-006） |
| `MAT` | F01 | Material (new IDs only; legacy Phase docs keep old names) |

Update **Next Feature #** when you register a new row.
