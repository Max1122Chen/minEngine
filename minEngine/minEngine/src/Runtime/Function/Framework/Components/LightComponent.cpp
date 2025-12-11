#include "LightComponent.h"
#include "Runtime/Function/Framework/World/WorldManager.h"
#include "Runtime/Function/Render/RenderScene.h"

namespace minEngine
{
    LightComponent::LightComponent()
    {
    }

    void LightComponent::SetLightColor(const Vector4 &inColor)
    {
        if(!(m_LightColor == inColor))
        {
            m_LightColor = inColor;
            MarkRenderStateDirty();
        }
    }
    
    void LightComponent::SetIntensity(float inIntensity)
    {
        if(!(m_Intensity == inIntensity))
        {
            m_Intensity = inIntensity < 0.0f ? 0.0f : inIntensity;
            MarkRenderStateDirty();
        }
    }
    
    void LightComponent::SetDiffuseFactor(float inDiffuseFactor)
    {
        if(!(m_DiffuseFactor == inDiffuseFactor))
        {
            m_DiffuseFactor = inDiffuseFactor < 0.0f ? 0.0f : inDiffuseFactor;
            MarkRenderStateDirty();
        }
    }

    void LightComponent::SetSpecularFactor(float inSpecularFactor)
    {
        if(!(m_SpecularFactor == inSpecularFactor))
        {
            m_SpecularFactor = inSpecularFactor < 0.0f ? 0.0f : inSpecularFactor;
            MarkRenderStateDirty();
        }
    }
    void LightComponent::DoEndOfFrameUpdate()
    {
        if(m_bRenderStateDirty)     
        {
            WorldManager::GetWorldManager().GetRenderScene()->UpdateLight(this);
            m_bRenderStateDirty = false;
        }
    }
}
