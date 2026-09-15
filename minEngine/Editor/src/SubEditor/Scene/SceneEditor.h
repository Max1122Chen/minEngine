#pragma once

#include "Core.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Commands/EditorSetObjectPropertyTarget.h"
#include "Commands/Scene/EditorObjectSnapshot.h"
#include "Runtime/Core/GUID/GUID.h"
#include "SubEditor/Scene/SceneEditorInspectorSource.h"
#include "SubEditor/Prefab/PrefabStageController.h"
#include "Shell/EditorSubModule.h"

#include <limits>
#include <string>
#include <vector>

namespace minEngine
{
    class AssetMeta;
    class Component;
    class GameObject;
    class IEditorContext;
    class Prefab;
    class Scene;
    struct Transform;

    class SceneEditor : public EditorSubModule, public EditorSetObjectPropertyTarget
    {
    public:
        static constexpr const char* kModuleId = "Scene";

        SceneEditor();

        std::string_view GetModuleId() const override { return kModuleId; }
        std::string_view GetDisplayName() const override { return "Scene"; }

        void Register(IEditorContext& context) override;
        void Shutdown() override;

        void OnActivate(IEditorContext& context) override;
        void OnDeactivate(IEditorContext& context) override;
        void RegisterCommands(IEditorContext& context) override;
        void UnregisterCommands(IEditorContext& context) override;
        void Tick(float deltaTime) override;

        void ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId) override;

        IEditorInspectorSource* GetInspectorSource() override { return &m_InspectorSource; }
        const IEditorInspectorSource* GetInspectorSource() const override { return &m_InspectorSource; }

        bool OpenAsset(const AssetMeta& meta) override;
        bool CanOpenAsset(const AssetMeta& meta) const override;
        bool RouteViewportInput(EditorViewportClient& client) override;

        void InitializeComponentTypeNames();
        void SyncSelectionWithScene();

        Scene* GetActiveScene() const;
        /** Document scene for Save/Dirty — always Editor, never PIE. */
        Scene* GetDocumentScene() const;

        std::vector<GameObject*> GetHierarchyGameObjects() const;
        GameObject* GetSelectedGameObject() const;
        bool HasSelectedGameObject() const;
        void SelectGameObject(uint64_t gameObjectId);
        void ClearSelectedGameObject();
        bool IsGameObjectSelected(uint64_t gameObjectId) const;
        std::string GetGameObjectDisplayName(const GameObject& gameObject) const;
        std::string GetSelectedGameObjectName() const;
        bool LoadScene(IEditorContext& context, const std::string& sceneName);
        bool OpenSceneByPath(IEditorContext& context, const std::string& projectRelativePath);
        bool CreateNewSceneDocument(IEditorContext& context);
        bool HasPersistedScenePath() const;

        bool ApplyRenameGameObject(uint64_t gameObjectId, const std::string& newName);
        void SubmitRenameGameObject(IEditorContext& context,
                                    uint64_t gameObjectId,
                                    const std::string& newName);

        bool ApplyRenameComponent(uint64_t ownerGameObjectId,
                                  const GUID& componentGuid,
                                  const std::string& newName);
        void SubmitRenameComponent(IEditorContext& context,
                                   uint64_t ownerGameObjectId,
                                   const GUID& componentGuid,
                                   const std::string& newName);
        Component* FindComponentByGuid(uint64_t ownerGameObjectId, const GUID& componentGuid) const;

        bool ApplyMoveComponent(uint64_t ownerGameObjectId, const GUID& componentGuid, size_t newIndex);
        void SubmitMoveComponent(IEditorContext& context,
                                 uint64_t ownerGameObjectId,
                                 const GUID& componentGuid,
                                 size_t newIndex);

        /** Sentinel: not a real GO id — means scene root / Detach. Never use 0 (GO ids start at 0). */
        static constexpr uint64_t kSceneRootParentId = std::numeric_limits<uint64_t>::max();

        /** newParentId == kSceneRootParentId means Detach. KeepWorldTransform (current Attach path). */
        bool ApplyReparentGameObject(uint64_t gameObjectId, uint64_t newParentId);
        void SubmitReparentGameObject(IEditorContext& context,
                                      uint64_t gameObjectId,
                                      uint64_t newParentId);

        void ApplyGameObjectTransform(uint64_t gameObjectId, const Transform& transform);
        void SubmitGameObjectTransform(IEditorContext& context,
                                       uint64_t gameObjectId,
                                       const Transform& before,
                                       const Transform& after);
        const std::vector<std::string>& GetAllComponentTypeNames() const;
        /** Explicit-target add; does not require current selection. */
        bool ApplyAddComponentToGameObject(uint64_t gameObjectId,
                                           const std::string& componentTypeName,
                                           Component*& outNewComponent);
        void SubmitAddComponentToGameObject(IEditorContext& context,
                                            uint64_t gameObjectId,
                                            const std::string& componentTypeName);
        /** GUI helper: resolve selection at submit time, then SubmitAddComponentToGameObject. */
        void SubmitAddComponentToSelectedGameObject(IEditorContext& context, const std::string& componentTypeName);
        bool ApplyRemoveComponentFromGO(GameObject& gameObject, Component& targetComponent);
        bool ApplyRemoveComponentFromGameObject(uint64_t ownerGameObjectId, Component& targetComponent);
        void SubmitRemoveComponentFromGO(IEditorContext& context, GameObject& gameObject, Component& targetComponent);

        void SaveCurrentScene();
        bool SaveCurrentScene(IEditorContext& context);
        bool SaveCurrentSceneAs(IEditorContext& context);
        uint64_t ApplyAddEmptyGOToScene();
        void SubmitAddEmptyGOToScene(IEditorContext& context);
        bool ApplyRemoveGameObjectFromScene(uint64_t gameObjectId, std::string& outName, Transform& outTransform);
        void SubmitRemoveGameObjectFromScene(IEditorContext& context, uint64_t gameObjectId);

        /** Level Scene: create `.meprefab` from GO (file dialog). Disk file Undo is out of scope. */
        bool CreatePrefabFromSelectedGameObject(IEditorContext& context, uint64_t gameObjectId);
        /** Level Scene: open Prefab picker then instantiate (Undo deletes instance root). */
        void SubmitInstantiatePrefab(IEditorContext& context, uint64_t attachParentGameObjectId);
        uint64_t ApplyInstantiatePrefab(Prefab& prefab, uint64_t attachParentGameObjectId);

        void RequestBeginRenameGameObject(uint64_t gameObjectId);
        uint64_t ConsumePendingRenameGameObjectId();
        void BeginRenameGameObjectInInspector(uint64_t gameObjectId);
        void BeginRenameComponentInInspector(Component& component);
        bool ApplySetObjectProperty(const GUID& ownerGuid,
                                    const std::string& ownerClassName,
                                    const std::string& propertyPath,
                                    const std::vector<uint8_t>& valueBlob) override;
        void SubmitSetObjectProperty(IEditorContext& context,
                                     const GUID& ownerGuid,
                                     const std::string& ownerClassName,
                                     const std::string& propertyPath,
                                     std::vector<uint8_t> beforeValue,
                                     std::vector<uint8_t> afterValue,
                                     bool applyOnFirstExecute = true);

        bool TryCaptureGameObjectSnapshotForDelete(uint64_t gameObjectId,
                                                   EditorObjectSnapshot& outSnapshot,
                                                   std::string& outDescription);
        bool TryCaptureComponentSnapshotForRemove(uint64_t ownerGameObjectId,
                                                  const GUID& componentGuid,
                                                  EditorObjectSnapshot& outSnapshot,
                                                  int32_t& outComponentIndex,
                                                  std::string& outDescription);
        bool ApplyRemoveComponentByGuid(uint64_t ownerGameObjectId, const GUID& componentGuid);

        uint64_t ApplyRestoreGameObjectFromSnapshot(const EditorObjectSnapshot& snapshot);
        Component* ApplyRestoreComponentFromSnapshot(uint64_t ownerGameObjectId, const EditorObjectSnapshot& snapshot);
        void PostRestoreSceneObject(GameObject& gameObject);

        void MarkSceneDirty();
        void ClearSceneDirty() { m_SceneDirty = false; }
        bool IsSceneDirty() const { return m_SceneDirty; }

        const std::string& GetOpenedSceneAssetPath() const { return m_OpenedSceneAssetPath; }

        void OnProjectOpened();

        PrefabStageController& GetPrefabStages() { return m_PrefabStages; }
        const PrefabStageController& GetPrefabStages() const { return m_PrefabStages; }
        bool IsEditingPrefabStage() const { return m_PrefabStages.HasActiveStage(); }
        bool EnterPrefabStage(const std::string& assetKey);
        void ExitPrefabStage();

        IEditorContext* GetEditorContext() const { return m_Context; }

        Serialization::SerializerOptions GetPropertyCommandSerializerOptions() const;

    private:
        static Serialization::SerializerOptions GetRestoreSerializerOptions();

        Serialization::SerializeResult CaptureGameObjectSnapshot(const GameObject& gameObject,
                                                                 EditorObjectSnapshot& outSnapshot) const;
        Serialization::SerializeResult CaptureComponentSnapshot(const Component& component,
                                                                GameObject& owner,
                                                                int32_t componentIndex,
                                                                EditorObjectSnapshot& outSnapshot) const;

        IEditorContext* m_Context = nullptr;
        SceneEditorInspectorSource m_InspectorSource;
        PrefabStageController m_PrefabStages;
        bool m_SceneDirty = false;
        std::string m_OpenedSceneAssetPath;
        uint64_t m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        GameObject* m_SelectedGameObject = nullptr;
        uint64_t m_PendingRenameGameObjectId = std::numeric_limits<uint64_t>::max();
        std::vector<std::string> m_AllComponentTypeNames;
    };
}
