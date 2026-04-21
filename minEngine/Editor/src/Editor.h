#pragma once

#include "Core.h"
#include "minEngine.h"

#include "EditorGUIManager.h"
#include "Viewport/EditorViewportClient.h"

#include <filesystem>
#include <limits>
#include <memory>
#include <unordered_map>

namespace minEngine
{
    class Scene;
    class GameObject;

    class Editor : public Application
    {
    public:
        Editor() = default;
        ~Editor() override = default;

        void Initialize(int argc, char** argv) override;
        void Shutdown() override;
        void Run() override;

        EditorGUIManager& GetGUIManager() { return m_EditorGUIManager; }
        const EditorGUIManager& GetGUIManager() const { return m_EditorGUIManager; }

        void RequestExit(){ m_ExitRequested = true; }

        Scene* GetActiveScene() const;
        std::vector<GameObject*> GetHierarchyGameObjects() const;
        GameObject* GetSelectedGameObject() const;
        void SelectGameObject(uint64_t gameObjectId);
        bool IsGameObjectSelected(uint64_t gameObjectId) const;
        std::string GetGameObjectDisplayName(const GameObject& gameObject) const;
        std::string GetSelectedGameObjectName() const;
        bool RenameGameObject(uint64_t gameObjectId, const std::string& newName);
        void RenameSelectedGameObject(const std::string& newName);
        std::vector<std::string> GetAllComponentTypeNames() const;
        bool AddComponentToSelectedGameObject(const std::string& componentTypeName);

        void MarkSceneDirty() { m_SceneDirty = true; }
        void ClearSceneDirty() { m_SceneDirty = false; }
        bool IsSceneDirty() const { return m_SceneDirty; }

        void SyncSelectionWithScene();

        EditorViewportClient& GetOrCreateViewportClient(const std::string& viewportId,
                                const std::string& viewportTitle = "Viewport");
        EditorViewportClient* FindViewportClient(const std::string& viewportId);
        const EditorViewportClient* FindViewportClient(const std::string& viewportId) const;
        void RemoveViewportClient(const std::string& viewportId);
        void ClearViewportClients();

    public:
        bool isPlaying = false;
        bool showDemoWindow = false;
        float lastDeltaTime = 0.0f;

        bool dockLayoutInitialized = false;
        bool requestResetLayout = false;

    private:

    private:
        Engine* m_Engine = nullptr;
        EditorGUIManager m_EditorGUIManager;
        std::unordered_map<std::string, std::unique_ptr<EditorViewportClient>> m_ViewportClients;
        bool m_ExitRequested = false;
        bool m_SceneDirty = false;
        uint64_t m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
    };

    Application* CreateApplication();
}
