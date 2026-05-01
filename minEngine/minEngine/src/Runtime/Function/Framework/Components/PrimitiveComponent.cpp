#include "PrimitiveComponent.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
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
        SceneManager& sceneManager = SceneManager::Get();
        if (sceneManager.GetRenderScene())
        {
            RenderScene* renderScene = sceneManager.GetRenderScene();
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
            SceneManager::Get().GetRenderScene()->UpdatePrimitive(this);
            m_bRenderStateDirty = false;
        }
    }

}
