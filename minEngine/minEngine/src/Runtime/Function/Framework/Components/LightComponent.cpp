#include "LightComponent.h"

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
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemoveLight(this);
            removedFromScene = true;
        }

        if (!removedFromScene)
        {
            m_LightSceneProxy->m_LightComponent = nullptr;
        }

        m_LightSceneProxy = nullptr;
    }

    void LightComponent::SetLightColor(const LinearColor& inColor)
    {
        const LinearColor clampedColor(
            std::clamp(inColor.R, 0.0f, 1.0f),
            std::clamp(inColor.G, 0.0f, 1.0f),
            std::clamp(inColor.B, 0.0f, 1.0f),
            std::clamp(inColor.A, 0.0f, 1.0f));
        if (!(m_LightColor == clampedColor))
        {
            m_LightColor = clampedColor;
            MarkRenderColorDirty();
        }
    }

    void LightComponent::SetIntensity(float inIntensity)
    {
        if (!(m_Intensity == inIntensity))
        {
            m_Intensity = inIntensity < 0.0f ? 0.0f : inIntensity;
            MarkRenderColorDirty();
        }
    }

    void LightComponent::SetDiffuseFactor(float inDiffuseFactor)
    {
        if (!(m_DiffuseFactor == inDiffuseFactor))
        {
            m_DiffuseFactor = inDiffuseFactor < 0.0f ? 0.0f : inDiffuseFactor;
            MarkRenderColorDirty();
        }
    }

    void LightComponent::SetSpecularFactor(float inSpecularFactor)
    {
        if (!(m_SpecularFactor == inSpecularFactor))
        {
            m_SpecularFactor = inSpecularFactor < 0.0f ? 0.0f : inSpecularFactor;
            MarkRenderColorDirty();
        }
    }

    void LightComponent::SetCastShadow(bool bInCastShadow)
    {
        if (!(m_CastShadow == bInCastShadow))
        {
            m_CastShadow = bInCastShadow;
            MarkRenderStateDirty();
        }
    }

    void LightComponent::DoEndOfFrameUpdate()
    {
        if ((!m_bRenderStateDirty && !m_bRenderTransformDirty && !m_bRenderColorDirty) || !IsActive())
        {
            return;
        }

        m_LightColor = LinearColor(
            std::clamp(m_LightColor.R, 0.0f, 1.0f),
            std::clamp(m_LightColor.G, 0.0f, 1.0f),
            std::clamp(m_LightColor.B, 0.0f, 1.0f),
            std::clamp(m_LightColor.A, 0.0f, 1.0f));
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

        RenderScene* renderScene = GetOwningRenderScene();
        if (renderScene == nullptr)
        {
            return;
        }

        LightSceneProxy* proxy = GetSceneProxy();
        if (proxy == nullptr)
        {
            renderScene->AddLight(this);
        }
        else if (m_bRenderStateDirty || proxy->m_LightType != GetLightType())
        {
            renderScene->RecreateLight(this);
        }
        else
        {
            if (m_bRenderTransformDirty)
            {
                renderScene->UpdateLightTransform(this);
            }
            if (m_bRenderColorDirty)
            {
                renderScene->UpdateLightColor(this);
            }
        }

        m_bRenderStateDirty = false;
        m_bRenderTransformDirty = false;
        m_bRenderColorDirty = false;
    }

    void LightComponent::ApplyActivationToSystems()
    {
        MarkRenderStateDirty();
    }

    void LightComponent::RemoveActivationFromSystems()
    {
        if (!m_LightSceneProxy)
        {
            m_bRenderStateDirty = false;
            m_bRenderTransformDirty = false;
            m_bRenderColorDirty = false;
            return;
        }

        bool removedFromScene = false;
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemoveLight(this);
            removedFromScene = true;
        }

        if (!removedFromScene)
        {
            m_LightSceneProxy->m_LightComponent = nullptr;
        }

        m_LightSceneProxy = nullptr;
        m_bRenderStateDirty = false;
        m_bRenderTransformDirty = false;
        m_bRenderColorDirty = false;
    }
}
