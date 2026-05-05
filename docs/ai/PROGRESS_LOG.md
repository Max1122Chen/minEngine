# minEngine Progress Log (for AI)

Last updated: 2026-04-16

## Purpose

This file is an AI-oriented progress digest converted from commit messages.
It is not a full changelog. It focuses on architecture moves, rendering milestones, and known pitfalls.

## Timeline Summary

1. Initial framework stage
- Set up basic engine framework and RenderSystem foundations.
- Built simple mesh/texture support and early input handling.

2. Scene and component architecture stage
- Introduced GameObject/Component model, Level/WorldManager, Transform.
- Added render-side abstraction using SceneProxy.
- Started AssetManager and resource-oriented structure.

3. Lighting and model import stage
- Added light components and light scene proxies.
- Extended Phong shader for directional/point/spot lights.
- Added basic model import via Assimp.

4. Rendering pipeline structuring stage
- Reorganized render files (RHI, light proxies, primitive proxies).
- Added translucency pass.
- Added present pass for offscreen-to-screen output.

5. Data upload optimization stage
- Added per-frame camera UniformBuffer.
- Added separate light UBO.
- Reduced redundant light data upload frequency.

6. Shadow and stability stage (recent)
- Added directional light shadow pass and shadow map sampling.
- Fixed repeated queue growth issue (missing clear each frame).
- Fixed Texture2D release issue causing GPU memory leak.
- Fixed light proxy write-back issue causing repeated creation/leak risk.

7. Reflection modernization stage (2026-04-10 ~ 2026-04-12)
- Introduced MEReflection in parallel with legacy reflection for migration.
- Refactored reflection/serialization boundaries and startup finalization flow.
- Added better reflection finalization error logging and repaired GameObject reflection path.

8. Serialization capability expansion stage (2026-04-12 ~ 2026-04-14)
- Implemented pointer deserialization foundation in Serializer.
- Added primitive codec registry and MEEnum codec support.
- Unblocked GameObject serialization for owned inline component arrays (shared_ptr<Component>).
- Reenabled Inspector path for GameObject and component fields.

9. Asset identity and registry kickoff stage (WIP, 2026-04-16)
- Started GUID type and generation utility for asset identity.
- Introduced AssetMeta and initial in-memory asset registry shape.
- Added AssetManager scan/register flow and Texture2DResource reflection stub.

## Current Technical Focus (Inferred)

- Complete reflection/serialization migration without breaking editor workflows.
- Finalize pointer/reference semantics for MEObject relationships.
- Build asset metadata persistence (.meta read/write) and GUID lifecycle stability.
- Keep scene serialization, inspector, and resource loading behavior aligned.

## Known Pitfalls to Recheck During Refactors

- Any per-frame vector/list/map used in pipeline build path must be cleared or reused safely.
- Any GL resource wrapper must own and release native handles in destructor.
- Scene proxies or render entries must not be recreated indefinitely without ownership policy.
- Shadow pass changes (viewport/state/targets) must be restored before later passes.
- Pointer deserialization must clearly separate ownership and reference semantics.
- Serializer signature changes must be synchronized across all callsites.
- Asset scanning must avoid duplicate registration and accidental GUID regeneration.

## Entry Template (Append for each meaningful task)

### YYYY-MM-DD - Task title
- Goal:
- Main changes:
- Risks or caveats:
- Validation done:
- Next step:

### 2026-03-26 - Playground player control enhancement
- Goal:
	Add vertical flight and improve camera control experience in Playground.
- Main changes:
	Added IA_UpAndDown for E/Q vertical movement.
	Added IA_Look camera rotation logic with pitch clamp and sensitivity.
	Added wheel input support path via WindowSystem/GLFWWindowSystem/InputSystem and mapped MouseScroll in Playground.
	Fixed horizontal look direction sign to match expected user control direction.
- Risks or caveats:
	Mouse2D and MouseScroll semantics are still evolving between engine-side abstraction and gameplay-side handling.
	Future refactor may separate event-like input and state-like input more clearly.
- Validation done:
	CMake build completed successfully after control/input changes.
	Manual interaction checks performed in Playground for movement and look direction.
- Next step:
	Standardize mouse delta semantics at one layer only (engine or gameplay) to avoid double-delta bugs.

### 2026-04-06 - Scene serialization MVP pipeline wiring
- Goal:
	Wire a minimal end-to-end scene save/load loop for editor usage.
- Main changes:
	Added SceneSerializer with JSON save/load for scene name and game object transform list.
	Extended Scene object creation to support explicit object IDs for deterministic reload.
	Implemented SceneManager::CreateNewScene/LoadScene and default scene path resolution.
	Wired Editor File menu actions (New/Open/Save/Exit) to SceneManager and SceneSerializer.
	Created default editor scene on startup for immediate save/load testing.
- Risks or caveats:
	Current MVP only serializes scene name, object id, and transform; component graph is not serialized yet.
	Open Scene currently uses a fixed default path and has no file dialog.
- Validation done:
	File-level diagnostics passed for all touched scene/editor source files.
- Next step:
	Add component type registry and polymorphic component serialization/deserialization.

### 2026-04-10 - MEReflection parallel migration start
- Goal:
	Introduce a safer migration path by running new reflection flow in parallel with legacy reflection.
- Main changes:
	Added MEReflection alongside legacy reflection system.
	Prepared architecture boundaries for incremental migration instead of one-shot replacement.
- Risks or caveats:
	Dual-stack reflection can drift if registration/finalization behavior diverges.
- Validation done:
	Integrated into branch history and enabled follow-up serializer/reflection work.
- Next step:
	Consolidate registration/finalization ownership and remove duplicate paths gradually.

### 2026-04-12 - Reflection finalization reliability and GameObject fix
- Goal:
	Restore reflection correctness and unblock dependent serialization/editor features.
- Main changes:
	Added reflection finalization into engine startup flow.
	Added reflection finalization error logs to surface missing registration issues.
	Fixed GameObject-related reflection path issues and generated required reflection artifacts.
- Risks or caveats:
	Generated code order and startup timing still require strict consistency.
- Validation done:
	Reflection path recovered enough to continue pointer/serializer feature work.
- Next step:
	Continue serializer refactor with stronger pointer handling and field coverage.

### 2026-04-14 - Serializer pointer path, enum codec, inspector recovery
- Goal:
	Expand serialization coverage to component graphs and reconnect editor inspection flow.
- Main changes:
	Implemented pointer deserialization baseline in Serializer.
	Added primitive codec registry with MEEnum support.
	Successfully serialized a GameObject containing owned inline shared_ptr<Component> arrays.
	Reenabled Inspector for GameObject and component fields.
- Risks or caveats:
	Non-owned shared_ptr<MEObject> reference serialization is not finished.
	Pointer ownership/memory behavior still needs hardening in edge cases.
- Validation done:
	Commit notes confirm end-to-end success for owned component array scenario.
- Next step:
	Implement reference-mode object identity path and resolve semantics for external object references.

### 2026-04-16 - Asset metadata and GUID foundation (WIP)
- Goal:
	Prepare asset identity and registry infrastructure for future asset database workflow.
- Main changes:
	Added GUID type and GenerateGUID utility.
	Introduced AssetMeta (path/type/guid) and AssetManager in-memory registry map.
	Added AssetManager::ScanAssets and RegisterAsset initial flow.
	Added Texture2DResource and generated reflection header for resource-level metadata serialization prep.
	Refactored editor default-scene population helper and aligned serializer callsites with updated parameter flow.
- Risks or caveats:
	.meta persistence is still TODO; GUID reuse/load policy is not finalized.
	Current workspace includes uncommitted WIP changes and may still need integration fixes.
- Validation done:
	Local scope and file topology are in place; full fresh build validation has not been recorded yet for this WIP batch.
- Next step:
	Implement meta read/write + GUID reuse strategy, then run full build and editor open/save smoke test.

### 2026-04-17 - ObjectPtr probe-based GUID reference path
- Goal:
	Continue serializer evolution after ObjectManager became the central owner of runtime objects.
- Main changes:
	Added GUID reference node interfaces to Archive/JsonArchive (`BeginGuidRef`/`EndGuidRef`).
	Implemented probe-based object pointer deserialization in Serializer: try inline object node first, then GUID reference node.
	Implemented direct-Outer ownership rule in object pointer serialization: direct child uses inline serialization, otherwise writes GUID reference.
	Hooked GUID reference resolve chain in deserialization: ObjectManager first, then AssetManager fallback for known asset types.
	When deserializing inline MEObject-derived pointers, assign Class/Outer and register into ObjectManager.
- Risks or caveats:
	Current object-pointer serializer path still assumes MEObject-derived pointer property types for full ownership semantics.
	Asset fallback currently relies on known class-name mapping (`StaticMesh`/`Texture2D`) and may need extension once more resource wrapper types are reflected.
- Validation done:
	File-level diagnostics passed for touched serialization files (Archive/JsonArchive/Serializer).
- Next step:
	Add a focused round-trip case for mixed inline + GUID references (including cross-owner references) to lock behavior.

### 2026-04-18 - Scene vector ownership serialization alignment
- Goal:
	Align scene save/load flow with vector-owned GameObject data and non-persistent runtime IDs.
- Main changes:
	Switched Scene tick path to iterate m_GameObjects vector (ownership source of truth).
	Added Scene::RebuildRuntimeGameObjectIndex to compact null entries, rebuild id->pointer query map, and reassign runtime GameObject IDs after load.
	Refactored SceneSerializer to serialize/deserialize Scene directly through Serializer (sceneData payload) and removed persisted per-GameObject id fields.
	Added Scene reflection fields for sceneName and m_GameObjects in generated Scene.gen.h so direct Scene serialization includes required data.
	Kept pending object reference workflow intact: clear queue before load, deserialize scene, rebuild runtime index, resolve pending refs at load-unit end.
- Risks or caveats:
	Scene file format changed to version 2 and currently follows forward-only path (no legacy id-based scene schema compatibility).
	Pylance/Problems diagnostics for SceneSerializer still showed stale legacy entries in this session despite file content update; no legacy symbols remain in source file.
- Validation done:
	File-level diagnostics passed for Scene.h, Scene.cpp, Scene.gen.h.
	Source grep confirmed legacy id-serialization symbols were removed from SceneSerializer.cpp.
- Next step:
	Run editor-level save/load smoke test with at least one cross-GameObject reference case to verify runtime id remap and pending reference resolve behavior end-to-end.

### 2026-04-18 - Editor Scene API alignment follow-up
- Goal:
	Migrate editor-side scene/gameobject queries to the new Scene storage model and encapsulated GameObject ID accessor.
- Main changes:
	Updated Editor::GetHierarchyGameObjects to read from Scene::GetGameObjects() and sort by GameObject::GetID().
	Updated Editor::GetSelectedGameObject / RenameGameObject to use vector scan by GetID instead of old map-style find on m_GameObjects.
	Updated Editor::SyncSelectionWithScene to use Scene::GetGameObjectsById() for existence check and fallback to hierarchy list front item.
	Updated HierarchyWindow and InspectorWindow to use GameObject::GetID() instead of direct m_ID field access.
- Risks or caveats:
	Current editor selection lookup by ID is linear over scene gameobject vector when shared_ptr ownership is needed.
	Pylance diagnostics may lag in this session; source-level grep confirms old m_ID and old m_GameObjects.find usage in editor code paths were removed.
- Validation done:
	File contents verified for Editor.cpp / HierarchyWindow.h / InspectorWindow.h with new Scene/GameObject access pattern.
- Next step:
	Decide whether to introduce non-owning raw pointer query APIs for hierarchy/selection to reduce shared_ptr churn in per-frame UI code.

### 2026-04-18 - Editor non-owning raw pointer query pass
- Goal:
	Adopt non-owning raw pointer query APIs for per-frame editor scene inspection paths.
- Main changes:
	Changed Editor API signatures: GetActiveScene -> Scene*, GetHierarchyGameObjects -> vector<GameObject*>, GetSelectedGameObject -> GameObject*.
	Updated Editor.cpp callsites accordingly, while preserving Scene/SceneManager ownership via shared_ptr internally.
	Updated HierarchyWindow and InspectorWindow to consume raw pointer query results.
	Selection and rename lookups now use Scene::GetGameObjectsById() map for direct ID lookup.
- Risks or caveats:
	Returned raw pointers are non-owning and only valid while scene ownership remains stable in current frame.
	Callers must not cache returned pointers across scene reload/destruction events.
- Validation done:
	File-level diagnostics passed for Editor.h, Editor.cpp, HierarchyWindow.h, InspectorWindow.h, MainMenuWindow.h.
- Next step:
	Optionally add naming convention docs (e.g., *Raw suffix) if both owning and non-owning query APIs coexist later.

### 2026-04-18 - Reflection derived-check centralization
- Goal:
	Eliminate duplicated ad-hoc inheritance checks in serialization paths by adding a reusable reflection-level API.
- Main changes:
	Added `MEClass::IsA(const MEClass*)` as the canonical class ancestry query (self match included by default).
	Added `ReflectionSystem::IsClassSameOrDerived` and `ReflectionSystem::IsClassNameSameOrDerived` utility methods.
	Refactored Serializer to remove local `IsClassSameOrDerived` helper and use `ReflectionSystem` APIs.
	Refactored JsonArchive object/object-ptr type checks to use reflection system class-name derived matching instead of direct-child-only scans.
- Risks or caveats:
	Type-name checks now require reflected type names to be registered in `ReflectionSystem`; unresolved names fail fast.
- Validation done:
	File-level diagnostics passed for MEClass.h, Reflection.h, Serializer.cpp, JsonArchive.cpp.
	Source grep confirmed serializer-local inheritance helper was removed.
- Next step:
	Re-run scene load smoke test and confirm no "neither inline object node nor GUID reference node" error on multi-level inheritance pointer fields.

### 2026-04-18 - Resource reference GUID stabilization and scene migration
- Goal:
	Fix unresolved asset references during scene deserialization by ensuring serialized resource GUIDs match asset meta GUIDs.
- Main changes:
	Updated AssetManager static-mesh loading path to normalize cache key/path consistently.
	Updated `LoadStaticMeshByMeta` and `LoadStaticMesh` to bind loaded `StaticMesh` runtime object GUID to asset meta GUID.
	Migrated `bin/Assets/Scenes/EditorDefault.scene.json` mesh GUID nodes from stale random values to current mesh meta GUIDs.
- Risks or caveats:
	If older scene files contain random runtime GUIDs that have no corresponding `.meta` entries, they still require migration.
- Validation done:
	Rebuilt CMake targets successfully (minEngine + Editor).
	Runtime log verification shows pending reference resolve pass `resolved=5, unresolved=0`.
- Next step:
	Optionally add an automatic scene GUID migration pass for legacy files with unresolved asset GUID references.

### 2026-04-18 - ProjectManager skeleton bootstrap
- Goal:
	Start project-system architecture separation by introducing a dedicated `ProjectManager` subsystem scaffold.
- Main changes:
	Added `Runtime/Function/Framework/Project/ProjectManager.h/.cpp` with empty open/close interfaces and baseline data-model structs (`ProjectDescriptor`, `ProjectContext`, `ProjectOpenResult`).
	Added project file naming constants for new convention: `.meproject` and `.measset`.
	Added startup-scene resolution interface with engine-default fallback path placeholder.
	Registered `ProjectManager` in `RuntimeGlobalContext` startup/shutdown lifecycle.
- Risks or caveats:
	Current `OpenProject` behavior is intentionally non-functional (`NotImplemented`) to keep this iteration architecture-only.
	No runtime feature wiring to editor startup path yet.
- Validation done:
	File-level diagnostics passed for new and updated framework files.
	No build/run step executed in this task (per user request).
- Next step:
	Implement minimal project discovery/load flow and wire editor startup to `ProjectManager` scene resolution chain.

### 2026-04-18 - ProjectManager descriptor split and open-flow implementation
- Goal:
	Move project metadata into a dedicated header and replace `OpenProject` placeholder logic with real descriptor loading/validation.
- Main changes:
	Split project metadata into `Runtime/Function/Framework/Project/ProjectDescriptor.h`.
	Added `ProjectDescriptorSerializer.h/.cpp` to load/save descriptor files through the generic archive layer (`JsonArchive` read/write APIs).
	Implemented `ProjectManager::OpenProject` flow: normalize root, locate descriptor, deserialize, validate required fields, fill defaults (`Assets` / `Config`), resolve startup scene with engine-default fallback and diagnostics.
	Moved `ProjectManager::Get()` implementation to cpp, decoupling header from direct `RuntimeGlobalContext` include.
- Risks or caveats:
	Descriptor format is now strict on required fields (`schemaVersion`, `projectName`, `projectId`).
	Startup-scene fallback currently records diagnostics but does not yet emit UI-level notifications.
- Validation done:
	File-level diagnostics passed for `ProjectDescriptor.h`, `ProjectDescriptorSerializer.h/.cpp`, `ProjectManager.h/.cpp`.
	No build/run step executed in this task (per user request).
- Next step:
	Wire editor startup bootstrap to call `ProjectManager::OpenProject`, then consume `ResolveEditorStartupScenePath()` as the scene-entry source.

### 2026-05-04 - Spot/Point shadow pass MVP
- Goal:
	Add shadow rendering support for spot lights and point lights without changing the base pass sampling yet.
- Main changes:
	Added spot/point shadow requests and draw command building paths.
	Implemented spot and point shadow rendering in ShadowPass, including cube face rendering for point lights.
	Added spot 2D depth and point cube depth resource allocation in ShadowResourceManager.
	Extended OpenGL cubemap creation to respect depth texture formats.
- Risks or caveats:
	Spot/point shadow resources are single-instance MVP allocations; multiple shadow-casting lights will overwrite in this stage.
	Point light shadow range uses a fixed near/far plane; sampling path must match this later.
- Validation done:
	No build/run step executed in this task.
- Next step:
	Wire base pass sampling for spot and point shadows and pass down required parameters (shadow map index, far plane).

### 2026-05-04 - BasePass shadow sampling (spot/point)
- Goal:
	Connect spot and point shadow maps into BasePass sampling with a fixed resource limit of two each.
- Main changes:
	Added spot/point shadow sampler arrays and view-projection UBO in the Phong shader.
	Bound spot/point shadow textures in BasePass and wired shadow indices through light Params.w.
	Added spot view-projection UBO updates and point shadow far-plane data in light UBO updates.
	Made point-shadow cubemap depth linear in shadow pass for correct distance comparisons.
- Risks or caveats:
	Shadow resource maps are capped at two per light type; extra shadow-casting lights skip shadows.
	Point/spot shadow near/far planes are fixed constants; keep sampling logic consistent if they change.
- Validation done:
	No build/run step executed in this task.
- Next step:
	Run an editor scene with multiple spot/point lights to verify shadow indexing and bias tuning.
