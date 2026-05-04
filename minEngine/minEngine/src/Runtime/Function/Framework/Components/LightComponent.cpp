#include "LightComponent.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"

#include <algorithm>

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
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::Get();
        if (globalContext.m_SceneManager)
        {
            RenderScene* renderScene = globalContext.m_SceneManager->GetRenderScene();
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
        Vector4 clampedColor = glm::clamp(inColor, Vector4(0.0f), Vector4(1.0f));
        if(!(m_LightColor == clampedColor))
        {
            m_LightColor = clampedColor;
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
            Vector4 clampedColor = glm::clamp(m_LightColor, Vector4(0.0f), Vector4(1.0f));
            m_LightColor = clampedColor;
            if (m_Intensity < 0.0f)
            {
                m_Intensity = 0.0f;
            }
            if (m_DiffuseFactor < 0.0f)
            {
                m_DiffuseFactor = 0.0f;
            }
            if (m_SpecularFactor < 0.0f)
            {
                m_SpecularFactor = 0.0f;
            }

            SceneManager::Get().GetRenderScene()->UpdateLight(this);
            m_bRenderStateDirty = false;
        }
    }
}
