#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

namespace minEngine
{
    class SkyBoxComponent;

    class SkyBoxSceneProxy
    {
    public:
        SkyBoxSceneProxy() = default;
        virtual ~SkyBoxSceneProxy() = default;

        SkyBoxComponent* m_SkyBoxComponent = nullptr;
        Transform m_Transform;
        float m_SkyIntensity = 1.0f;
        bool m_Enabled = true;
    };
}
