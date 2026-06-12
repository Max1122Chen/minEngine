#pragma once
#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTexture.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <vector>

namespace minEngine
{
    class RHICommandList;
    class RenderGraphFrameResources;
    class RenderPassBuilder;

    class BasePass : public RenderPassBase, public IRenderPass
    {
    public:
        BasePass() = default;
        virtual ~BasePass() = default;

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void SetClearSceneTargets(bool clear) { m_ClearSceneTargets = clear; }

        void Setup(RenderPassBuilder& builder) override;
        void PreparePass(RenderGraphFrameResources& frameResources) override;
        void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) override;

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
        RenderGraphFrameResources* m_ActiveFrameResources = nullptr;
    };
}
