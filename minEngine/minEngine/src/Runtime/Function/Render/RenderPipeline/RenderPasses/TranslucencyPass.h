#pragma once
#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

namespace minEngine
{
    class RenderCamera;
    class RHICommandList;
    class RenderGraph;
    class RenderPass;

    class TranslucencyPass : public RenderPassBase, public IRenderPass
    {
    public:
        TranslucencyPass() = default;
        virtual ~TranslucencyPass() = default;

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;

    public:
        std::vector<MeshDrawCommand> m_DrawCommands;
        RenderCamera* m_SortCamera = nullptr;
        ShadowResourceHandle m_DirectionalShadowHandle;
        std::vector<ShadowResourceHandle> m_SpotShadowHandles;
        std::vector<ShadowResourceHandle> m_PointShadowHandles;

    private:
        void Render(RHICommandList& cmdList);
        void SortDrawCommands();

        std::vector<MeshDrawPacket> m_DrawPackets;
    };
}
