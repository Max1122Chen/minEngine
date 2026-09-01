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

        RenderScene* renderScene = SceneManager::HasInstance() ? SceneManager::Get().GetRenderScene() : nullptr;
        if (renderScene)
        {
            renderScene->RemoveSkyBox(this);
        }
        else if (m_SkyBoxSceneProxy)
        {
            m_SkyBoxSceneProxy->m_SkyBoxComponent = nullptr;
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
        if (!m_bRenderStateDirty || !IsActive())
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

        if (SceneManager::HasInstance())
        {
            RenderScene* renderScene = SceneManager::Get().GetRenderScene();
            if (renderScene)
            {
                renderScene->RemoveSkyBox(this);
            }
            else if (m_SkyBoxSceneProxy)
            {
                m_SkyBoxSceneProxy->m_SkyBoxComponent = nullptr;
                DetachSceneProxy();
            }
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
        proxy->m_Transform = GetTransform();
        proxy->m_SkyIntensity = m_SkyIntensity;
        proxy->m_Enabled = IsActive();
        proxy->m_EnvironmentMap = m_EnvironmentMap;
        m_SkyBoxSceneProxy = proxy;
        return proxy;
    }
}
