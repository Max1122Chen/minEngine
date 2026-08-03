#pragma once
#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <vector>

namespace minEngine
{
    class RHICommandList;
    class RenderGraph;
    class RenderPass;

    class BasePass : public RenderPassBase, public IRenderPass
    {
    public:
        BasePass() = default;
        virtual ~BasePass() = default;

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void SetClearSceneTargets(bool clear) { m_ClearSceneTargets = clear; }

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;

    private:
        void Render(RHICommandList& cmdList);

    public:
        std::vector<MeshDrawCommand> m_DrawCommands;
        ShadowResourceHandle m_DirectionalShadowHandle;
        std::vector<ShadowResourceHandle> m_SpotShadowHandles;
        std::vector<ShadowResourceHandle> m_PointShadowHandles;

    private:
        bool m_ClearSceneTargets = true;
        std::vector<MeshDrawPacket> m_DrawPackets;
    };
}
