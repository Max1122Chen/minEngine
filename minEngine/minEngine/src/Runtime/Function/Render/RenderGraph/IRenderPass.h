#pragma once

#include "Core.h"

namespace minEngine
{
    class RenderGraph;
    class RenderPass;
    class RHI;
    class RHICommandList;

    /**
     * Pass implementation hook (Granite RenderPassInterface).
     * RND-F07: SetupDependencies → Bake → Setup → Prepare → BuildRenderPass.
     */
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        /** Once before Bake topology: declare IO only. */
        virtual void SetupDependencies(RenderPass& self, RenderGraph& graph) = 0;

        /** Once after Bake: create long-lived PSO/samplers. Must not allocate graph-owned frame RTs. */
        virtual void Setup(RHI& rhi) { (void)rhi; }

        /** Each frame CPU prep (may read graph.GetFrameContext()). */
        virtual void Prepare(RenderGraph& graph) { (void)graph; }

        /** Each frame GPU record; use graph.GetPhysicalTexture(...). */
        virtual void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) = 0;

        virtual bool NeedRenderPass() const { return true; }

        virtual bool GetClearColor(uint32_t attachment, float outRGBA[4]) const
        {
            (void)attachment;
            (void)outRGBA;
            return false;
        }

        virtual bool GetClearDepthStencil(float& depth, uint8_t& stencil) const
        {
            (void)depth;
            (void)stencil;
            return false;
        }
    };
}
