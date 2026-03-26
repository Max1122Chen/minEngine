#include "PrimitiveComponent.h"
#include "Runtime/Function/Framework/World/WorldManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"

namespace minEngine
{
    PrimitiveComponent::PrimitiveComponent()
    {
    }

    PrimitiveComponent::~PrimitiveComponent()
    {
        if (!m_SceneProxy)
        {
            return;
        }

        bool removedFromScene = false;
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetRuntimeGlobalContext();
        if (globalContext.m_WorldManager)
        {
            RenderScene* renderScene = globalContext.m_WorldManager->GetRenderScene();
            if (renderScene)
            {
                renderScene->RemovePrimitive(this);
                removedFromScene = true;
            }
        }

        if (!removedFromScene)
        {
            m_SceneProxy->m_PrimitiveComponent = nullptr;
        }

        m_SceneProxy = nullptr;
    }

    void PrimitiveComponent::DoEndOfFrameUpdate()
    {
        if(m_bRenderStateDirty)     // why do we need this check again? 
        {
            WorldManager::GetWorldManager().GetRenderScene()->UpdatePrimitive(this);
            m_bRenderStateDirty = false;
        }
    }

}
