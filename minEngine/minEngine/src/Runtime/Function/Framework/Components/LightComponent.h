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

    ME_CLASS()
    class LightComponent : public SceneComponent
    {
        ME_REFLECTION_FRIEND(LightComponent)

    public:
        LightComponent();
        virtual ~LightComponent() override;

        virtual LightType GetLightType() const = 0;

        void SetLightColor(const Vector4& inColor);
        Vector4 GetLightColor() const { return m_LightColor; }
        
        void SetIntensity(float inIntensity);
        float GetIntensity() const { return m_Intensity; }

        void SetDiffuseFactor(float inDiffuseFactor);
        float GetDiffuseFactor() const { return m_DiffuseFactor; }

        void SetSpecularFactor(float inSpecularFactor);
        float GetSpecularFactor() const { return m_SpecularFactor; }

        void SetCastShadow(bool bInCastShadow);
        bool CastShadow() const { return m_CastShadow; }

        virtual void DoEndOfFrameUpdate() override;

        virtual LightSceneProxy* CreateSceneProxy() = 0;
        LightSceneProxy* GetSceneProxy() const { return m_LightSceneProxy; }

        // properties
        ME_PROPERTY(EditAnywhere)
        Vector4 m_LightColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        ME_PROPERTY(EditAnywhere)
        float m_Intensity{ 1.0f };

        ME_PROPERTY(EditAnywhere)
        float m_DiffuseFactor{ 1.0f };

        ME_PROPERTY(EditAnywhere)
        float m_SpecularFactor{ 1.0f };

        ME_PROPERTY(EditAnywhere)
        bool m_CastShadow{ false };
        

        //
        LightSceneProxy* m_LightSceneProxy{ nullptr };
    };
}

#include "Generated/Reflection/LightComponent.gen.h"