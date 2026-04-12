#include "SceneManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/SceneSerializer.h"

namespace minEngine
{
    void SceneManager::Initialize()
    {
        m_RenderScene = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->m_RenderScene.get();
    }

    void SceneManager::Shutdown()
    {
        m_ComponentsThatNeedEndOfFrameUpdate.clear();
        m_CurrentActiveScene.reset();
        m_RenderScene = nullptr;
    }

    void SceneManager::Tick(float deltaTime)
    {
        if (m_CurrentActiveScene)
        {
            m_CurrentActiveScene->Tick(deltaTime);
        }
    }

    std::shared_ptr<Scene> SceneManager::CreateNewScene(const std::string& sceneName)
    {
        m_CurrentActiveScene = std::make_shared<Scene>();
        m_CurrentActiveScene->sceneName = sceneName;
        return m_CurrentActiveScene;
    }

    bool SceneManager::LoadScene(const std::string& sceneName)
    {
        if (sceneName.empty())
        {
            return false;
        }

        std::shared_ptr<Scene> loadedScene = std::make_shared<Scene>();
        if (!SceneSerializer::LoadScene(sceneName, *loadedScene))
        {
            return false;
        }

        if (loadedScene->sceneName.empty())
        {
            loadedScene->sceneName = sceneName;
        }

        m_CurrentActiveScene = std::move(loadedScene);
        return true;
    }

    bool SceneManager::SaveScene(const std::string& sceneName)
    {
        if (!m_CurrentActiveScene)
        {
            return false;
        }

        std::string outputPath = sceneName;
        if (outputPath.empty())
        {
            outputPath = m_CurrentActiveScene->sceneName;
        }

        if (outputPath.empty())
        {
            return false;
        }

        if (!SceneSerializer::SaveScene(outputPath, *m_CurrentActiveScene))
        {
            return false;
        }

        m_CurrentActiveScene->sceneName = outputPath;
        return true;
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
