#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    enum class LightType : uint8_t
    {
        Directional,
        Point,
        Spot
    };

    class SceneComponent;
    class LightSceneProxy;

    class LightComponent : public SceneComponent
    {
    public:
        LightComponent();
        virtual ~LightComponent() = default;

        virtual LightType GetLightType() const = 0;

        void SetLightColor(const Vector4& inColor);
        Vector4 GetLightColor() const { return m_LightColor; }
        
        void SetIntensity(float inIntensity);
        float GetIntensity() const { return m_Intensity; }

        void SetDiffuseFactor(float inDiffuseFactor);
        float GetDiffuseFactor() const { return m_DiffuseFactor; }

        void SetSpecularFactor(float inSpecularFactor);
        float GetSpecularFactor() const { return m_SpecularFactor; }

        virtual void DoEndOfFrameUpdate() override;

        virtual LightSceneProxy* CreateSceneProxy() = 0;
        LightSceneProxy* GetSceneProxy() const { return m_LightSceneProxy; }

        // properties
        Vector4 m_LightColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        float m_Intensity{ 1.0f };

        float m_DiffuseFactor{ 1.0f };
        float m_SpecularFactor{ 1.0f };
        

        //
        LightSceneProxy* m_LightSceneProxy{ nullptr };
    };
}