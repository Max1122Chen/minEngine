#pragma once

#include "Core.h"
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
    class Scene;

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
        bool RenameGameObject(uint64_t gameObjectId, const std::string& newName);
        void RenameSelectedGameObject(const std::string& newName);
        const std::vector<std::string>& GetAllComponentTypeNames() const;
        bool AddComponentToSelectedGameObject(const std::string& componentTypeName);
        bool RemoveComponentFromGO(GameObject& gameObject, class Component& targetComponent);

        void SaveCurrentScene();
        void AddEmptyGOToScene();
        bool RemoveGameObjectFromScene(uint64_t gameObjectId);
        void MarkSceneDirty() { m_SceneDirty = true; }
        void ClearSceneDirty() { m_SceneDirty = false; }
        bool IsSceneDirty() const { return m_SceneDirty; }

        void OnProjectOpened();

    private:
        SceneEditorInspectorSource m_InspectorSource;
        bool m_SceneDirty = false;
        uint64_t m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        GameObject* m_SelectedGameObject = nullptr;
        std::vector<std::string> m_AllComponentTypeNames;
    };
}
