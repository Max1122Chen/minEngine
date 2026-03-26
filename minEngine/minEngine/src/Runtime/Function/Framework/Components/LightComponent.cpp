#include "LightComponent.h"
#include "Runtime/Function/Framework/World/WorldManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"

namespace minEngine
{
    LightComponent::LightComponent()
    {
    }

    LightComponent::~LightComponent()
    {
        if (!m_LightSceneProxy)
        {
            return;
        }

        bool removedFromScene = false;
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetRuntimeGlobalContext();
        if (globalContext.m_WorldManager)
        {
            RenderScene* renderScene = globalContext.m_WorldManager->GetRenderScene();
            if (renderScene)
            {
                renderScene->RemoveLight(this);
                removedFromScene = true;
            }
        }

        if (!removedFromScene)
        {
            m_LightSceneProxy->m_LightComponent = nullptr;
        }

        m_LightSceneProxy = nullptr;
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

    void LightComponent::SetCastShadow(bool bInCastShadow)
    {
        if(!(m_CastShadow == bInCastShadow))
        {
            m_CastShadow = bInCastShadow;
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
