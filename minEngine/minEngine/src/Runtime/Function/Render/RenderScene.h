#pragma once
#include "Core.h"

namespace minEngine
{
    class Component;
    class PrimitiveComponent;
    class PrimitiveSceneProxy;

    class RenderScene
    {
    public:
        RenderScene() = default;
        virtual ~RenderScene() = default;

        std::vector<PrimitiveSceneProxy*> m_PrimitiveSceneProxies;
    };
}