#include "SceneManager.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    SceneManager* SceneManager::s_Instance = nullptr;

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
        m_ComponentsThatNeedEndOfFrameUpdate.clear();
        m_PIEPlayActive = false;
        m_ActiveSceneOverride = nullptr;

        for (const SceneContext& pieContext : m_PIEContexts)
        {
            if (pieContext.Scene && PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().DestroyWorld(pieContext.Scene.get());
            }
        }
        m_PIEContexts.clear();

        UnloadActiveScene();
        m_EditorSceneContext = {};
        m_RegisteredScenes.clear();
        ME_CORE_INFO("SceneManager Shutdown.");
    }

    void SceneManager::UnloadActiveScene()
    {
        if (m_CurrentActiveScene)
        {
            if (AudioSystem::HasInstance())
            {
                AudioSystem::Get().OnSceneUnloaded(m_CurrentActiveScene.get());
            }

            if (PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().DestroyWorld(m_CurrentActiveScene.get());
            }
        }

        m_CurrentActiveScene.reset();

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().CollectGarbage();
        }
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
            if (AudioSystem::HasInstance())
            {
                AudioSystem::Get().OnSceneUnloaded(pieScene);
            }

            if (PhysicsSystem::HasInstance())
            {
                PhysicsSystem::Get().DestroyWorld(pieScene);
            }
        }

        m_PIEContexts.erase(
            std::remove_if(
                m_PIEContexts.begin(),
                m_PIEContexts.end(),
                [instanceId](const SceneContext& context)
                {
                    return context.PIEInstanceId == instanceId;
                }),
            m_PIEContexts.end());
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
                SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
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
                SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
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

    void SceneManager::FinalizeLoadedScene(Scene* scene)
    {
        if (scene == nullptr)
        {
            return;
        }

        scene->RebuildRuntimeGameObjectIndex();
        RebuildSceneComponentAttachHierarchy(scene);

        if (HasInstance())
        {
            Get().ResolvePendingActivationsForScene(scene);
        }
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
        m_CurrentActiveScene->m_SceneName = sceneName;
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

        ResolvePendingActivationsForScene(m_CurrentActiveScene.get());
        RebuildSceneComponentAttachHierarchy(m_CurrentActiveScene.get());

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
        std::shared_ptr<Scene> scene = AssetManager::Get().LoadAsset<Scene>(path);
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
                        SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
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
        if (!m_CurrentActiveScene || m_CurrentActiveScene->m_SceneName.empty())
        {
            return false;
        }
        if (m_RegisteredScenes.find(m_CurrentActiveScene->m_SceneName) == m_RegisteredScenes.end())
        {
            return false;
        }
        const std::string& path = m_RegisteredScenes[m_CurrentActiveScene->m_SceneName];
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

    void SceneManager::SendAllEndOfFrameUpdates()
    {
        for (Component* component : m_ComponentsThatNeedEndOfFrameUpdate)
        {
            if (component)
            {
                component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Unmarked);
                component->DoEndOfFrameUpdate();
            }
        }
        m_ComponentsThatNeedEndOfFrameUpdate.clear();
    }

    void SceneManager::ResolvePendingActivationsForScene(Scene* scene)
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
                if (component)
                {
                    // Reconcile runtime activation with m_bActive after deserialize.
                    // Scene load assigns m_Owner via pending ref without SetOwner (TD-026).
                    component->SyncActivationWithActiveFlag();
                }
            }
        }
    }
}
