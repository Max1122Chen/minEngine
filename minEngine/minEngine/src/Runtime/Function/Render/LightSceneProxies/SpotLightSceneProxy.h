#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "PointLightSceneProxy.h"

namespace minEngine
{
    class SpotLightSceneProxy : public PointLightSceneProxy
    {
    public:
        SpotLightSceneProxy() = default;
        virtual ~SpotLightSceneProxy() = default;

        // SpotLight properties
        Vector3 m_Direction;
        float m_InnerConeAngle;
        float m_OuterConeAngle;
    };
}