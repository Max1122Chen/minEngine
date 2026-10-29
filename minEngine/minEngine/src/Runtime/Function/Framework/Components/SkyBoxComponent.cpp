#include "SkyBoxComponent.h"

#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"

namespace minEngine
{
    SkyBoxComponent::SkyBoxComponent()
    {
        MarkRenderStateDirty();
    }

    SkyBoxComponent::~SkyBoxComponent()
    {
        if (!m_SkyBoxSceneProxy)
        {
            return;
        }

        // The identity token is written only by the removal command on the render role. A clear
        // here would take the key that command matches on, and the sky would stay installed until
        // the scene it belongs to is torn down.
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemoveSkyBox(this);
        }

        DetachSceneProxy();
    }

    void SkyBoxComponent::SetSkyIntensity(float intensity)
    {
        const float clamped = intensity < 0.0f ? 0.0f : intensity;
        if (m_SkyIntensity != clamped)
        {
            m_SkyIntensity = clamped;
            MarkRenderStateDirty();
        }
    }

    void SkyBoxComponent::SetEnvironmentMap(const std::shared_ptr<EnvironmentMap>& environmentMap)
    {
        if (m_EnvironmentMap != environmentMap)
        {
            m_EnvironmentMap = environmentMap;
            MarkRenderStateDirty();
        }
    }

    void SkyBoxComponent::DoEndOfFrameUpdate()
    {
        if ((!m_bRenderStateDirty && !m_bRenderTransformDirty) || !IsActive())
        {
            return;
        }

        if (RenderScene* renderScene = GetOwningRenderScene())
        {
            if (GetSceneProxy() == nullptr)
            {
                renderScene->SetSkyBox(this);
            }
            else
            {
                // MVP: property / transform changes recreate (no Sky snapshot packet).
                renderScene->RecreateSkyBox(this);
            }
        }

        m_bRenderStateDirty = false;
        m_bRenderTransformDirty = false;
    }

    void SkyBoxComponent::ApplyActivationToSystems()
    {
        MarkRenderStateDirty();
    }

    void SkyBoxComponent::RemoveActivationFromSystems()
    {
        if (!m_SkyBoxSceneProxy)
        {
            m_bRenderStateDirty = false;
            return;
        }

        // Same rule as the destructor: the proxy keeps its token, so the removal queued for it -
        // here, or by a scene teardown that still knows this component - still finds it.
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemoveSkyBox(this);
        }
        else
        {
            DetachSceneProxy();
        }

        m_bRenderStateDirty = false;
    }

    SkyBoxSceneProxy* SkyBoxComponent::CreateSceneProxy()
    {
        SkyBoxSceneProxy* proxy = new SkyBoxSceneProxy();
        proxy->m_SkyBoxComponent = this;
        proxy->m_Transform = GetWorldTransform();
        proxy->m_SkyIntensity = m_SkyIntensity;
        proxy->m_Enabled = IsActiveAndVisible();
        proxy->m_EnvironmentMap = m_EnvironmentMap;
        m_SkyBoxSceneProxy = proxy;
        return proxy;
    }
}
