#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    class Engine;
    class Scene;
    class RenderScene;
    class SceneViewport;
    class Component;
    class PrimitiveComponent;
    class PhysicsSmokeTestScope;
    class PhysicsSyncTestScope;
    class PhysicsLoadTestScope;
    class PhysicsContactTestScope;
    class PhysicsLineTraceTestScope;
    class PhysicsShapesTestScope;

    class SceneManager
    {
    public:
        SceneManager() = default;
        virtual ~SceneManager() = default;

        void Initialize();
        void Shutdown();

        static SceneManager& Get();
        static bool HasInstance();
        
        void Tick(float deltaTime);

        std::shared_ptr<Scene> GetCurrentActiveScene() const { return m_CurrentActiveScene; }
        RenderScene* GetRenderScene();

        bool RegisterScene(const std::string& sceneName, const std::string& path);
        bool UnregisterScene(const std::string& sceneName);
        bool IsSceneRegistered(const std::string& sceneName) const;
        std::shared_ptr<Scene> CreateNewScene(const std::string& sceneName);
        bool LoadScene(const std::string& sceneName);
        bool LoadSceneByPath(const std::string& path);
        bool SaveCurrentScene();

        void UnloadActiveScene();

        void MarkComponentForNeededEndOfFrameUpdate(Component* component);
        void SendAllEndOfFrameUpdates();

        /** Non-owning; registered by Editor scene-editing viewport (P3 bridge until P4). */
        void SetEditorSceneViewport(SceneViewport* viewport) { m_EditorSceneViewport = viewport; }
        SceneViewport* GetEditorSceneViewport() const { return m_EditorSceneViewport; }

    // private: // temporarily public for testing
        std::shared_ptr<Scene> m_CurrentActiveScene{ nullptr };
        std::vector<Component*> m_ComponentsThatNeedEndOfFrameUpdate;

    private:
        friend class Engine;
        friend class AssetManagerTestScope;
        friend class PhysicsSmokeTestScope;
        friend class PhysicsSyncTestScope;
        friend class PhysicsLoadTestScope;
        friend class PhysicsContactTestScope;
        friend class PhysicsLineTraceTestScope;
        friend class PhysicsShapesTestScope;

        static void SetInstance(SceneManager* instance);
        static SceneManager* s_Instance;

        std::unordered_map<std::string, std::string> m_RegisteredScenes;
        SceneViewport* m_EditorSceneViewport = nullptr;
    };
}
