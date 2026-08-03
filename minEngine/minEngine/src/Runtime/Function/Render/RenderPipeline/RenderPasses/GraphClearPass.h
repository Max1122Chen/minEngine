#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"

namespace minEngine
{
    class RenderGraph;
    class RenderPass;
    class RHICommandList;

    /**
     * RND-F07-S05 vertical slice: declare SceneColor and clear it.
     * Replaced when scene passes reconnect (S07).
     */
    class GraphClearPass : public IRenderPass
    {
    public:
        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;

        bool GetClearColor(uint32_t attachment, float outRGBA[4]) const override;
    };
}
