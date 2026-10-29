#include "SceneManager.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Framework/Components/SkyBoxComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>
#include <iterator>
#include <vector>
#include "Runtime/Core/Object/ObjectCast.h"

namespace minEngine
{
    SceneManager* SceneManager::s_Instance = nullptr;

    namespace
    {
        /** True when the caller may drive the render role: no topology, or this thread owns it. */
        bool OwnsRenderRole()
        {
            return !FrameThreading::HasInstance() || FrameThreading::Get().IsInRenderingThread();
        }
    }

    /**
     * Takes every render proxy of a Scene off its draw lists, on the game role.
     *
     * Uses the same per-component path a destroyed component uses, so a proxy leaves through a
     * removal command - marked, unregistered, parked - rather than being released with the
     * Scene. Only render components are touched: the rest keep their own teardown for their own
     * systems.
     *
     * Called by SceneManager::RetireScene, and by a Scene's own destructor: once the GameObjects
     * are coming apart, a component can no longer answer which render scene it belonged to, so the
     * removal has to be driven from the Scene while it still knows.
     */
    void DetachSceneRenderState(Scene& scene)
    {
        const std::shared_ptr<RenderScene>& renderScene = scene.GetRenderSceneShared();
        if (!renderScene || !renderScene->HasPendingProxyWork())
        {
            return;
        }

        if (FrameThreading::HasInstance() && !FrameThreading::Get().IsInGameThread())
        {
            // The components below are game-role state; touching them from another role would
            // race the very tick that owns them. The scene still retires, and the RenderScene it
            // still holds reports the proxies it was not allowed to remove.
            ME_LOG(
                LogCore,
                Error,
                "Scene teardown: the Scene still owns render proxies and is being torn down off "
                "the game role, so they cannot be removed first. Destroying it now would free "
                "proxies the frame may still reference.");
            return;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }
            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (!component)
                {
                    continue;
                }

                // The component's own pointer is cleared here, on the game role, right after
                // the removal is queued: the teardown that follows must not queue a second
                // removal for a scene that will be gone by the time it could be applied.
                if (auto* primitive = dynamic_cast<PrimitiveComponent*>(component.get()))
                {
                    if (primitive->GetSceneProxy() != nullptr)
                    {
                        renderScene->RemovePrimitive(primitive);
                        primitive->DetachSceneProxy();
                    }
                }
                else if (auto* light = dynamic_cast<LightComponent*>(component.get()))
                {
                    if (light->GetSceneProxy() != nullptr)
                    {
                        renderScene->RemoveLight(light);
                        light->DetachSceneProxy();
                    }
                }
                else if (auto* widget = dynamic_cast<WidgetComponent*>(component.get()))
                {
                    if (widget->GetSceneProxy() != nullptr)
                    {
                        renderScene->RemoveWidget(widget);
                        widget->DetachSceneProxy();
                    }
                }
                else if (auto* skyBox = dynamic_cast<SkyBoxComponent*>(component.get()))
                {
                    if (skyBox->GetSceneProxy() != nullptr)
                    {
                        renderScene->RemoveSkyBox(skyBox);
                        skyBox->DetachSceneProxy();
                    }
                }
            }
        }
    }

    namespace
    {
        /**
         * Applies the removal commands a retirement queued and frees what they parked.
         *
         * Only for the shutdown path, where no frame boundary is left to do it: it runs the same
         * two steps a frame runs (apply, then the frame-end delete) on the thread that owns the
         * render role at that point.
         */
        void DrainQueuedRemovals()
        {
            if (!RenderCommandPipe::HasInstance())
            {
                return;
            }
            if (!OwnsRenderRole())
            {
                // Nothing is forced through: applying commands is the render role's job, and a
                // scene that still holds proxies reports them where it is destroyed.
                ME_LOG(
                    LogCore,
                    Warn,
                    "SceneManager: the render commands a scene retirement queued cannot be applied "
                    "from this thread; they stay queued for the render role, and a scene that still "
                    "holds proxies reports them.");
                return;
            }

            RenderCommandPipe& pipe = RenderCommandPipe::Get();
            // The newest frame any command can be tagged with: the boundary that would have drained
            // it is the one that is not coming.
            (void)pipe.FlushApplyForFrameChecked(pipe.GetCurrentGameFrame());
            RenderScene::FlushPendingProxyDeletes();
        }
    }

    void SceneManager::SetInstance(SceneManager* instance)
    {
        s_Instance = instance;
    }

    SceneManager& SceneManager::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "SceneManager is not initialized");
        return *s_Instance;
    }

    bool SceneManager::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void SceneManager::Initialize()
    {
    }

    void SceneManager::Shutdown()
    {
        // The frame roles were stopped before shutdown reached here, so this thread owns the render
        // role and no boundary is left to apply the removals a retirement queued. Apply them now,
        // before the gate is asked to destroy: a Scene destroyed with its proxies still registered
        // is the one thing the gate must never have to report.
        DrainQueuedRemovals();
        FlushRetiredScenes();
        m_ComponentsThatNeedEndOfFrameUpdate.clear();
        m_PIEPlayActive = false;
        m_ActiveSceneOverride = nullptr;

        for (const SceneContext& pieContext : m_PIEContexts)
        {
            if (!pieContext.Scene)
            {
                continue;
            }

            if (pieContext.Scene->HasBegunPlay())
            {
                pieContext.Scene->DispatchEndPlay(EEndPlayReason::Destroyed);
            }
            pieContext.Scene->DeinitializeWorldSubsystems();

            if (PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().DestroyWorld(pieContext.Scene.get());
            }
        }
        // The contexts are the last owner of the play worlds. Clearing the list would destroy each
        // world here, in the middle of a teardown whose remaining steps still walk the systems those
        // destructors touch; hand them to the gate instead and destroy them after the teardown.
        for (SceneContext& pieContext : m_PIEContexts)
        {
            RetireScene(std::move(pieContext.Scene));
        }
        m_PIEContexts.clear();

        UnloadActiveScene();

        // UnloadActiveScene already released an editor context that aliased the active scene; what
        // is left here belongs to the editor context alone.
        if (m_EditorSceneContext.Scene)
        {
            RetireScene(std::move(m_EditorSceneContext.Scene));
        }
        m_EditorSceneContext = {};
        m_RegisteredScenes.clear();

        // No frame boundary is coming any more: a flush that waited for "the next frame" would
        // never run. Apply the removals the retirements above queued where this thread owns the
        // render role, then do what is left of them here - the roles are stopped, so no frame is in
        // flight and no command of theirs will ever be applied - and destroy the scenes on this
        // thread, while the device and the systems their destructors touch are still alive.
        DrainQueuedRemovals();
        {
            std::lock_guard<std::mutex> lock(m_RetiredScenesMutex);
            for (const std::shared_ptr<Scene>& retiredScene : m_RetiredScenes)
            {
                if (!retiredScene)
                {
                    continue;
                }
                if (const std::shared_ptr<RenderScene>& renderScene = retiredScene->GetRenderSceneShared())
                {
                    DiscardRenderSceneProxiesForTeardown(*renderScene);
                }
            }
        }
        DiscardOrphanedRenderScenesForTeardown();
        FlushRetiredScenes();

        ME_LOG(LogCore, Info, "SceneManager Shutdown.");
    }

    void SceneManager::UnloadActiveScene()
    {
        if (m_CurrentActiveScene)
        {
            if (m_CurrentActiveScene->HasBegunPlay())
            {
                m_CurrentActiveScene->DispatchEndPlay(EEndPlayReason::Destroyed);
            }

            m_CurrentActiveScene->DeinitializeWorldSubsystems();

            if (AudioSystem::HasInstance())
            {
                AudioSystem::Get().OnSceneUnloaded(m_CurrentActiveScene.get());
            }

            if (PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().DestroyWorld(m_CurrentActiveScene.get());
            }
        }

        if (m_ActiveSceneOverride != nullptr && m_ActiveSceneOverride == m_CurrentActiveScene.get())
        {
            m_ActiveSceneOverride = nullptr;
        }

        // Drop editor-context ownership of the same Scene so unload actually releases it.
        // (LoadScene keeps both m_CurrentActiveScene and m_EditorSceneContext sharing one instance.)
        if (m_EditorSceneContext.Scene && m_CurrentActiveScene
            && m_EditorSceneContext.Scene.get() == m_CurrentActiveScene.get())
        {
            m_EditorSceneContext = {};
        }

        // This is the last reference the manager owns, but a draw or an end-of-frame update
        // submitted this frame may still point into the Scene: park it for the frame boundary
        // instead of destroying it here.
        RetireScene(std::move(m_CurrentActiveScene));

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().CollectGarbage();
        }
    }

    bool SceneManager::HasBegunPlayWorld() const
    {
        for (const SceneContext& pieContext : m_PIEContexts)
        {
            if (pieContext.Scene && pieContext.Scene->HasBegunPlay())
            {
                return true;
            }
        }

        if (m_CurrentActiveScene && m_CurrentActiveScene->HasBegunPlay())
        {
            return true;
        }

        return false;
    }

    void SceneManager::Tick(float deltaTime)
    {
        TickScenes(deltaTime);
    }

    void SceneManager::TickScenes(float deltaTime)
    {
        auto tickSceneIfNeeded = [deltaTime](const SceneContext& context)
        {
            if (context.Scene == nullptr || context.TickPolicy == ESceneTickPolicy::None)
            {
                return;
            }

            context.Scene->Tick(deltaTime);
        };

        tickSceneIfNeeded(m_EditorSceneContext);
        for (const SceneContext& pieContext : m_PIEContexts)
        {
            tickSceneIfNeeded(pieContext);
        }
    }

    void SceneManager::TickWorldSubsystems(float deltaTime)
    {
        auto tickWorldIfNeeded = [deltaTime](const SceneContext& context)
        {
            if (context.Scene == nullptr || context.TickPolicy == ESceneTickPolicy::None)
            {
                return;
            }

            context.Scene->TickWorldSubsystems(deltaTime);
        };

        tickWorldIfNeeded(m_EditorSceneContext);
        for (const SceneContext& pieContext : m_PIEContexts)
        {
            tickWorldIfNeeded(pieContext);
        }
    }

    void SceneManager::TickVariableGameplay(float deltaTime)
    {
        auto tickIfNeeded = [deltaTime](const SceneContext& context)
        {
            if (context.Scene == nullptr || context.TickPolicy == ESceneTickPolicy::None)
            {
                return;
            }

            context.Scene->TickWorldSubsystems(deltaTime);
            context.Scene->Tick(deltaTime);
        };

        tickIfNeeded(m_EditorSceneContext);
        for (const SceneContext& pieContext : m_PIEContexts)
        {
            tickIfNeeded(pieContext);
        }
    }

    void SceneManager::FixedTickGameplay(float fixedDeltaSeconds)
    {
        auto tickIfNeeded = [fixedDeltaSeconds](const SceneContext& context)
        {
            if (context.Scene == nullptr || context.TickPolicy == ESceneTickPolicy::None)
            {
                return;
            }

            context.Scene->FixedTickWorldSubsystems(fixedDeltaSeconds);
            context.Scene->FixedTickGameObjects(fixedDeltaSeconds);
        };

        tickIfNeeded(m_EditorSceneContext);
        for (const SceneContext& pieContext : m_PIEContexts)
        {
            tickIfNeeded(pieContext);
        }
    }

    Scene* SceneManager::GetEditorScene() const
    {
        if (m_EditorSceneContext.Scene)
        {
            return m_EditorSceneContext.Scene.get();
        }

        return m_CurrentActiveScene.get();
    }

    Scene* SceneManager::GetPIEScene(int32_t instanceId) const
    {
        for (const SceneContext& context : m_PIEContexts)
        {
            if (context.PIEInstanceId == instanceId && context.Scene)
            {
                return context.Scene.get();
            }
        }

        return nullptr;
    }

    Scene* SceneManager::GetTickTargetScene() const
    {
        if (m_ActiveSceneOverride != nullptr)
        {
            return m_ActiveSceneOverride;
        }

        if (m_PIEPlayActive && !m_PIEContexts.empty())
        {
            for (const SceneContext& pieContext : m_PIEContexts)
            {
                if (pieContext.Scene)
                {
                    return pieContext.Scene.get();
                }
            }
        }

        return GetEditorScene();
    }

    const std::vector<SceneContext>& SceneManager::GetSceneContexts() const
    {
        m_CachedSceneContexts.clear();
        if (m_EditorSceneContext.Scene)
        {
            m_CachedSceneContexts.push_back(m_EditorSceneContext);
        }
        m_CachedSceneContexts.insert(m_CachedSceneContexts.end(), m_PIEContexts.begin(), m_PIEContexts.end());
        return m_CachedSceneContexts;
    }

    void SceneManager::SetEditorSceneContext(SceneContext context)
    {
        // Replacing the context drops the reference it held. Whether that reference was the last
        // one is not knowable here, so hand it to the gate: a Scene someone else still owns stays
        // alive, and one nobody else owns is destroyed at the frame boundary instead of now.
        if (m_EditorSceneContext.Scene && m_EditorSceneContext.Scene != context.Scene)
        {
            RetireScene(std::move(m_EditorSceneContext.Scene));
        }

        m_EditorSceneContext = std::move(context);
        if (m_EditorSceneContext.Scene)
        {
            m_EditorSceneContext.Scene->SetSceneType(ESceneType::Editor);
            m_EditorSceneContext.Scene->SetTickPolicy(m_EditorSceneContext.TickPolicy);
            m_EditorSceneContext.Type = ESceneType::Editor;
        }
    }

    void SceneManager::RegisterPIEScene(std::shared_ptr<Scene> pieScene, int32_t instanceId)
    {
        if (!pieScene)
        {
            return;
        }

        pieScene->SetSceneType(ESceneType::PIE);
        pieScene->SetTickPolicy(ESceneTickPolicy::Gameplay);
        pieScene->SetPIEInstanceId(instanceId);
        pieScene->EnsureRenderScene();

        SceneContext context;
        context.Type = ESceneType::PIE;
        context.TickPolicy = ESceneTickPolicy::Gameplay;
        context.PIEInstanceId = instanceId;
        context.Scene = std::move(pieScene);
        context.ContextHandle = "PIE_" + std::to_string(instanceId);

        m_PIEContexts.erase(
            std::remove_if(
                m_PIEContexts.begin(),
                m_PIEContexts.end(),
                [instanceId](const SceneContext& existingContext)
                {
                    return existingContext.PIEInstanceId == instanceId;
                }),
            m_PIEContexts.end());

        m_PIEContexts.push_back(std::move(context));
    }

    void SceneManager::UnregisterPIEScene(int32_t instanceId)
    {
        Scene* pieScene = GetPIEScene(instanceId);
        if (pieScene != nullptr)
        {
            if (pieScene->HasBegunPlay())
            {
                pieScene->DispatchEndPlay(EEndPlayReason::Destroyed);
            }
            pieScene->DeinitializeWorldSubsystems();

            if (AudioSystem::HasInstance())
            {
                AudioSystem::Get().OnSceneUnloaded(pieScene);
            }

            if (PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().DestroyWorld(pieScene);
            }
        }

        // Take the context's reference out before erasing it: erasing would drop the last strong
        // reference and destroy the play world here, while this frame's draws may still point
        // into it. PIE exit is one of the paths that must not bypass the frame boundary.
        std::vector<std::shared_ptr<Scene>> retiredPIEScenes;
        for (auto it = m_PIEContexts.begin(); it != m_PIEContexts.end();)
        {
            if (it->PIEInstanceId == instanceId)
            {
                retiredPIEScenes.push_back(std::move(it->Scene));
                it = m_PIEContexts.erase(it);
                continue;
            }
            ++it;
        }

        for (std::shared_ptr<Scene>& retiredPIEScene : retiredPIEScenes)
        {
            RetireScene(std::move(retiredPIEScene));
        }
    }

    void SceneManager::RebuildSceneComponentAttachHierarchy(Scene* scene)
    {
        if (scene == nullptr)
        {
            return;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                SceneComponent* sceneComponent = Cast<SceneComponent>(component.get());
                if (sceneComponent != nullptr)
                {
                    sceneComponent->GetAttachChildren().clear();
                }
            }
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                SceneComponent* sceneComponent = Cast<SceneComponent>(component.get());
                if (sceneComponent == nullptr)
                {
                    continue;
                }

                SceneComponent* parent = sceneComponent->GetAttachParent();
                if (parent != nullptr)
                {
                    parent->GetAttachChildren().push_back(sceneComponent);
                }
            }
        }
    }

    void SceneManager::FinalizeSceneStructure(Scene* scene)
    {
        if (scene == nullptr)
        {
            return;
        }

        scene->RebuildRuntimeGameObjectIndex();
        scene->ResolveGameObjectHierarchy();
        RebuildSceneComponentAttachHierarchy(scene);

        if (HasInstance())
        {
            Get().ResolvePendingActivationsForScene(scene);
        }
    }

    void SceneManager::FinalizeLoadedScene(Scene* scene)
    {
        FinalizeSceneStructure(scene);
        if (scene == nullptr)
        {
            return;
        }

        scene->InitializeWorldSubsystems();
        scene->PostInitializeWorldSubsystems();
    }

    RenderScene* SceneManager::GetRenderScene()
    {
        Scene* tickTargetScene = GetTickTargetScene();
        if (tickTargetScene == nullptr)
        {
            return nullptr;
        }

        return tickTargetScene->GetRenderScene();
    }

    bool SceneManager::RegisterScene(const std::string& sceneName, const std::string& path)
    {
        m_RegisteredScenes[sceneName] = path;
        return true;
    }

    bool SceneManager::UnregisterScene(const std::string& sceneName)
    {
        auto it = m_RegisteredScenes.find(sceneName);
        if (it != m_RegisteredScenes.end())
        {
            m_RegisteredScenes.erase(it);
            return true;
        }
        return false;
    }

    bool SceneManager::IsSceneRegistered(const std::string& sceneName) const
    {
        return m_RegisteredScenes.find(sceneName) != m_RegisteredScenes.end();
    }

    std::shared_ptr<Scene> SceneManager::CreateNewScene(const std::string& sceneName)
    {
        UnloadActiveScene();
        m_CurrentActiveScene = NewObject<Scene>();
        m_CurrentActiveScene->SetSceneName(sceneName);
        m_CurrentActiveScene->EnsureRenderScene();
        m_CurrentActiveScene->SetSceneType(ESceneType::Editor);
        m_CurrentActiveScene->SetTickPolicy(ESceneTickPolicy::Gameplay);

        SceneContext editorContext;
        editorContext.Type = ESceneType::Editor;
        editorContext.TickPolicy = ESceneTickPolicy::Gameplay;
        editorContext.PIEInstanceId = -1;
        editorContext.Scene = m_CurrentActiveScene;
        editorContext.ContextHandle = "Editor";
        SetEditorSceneContext(std::move(editorContext));

        FinalizeLoadedScene(m_CurrentActiveScene.get());

        if (PhysicsSystem::HasInstance())
        {
            PhysicsSystem::Get().GetOrCreateWorld(m_CurrentActiveScene.get());
            PhysicsSystem::Get().RebuildWorldBodies(m_CurrentActiveScene.get());
        }
        return m_CurrentActiveScene;
    }

    bool SceneManager::LoadScene(const std::string& sceneName)
    {
        if (m_RegisteredScenes.find(sceneName) == m_RegisteredScenes.end())
        {
            return false;
        }
        const std::string& path = m_RegisteredScenes[sceneName];
        return LoadSceneByPath(path);
    }

    bool SceneManager::LoadSceneByPath(const std::string& path)
    {
        std::shared_ptr<Scene> scene = ([&]() -> std::shared_ptr<Scene> { std::string _loadAssetError; return Cast<Scene>(AssetManager::Get().LoadAssetByPath(path, _loadAssetError)); })();
        if (scene)
        {
            UnloadActiveScene();
            m_CurrentActiveScene = scene;
            m_CurrentActiveScene->EnsureRenderScene();
            m_CurrentActiveScene->SetSceneType(ESceneType::Editor);
            m_CurrentActiveScene->SetTickPolicy(ESceneTickPolicy::Gameplay);

            SceneContext editorContext;
            editorContext.Type = ESceneType::Editor;
            editorContext.TickPolicy = ESceneTickPolicy::Gameplay;
            editorContext.PIEInstanceId = -1;
            editorContext.Scene = m_CurrentActiveScene;
            editorContext.ContextHandle = "Editor";
            SetEditorSceneContext(std::move(editorContext));

            FinalizeLoadedScene(m_CurrentActiveScene.get());

            for (const std::shared_ptr<GameObject>& gameObject : m_CurrentActiveScene->GetAllGameObjects())
            {
                if (gameObject)
                {
                    for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
                    {
                        if (component)
                        {
                            MarkComponentForNeededEndOfFrameUpdate(component.get());
                        }
                        SceneComponent* sceneComponent = Cast<SceneComponent>(component.get());
                        if (sceneComponent)
                        {
                            sceneComponent->MarkRenderStateDirty();
                        }
                    }
                }
            }

            if (PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().GetOrCreateWorld(m_CurrentActiveScene.get());
                PhysicsSystem::Get().RebuildWorldBodies(m_CurrentActiveScene.get());
            }

            if (ObjectManager::HasInstance())
            {
                ObjectManager::Get().CollectGarbage();
            }
            return true;
        }
        return false;
    }

    bool SceneManager::SaveCurrentScene()
    {
        if (!m_CurrentActiveScene || m_CurrentActiveScene->GetSceneName().empty())
        {
            return false;
        }
        if (m_RegisteredScenes.find(m_CurrentActiveScene->GetSceneName()) == m_RegisteredScenes.end())
        {
            return false;
        }
        const std::string& path = m_RegisteredScenes[m_CurrentActiveScene->GetSceneName()];
        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(path);
        if (meta == nullptr || meta->AssetType != "Scene")
        {
            return false;
        }
        return AssetManager::Get().SaveAsset<Scene>(path, *m_CurrentActiveScene);
    }

    void SceneManager::MarkComponentForNeededEndOfFrameUpdate(Component* component)
    {
        if (component == nullptr)
        {
            return;
        }

        if (component->GetMarkedForNeededEndOfFrameUpdate() == ComponentMarkedForNeededEndOfFrameUpdate::Marked)
        {
            return;
        }

        component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Marked);
        m_ComponentsThatNeedEndOfFrameUpdate.push_back(component);
    }

    void SceneManager::UnmarkComponentForNeededEndOfFrameUpdate(Component* component)
    {
        if (component == nullptr)
        {
            return;
        }

        if (component->GetMarkedForNeededEndOfFrameUpdate()
            != ComponentMarkedForNeededEndOfFrameUpdate::Marked)
        {
            return;
        }

        component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Unmarked);
        m_ComponentsThatNeedEndOfFrameUpdate.erase(
            std::remove(
                m_ComponentsThatNeedEndOfFrameUpdate.begin(),
                m_ComponentsThatNeedEndOfFrameUpdate.end(),
                component),
            m_ComponentsThatNeedEndOfFrameUpdate.end());
    }

    void SceneManager::SendAllEndOfFrameUpdates()
    {
        StructuralTraversalScope scope(*this);

        std::vector<Component*> snapshot;
        snapshot.swap(m_ComponentsThatNeedEndOfFrameUpdate);

        for (Component* component : snapshot)
        {
            if (component)
            {
                component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Unmarked);
                component->DoEndOfFrameUpdate();
            }
        }
        // Intentionally do not clear m_ComponentsThatNeedEndOfFrameUpdate:
        // Marks during DoEndOfFrameUpdate stay for the next SendAll (next frame).
    }

    void SceneManager::FlushDeferredStructuralEdits()
    {
        auto flushScene = [](Scene* scene)
        {
            if (scene)
            {
                scene->FlushDeferredDestroys();
            }
        };

        flushScene(m_CurrentActiveScene.get());
        flushScene(m_EditorSceneContext.Scene.get());
        for (const SceneContext& pieContext : m_PIEContexts)
        {
            flushScene(pieContext.Scene.get());
        }
    }

    void SceneManager::RetireScene(std::shared_ptr<Scene> scene)
    {
        if (!scene)
        {
            return;
        }

        // Removal first, destruction second: a Scene may only go away once it holds no proxy, and
        // the removals that empty it are commands of this frame. Nothing is freed here - the render
        // role applies them, the frame end frees what they parked - but from this point on the
        // Scene's own teardown finds nothing left to remove.
        DetachSceneRenderState(*scene);

        std::lock_guard<std::mutex> lock(m_RetiredScenesMutex);
        m_RetiredScenes.push_back(std::move(scene));
    }

    void SceneManager::FlushRetiredScenes()
    {
        if (FrameThreading::HasInstance() && !FrameThreading::Get().IsInGameThread())
        {
            ME_LOG(
                LogCore,
                Error,
                "SceneManager::FlushRetiredScenes refused: scene destruction must run on the game "
                "frame role; the retired scenes stay queued.");
            return;
        }

        // Swap first, under the lock, and destroy outside it: a Scene destructor retires more
        // scenes (which waits for the next flush) and must not re-enter this lock. Releasing the
        // lock before the destructors also keeps a producer on another role from blocking behind
        // a teardown that is only happening because this thread asked for it.
        std::vector<std::shared_ptr<Scene>> retiredScenes;
        {
            std::lock_guard<std::mutex> lock(m_RetiredScenesMutex);
            if (m_RetiredScenes.empty())
            {
                return;
            }
            retiredScenes.swap(m_RetiredScenes);
        }

        std::vector<std::shared_ptr<Scene>> waitingForRemoval;
        for (std::shared_ptr<Scene>& scene : retiredScenes)
        {
            if (!scene)
            {
                continue;
            }
            const std::shared_ptr<RenderScene>& renderScene = scene->GetRenderSceneShared();
            if (renderScene && renderScene->HasPendingProxyWork())
            {
                // The removals this Scene was retired with have not been applied yet, so its
                // proxies are still owned or still parked. Destroying it now would free proxies a
                // frame that recorded against them may still name; the next boundary retries.
                waitingForRemoval.push_back(std::move(scene));
                continue;
            }
            scene.reset();
        }
        retiredScenes.clear();

        if (waitingForRemoval.empty())
        {
            return;
        }

        // Back in front of anything retired while these destructors ran, so the queue keeps the
        // order the scenes were retired in and the oldest one is retried first.
        std::lock_guard<std::mutex> lock(m_RetiredScenesMutex);
        m_RetiredScenes.insert(
            m_RetiredScenes.begin(),
            std::make_move_iterator(waitingForRemoval.begin()),
            std::make_move_iterator(waitingForRemoval.end()));
    }

    void SceneManager::ResolvePendingActivationsForScene(Scene* scene)
    {
        if (scene == nullptr)
        {
            return;
        }

        StructuralTraversalScope scope(*this);
        const std::vector<std::shared_ptr<GameObject>>& gameObjects = scene->GetAllGameObjects();
        const size_t goCount = gameObjects.size();

        // Phase 1: push every effective-state cache to Unknown WITHOUT evaluating, so phase 2 can
        // never read an ancestor value computed from a half-resolved hierarchy (a component may be
        // restored before the GameObjects around it). No authored flag is touched here.
        for (size_t goIndex = 0; goIndex < goCount; ++goIndex)
        {
            const std::shared_ptr<GameObject>& gameObject = gameObjects[goIndex];
            if (gameObject)
            {
                gameObject->InvalidateEffectiveStateSubtree();
            }
        }

        // Phase 2: publish the ancestor term to each component and reconcile the delta.
        // Scene load assigns m_Owner via pending ref without SetOwner, so this is the only place
        // where a deserialized component learns its owner chain.
        for (size_t goIndex = 0; goIndex < goCount; ++goIndex)
        {
            const std::shared_ptr<GameObject>& gameObject = gameObjects[goIndex];
            if (gameObject)
            {
                gameObject->ReconcileEffectiveState();
            }
        }
    }
}