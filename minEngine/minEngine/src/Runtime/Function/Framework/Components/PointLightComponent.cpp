#include "PointLightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"

namespace minEngine
{
    PointLightComponent::PointLightComponent()
    {
    }


    LightSceneProxy* PointLightComponent::CreateSceneProxy()
    {
        PointLightSceneProxy* proxy = new PointLightSceneProxy();
        // TODO: maybe wrap the belong logic later
        proxy->m_LightComponent = this;
        proxy->m_LightType = GetLightType();
        proxy->m_Position = GetPosition();
        proxy->m_LightColor = GetLightColor();
        proxy->m_Intensity = GetIntensity();
        proxy->m_DiffuseFactor = GetDiffuseFactor();
        proxy->m_SpecularFactor = GetSpecularFactor();
        proxy->m_CastsShadow = CastShadow();
        m_LightSceneProxy = proxy;
        return proxy;
    }
}

