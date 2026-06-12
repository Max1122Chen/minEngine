#pragma once

#include "Core.h"

namespace minEngine
{
    class RenderPipeline;
    class SceneDrawDesc;
    struct SceneRenderContext;

    /** Per-frame CPU context for graph pass PreparePass (RND-F01 S03). */
    struct FrameRenderGraphContext
    {
        const SceneDrawDesc* DrawDesc = nullptr;
        SceneRenderContext* SceneContext = nullptr;
        RenderPipeline* Pipeline = nullptr;
    };
}
