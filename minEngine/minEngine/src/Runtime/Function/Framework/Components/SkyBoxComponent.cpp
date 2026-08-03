#include "SkyBoxComponent.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
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

        RenderScene* renderScene = SceneManager::Get().GetRenderScene();
        if (renderScene)
        {
            renderScene->RemoveSkyBox(this);
        }
        else
        {
            m_SkyBoxSceneProxy->m_SkyBoxComponent = nullptr;
        }

        m_SkyBoxSceneProxy = nullptr;
    }

    void SkyBoxComponent::SetEnabled(bool enabled)
    {
        if (m_Enabled != enabled)
        {
            m_Enabled = enabled;
            MarkRenderStateDirty();
        }
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
        if (!m_bRenderStateDirty)
        {
            return;
        }

        RenderScene* renderScene = SceneManager::Get().GetRenderScene();
        if (renderScene)
        {
            renderScene->UpdateSkyBox(this);
        }

        m_bRenderStateDirty = false;
    }

    SkyBoxSceneProxy* SkyBoxComponent::CreateSceneProxy()
    {
        SkyBoxSceneProxy* proxy = new SkyBoxSceneProxy();
        proxy->m_SkyBoxComponent = this;
        proxy->m_Transform = GetTransform();
        proxy->m_SkyIntensity = m_SkyIntensity;
        proxy->m_Enabled = m_Enabled;
        proxy->m_EnvironmentMap = m_EnvironmentMap;
        m_SkyBoxSceneProxy = proxy;
        return proxy;
    }
}
