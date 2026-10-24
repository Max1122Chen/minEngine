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
        if (!IsActive())
        {
            return;
        }
        if (!m_bRenderStateDirty && !m_bRenderTransformDirty)
        {
            return;
        }

        RenderScene* renderScene = GetOwningRenderScene();
        if (renderScene == nullptr)
        {
            return;
        }

        if (GetSceneProxy() == nullptr)
        {
            renderScene->AddPrimitive(this);
        }
        else if (m_bRenderStateDirty || ShouldRecreateSceneProxy())
        {
            renderScene->RecreatePrimitive(this);
        }
        else if (m_bRenderTransformDirty)
        {
            renderScene->UpdatePrimitiveTransform(this);
        }

        m_bRenderStateDirty = false;
        m_bRenderTransformDirty = false;
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
        m_bRenderTransformDirty = false;
    }

    void PrimitiveComponent::SyncSceneProxy(PrimitiveSceneProxy& proxy)
    {
        proxy.m_PrimitiveComponent = this;
        proxy.m_Transform = GetWorldTransform();
        proxy.m_CastShadow = CastShadow();
    }

}
