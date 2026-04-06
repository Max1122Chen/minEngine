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
    public:
        PointLightComponent();
        virtual ~PointLightComponent() = default;

        virtual LightType GetLightType() const override { return LightType::Point; }

        virtual LightSceneProxy* CreateSceneProxy() override;
    };
}

#include "Generated/Reflection/PointLightComponent.gen.h"