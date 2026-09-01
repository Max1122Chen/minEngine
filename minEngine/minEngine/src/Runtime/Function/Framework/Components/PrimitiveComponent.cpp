#include "PrimitiveComponent.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
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
        if (!m_bRenderStateDirty || !IsActive())
        {
            return;
        }

        RenderScene* renderScene = SceneManager::Get().GetRenderScene();
        if (renderScene == nullptr)
        {
            return;
        }

        renderScene->UpdatePrimitive(this);
        m_bRenderStateDirty = false;
    }

    void PrimitiveComponent::ApplyActivationToSystems()
    {
        MarkRenderStateDirty();
    }

    void PrimitiveComponent::RemoveActivationFromSystems()
    {
        if (!m_SceneProxy)
        {
            return;
        }

        bool removedFromScene = false;
        if (SceneManager::HasInstance())
        {
            SceneManager& sceneManager = SceneManager::Get();
            if (sceneManager.GetRenderScene())
            {
                sceneManager.GetRenderScene()->RemovePrimitive(this);
                removedFromScene = true;
            }
        }

        if (!removedFromScene)
        {
            m_SceneProxy->m_PrimitiveComponent = nullptr;
        }

        m_SceneProxy = nullptr;
        m_bRenderStateDirty = false;
    }

}
