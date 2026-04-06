#include "SceneManager.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "SceneSerializer.h"

namespace
{
    std::filesystem::path ResolveScenePath(const std::string& sceneName)
    {
        std::filesystem::path scenePath(sceneName);
        if (scenePath.extension().empty())
        {
            scenePath += ".scene.json";
        }

        if (!scenePath.has_parent_path())
        {
            scenePath = std::filesystem::path("Assets/Scenes") / scenePath;
        }

        return scenePath;
    }
}

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
        std::filesystem::path scenePath = ResolveScenePath(sceneName);

        std::shared_ptr<Scene> newScene = std::make_shared<Scene>();
        newScene->sceneName = scenePath.string();

        auto createSampleGameObject = [&newScene](const std::string& objectName, const Vector3& position, const Vector3& rotation, const Vector3& scale)
        {
            std::shared_ptr<GameObject> gameObject = newScene->CreateGameObject();
            if (!gameObject)
            {
                return;
            }

            gameObject->SetName(objectName);

            std::shared_ptr<ReflectionSampleComponent> sampleComponent = gameObject->CreateAndAddComponent<ReflectionSampleComponent>();
            if (!sampleComponent)
            {
                return;
            }

            sampleComponent->Position = position;
            sampleComponent->Rotation = rotation;
            sampleComponent->Scale = scale;
        };

        createSampleGameObject("MainCamera", Vector3(0.0f, 1.5f, -6.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));
        createSampleGameObject("DirectionalLight", Vector3(2.0f, 4.0f, 1.0f), Vector3(-35.0f, 20.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));
        createSampleGameObject("Cube_A", Vector3(-1.5f, 0.5f, 0.0f), Vector3(0.0f, 30.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));
        createSampleGameObject("Cube_B", Vector3(1.5f, 0.5f, 0.0f), Vector3(0.0f, -15.0f, 0.0f), Vector3(1.25f, 1.25f, 1.25f));
        createSampleGameObject("Ground", Vector3(0.0f, -0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(8.0f, 1.0f, 8.0f));

        m_CurrentActiveScene = newScene;

        if (!SceneSerializer::SaveScene(*newScene, scenePath))
        {
            ME_CORE_WARN("[SceneManager] Failed to save new scene '{}'", scenePath.string());
        }

        return m_CurrentActiveScene;
    }

    void SceneManager::LoadScene(const std::string& sceneName)
    {
        std::filesystem::path scenePath = ResolveScenePath(sceneName);

        std::shared_ptr<Scene> loadedScene = std::make_shared<Scene>();
        if (!SceneSerializer::LoadScene(scenePath, *loadedScene))
        {
            ME_CORE_ERROR("[SceneManager] Failed to load scene '{}'", scenePath.string());
            return;
        }

        loadedScene->sceneName = scenePath.string();
        m_CurrentActiveScene = loadedScene;
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
