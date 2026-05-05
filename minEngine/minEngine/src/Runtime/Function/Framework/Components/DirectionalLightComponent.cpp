#include "DirectionalLightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"

namespace minEngine
{
    Vector3 DirectionalLightComponent::GetDirection() const
    {
        return -GetUpVector();
    }

    LightSceneProxy *DirectionalLightComponent::CreateSceneProxy()
    {
        DirectionalLightSceneProxy* proxy = new DirectionalLightSceneProxy();
        proxy->m_LightComponent = this;
        proxy->m_LightType = GetLightType();
        proxy->m_Direction = GetDirection();
        proxy->m_LightColor = GetLightColor();
        proxy->m_Intensity = GetIntensity();
        proxy->m_DiffuseFactor = GetDiffuseFactor();
        proxy->m_SpecularFactor = GetSpecularFactor();
        proxy->m_CastsShadow = CastShadow();
        m_LightSceneProxy = proxy;
        return proxy;
    }
}
