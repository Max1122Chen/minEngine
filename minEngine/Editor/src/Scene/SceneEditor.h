#pragma once

#include "Core.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Commands/Scene/EditorObjectSnapshot.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Scene/SceneEditorInspectorSource.h"
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
    class Scene;
    struct Transform;

    class SceneEditor : public EditorSubModule
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
        bool RouteViewportInput(EditorViewportClient& client) override;

        void InitializeComponentTypeNames();
        void SyncSelectionWithScene();

        Scene* GetActiveScene() const;

        std::vector<GameObject*> GetHierarchyGameObjects() const;
        GameObject* GetSelectedGameObject() const;
        bool HasSelectedGameObject() const;
        void SelectGameObject(uint64_t gameObjectId);
        void ClearSelectedGameObject();
        bool IsGameObjectSelected(uint64_t gameObjectId) const;
        std::string GetGameObjectDisplayName(const GameObject& gameObject) const;
        std::string GetSelectedGameObjectName() const;
        bool LoadScene(IEditorContext& context, const std::string& sceneName);

        bool ApplyRenameGameObject(uint64_t gameObjectId, const std::string& newName);
        void SubmitRenameGameObject(IEditorContext& context,
                                    uint64_t gameObjectId,
                                    const std::string& newName);

        void ApplyGameObjectTransform(uint64_t gameObjectId, const Transform& transform);
        void SubmitGameObjectTransform(IEditorContext& context,
                                       uint64_t gameObjectId,
                                       const Transform& before,
                                       const Transform& after);
        const std::vector<std::string>& GetAllComponentTypeNames() const;
        bool ApplyAddComponentToSelectedGameObject(const std::string& componentTypeName, Component*& outNewComponent);
        void SubmitAddComponentToSelectedGameObject(IEditorContext& context, const std::string& componentTypeName);
        bool ApplyRemoveComponentFromGO(GameObject& gameObject, Component& targetComponent);
        void SubmitRemoveComponentFromGO(IEditorContext& context, GameObject& gameObject, Component& targetComponent);

        void SaveCurrentScene();
        uint64_t ApplyAddEmptyGOToScene();
        void SubmitAddEmptyGOToScene(IEditorContext& context);
        bool ApplyRemoveGameObjectFromScene(uint64_t gameObjectId, std::string& outName, Transform& outTransform);
        void SubmitRemoveGameObjectFromScene(IEditorContext& context, uint64_t gameObjectId);
        bool ApplySetObjectProperty(const GUID& ownerGuid,
                                    const std::string& ownerClassName,
                                    const std::string& propertyName,
                                    const std::vector<uint8_t>& valueBlob);
        void SubmitSetObjectProperty(IEditorContext& context,
                                     const GUID& ownerGuid,
                                     const std::string& ownerClassName,
                                     const std::string& propertyName,
                                     std::vector<uint8_t> beforeValue,
                                     std::vector<uint8_t> afterValue);

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

        void MarkSceneDirty() { m_SceneDirty = true; }
        void ClearSceneDirty() { m_SceneDirty = false; }
        bool IsSceneDirty() const { return m_SceneDirty; }

        void OnProjectOpened();

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
        bool m_SceneDirty = false;
        uint64_t m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        GameObject* m_SelectedGameObject = nullptr;
        std::vector<std::string> m_AllComponentTypeNames;
    };
}
