#include "SpotLightComponent.h"
#include "Runtime/Function/Render/SpotLightSceneProxy.h"

namespace minEngine
{
    void SpotLightComponent::SetDirection(const Vector3 &inDirection)
    {
        if(!(m_Direction == inDirection))
        {
            m_Direction = inDirection;
            MarkRenderStateDirty();
        }
    }

    void SpotLightComponent::SetInnerConeAngle(float inInnerConeAngle)
    {
        if(!(m_InnerConeAngle == inInnerConeAngle))
        {
            m_InnerConeAngle = inInnerConeAngle < 0.0f ? 0.0f : inInnerConeAngle;
            MarkRenderStateDirty();
        }
    }

    void SpotLightComponent::SetOuterConeAngle(float inOuterConeAngle)
    {
        if(!(m_OuterConeAngle == inOuterConeAngle))
        {
            m_OuterConeAngle = inOuterConeAngle < 0.0f ? 0.0f : inOuterConeAngle;
            MarkRenderStateDirty();
        }
    }


    LightSceneProxy* SpotLightComponent::CreateSceneProxy()
    {
        SpotLightSceneProxy* proxy = new SpotLightSceneProxy();
        proxy->m_LightComponent = this;
        proxy->m_LightType = GetLightType();
        proxy->m_Position = GetPosition();
        proxy->m_Direction = GetDirection();
        proxy->m_LightColor = GetLightColor();
        proxy->m_Intensity = GetIntensity();
        proxy->m_DiffuseFactor = GetDiffuseFactor();
        proxy->m_SpecularFactor = GetSpecularFactor();
        proxy->m_InnerConeAngle = GetInnerConeAngle();
        proxy->m_OuterConeAngle = GetOuterConeAngle();
        return proxy;
    }
} // namespace minEngine

