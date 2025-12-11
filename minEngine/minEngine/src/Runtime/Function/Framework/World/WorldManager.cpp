#include "WorldManager.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    void WorldManager::Initialize()
    {
        m_RenderScene = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->m_RenderScene.get();
    }

    void WorldManager::Shutdown()
    {
    }

    void WorldManager::Tick(float deltaTime)
    {
        if (m_CurrentActiveLevel)
        {
            m_CurrentActiveLevel->Tick(deltaTime);
        }
    }

    void WorldManager::MarkComponentForNeededEndOfFrameUpdate(Component *component)
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

    void WorldManager::SendAllEndOfFrameUpdates()
    {
        for(Component* component : m_ComponentsThatNeedEndOfFrameUpdate)
        {
            if(component)
            {
                component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Unmarked);

                component->DoEndOfFrameUpdate();
            }
        }
        // m_ComponentsThatNeedEndOfFrameUpdate.clear();    // TODO：discomment after testing
    }

} // namespace minEngine
