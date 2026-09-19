#pragma once
#include "Core.h"
#include "PrimitiveSceneProxy.h"

namespace minEngine
{
    class StaticMeshSceneProxy : public PrimitiveSceneProxy
    {
    public:
        StaticMeshSceneProxy() = default;
        virtual ~StaticMeshSceneProxy() = default;
    };
}
