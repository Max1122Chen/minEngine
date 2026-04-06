#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"

namespace minEngine
{
    ME_CLASS()
    class DirectionalLightComponent : public LightComponent
    {
        ME_REFLECT_FRIEND(DirectionalLightComponent)

    public:
        DirectionalLightComponent() = default;
        virtual ~DirectionalLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Directional; }

        const Vector3& GetDirection() const { return m_Direction; }
        void SetDirection(const Vector3& inDirection);        

        virtual LightSceneProxy* CreateSceneProxy() override;

    protected:
        ME_PROPERTY(EditAnywhere)
        Vector3 m_Direction{ 0.0f, -1.0f, 0.0f };   // default direction pointing downwards
    };
}

#include "Generated/Reflection/DirectionalLightComponent.gen.h"