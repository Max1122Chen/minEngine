#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"

namespace minEngine
{
    ME_CLASS()
    class DirectionalLightComponent : public LightComponent
    {
        ME_GENERATED_BODY(DirectionalLightComponent)

    public:
        DirectionalLightComponent() = default;
        virtual ~DirectionalLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Directional; }

        Vector3 GetDirection() const;       

        virtual LightSceneProxy* CreateSceneProxy() override;

    protected:
    };
}

#include "Generated/Reflection/DirectionalLightComponent.gen.h"