#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

namespace minEngine
{
    class PrimitiveComponent;

    class PrimitiveSceneProxy
    {
    public:
        PrimitiveSceneProxy() = default;
        virtual ~PrimitiveSceneProxy() = default;

        PrimitiveComponent* m_PrimitiveComponent{ nullptr };
        Transform m_Transform;
        bool m_CastShadow = true;
    };
}