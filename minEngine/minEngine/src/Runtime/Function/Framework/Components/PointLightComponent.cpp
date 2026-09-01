#include "PointLightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"

namespace minEngine
{
    PointLightComponent::PointLightComponent()
    {
    }

    void PointLightComponent::SetAttenuationRadius(float attenuationRadius)
    {
        const float clampedRadius = attenuationRadius < 0.0f ? 0.0f : attenuationRadius;
        if (m_AttenuationRadius == clampedRadius)
        {
            return;
        }

        m_AttenuationRadius = clampedRadius;
        MarkRenderStateDirty();
    }

    void PointLightComponent::SetAttenuationFalloff(float attenuationFalloff)
    {
        const float clampedFalloff = attenuationFalloff < 0.0f ? 0.0f : attenuationFalloff;
        if (m_AttenuationFalloff == clampedFalloff)
        {
            return;
        }

        m_AttenuationFalloff = clampedFalloff;
        MarkRenderStateDirty();
    }

    LightSceneProxy* PointLightComponent::CreateSceneProxy()
    {
        PointLightSceneProxy* proxy = new PointLightSceneProxy();
        proxy->m_LightComponent = this;
        proxy->m_LightType = GetLightType();
        proxy->m_Position = GetPosition();
        proxy->m_LightColor = GetLightColor();
        proxy->m_Intensity = GetIntensity();
        proxy->m_DiffuseFactor = GetDiffuseFactor();
        proxy->m_SpecularFactor = GetSpecularFactor();
        proxy->m_CastsShadow = CastShadow();
        proxy->m_AttenuationRadius = GetAttenuationRadius();
        proxy->m_AttenuationFalloff = GetAttenuationFalloff();
        m_LightSceneProxy = proxy;
        return proxy;
    }
}
