#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/RenderPipeline/ScenePerFrameData.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"

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

        /**
         * The block the screen-space draws publish: identity view, pixel ortho projection, camera at
         * the origin. Pure data, so the contract can be checked without a GPU.
         */
        static PerFrameData MakeScreenSpacePerFrameData(float viewportWidth, float viewportHeight)
        {
            PerFrameData perFrameData{};
            perFrameData.View = Matrix4(1.0f);
            perFrameData.Proj = ScreenUICoords::MakePixelOrthoProjection(viewportWidth, viewportHeight);
            perFrameData.ViewProj = perFrameData.Proj * perFrameData.View;
            perFrameData.CameraPos = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            return perFrameData;
        }

        std::vector<MeshDrawCommand> m_DrawCommands;
        /** This pass's own per-frame block; never the one the 3D passes read. */
        RHIBuffer* m_ScreenSpacePerFrameUniformBuffer = nullptr;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;

    private:
        friend class Testing::TestAccess<ScreenUIPass>;

        void BindIdentityCameraForScreenSpace(RHICommandList& cmdList);

        std::vector<MeshDrawPacket> m_DrawPackets;
    };
}
