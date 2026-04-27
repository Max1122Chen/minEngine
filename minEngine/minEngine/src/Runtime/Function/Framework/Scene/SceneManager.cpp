#include "SceneManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Resource/AssetManager.h"

#include <filesystem>

namespace minEngine
{
    void SceneManager::Initialize()
    {
        m_RenderScene = RuntimeGlobalContext::Get().m_RenderSystem->m_RenderScene.get();
    }

    void SceneManager::Shutdown()
    {
        m_ComponentsThatNeedEndOfFrameUpdate.clear();
        m_CurrentActiveScene.reset();
        m_RegisteredScenes.clear();
        m_RenderScene = nullptr;
    }

    void SceneManager::Tick(float deltaTime)
    {
        if (m_CurrentActiveScene)
        {
            m_CurrentActiveScene->Tick(deltaTime);
        }
    }

    bool SceneManager::RegisterScene(const std::string &sceneName, const std::string &path)
    {
        m_RegisteredScenes[sceneName] = path;
        return true;
    }

    bool SceneManager::UnregisterScene(const std::string &sceneName)
    {
        auto it = m_RegisteredScenes.find(sceneName);
        if (it != m_RegisteredScenes.end())
        {
            m_RegisteredScenes.erase(it);
            return true;
        }
        return false;
    }

    std::shared_ptr<Scene> SceneManager::CreateNewScene(const std::string &sceneName)
    {
        // TODO: change this logic
        m_CurrentActiveScene = NewObject<Scene>();
        m_CurrentActiveScene->m_SceneName = sceneName;
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
            m_CurrentActiveScene = scene;
            // Mark all scene components for end of frame update to make sure the render scene gets updated with the new scene's data
            for (const std::shared_ptr<GameObject>& gameObject : m_CurrentActiveScene->GetGameObjects())
            {
                if (gameObject)
                {
                    for (const std::shared_ptr<Component>& component : gameObject->GetComponents())
                    {
                        if (component)
                        {
                            MarkComponentForNeededEndOfFrameUpdate(component.get());
                        }
                        SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
                        if(sceneComponent)
                        {
                            sceneComponent->MarkRenderStateDirty();
                        }
                    }
                }
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
        if ( m_CurrentActiveScene->m_SceneName.empty())
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

    void SceneManager::MarkComponentForNeededEndOfFrameUpdate(Component *component)
    {
        if(component == nullptr) return;

        if(component->GetMarkedForNeededEndOfFrameUpdate() == ComponentMarkedForNeededEndOfFrameUpdate::Marked)
        {
            return;
        }
        else
        {
            // Mark the component and add to the list
            component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Marked);
            m_ComponentsThatNeedEndOfFrameUpdate.push_back(component);
        }
    }

    void SceneManager::SendAllEndOfFrameUpdates()
    {
        for(Component* component : m_ComponentsThatNeedEndOfFrameUpdate)
        {
            if(component)
            {
                component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Unmarked);

                component->DoEndOfFrameUpdate();
            }
        }
        m_ComponentsThatNeedEndOfFrameUpdate.clear();
    }

} // namespace minEngine
