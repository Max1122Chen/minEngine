#pragma once
#include "Core.h"

namespace minEngine
{
    class PrimitiveComponent;
    class PrimitiveSceneProxy;
    class LightComponent;
    class LightSceneProxy;
    class DirectionalLightSceneProxy;
    class PointLightSceneProxy;
    class SpotLightSceneProxy;

    class RenderScene
    {
    public:
        RenderScene() = default;
        virtual ~RenderScene() = default;

        void UpdatePrimitive(PrimitiveComponent* primitiveComponent);

        void UpdateLight(LightComponent* lightComponent);

        // Light scene proxies
        std::vector<DirectionalLightSceneProxy*> m_DirectionalLightSceneProxies;
        std::vector<PointLightSceneProxy*> m_PointLightSceneProxies;
        std::vector<SpotLightSceneProxy*> m_SpotLightSceneProxies;

        // Primitive scene proxies
        std::vector<PrimitiveSceneProxy*> m_PrimitiveSceneProxies;
    };
}