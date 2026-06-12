# minEngine Progress Log (for AI)

Last updated: 2026-06-11

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

### 2026-05-25 - Asset Pipeline 业务线设计草案
- Goal:
	End-to-end design for AssetManager CRUD/events, cross-platform import dialog, project file watcher, Content Browser framework (asset-workflow worktree).
- Main changes:
	Added `docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md` (data flow, R0–R2, E4, AssetWorkflow, Browser v0, implementation order P1–P7, decision table §10).
	Linked from `CONTENT_BROWSER_DESIGN.md`.
- Risks or caveats:
	Awaiting user sign-off on §10 (NFD vs Win32, watcher polling vs native, Import rename policy, Browser list scope).
- Validation done:
	Reviewed `AssetManager` / `AssetWorkflowModule` / `ProjectManager::OpenProject` scan path against design.
- Next step:
	§10 已拍板（NFD、efsw、D12、相对 AssetPath §14）→ P1 + P1b 实现。

### 2026-05-25 - P1 API 定稿（待审批）
- Goal:
	Freeze header-level P1/P1b interface before coding.
- Main changes:
	`docs/ai/Platform/ContentBrowser/ASSET_PIPELINE_P1_API.md` — AssetRegistryTypes, AssetTypeRegistry, AssetManager deltas, contracts, migration table, acceptance §9, approval checklist §10.
- Risks or caveats:
	Breaking `FindAssetMetasByType` return type; Editor Material/Scene call sites updated in same PR.
- Validation done:
	Grep call sites; path rules aligned with §14.
- Next step:
	User approves §10 checklist → implement P1.

### 2026-05-25 - Asset Pipeline P1/P1b implemented
- Goal:
	AssetTypeRegistry, type buckets, registry events, ImportAsset, project-relative AssetPath, Editor EngineDefault scan removed.
- Main changes:
	New `AssetRegistryTypes.h`, `AssetTypeRegistry.{h,cpp}`; `AssetManager` extended (Subscribe, ImportAsset, FindAssetMetasByType/ByRuntimeClass, ResolveAssetAbsolutePath).
	Loaders use absolute resolve for disk IO; Editor Material/Scene call sites updated; `MyMEProject` metas → relative `AssetPath`.
- Risks or caveats:
	RegisterAsset requires `PathRegistry` project root; legacy absolute registry keys only via FindAssetMetaByPath fallback until rescan.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded.
- Next step:
	P2 DeleteAsset/MoveAsset; then P3 NFD, P5 efsw, P6 Content Browser.

### 2026-05-25 - Asset Pipeline P2 implemented
- Goal:
	Delete/Move/Rename/Unregister registry CRUD, Moved events, ClearProjectRegistry on CloseProject, headless tests.
- Main changes:
	`AssetManager` — `DeleteAsset`, `MoveAsset`, `RenameAsset`, `UnregisterAsset`, `ClearProjectRegistry`; `ProjectManager::CloseCurrentProject` clears registry; `SceneManager::IsSceneRegistered`.
	`AssetManagerTest` + `--asset-manager-test` (temp project `Assets/_P2UnitTest/`, copies EngineDefault cube; never deletes MyMEProject assets).
- Validation done:
	`Editor.exe --asset-manager-test` exit 0 (delete, move, rename, extension guard, unregister, clear, scene unregister).
- Next step:
	P3 NFD / P4 AssetWorkflow / P5 efsw.

### 2026-05-25 - Asset Pipeline P3 FileDialog (Runtime Platform)
- Goal:
	Runtime IFileDialogService + NFD; Editor consumes via Engine; asset filters from Registry.
- Main changes:
	`Runtime/Platform/FileDialog/*`, `FileDialogService` in `Engine`; `AssetTypeRegistry::BuildFileDialogFilters`;
	Editor `GetFileDialogService()`; Tools menu **File Dialog (P3)** for Open/Save/Folder smoke.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded (single-threaded link after parallel truncate).
- Next step:
	P4 `AssetWorkflowModule::ImportAssetDialog`.

### 2026-05-26 - Asset Pipeline P5 ProjectAssetWatcher (efsw)
- Goal:
	Editor-only filesystem watcher syncing external disk changes to `AssetManager` registry.
- Main changes:
	`Third-Party/efsw` @ 1.4.0; `ProjectAssetWatcher` (queue + 400 ms debounce + main-thread `Tick`);
	`AssetManager::SuppressExternalSyncScope`; Editor `OpenProject`/`CloseProject`/`Run` lifecycle;
	bulk fallback `ScanAssets`; P5 API doc status → 已实现.
- Validation done:
	`cmake --build minEngine/build --target Editor -j 1` succeeded.
- Next step:
	Manual P5 acceptance (copy/delete in Explorer, Import no storm); P5.1 `Reimported`; P6 Content Browser.

### 2026-05-26 - Asset Pipeline P6 Content Browser
- Goal:
	Content Browser module: directory tree, registered asset list, selection, Inspector bridge, Open/Delete/Import.
- Main changes:
	`ContentBrowserModule`, `AssetTreeModel`, `ContentBrowserWindow`; `FindAssetMetasUnderDirectory`;
	`AssetWorkflowModule` selection/Delete/InspectorSource; `InspectorWindow` focus patch.
- Risks or caveats:
	**Browser UI 展示需后期再设计**（当前裸 ImGui 线框；主题/图标/缩略图待 Appearance merge 后 P6.1）。
- Validation done:
	`cmake --build` OK; user confirmed Content Browser visible and core flows work.
- Next step:
	P7 default Dock + menu integration; P6.1 Browser visual design after Appearance merge; P5.1 Reimported optional.

### 2026-05-26 - Editor / Runtime 文件分层迁移
- Goal:
	Align Editor SubEditor/Inspector layout and consolidate Runtime asset loaders under `Resource/Loaders/`.
- Main changes:
	Editor: `Services/Inspector/InspectorModule`; `SubEditor/Material|Scene` + viewport clients; plan `docs/ai/Editor/EDITOR_FILE_LAYOUT_MIGRATION.md`.
	Runtime: all `*Loader` → `Runtime/Resource/Loaders/`; merge `MaterialAssetLoader` into `MaterialLoader::Load`; new `ShaderLoader`; `TextureCubeLoader` unchanged in `Render/`.
- Risks or caveats:
	`MaterialEditor::OpenSession` may still double-compile after `LoadAsset<Material>` (pre-existing).
- Validation done:
	`cmake --build minEngine/build --target minEngine Editor`; `--asset-manager-test` / `--material-ir-test` exit 0.
- Next step:
	Editor 目视 spot-check; optional `MaterialEditor` dedupe compile.

### 2026-05-25 - Seed EngineDefault BasicShapes into MyMEProject
- Goal:
	Restore scene mesh GUID refs after stopping EngineDefault Registry scan.
- Main changes:
	Copied 7 meshes to `MyMEProject/Assets/Meshes/BasicShapes/` with relative `.meta` (GUID preserved); script `scripts/seed_basic_shapes_to_project.py`.
- Validation done:
	`cube`/`plane` GUIDs match `default.mescene` and `test.mescene` mesh references.
- Next step:
	Re-open project in Editor to `ScanAssets` register new paths (or rely on existing meta on disk).

### 2026-05-25 - Asset Pipeline 设计拍板 + 相对路径策略
- Goal:
	Freeze §10 decisions; resolve Engine vs Project asset roots and meta relative paths.
- Main changes:
	`ASSET_PIPELINE_DESIGN.md` → Status 已拍板; D8=NFD, D9=efsw, D12=registered-only; new §14 dual-root + relative `AssetPath`.
- Risks or caveats:
	P1b migration of existing absolute metas; remove Editor scan of EngineDefault from Registry.
- Validation done:
	User confirmed defaults + path strategy discussion.
- Next step:
	P1 code slice (type bucket + events + ImportAsset); P1b relative path + stop EngineDefault Registry scan.

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

### 2026-05-08 - Material IR MVP pipeline bootstrap
- Goal:
	Stand up a minimal MaterialEdGraph -> MIR -> GLSL flow with a startup test dump.
- Main changes:
	Added MIR graph/value/node data structures and literal value support for float/vector constants.
	Implemented MIRBuilder with caching, binary ops, texture parameter, and texture sampling support.
	Expanded material node defs (constant2, multiply, texture param/sample) and wired Add/Multiply builds.
	Extended GLSL compiler with texture uniform tracking and texture() sampling emission.
	Added MaterialIR test helpers and editor startup logging for IR dump and GLSL output.
	Fixed editor graph pin ownership to allow graph connections in tests.
- Risks or caveats:
	No constant folding, dead code elimination, or stage separation yet; scalar-only type promotion.
	Texture sampling uses a simple sampler2D uniform and constant UVs in the test graph.
- Validation done:
	Not run (log-based test added; no runtime shader compile executed).
- Next step:
	Add basic diagnostics surfaced in MaterialCompileResult and extend node set (lerp, params).

### 2026-05-08 - MaterialOutput Albedo wiring
- Goal:
	Force compilation through a MaterialOutput node and map Albedo to MIR/GLSL output.
- Main changes:
	Added MaterialOutput node and wired Albedo into MIRGraph outputs.
	Enforced compile entry to be MaterialOutput only and removed arbitrary node compile path.
	Updated MVP test graph to end in MaterialOutput and compile through that entry.
	GLSL output now names Albedo explicitly for the final fragment color.
- Risks or caveats:
	Only Albedo is supported; other material properties are ignored.
	Missing MaterialOutput now hard-fails compilation.
- Validation done:
	Not run (startup log is available when the editor launches).
- Next step:
	Add Emissive/Opacity outputs or wire defaults for a fuller unlit MVP.

### 2026-05-19 - Material IR P8 texture and scalar parameters
- Goal:
	Compile texture sampling and scalar uniforms through MIR to GLSL (minimal P8).
- Main changes:
	Added MIR instructions ExternalInput, TextureObject, TextureRead, UniformParameter.
	MIREmitter: ExternalInput(TexCoord0), TextureObject, TextureSample, UniformScalar; Cast float4->float3 via subscripts.
	MIRToGLSLTranslator: shader preamble (sampler2D, in vec2 v_TexCoord0, uniform float), texture() lowering.
	Graph nodes: TextureCoordinate, TextureObject, TextureSample (RGBA+RGB outputs), ScalarParameter.
	Smoke test uses texture->Albedo, uniform->Metallic, constant Emissive.
- Validation done:
	Editor.exe --material-ir-test exit 0.
- Next step:
	S2 cross-stage TexCoords (UE-style ExternalInput -> MaterialParameters.TexCoords), then runtime bridge / P9 DefaultLit.

### 2026-05-19 - Material compile S0+S1 (multi-stage contract + FragmentMaterialInputs)
- Goal:
	UE-aligned compile contract: per-stage MIR lowering, ShadingModel assembler, FragmentMaterialInputs naming.
- Main changes:
	MaterialCompileTypes.h: MaterialCompileEnvironment, MaterialCompiledShader (Stages, FullVertex/FragmentShader).
	MaterialCompiler::Compile(graph, translator, env); MaterialShaderAssembler + UnlitShadingModel.
	MIRToGLSLTranslator lowers Vertex+Fragment bodies; FragColor only in Unlit assembler.
	FragmentMaterialInputs.Albedo etc.; MP_WorldPositionOffset placeholder + MaterialShadingPropertyCount.
	SetMaterialOutput registers outputs per MaterialPropertyEvaluatesInStage (UE-style).
	Removed MaterialCompileResult; smoke tests assert stage body + full shaders.
- Validation done:
	cmake build minEngine + Editor; Editor.exe --material-ir-test exit 0.
- Next step:
	Runtime bridge (compile to Shader + viewport) or DefaultLit shading model.

### 2026-05-19 - Material compile S2 UE-style MaterialParameters.TexCoords
- Goal:
	Cross-stage UV like UE: ExternalInput lowers to MaterialParameters.TexCoords[N]; assembler fills/interpolates.
- Main changes:
	MaterialShaderParameters.h: TexCoords access + v_MaterialTexCoordN varying names.
	Unlit assembler: vertex sets MaterialParameters from a_TexCoord, fragment restores from varying.
	MIRToGLSLTranslator: texture() uses MaterialParameters.TexCoords[0] (not v_TexCoord0 in material body).
	MaterialIRTest: logs full shaders; writes Saved/Materials/GeneratedVertex.glsl and GeneratedFragment.glsl.
- Validation done:
	Editor.exe --material-ir-test exit 0.

### Material MIR – roadmap snapshot (Path A, for planning)
- **P9 (shading):** Replace unlit `FragColor = vec4(Albedo + Emissive, Opacity)` with minimal lit shading that consumes Metallic/Roughness (minimal metallic-roughness BRDF or interim Blinn-Phong); extend smoke asserts for lighting-related GLSL.
- **Runtime bridge (often before or overlapping P9):** Vertex stage `v_TexCoord0` from mesh UV; compile `MaterialEdGraph` → `Shader` at load/startup; `Material` asset holds graph + compiled shader; BasePass binds `u_TextureN` / scalar uniforms from asset; assign material on scene mesh — **first viewport-visible custom material without graph UI**.
- **Editor graph UI (parallel track):** Visual material editor, pin wiring, recompile-on-save, preview mesh — **“真正在编辑器里改图看效果”** depends on this plus runtime bridge.
- **P10 (polish):** Vertex/custom interpolants beyond UV, `TrySimplifyOperator`, boolN Select, `Step_Finalize`, float2/float4 expansion, etc.

## Deferred Reminders (for future sessions)

### 2026-05-19 - Material R1: C1a vertex transform + BasePass graph branch
- Goal:
	Viewport-visible compiled graph unlit materials (R1).
- Main changes:
	Unlit assembler: PerFrameData + u_Model + ViewProj * u_Model * vec4(a_Position,1).
	Material: m_bUsesCompiledGraph, m_GraphTextureSlots, m_GraphScalarParams, BindCompiledGraph().
	BasePass: legacy Phong vs compiled graph binding split.
	Editor smoke scene: graph binding + SyncSceneToRenderPipeline.
	MaterialIRTest: vertex transform assertions.
- Validation done:
	cmake build Editor; --material-ir-test exit 0.
- Next step:
	R2 polish / R3 or visual confirm in editor viewport.

### 2026-05-19 - Shader asset API + editor material smoke scene
- Goal:
	Converge file→GPU shader on Shader class; editor startup active scene with MIR smoke material quad.
- Main changes:
	Removed ShaderSourceIO; Shader.cpp owns ReadSourceFile, CompileFromFiles, CreateFromSource, TryCompileSourcesOnGpu, EngineShaderPath.
	MaterialSmokeGraph shared by MaterialIRTest and EditorDefaultScene.
	SetEditorMaterialIRSmokeActiveScene() after OpenProject; quad + camera + light.
	Fixed GameObject::AddComponent return type (return newComponentBase).
	Reverted Playground shader changes (legacy, BUILD_PLAYGROUND OFF).
- Validation done:
	cmake build Editor; --material-ir-test exit 0.

### 2026-05-19 - Material R0: RHIShader source compile + GPU test
- Goal:
	R0: RHIShader accepts GLSL source strings; file IO at AssetManager; GPU compile smoke test.
- Main changes:
	OpenGLShader(vertexSource, fragmentSource) with IsValid/GetCompileLog; CreateRHIShader returns nullptr on failure.
	ShaderSourceIO: ReadShaderSourceFile, CreateRHIShaderFromFiles, TryCompileShaderSourcesOnGpu, EngineShaderPath.
	AssetManager/PresentPass/ShadowPass/RenderPipeline/Playground use file read + source API (no paths in RHI).
	MaterialIRTest: headless GL context when needed; GPU compile/link assert on smoke shaders.
- Validation done:
	cmake build Editor; Editor.exe --material-ir-test exit 0 (includes GPU compile PASSED).
- Next step:
	R1 C1a Unlit vertex PerFrameData + BasePass graph-unlit branch.

### 2026-05-19 - Material runtime bridge checklist
- Goal:
	Document GPU/viewport integration path after S0–S2 compile pipeline.
- Main changes:
	Added docs/ai/MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md (layer checklist A–E, risks R1–R7, phases R0–R4, PR split).
- Next step:
	User confirm C1a (Unlit VS + PerFrameData/u_Model); implement R0 GPU compile test then R1 BasePass branch.

### Material compiler – shader formatting (deferred)
- **Indentation:** Generated GLSL stage body / main blocks use inconsistent leading whitespace; normalize in Assembler or MIRGLSLPrinter when convenient (not blocking compile or tests).

### Material IR – cross-stage modeling (locked for S2, UE-aligned)
- **Decision (2026-05-19):** No separate MaterialInterpolator IR. Default UV = `ExternalInput(TexCoordN)` lowered to `MaterialParameters.TexCoords[N]`; vertex pass-through + varying owned by Assembler/模板. User-modified UV later via `MP_CustomizedUVs*` (vertex MaterialProperty), like UE.
- **S2 scope:** Replace translator `v_TexCoord0` with Parameters naming; vert fills TexCoords from `a_TexCoord`.

### Material IR – texture sampling variants (post-P8)
- **When to surface:** After runtime can display at least one compiled material with `texture()` in the viewport (see P9 / runtime-bridge milestones below), or when a graph needs non-default mip/grad behavior.
- **What (incremental, UE-aligned):**
  - `TextureSampleLevel` → GLSL `textureLod` (+ mip level input on node)
  - `TextureSampleBias` → `texture(..., bias)`
  - `TextureSampleGrad` → `textureGrad` (+ ddx/ddy inputs)
  - `TextureGather`, Cube/Volume/Array object types (later)
  - Sampler state (wrap/filter) separate from texture asset
  - UE-style stage switch (hardware vs analytic gradient path) — only if needed
- **Already supported (do not re-implement):** multiple textures via `TextureSlotIndex` (`u_Texture0`, `u_Texture1`, …); single 2D path `MIRTextureRead` → `texture(sampler2D, vec2)`.
- **Why deferred (2026-05-19):** P8 goal was one end-to-end 2D sample path + uniforms; more modes add MIR/GLSL/node/test surface before Editor/GPU binding exists.
- **Suggested order:** multi-slot smoke → Level → Bias/Grad → Gather / exotic types → sampler metadata.

### Material editor – vector node pin expansion (UE-style)
- **Done (E4, 2026-05-19):** `MaterialGraphNodeDef_Constant3` 增加 Value/R/G/B 四个 output；`BuildIR` 用 `SubscriptChannel`；编辑器多引脚 UI 已随 `m_Outputs` 显示。
- **Still deferred:** `MakeFloat3` 等其它向量节点多引脚；见 `MATERIAL_EDITOR_PLAN.md` 后续迭代。

### Material asset file round-trip (Instanced graph)
- **2026-05-21:** `EditorGraph` / `EditorGraphNode` → `MEObject`; `MaterialEdGraph` / `MaterialEdGraphNode` inherit; `Material::m_Graph` → `shared_ptr` + `ME_PROPERTY(Instanced)`.
- **Test:** `RunMaterialAssetSerializationTests()` writes `%TEMP%/minengine_material_asset_roundtrip.memtl`, `ToFile` → `FromFile` → `FinalizeGraphAfterLoad`; checks inline JSON types, editor fields, Metallic link, Outer chain.
- **CLI:** `Editor.exe --material-serialize-test` (serialize only); `--material-ir-test` includes file round-trip at end.

### Material system Phase 1 design (2026-05-22)
- **Plan:** `docs/ai/MATERIAL_SYSTEM_PHASE1.md` — P1.1 Capability+Blend, P1.2 Normal, P1.3 AO, P1.4 nodes; Translucent deferred to Phase 2.
- **Next implement:** P1.1 recommended first.

### Bug: MATERIAL-001 Constant3 → Normal GLSL lowering crash (2026-05-23) — **Fixed**
- **Doc:** `docs/ai/bugs/MATERIAL-001-constant3-normal-glsl-lowering.md`；管线审查 `docs/ai/MATERIAL_PIPELINE_REVIEW.md`
- **Fix:** live-reachability `NumUsers`；`Constant3`/`TextureSample` 按需子 output；GLSL lowering diagnostic + foldable multi-use inline；`VerifyConstant3ToNormalBlinnPhong` in `--material-ir-test`.
- **Also:** `ME_ASSERT` 现输出 message 到 stderr。

### Material system Phase 3 accepted + Phase 4 IBL design (2026-05-23)
- **P3 done:** `MaterialShadingModel::PBR`、GGX、`MaterialPBR.glslinc`、`PBR.*.template`、ComponentMask、TextureSample.R、BlinnPhong Roughness；`VerifyPBRWorkflow`；用户目视 OK。
- **Docs:** [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) checklist ✅；[MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) **IBL only**（split-sum、三贴图 + BRDF LUT）。
- **Deferred:** Parallax、WPO、编辑器 Undo、可选 `MaterialIRSmoke_PBR.memtl`。
- **Next:** 用户更高优先级非材质工作；实现 P4 时从 P4.1 环境 cubemap 加载 + Pass 绑定开始。

### Material system P2.3 Normal map workflow (2026-05-23)
- **Done:** `MaterialGraphNodeDef_NormalUnpack`（`rgb*2-1`）；调色板；`VerifyNormalMapWorkflow`。
- **Editor recipe:** TexCoord → TextureSample(Normal) → NormalUnpack → MaterialOutput.Normal（见 PHASE2 §5.1）。
- **Next:** 目视 + 可选 `MaterialIRSmoke_NormalMap.memtl` 金样例。

### Material system Phase 4 IBL — closed (2026-05-23)
- **P4.1–P4.4:** `EngineIBLEnvironment` + unit 4–6 bind（`pipeline` 修复）；`MaterialIBL.glslinc` / `CalcIndirectPBR`；移除常数 ambient；`u_EnvIntensity`。
- **R2:** `EnvMapCapture` HDR → mipmapped cubemap；`brdf_lut.png` 或 `BrdfLutGenerator`。
- **Test:** `--material-ir-test`（PBR IBL shader、环境 init、missing-assets fallback）。
- **Doc:** [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) §5 目视指南；IBL README。
- **Deferred:** irradiance 卷积、专用 prefilter pass、skybox、Translucent IBL。

### Material system P2.2 Tangent + TBN (2026-05-23)
- **Done:** `Vertex` + `a_Tangent`（Assimp `CalcTangentSpace` + fallback）；`UsesTangentFrame`；`MaterialTangentFrame.glslinc`；BlinnPhong TBN 路径；默认 Normal TSN `(0,0,1)`。
- **Next:** P2.2 目视；P2.3 `NormalUnpack` + 法线贴图节点。

### IBL load timing owned by RenderSystem (2026-05-23)
- **Change:** Removed `EngineIBLEnvironment::Initialize(rhi, "")` from `RenderPipeline::Initialize`; single load via `RenderSystem::LoadEngineRenderingAssets()` after `PathRegistry` in `Engine::Initialize`.
- **Removed:** `PathRegistry::ReloadEngineDependentSystems`; Editor duplicate config/IBL bootstrap.

### Platform M0 PathRegistry + engine startup (2026-05-23)
- **Code:** `Runtime/Core/Paths/PathRegistry` — discover `EngineConfig.meconfig` (cwd, parent walk, CLI, env); relative `EngineDefaultAssetsRoot`; `ProjectManager` sets `ProjectContent`.
- **Config:** `EngineConfig.meconfig` → `"Assets/EngineDefault"` relative to engine root (config file directory).
- **CLI:** `--engine-config=`, `--engine-root=`; env `MINENGINE_ENGINE_CONFIG`, `MINENGINE_ENGINE_ROOT`.
- **Validation:** build Editor; `Editor.exe --material-ir-test` from `minEngine/bin`.

### docs/ai reorg + Platform design drafts (2026-05-23)
- **Layout:** Material/Render docs → `docs/ai/Render/Material/`；`RENDER_REFACTOR`、`RESOURCE_PIPELINE` → `docs/ai/Render/`；Editor 视口 → `docs/ai/Editor/`；新增 `Platform/`、`README.md`、`.cursor/rules/docs-ai-layout.mdc`。
- **Design:** [PLATFORM_ROADMAP.md](./Platform/PLATFORM_ROADMAP.md)、[ENGINE_STARTUP_DESIGN.md](./Platform/Startup/ENGINE_STARTUP_DESIGN.md)、[MEMORY_MANAGEMENT_DESIGN.md](./Platform/MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md)（启动 M0 → **原地重构 ObjectManager** M1/M2）。
- **Direction:** UE-like；反射/Lua/Content Browser/Undo 排在平台线之后。

### Material system Phase 5 P5.3 Skybox (2026-05-23)
- **Done:** `SkyBoxComponent` + `SkyBoxSceneProxy` + `SkyBoxPass`（`background.*`）；`RenderPipeline` Clear → SkyBox → BasePass；`SceneDrawFlags::EnableSkyBox`；编辑器场景视口启用 flag；`test.mescene` SkyBox GO。
- **Fix:** `SceneEditingViewportClient` 未传 `EnableSkyBox` 导致天空不绘制。
- **Validation:** Editor `test` 场景目视 OK；`cmake --build Editor`；`--material-ir-test`（与 P5.1–P5.2 同批）。
- **Docs:** [MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md) §6.7 勾选；§10 6a–6c/7 ✅。

### Material system P2.1 Translucent (2026-05-23)
- **Done:** `MaterialBlendMode::Translucent`；`Material::IsTranslucent()`；Capability Opacity；Details **Translucent**；`--material-ir-test`。
- **Fix:** `TranslucencyPass` 与 `BasePass` 一致绑定 `LightsData` + shadow（BlinnPhong Translucent 不再全黑）。
- **Next:** P2.1 目视验收；P2.2 切线 + TBN。

### Material system Phase 1 accepted (2026-05-23)
- **Status:** P1.1–P1.4 目视验收通过；[MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md) checklist 已全部勾选。
- **Automation:** `--material-ir-test`（Unlit/BlinnPhong、Constant3→Normal、IfThenElse、Texture 双属性、divide poison、capability struct）。
- **Next:** **Phase 2.1** — `MaterialBlendMode::Translucent` + `IsTranslucent()` + `TranslucencyPass`（见 [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md) §3）。

### Material system P1.2–P1.4 implemented (2026-05-22)
- **Done:** `MP_Normal` + `EI_WorldNormal` 默认、`MP_AO`、BlinnPhong 模板用 `FragmentMaterialInputs.Normal/AO`；`IsPropertyEmittedAtCompile`（Unlit 不生成 Hidden 属性）；节点 Lerp/Subtract/Divide/Min/Max 可创建 + Lerp `Alpha` 类型；`MaterialIRSmoke.memtl` MaterialOutput pin 顺序迁移；`--material-ir-test` Unlit + BlinnPhong GPU 双路径。

### Material system P1.1 + Phase 2 design (2026-05-22)
- **Done:** P1.1 `MaterialBlendMode` (Opaque/Masked), `MaterialCapabilityUtil`, editor Blend Mode, Masked `discard`, golden `MaterialIRSmoke.memtl` + `m_BlendMode`, on-disk / IR smoke 字段校验。
- **Plan:** `docs/ai/MATERIAL_SYSTEM_PHASE2.md` — P2.1 Translucent+Pass, P2.2 TBN+tangent, P2.3 normal map; Masked 不再重复。

### Material system Phase 0 implemented (2026-05-22)
- **P0-A:** Preview camera `eye(1.15, 0.8, 1.15)` in `MaterialPreviewViewport`.
- **P0-B:** `MaterialValueType` + `MaterialValueTypeUtil`；`MaterialEdGraph::CanConnectPins`；Editor `TryConnectPins` 拒绝非法连线；`ValidateMaterialAsset` 校验；`--material-ir-test` 增加 pin 测试 + `MaterialIRTestObjectManagerScope`（friend `ObjectManager`）。
- **Next:** Phase 1 详细设计（Normal、Capability、少量节点）；`MaterialIRSmoke.memtl` Editor 目视回归。

### Material system roadmap — Phase 0 spec (2026-05-22)
- **Plan:** `docs/ai/MATERIAL_SYSTEM_ROADMAP.md`

### Material Editor E0–E4 plan sign-off (2026-05-22)
- **Plan:** `MATERIAL_EDITOR_PLAN.md` 验收勾选已全部完成（E0–E4）；用户目视 + `--material-ir-test` 确认。
- **Deferred（计划外）:** 画布右键 Palette；独立 `Material*View` 分层；Undo/Command 队列；Content Browser 双击打开。

### Material Editor E0–E2 — UI mode + Preview + node graph (2026-05-19 ~ 2026-05-22)
- **EditorUIMode:** `SceneEditing` ↔ `MaterialEditing`；`EditorWindowSuite`（Shared / Scene / Material）；切换时重建 Dock。
- **Material 套件：** `MaterialGraphWindow`（右，Picker/Compile/Save + node-editor 画布）、`MaterialPreviewWindow`、`MaterialDetailsWindow`。
- **MaterialEditor：** Session/命令中枢（非 Window）；`OpenSession` / `Save` / `Compile` / `NotifyGraphChanged`；`InvalidateGraphCanvas()` 通知图窗刷新。
- **E1.5：** `MaterialEditorPreview` 拥有预览世界；`MaterialPreviewViewportClient` 仅 resize + Submit；ImGui 在 `*Window` 内（无独立 `*View` 层）。
- **E2（已验收）：** imgui-node-editor；`MaterialGraphIds`（Node/Pin/Link 分 tag，避免与 NodeId 冲突）；`MaterialGraphNodeRegistry`；连线/断线 → `ConnectPins` / `DisconnectInput`；全零坐标自动网格布局；仅 Invalidate 时 `SetNodePosition`（可拖节点）；`BeginCreate` 失败也须 `EndCreate`。
- **渲染：** Scene 模式主视口 Submit；Material 模式仅 `material_editor_preview` Submit。
- **本地增量（未全部入库时以工作区为准）：** `Reflection::GetDerivedClasses` + `MEClass::IsA<T>()`（为 NodeDef 注册表/调色板铺路）；更多 `MaterialGraphNodeDef_*` 反射注册；`Runtime/Core/Hash/Hash.h`。
- **E3（2026-05-22）：** E3.1 Details Combo 加节点；E3.2 `Registry::DrawNode` 门面；E3.3 `MaterialNodeDefPropertyDrawer` 反射；E3.4 `RemoveNode` + 编辑器 Delete 节点（禁止删 MaterialOutput）。
- **E4（2026-05-19）：** `MaterialCompileDiagnosticsDrawer`；Compile debounce 0.3s + `MaterialEditor::Tick`；`Constant3` R/G/B 多输出引脚；退出 Material 模式时 `RemoveViewportClient` + Preview 仅 Material 模式 Submit。

### Material Editor plan + imgui-node-editor vendoring (2026-05-19)
- **Plan:** `docs/ai/MATERIAL_EDITOR_PLAN.md` — E0–E4；窗口布局 **左 Preview+Details / 右节点图**。
- **Third-Party:** `minEngine/Third-Party/imgui-node-editor/`（ImGui 1.92 `imgui_extra_math` patch）。

### Golden MaterialIRSmoke + Editor default scene
- **2026-05-21:** `MyMEProject/Assets/Materials/MaterialIRSmoke.memtl` (+ `.meta`) committed as golden IR smoke graph asset.
- **Editor:** `PopulateEditorDefaultScene` loads via `AssetManager::LoadAsset<Material>` then `ApplyMaterialIRSmokeRuntimeDefaults` (white BaseColor texture).
- **Tests:** serialize round-trip also verifies golden `.memtl` deserializes + `FinalizeGraphAfterLoad`.

### Material asset texture refs + test cleanup
- **2026-05-21:** `TextureObject.DefaultTexture` serializes as texture asset `$guid` (`BaseColorWhite.png`); removed `ApplyMaterialIRSmokeRuntimeDefaults`.
- **Deprecated:** `Simple.memtl` (legacy MaterialResource JSON); `test.mescene` / `default.mescene` reference `MaterialIRSmoke` GUID.
- **Tests:** removed `MaterialAssetSerializationTest` and `--material-serialize-test`; `MaterialIRTest` keeps MIR compile/GPU smoke only (asset tests TBD).

### 2026-05-26 - Platform ROADMAP §8 完成情况总览
- Goal:
	Sync PLATFORM_ROADMAP with design docs and repo state (P0–P5, P2 submodules, Material maintenance).
- Main changes:
	`PLATFORM_ROADMAP.md` §8 per-module done/deferred tables + summary; §2/§3/§5 updated (P0/P1 no longer “全后置”).
- Next step:
	E1 → P7 per §8 总结顺序.

### 2026-05-27 - ROADMAP 新任务安排（滚动）落盘
- Goal:
	Record today’s incremental task order aligned with ROADMAP, while leaving room for additional tasks.
- Main changes:
	`PLATFORM_ROADMAP.md` 新增 §10（A/B/C 三条主线）；新增 `docs/ai/Editor/EDITOR_TASK_ROLLOUT_2026-05-27.md`（推进切片 S1–S6）。
- Next step:
	按 S1 开始：右键 action 统一抽象 + Content Browser 先接入。

### 2026-05-27 - M1 收口：Reveal/Rename 暂缓（设计）
- Goal:
	Align design after trial: drop non-cross-platform Reveal and modal CB Rename.
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` §6.1、§12、§13、§15.4；rollout S1b 代码清理项。
- Next step:
	M2 Hierarchy + Inspector.

### 2026-05-27 - CB Import/Delete 全量刷新问题记录
- Doc:
	`docs/ai/Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md`；`CONTENT_BROWSER_DESIGN.md` 链接。
- Next:
	后续优化 AssetTreeModel 增量刷新 / 通知合并。

### 2026-05-27 - M1 代码清理（Reveal/Rename 移除）
- Main changes:
	删 `EditorPlatformShell`；CB Action 仅 Delete/Import/Refresh；移除 AssetWorkflow/ContentBrowser Rename 模态链。
- Next step:
	M2.

### 2026-05-27 - 右键菜单 M1（Content Browser）
- Goal:
	Scheme A visibility + CB context menu actions.
- Main changes:
	`IsVisibleInMenu`；`ContentBrowserMenuContext`；五 CB Action；CB 树/Tile/空白右键；`ImportAssetDialog(rel)`；`EditorPlatformShell` Reveal。
- Next step:
	M2 Hierarchy + Inspector.

### 2026-05-27 - 计划对齐（M0 Done / M1 待批）
- Goal:
	勾选 S0；设计 §15 实现对照；M1 审批项（菜单灰显 A/B）。
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` §13–§15；`EDITOR_TASK_ROLLOUT` 子任务表；`PLATFORM_ROADMAP` §10 备注。
- Next step:
	用户审批 §15.3 → M1 ContentBrowser。

### 2026-05-28 - TEST-F02 doctest, minEngineTests, Tests/ layout
- Goal:
	Move headless suites out of Runtime; vendored doctest; primary entry minEngineTests.exe.
- Main changes:
	`minEngine/Tests/` + `Third-Party/doctest/`; `Editor.exe test` forwards to minEngineTests;
	`scripts/verify.ps1` uses minEngineTests; one `TEST_CASE` per suite with CHECK.
- Validation done:
	`minEngineTests.exe test smoke` exit 0; `verify.ps1` exit 0.
- Next step:
	Optional: split large suites into finer doctest cases; per-suite runtime reset (reflection ordering).

### 2026-05-28 - TEST-F01 unified test runner (S01–S05)
- Goal:
	TestRunner + registry + TestContext; all five suites via `Editor.exe test`; verify.ps1.
- Main changes:
	`Runtime/Test/*`; `main.h` legacy pre-parse + TestRunner dispatch; removed `ShouldRun*` from `*Test.cpp`.
	`scripts/verify.ps1`; smoke order puts reflection-function first (in-process isolation).
- Validation done:
	`cmake --build minEngine/build --target Editor`; `test smoke`/`test material-ir`/legacy flags; `.\scripts\verify.ps1`.
- Next step:
	TEST-F02 — doctest + `minEngine/Tests/` + `minEngineTests.exe`.

### 2026-05-28 - TEST-F03 suite slim-down complete (S00–S06)
- Goal:
	Doctest cases per suite; fixture B; smoke/full tags; faster verify smoke.
- Main changes:
	All five suites split; `EngineReflectionFixture` / `EngineTestFixture`; Material IR smoke vs full;
	reflection meta/invoke smoke, types/static/ref full; active docs CLI strings updated.
- Validation done:
	`minEngineTests test smoke` exit 0.
- Next step:
	Optional: further split reflection/material cases; enable C3 FillOut when registered.

### 2026-05-28 - TEST-F03 S00–S01 fixture B + object-manager split
- Goal:
	Implement fixture B smoke order; split object-manager into doctest cases.
- Main changes:
	`EngineReflectionFixture` / `EngineTestFixture`; `DoctestSuiteRunner::RunSuiteForContext`;
	smoke order in `TestSuiteRegistry`; `ObjectManagerTest` → 3 `TEST_CASE`s; reflection A6/A7/B6 skip when already Ready.
- Validation done:
	`minEngineTests test object-manager`, `test reflection-function`, `test smoke` exit 0.
- Next step:
	F03-S02 serialization-archive split.

### 2026-05-28 - TEST-F03 suite slim-down plan (fixture B)
- Goal:
	Per-suite doctest cases; central reflection init; faster smoke; dependency-ordered registry.
- Main changes:
	`Platform/Test/TEST_F03_SUITE_SLIM_PLAN.md`; Registry `TEST-F03` Planned; INFRASTRUCTURE/TEST_UNIFIED/BOOTSTRAP links.
- Validation done:
	Docs-only; implementation starts at F03-S00 (fixture + smoke order).
- Next step:
	F03-S00 validation gate, then F03-S01 object-manager split.

### 2026-05-28 - Legacy headless test flags removed (commit)
- Goal:
	Drop deprecated `--*-test` argv; align infra docs with minEngineTests primary UX.
- Main changes:
	`TestRunner`, `ApplicationCommandLine`, `main.h`, `TestMain`; TECH_DEBT TD-001–003 Done.
- Validation done:
	`minEngineTests.exe test smoke` exit 0.
- Next step:
	TEST-F03 planning (see entry above).

### 2026-05-28 - TEST-F01/F02 unified test runner design
- Goal:
	Design engine TestRunner (self-built) + doctest in F02; smoke/full tables; verify.ps1 path.
- Main changes:
	`Platform/Test/TEST_UNIFIED_DESIGN.md`, `TEST_F01_IMPLEMENTATION.md`, `TEST_F02_LAYOUT_MIGRATION.md`;
	Registry `TEST-F01`/`TEST-F02`; INFRASTRUCTURE_ROADMAP M1–M2 Done, M3–M4 planned.
- Validation done:
	Docs-only; user approved doctest + Editor-then-minEngineTests exe strategy.
- Next step:
	TEST-F01-S01 implementation.

### 2026-05-28 - CLI-F01 S01–S03 unified command-line parse + dispatch
- Goal:
	Introduce CLI11-backed `ApplicationCommandLine`; single `main.h` dispatch; migrate Material IR test to `test material-ir`.
- Main changes:
	`Third-Party/CLI11/CLI11.hpp` (v2.4.2); `Runtime/Core/CLI/*` (`CommandLineResult`, `ApplicationCommandLine`).
	`main.h` — parse first; `test material-ir` + legacy `--material-ir-test` alias (warn); other `--*-test` flags unchanged until TEST-F01.
	`PathRegistry::LoadEngineConfiguration(CommandLineResult)`; `Editor` project path from parsed result (removed argv scan loop).
- Validation done:
	`cmake --build minEngine/build --target minEngine Editor`.
	`Editor.exe --help`, `Editor.exe test --help`, `Editor.exe test material-ir` exit 0; `--material-ir-test` warns + exit 0.
- Next step:
	TEST-F01 — suite registry, migrate remaining four `--*-test` flags, `test smoke`/`full`.

### 2026-05-28 - CLI-F01 unified command-line design (approved)
- Goal:
	Design CLI: subcommand = mode, -- = params; CLI11 + ApplicationCommandLine.
- Main changes:
	`Platform/CLI/CLI_UNIFIED_DESIGN.md`, `CLI_F01_IMPLEMENTATION.md`; Registry/roadmap/README links; Status Planned.
- Validation done:
	User approved convention and command tree.
- Next step:
	CLI-F01-S01 implementation (vendor CLI11 + parse core).

### 2026-05-28 - Infrastructure roadmap (CLI · Test · Verify)
- Goal:
	Near-term plan: CLI unification, test centralization, verify + DoD; defer product features.
- Main changes:
	`Platform/INFRASTRUCTURE_ROADMAP.md`; Registry `CLI-F01`/`TEST-F01`; `PLATFORM_ROADMAP` §12; README/BOOTSTRAP/TECH_DEBT links.
- Validation done:
	Docs-only.
- Next step:
	`CLI-F01` Design Spec + S01 when implementation starts; commit batch B.

### 2026-05-28 - Bootstrap digest and tech debt register
- Goal:
	One-page session recovery + explicit debt queue before infra roadmap.
- Main changes:
	`BOOTSTRAP_DIGEST.md`; `TECH_DEBT.md` (TD-001..012); `README` links; session-bootstrap reads digest.
- Validation done:
	Docs-only.
- Next step:
	Commit batch A; then infra roadmap (CLI + Test + verify) — not in digest/debt files.

### 2026-05-28 - WF collaboration v2 (registry, Slice DoD, handoff)
- Goal:
	Team workflow: Feature registry, engineering+doc Slice DoD, session handoff; rules/skills wired.
- Main changes:
	`FEATURE_REGISTRY.md`; `DOC_GOVERNANCE` §7 Slice DoD, §9.1 Handoff, registry rule; `docs-workflow-triggers` handoff row; mentor + git-commit-mentor DoD; `WORKING_WITH_AI` handoff prompt.
- Validation done:
	Docs-only; no build required.
- Next step:
	Use registry for next Feature (e.g. TEST-F01 CLI); optional post-commit hook later.

### 2026-05-27 - P4/P5 Core 路线对齐（函数反射 + 委托 + Lua）
- Goal:
	Align platform roadmap with next master work: MEFunction, delegates, Lua scripting.
- Main changes:
	`PLATFORM_ROADMAP.md` §11 Core 主线；P4/P5 设计草稿 `Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md`、`Scripting/LUA_SCRIPTING_DESIGN.md`；`PROJECT_CONTEXT` / `README` 链接更新。
- Next step:
	P4.1 `MEFunction` 描述符 + 手写注册一个测试函数。

### 2026-05-27 - 右键菜单 M0 骨架落地
- Goal:
	EditorContextMenuSystem + Context/Registry/Builder shells; wire IEditorContext.
- Main changes:
	`minEngine/Editor/src/ContextMenu/`（EditorMenuContext、IEditorAction、EditorActionRegistry、EditorMenuBuilder、EditorContextMenuSystem）。
	`IEditorContext::GetContextMenu()`；`Editor` PostInitialize/OpenProject RegisterBuiltInActions、CloseProject/Shutdown Shutdown。
- Next step:
	M1 ContentBrowser 接入 + 首批 Action 注册。

### 2026-05-27 - 右键菜单设计 v2（归属 + 门面）
- Goal:
	Clarify Editor infrastructure ownership; single GetContextMenu() facade.
- Main changes:
	`EDITOR_CONTEXT_MENU_DESIGN.md` §1 归属与生命周期；`EditorContextMenuSystem` 组合 Registry/Builder；明确不继承 EditorServiceModule。
- Next step:
	User review v2 → M0 implementation.

### 2026-05-27 - 右键菜单设计重置（可扩展架构）
- Goal:
	Replace interim enum/service draft with UE-aligned extensible context-menu system.
- Main changes:
	重写 `EDITOR_CONTEXT_MENU_DESIGN.md`（EditorMenuContext 袋、IEditorAction、Registry、MenuBuilder、Command、M0–M5）。
	`docs/external/README.md`；rollout 更新 S0–S3。

### 2026-05-27 - 右键菜单统一设计草案（已废弃）
- 轻量 `EditorContextMenuService` + enum 方案已由上条重置版替代。

### 2026-05-26 - Content Browser P6.1-polish implemented
- Goal:
	Flat Caption breadcrumb + square icon tile grid per §2.6.
- Main changes:
	`ContentBrowserWindow` — `ViewMetrics`, `DrawBreadcrumbLink`, `DrawAssetTile`; Appearance Caption/Selection/Field tokens.
- Validation done:
	`cmake --build minEngine/build --target Editor` succeeded.
- Next step:
	Editor 目视 Dark/Light；§2.6.4 主题项勾选。

### 2026-06-02 - WF-F02 handbook UX phase 2 (S04–S07)
- Goal:
  Auto nav, TOC, sidebar, and git last-updated footer for public handbook.
- Main changes:
  `mkdocs-awesome-pages-plugin` + `docs/handbook/**/.pages` (no root `nav` in mkdocs.yml).
  Material: `toc.follow`, `toc_depth 2-3`, `navigation.prune/path/top/indexes`.
  `mkdocs-git-revision-date-localized-plugin`; docs workflow `fetch-depth: 0`.
  `_authoring.md` + `exclude_docs`; expanded `render/overview.md` for TOC sample.
- Validation done:
  `mkdocs build --strict`.

### 2026-06-12 - Worktree bootstrap: submodules, scene asset migration, Editor run
- Goal:
  Fix physics worktree third-party gitdir paths; migrate scene Rotation to quaternion; ensure Editor launches.
- Main changes:
  `scripts/fix-worktree-submodule-gitdirs.ps1`, `scripts/migrate_transform_rotation_to_quat.py`;
  `default.mescene` / `test.mescene` Rotation → `{W,X,Y,Z}`; `MyMEProject.meproject` ProjectRoot → physics worktree.
- Validation done:
  `git status` OK after submodule gitdir fix; Editor loads `test` scene successfully with explicit `--engine-config`.
- Next step:
  Commit CORE-F01 code + asset migration batch.

### 2026-06-12 - CORE-F01 S01+S02+S04 partial: Quaternion storage and Scene API
- Goal:
  Land Transform quaternion storage (GLM-backed), Scene/RenderCamera API, Inspector Euler widget mapping.
- Main changes:
  `Quaternion.h/.cpp`, `Transform.h`, `SceneComponent`, `GameObject`, `RenderCamera`,
  `TransformWidget`, Editor viewport camera + PreviewScene + Playground call sites;
  reflection codegen for `Quaternion`; serialization round-trip test for `Transform.Rotation`.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests Editor`; `minEngineTests.exe test smoke` PASSED.
- Next step:
  CORE-F01-S05 call-site grep sweep; S06 verify + commit; then PHYS-F01.

### 2026-06-11 - CORE-F01 Transform quaternion design (physics branch)
- Goal:
  Register Transform storage migration as `CORE-F01` before `PHYS-F01` (Jolt); document scope, Inspector Euler widget mapping, and slice plan.
- Main changes:
  `docs/ai/Platform/Core/CORE-F01_TRANSFORM_QUATERNION_DESIGN.md`,
  `CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md`;
  `FEATURE_REGISTRY.md` (`CORE-F01` Planned, `PHYS-F01` blocked);
  `ACTIVE_WORK.md` priority order updated.
- Validation done:
  Docs only; no build.
- Next step:
  Design §6 decisions recorded (D4 no auto read; D5 RenderCamera in scope) → `CORE-F01-S01`+`S02` first landable PR.

### 2026-06-01 - WF-F02 handbook nav under `runtime/` tree
- Goal:
  Align handbook paths and stubs with revised `mkdocs.yml` (Runtime tab + Function/Platform/Resource children).
- Main changes:
  `docs/handbook/runtime/**` placeholders; removed flat `runtime/{function,platform,resource}/overview.md`; design/impl docs updated.
- Validation done:
  `mkdocs build --strict`.

### 2026-06-11 - PHYS-F01 Jolt bootstrap design + implementation plan
- Goal:
  Physics subsystem bootstrap scope: Jolt vendor, thin `Physics/` facade, `RigidBodyComponent` + `BoxColliderComponent`, fixed-step simulate + pose pull before `SendAllEndOfFrameUpdates` (UE-aligned tick order).
- Main changes:
  `docs/ai/Physics/PHYS-F01_JOLT_INTEGRATION_DESIGN.md`, `PHYS-F01_JOLT_INTEGRATION_IMPLEMENTATION.md`; P1–P9 defaults recorded; `FEATURE_REGISTRY` / `ACTIVE_WORK` → PHYS-F01 In Progress.
- Next step:
  PHYS-F01-S01-a (Jolt submodule + CMake).

### 2026-06-12 - PHYS-F01-S01 bootstrap complete (Jolt + physics vertical slice)
- Goal:
  Land S01-a–d: Jolt submodule/CMake, PhysicsSystem/World, RigidBody+BoxCollider (P10 proxy), LogicalTick simulate, physics-smoke falling box.
- Main changes:
  `Runtime/Function/Physics/*`; Jolt submodule; Engine/SceneManager lifecycle; `RigidBodyComponent`/`BoxColliderComponent` + reflection; `PhysicsSmokeTest`; fix `GameObject::AddComponent_Internal` SceneComponent-only attach.
- Validation done:
  `cmake --build minEngine/build --target minEngineTests`; `minEngineTests.exe test physics-smoke` + `test smoke` from `minEngine/bin`.
- Next step:
  Commit S01 batch; then PHYS-F01-S02 (collision layers + contact events).

### 2026-06-12 - PHYS-F01-S01-b PhysicsSystem and PhysicsWorld shell
- Goal:
  Engine singleton + per-Scene Jolt world with fixed-step accumulator; Scene load/unload lifecycle; PhysicsConversion axis helpers.
- Main changes:
  `Runtime/Function/Physics/*`; `Engine` Start/Shutdown; `SceneManager` create/load/unload hooks.
- Validation done:
  `cmake --build minEngine/build --target minEngine minEngineTests`; `minEngineTests.exe test smoke` from `minEngine/bin`.
- Next step:
  PHYS-F01-S01-c (RigidBodyComponent + BoxColliderComponent).

### 2026-06-12 - PHYS-F01-S01-a Jolt submodule and CMake link
- Goal:
  Vendor Jolt via git submodule; link `Jolt` static target into `minEngine` with nested `add_subdirectory(Jolt/Build)`.
- Main changes:
  `.gitmodules` + `Third-Party/Jolt`; `minEngine/CMakeLists.txt` cmake 3.20; `minEngine/minEngine/CMakeLists.txt` Jolt options and `target_link_libraries`.
- Validation done:
  `cmake --build minEngine/build --target minEngine minEngineTests`; `minEngineTests.exe test smoke`.
- Next step:
  PHYS-F01-S01-b (`PhysicsSystem` / `PhysicsWorld` shell).

### 2026-06-12 - PHYS-F01 design: RigidBodyComponent as physics proxy (P10)
- Goal:
  Align rigid body model with user intent: `RigidBodyComponent` is a `Component` agent, not `SceneComponent`; no own Transform; reads/writes GO RootComponent for physics sync.
- Main changes:
  Design §3.2, §4 P10, §5 options E/F; Implementation S01-c/d assembly and sync wording.
- Next step:
  User approval → commit docs; then S01-a.

### 2026-06-01 - WF-F02 handbook site skeleton (MkDocs + GitHub Pages CI)
- Goal:
  Public docs site aligned with `src/Runtime` top-level layers; placeholders only.
- Main changes:
  `docs/handbook/` (index, getting-started, core/function/platform/resource overview stubs).
  Root `mkdocs.yml`, `requirements-docs.txt`, `.github/workflows/docs.yml`.
  README link to https://max1122chen.github.io/minEngine/
- Validation done:
  `mkdocs build --strict` from repo root succeeded.
- Next step:
  Merge to `main`; enable Pages source GitHub Actions; fill handbook content per layer.

### 2026-05-26 - Roadmap sync (P3 Undo + E2 + P6.1)
- **Docs:** `PLATFORM_ROADMAP.md`、`EDITOR_PLATFORM_PLAN.md`、`PROJECT_CONTEXT.md` aligned with repo state.
- **Done (marked):** P6.1 CB UI; P3 Undo E1.1–E1.4 + S1–S2; E2.1–E2.3a Inspector/Material viewport preview (`6ccd9bf`).
- **Deferred (consolidated):** E1 Inspector unification; E2.2b Texture preview; E2.3b CB thumbnails; E2.4; E1.5 Material Undo; Command E2 TryMerge; P7; P0/P1; P4/P5.
