#include "SceneManager.h"
#include "Runtime/Function/Framework/Components/Component.h"

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
