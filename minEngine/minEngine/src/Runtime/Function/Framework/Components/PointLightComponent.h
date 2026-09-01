#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"

namespace minEngine
{
    class LightComponent;
    class LightSceneProxy;

    ME_CLASS()
    class PointLightComponent : public LightComponent
    {
        ME_GENERATED_BODY(PointLightComponent)
    public:
        PointLightComponent();
        virtual ~PointLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Point; }

        float GetAttenuationRadius() const { return m_AttenuationRadius; }
        void SetAttenuationRadius(float attenuationRadius);

        float GetAttenuationFalloff() const { return m_AttenuationFalloff; }
        void SetAttenuationFalloff(float attenuationFalloff);

        virtual LightSceneProxy* CreateSceneProxy() override;

    protected:
        /** World-space influence radius; lighting and shadow fade to zero beyond this distance. */
        ME_PROPERTY(EditAnywhere)
        float m_AttenuationRadius{10.0f};

        /** Window falloff exponent applied to (1 - dist/radius); 2.0 approximates inverse-square feel. */
        ME_PROPERTY(EditAnywhere)
        float m_AttenuationFalloff{2.0f};
    };
}

#include "Generated/Reflection/PointLightComponent.gen.h"