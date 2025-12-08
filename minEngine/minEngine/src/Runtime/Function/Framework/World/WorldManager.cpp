#include "WorldManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Framework/Level/Level.h"

namespace minEngine
{
    void WorldManager::Initialize()
    {
        m_RenderScene = RuntimeGlobalContext::GetInstance().m_RenderSystem->m_RenderScene.get();
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
        m_RenderScene->m_PrimitiveSceneProxies.clear();
        for(Component* component : m_ComponentsThatNeedEndOfFrameUpdate)
        {
            if(component)
            {
                component->SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate::Unmarked);
                PrimitiveComponent* primComp = dynamic_cast<PrimitiveComponent*>(component);
                if(primComp)
                {
                    PrimitiveSceneProxy* proxy = primComp->CreateSceneProxy();
                    if (proxy)
                    {
                        m_RenderScene->m_PrimitiveSceneProxies.push_back(proxy);
                    }
                }
            }
        }
        // m_ComponentsThatNeedEndOfFrameUpdate.clear();    // TODO：discomment after testing
    }

} // namespace minEngine
