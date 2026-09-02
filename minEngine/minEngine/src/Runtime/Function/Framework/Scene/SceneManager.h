#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

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
    class AudioSmokeTestScope;
    class SceneCloneTestScope;
    class CommandSystemTestScope;

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
        void TickScenes(float deltaTime);

        std::shared_ptr<Scene> GetCurrentActiveScene() const { return m_CurrentActiveScene; }
        Scene* GetEditorScene() const;
        Scene* GetPIEScene(int32_t instanceId = 0) const;
        Scene* GetTickTargetScene() const;
        const std::vector<SceneContext>& GetSceneContexts() const;

        void SetEditorSceneContext(SceneContext context);
        void RegisterPIEScene(std::shared_ptr<Scene> pieScene, int32_t instanceId);
        void UnregisterPIEScene(int32_t instanceId);
        void SetPIEPlayActive(bool active) { m_PIEPlayActive = active; }
        bool IsPIEPlayActive() const { return m_PIEPlayActive; }

        void SetActiveSceneOverride(Scene* scene) { m_ActiveSceneOverride = scene; }
        Scene* GetActiveSceneOverride() const { return m_ActiveSceneOverride; }

        static void RebuildSceneComponentAttachHierarchy(Scene* scene);
        static void FinalizeLoadedScene(Scene* scene);

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

        void ResolvePendingActivationsForScene(Scene* scene);

        /** Non-owning; registered by Editor scene-editing viewport (P3 bridge until P4). */
        void SetEditorSceneViewport(SceneViewport* viewport) { m_EditorSceneViewport = viewport; }
        SceneViewport* GetEditorSceneViewport() const { return m_EditorSceneViewport; }

    // private: // temporarily public for testing
        std::shared_ptr<Scene> m_CurrentActiveScene{ nullptr };
        std::vector<Component*> m_ComponentsThatNeedEndOfFrameUpdate;
        SceneContext m_EditorSceneContext;
        std::vector<SceneContext> m_PIEContexts;
        bool m_PIEPlayActive = false;
        Scene* m_ActiveSceneOverride = nullptr;
        mutable std::vector<SceneContext> m_CachedSceneContexts;

    private:
        friend class Engine;
        friend class AssetManagerTestScope;
        friend class LuaScriptMvpTestScope;
        friend class PhysicsSmokeTestScope;
        friend class PhysicsSyncTestScope;
        friend class PhysicsLoadTestScope;
        friend class PhysicsContactTestScope;
        friend class PhysicsLineTraceTestScope;
        friend class PhysicsShapesTestScope;
        friend class AudioSmokeTestScope;
        friend class SceneCloneTestScope;
        friend class CommandSystemTestScope;

        static void SetInstance(SceneManager* instance);
        static SceneManager* s_Instance;

        std::unordered_map<std::string, std::string> m_RegisteredScenes;
        SceneViewport* m_EditorSceneViewport = nullptr;
    };
}
