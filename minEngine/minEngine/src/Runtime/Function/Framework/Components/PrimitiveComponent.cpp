#include "PrimitiveComponent.h"

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
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemovePrimitive(this);
            removedFromScene = true;
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

        RenderScene* renderScene = GetOwningRenderScene();
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
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemovePrimitive(this);
            removedFromScene = true;
        }

        if (!removedFromScene)
        {
            m_SceneProxy->m_PrimitiveComponent = nullptr;
        }

        m_SceneProxy = nullptr;
        m_bRenderStateDirty = false;
    }

}
