#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

namespace minEngine
{
    class ForwardRenderer;

    class RenderPassBase
    {
    public:
        RenderPassBase() = default;
        virtual ~RenderPassBase() = default;

        virtual void Execute() = 0;

        ForwardRenderer* pipeline = nullptr;
    };
}
