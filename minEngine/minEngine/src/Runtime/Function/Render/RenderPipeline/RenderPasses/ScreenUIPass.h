#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"

#include <vector>

namespace minEngine
{
    class RHICommandList;
    class RenderGraph;
    class RenderPass;
    class RHIBuffer;

    class ScreenUIPass : public RenderPassBase, public IRenderPass
    {
    public:
        ScreenUIPass() = default;
        ~ScreenUIPass() override = default;

        void Execute() override;

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;

        std::vector<MeshDrawCommand> m_DrawCommands;
        RHIBuffer* m_PerFrameUniformBuffer = nullptr;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;

    private:
        void BindIdentityCameraForScreenSpace();

        std::vector<MeshDrawPacket> m_DrawPackets;
    };
}
