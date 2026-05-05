#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/PointLightComponent.h"

namespace minEngine
{

    ME_CLASS()
    class SpotLightComponent : public PointLightComponent
    {
        ME_GENERATED_BODY(SpotLightComponent)

    public:
        SpotLightComponent() = default;
        virtual ~SpotLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Spot; }

        Vector3 GetDirection() const;

        float GetInnerConeAngle() const { return m_InnerConeAngle; }
        void SetInnerConeAngle(float inInnerConeAngle);

        float GetOuterConeAngle() const { return m_OuterConeAngle; }
        void SetOuterConeAngle(float inOuterConeAngle);

        virtual LightSceneProxy* CreateSceneProxy() override;

    protected:
        ME_PROPERTY(EditAnywhere)
        float m_InnerConeAngle{ 15.0f }; // degrees

        ME_PROPERTY(EditAnywhere)
        float m_OuterConeAngle{ 20.0f }; // degrees
    };
}

#include "Generated/Reflection/SpotLightComponent.gen.h"