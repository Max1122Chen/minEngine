#include "SceneManager.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
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
        UnloadActiveScene();
        m_RegisteredScenes.clear();
        ME_CORE_INFO("SceneManager Shutdown.");
    }

    void SceneManager::UnloadActiveScene()
    {
        if (m_CurrentActiveScene && PhysicsSystem::HasInstance())
        {
            PhysicsSystem::Get().DestroyWorld(m_CurrentActiveScene.get());
        }

        m_CurrentActiveScene.reset();

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().CollectGarbage();
        }
    }

    void SceneManager::Tick(float deltaTime)
    {
        if (m_CurrentActiveScene)
        {
            m_CurrentActiveScene->Tick(deltaTime);
        }
    }

    RenderScene* SceneManager::GetRenderScene()
    {
        if (!m_CurrentActiveScene)
        {
            return nullptr;
        }
        return m_CurrentActiveScene->GetRenderScene();
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
        if (PhysicsSystem::HasInstance())
        {
            PhysicsSystem::Get().GetOrCreateWorld(m_CurrentActiveScene.get());
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
}
