#pragma once
#include "Core.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    class RuntimeGlobalContext;
    class Scene;
    class RenderScene;
    class Component;
    class PrimitiveComponent;

    class SceneManager
    {
    public:
        SceneManager() = default;
        virtual ~SceneManager() = default;

        void Initialize();
        void Shutdown();
        static SceneManager& Get() { return *RuntimeGlobalContext::Get().m_SceneManager; }
        
        void Tick(float deltaTime);

        std::shared_ptr<Scene> GetCurrentActiveScene() const { return m_CurrentActiveScene; }
        RenderScene* GetRenderScene() const { return m_RenderScene; }

        bool RegisterScene(const std::string& sceneName, const std::string& path);
        bool UnregisterScene(const std::string& sceneName);
        std::shared_ptr<Scene> CreateNewScene(const std::string& sceneName);
        bool LoadScene(const std::string& sceneName);
        bool LoadSceneByPath(const std::string& path);
        bool SaveCurrentScene();
        

        void MarkComponentForNeededEndOfFrameUpdate(Component* component);
        void SendAllEndOfFrameUpdates();    // to render thread

    // private: // temporarily public for testing
        std::shared_ptr<Scene> m_CurrentActiveScene{ nullptr };

        // Components that need end of frame render data update
        std::vector<Component*> m_ComponentsThatNeedEndOfFrameUpdate;

    private:
        std::unordered_map<std::string, std::string> m_RegisteredScenes; // scene name -> scene asset path
        RenderScene* m_RenderScene{ nullptr };

    };
}