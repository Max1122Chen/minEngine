#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

namespace minEngine
{
    class PrimitiveSceneProxy
    {
    public:
        PrimitiveSceneProxy() = default;
        virtual ~PrimitiveSceneProxy() = default;

        Transform m_Transform;
    };
}