#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    enum class LightType : uint8_t;
    class LightComponent;

    class LightSceneProxy
    {
    public:
        LightSceneProxy() = default;
        virtual ~LightSceneProxy() = default;

        // Light properties
        LightType m_LightType;

        Vector3 m_Position;
        Vector3 m_LightColor;
        float m_Intensity;
        float m_DiffuseFactor;
        float m_SpecularFactor;

        LightComponent* m_LightComponent;
    };
}
