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
        /** Tick WorldTickable subsystems for scenes with a non-None tick policy. */
        void TickWorldSubsystems(float deltaTime);
        /**
         * Variable gameplay: per scene WorldTickable then GO→Comp.
         * Call after GI variable tick, before Physics.
         */
        void TickVariableGameplay(float deltaTime);
        /** Fixed: per scene WorldTickable Fixed then GO Fixed participants. */
        void FixedTickGameplay(float fixedDeltaSeconds);

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

        bool HasBegunPlayWorld() const;

        void SetActiveSceneOverride(Scene* scene) { m_ActiveSceneOverride = scene; }
        Scene* GetActiveSceneOverride() const { return m_ActiveSceneOverride; }

        static void RebuildSceneComponentAttachHierarchy(Scene* scene);
        /** Hierarchy / attach / activation only — no WorldSubsystem collection. */
        static void FinalizeSceneStructure(Scene* scene);
        /** Structure + InitializeWorldSubsystems + PostInitialize (Editor load / CreateNewScene). */
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
        /** Drop a destroyed/removed component from the pending EOF list (no-op if unmarked). */
        void UnmarkComponentForNeededEndOfFrameUpdate(Component* component);
        /** Swap snapshot: process only components dirty before this call; new Marks wait for next SendAll. */
        void SendAllEndOfFrameUpdates();
        /** Erase GameObjects marked by DestroyGameObject (after EOF). */
        void FlushDeferredStructuralEdits();

        /**
         * Hand a Scene over for destruction at a frame-safe point instead of releasing it now.
         * Work already submitted this frame (scene draws, end-of-frame updates) may still
         * reference the Scene, so the last strong reference parks here.
         */
        void RetireScene(std::shared_ptr<Scene> scene);

        /** Destroy every retired Scene; call once the frame's submissions were consumed. */
        void FlushRetiredScenes();

        bool IsInStructuralTraversal() const { return m_StructuralTraversalDepth > 0; }

        void ResolvePendingActivationsForScene(Scene* scene);

        /** Non-owning; registered by Editor scene-editing viewport (P3 bridge until P4). */
        void SetEditorSceneViewport(SceneViewport* viewport) { m_EditorSceneViewport = viewport; }
        SceneViewport* GetEditorSceneViewport() const { return m_EditorSceneViewport; }

    private:
        friend class Engine;
        friend class Testing::TestAccess<SceneManager>;

        class StructuralTraversalScope
        {
        public:
            explicit StructuralTraversalScope(SceneManager& manager)
                : m_Manager(manager)
            {
                ++m_Manager.m_StructuralTraversalDepth;
            }

            ~StructuralTraversalScope()
            {
                --m_Manager.m_StructuralTraversalDepth;
            }

            StructuralTraversalScope(const StructuralTraversalScope&) = delete;
            StructuralTraversalScope& operator=(const StructuralTraversalScope&) = delete;

        private:
            SceneManager& m_Manager;
        };

        static void SetInstance(SceneManager* instance);
        static SceneManager* s_Instance;

        std::shared_ptr<Scene> m_CurrentActiveScene{ nullptr };
        std::vector<Component*> m_ComponentsThatNeedEndOfFrameUpdate;
        SceneContext m_EditorSceneContext;
        std::vector<SceneContext> m_PIEContexts;
        bool m_PIEPlayActive = false;
        Scene* m_ActiveSceneOverride = nullptr;
        mutable std::vector<SceneContext> m_CachedSceneContexts;
        int32_t m_StructuralTraversalDepth{ 0 };

        std::unordered_map<std::string, std::string> m_RegisteredScenes;
        std::vector<std::shared_ptr<Scene>> m_RetiredScenes;
        SceneViewport* m_EditorSceneViewport = nullptr;
    };
}
