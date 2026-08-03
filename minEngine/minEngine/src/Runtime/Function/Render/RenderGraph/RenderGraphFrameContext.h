#pragma once

#include "Core.h"

namespace minEngine
{
    class ForwardRenderer;
    class RHICommandList;
    class SceneDrawDesc;
    class SceneRenderContext;

    /** Per-frame host pointers for IRenderPass::Prepare / NeedRenderPass (RND-F07). */
    struct RenderGraphFrameContext
    {
        const SceneDrawDesc* DrawDesc = nullptr;
        SceneRenderContext* SceneContext = nullptr;
        ForwardRenderer* Renderer = nullptr;
        RHICommandList* CommandList = nullptr;
    };
}
