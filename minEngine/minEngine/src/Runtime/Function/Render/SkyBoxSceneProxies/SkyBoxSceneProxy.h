#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <memory>

namespace minEngine
{
    class SkyBoxComponent;
    class EnvironmentMap;

    class SkyBoxSceneProxy
    {
    public:
        SkyBoxSceneProxy() = default;
        virtual ~SkyBoxSceneProxy() = default;

        SkyBoxComponent* m_SkyBoxComponent = nullptr;
        Transform m_Transform;
        float m_SkyIntensity = 1.0f;
        bool m_Enabled = true;
        std::shared_ptr<EnvironmentMap> m_EnvironmentMap;
    };
}
