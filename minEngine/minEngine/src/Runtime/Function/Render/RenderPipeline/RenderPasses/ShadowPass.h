#pragma once
#include "Core.h"
#include "RenderPassBase.h"
#include "Math/Math.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

namespace minEngine
{
    class RHI;
    class MeshDrawCommand;
    class UniformBuffer;
    class FrameBuffer;
    class RHITexture2DArray;
    class RHIShaderLegacy;
    class RHICommandList;
    class RHIGraphicsPipelineState;
    class RHIShader;



    class ShadowPass : public RenderPassBase
    {
    public:
        ShadowPass() = default;
        virtual ~ShadowPass() = default;    

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

    private:
        void Render(RHICommandList& cmdList);
        void Render();
        void DrawOpaqueMeshes(RHICommandList& cmdList);

    public:
        UniformBuffer* m_LightViewProjUniformBuffer;
        std::vector<MeshDrawCommand> m_OpaqueQueue; // We only do depth test for opaque objects in the shadow pass

        std::vector<ShadowDrawCommand> m_ShadowDrawCommands;

    private:
        std::shared_ptr<RHIShaderLegacy> m_DepthOnlyShader;
        std::shared_ptr<RHIShader> m_DepthShader;
        std::shared_ptr<RHIGraphicsPipelineState> m_ShadowPipelineState;

        void UpdateLightViewProjBuffer(Matrix4 inMatrix);
        void RenderDirectionalShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
        void RenderSpotShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
        void RenderPointShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
    };
}