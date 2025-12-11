#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/PointLightComponent.h"

namespace minEngine
{

    class SpotLightComponent : public PointLightComponent
    {
    public:
        SpotLightComponent() = default;
        virtual ~SpotLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Spot; }

        const Vector3& GetDirection() const { return m_Direction; }
        void SetDirection(const Vector3& inDirection);

        float GetInnerConeAngle() const { return m_InnerConeAngle; }
        void SetInnerConeAngle(float inInnerConeAngle);

        float GetOuterConeAngle() const { return m_OuterConeAngle; }
        void SetOuterConeAngle(float inOuterConeAngle);

        virtual LightSceneProxy* CreateSceneProxy() override;

    protected:
        Vector3 m_Direction{ 0.0f, -1.0f, 0.0f };   // default direction pointing downwards
        float m_InnerConeAngle{ 15.0f }; // degrees
        float m_OuterConeAngle{ 20.0f }; // degrees
    };
}