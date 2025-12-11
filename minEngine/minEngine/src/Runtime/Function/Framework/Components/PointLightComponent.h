#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Render/LightSceneProxy.h"

namespace minEngine
{
    class LightComponent;
    class LightSceneProxy;

    class PointLightComponent : public LightComponent
    {
    public:
        PointLightComponent();
        virtual ~PointLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Point; }

        virtual LightSceneProxy* CreateSceneProxy() override;
    };
}