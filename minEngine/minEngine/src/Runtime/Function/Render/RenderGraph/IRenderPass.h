#pragma once

#include "Core.h"

namespace minEngine
{
    class RHICommandList;
    class RenderGraphFrameResources;
    class RenderPassBuilder;

    /** Per-pass CPU state container base (typed subclasses deferred to later slices). */
    struct PassParameters
    {
        virtual ~PassParameters() = default;
    };

    /** Complex pass implementation hook (Granite RenderPassInterface, RND-F01 P3). */
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void Setup(RenderPassBuilder& builder) = 0;
        virtual void PreparePass(RenderGraphFrameResources& frameResources) = 0;
        virtual void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) = 0;
    };
}
