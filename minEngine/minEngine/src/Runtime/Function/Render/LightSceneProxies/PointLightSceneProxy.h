#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "LightSceneProxy.h"

namespace minEngine
{
    class PointLightSceneProxy : public LightSceneProxy
    {
    public:
        PointLightSceneProxy() = default;
        virtual ~PointLightSceneProxy() = default;

        float m_AttenuationRadius{10.0f};
        float m_AttenuationFalloff{2.0f};
    };
}