#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Math/Math.h"
#include "Runtime/Function/Render/EnginePassUniforms.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"

#include <unordered_map>

namespace minEngine
{
    class RHI;
    class MeshDrawCommand;
    class RHIBuffer;
    class RHIShader;
    class RHICommandList;
    class RHIGraphicsPipelineState;
    class RHIShaderBindingSetLayout;
    class RHIShaderBindingSet;
    class RHIVertexInputLayout;

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
        void EnsureShadowShaderBindingSet(RHICommandList& cmdList);
        void UpdateShadowParams(RHICommandList& cmdList, const ShadowPassParamsUBO& params);
        RHIGraphicsPipelineStateRef GetOrCreateShadowPipelineForLayout(
            RHIVertexInputLayout* vertexInputLayout,
            RHICommandList& cmdList);

    public:
        RHIBuffer* m_LightViewProjUniformBuffer = nullptr;
        RHIBuffer* m_PerObjectUniformBuffer = nullptr;
        std::vector<MeshDrawCommand> m_OpaqueQueue;

        std::vector<ShadowDrawCommand> m_ShadowDrawCommands;

    private:
        std::shared_ptr<RHIShader> m_DepthShader;
        RHIGraphicsPSODesc m_ShadowPSODescTemplate{};
        std::unordered_map<RHIVertexInputLayout*, std::shared_ptr<RHIGraphicsPipelineState>> m_ShadowPipelineByLayout;
        std::shared_ptr<RHIShaderBindingSet> m_ShadowShaderBindingSet;
        RHIBufferRef m_ShadowParamsUniformBuffer;

        void UpdateLightViewProjBuffer(Matrix4 inMatrix);
        void RenderDirectionalShadow(RHICommandList& cmdList, const ShadowDrawCommand& command);
        void RenderSpotShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
        void RenderPointShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
    };
}
