#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "LightSceneProxy.h"

namespace minEngine
{
    class DirectionalLightSceneProxy : public LightSceneProxy
    {
    public:
        DirectionalLightSceneProxy() = default;
        virtual ~DirectionalLightSceneProxy() = default;

        // DirectionalLight properties
        Vector3 m_Direction;
    };
}