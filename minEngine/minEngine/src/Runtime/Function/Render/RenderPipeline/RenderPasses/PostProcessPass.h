#pragma once
#include "Core.h"
#include "Render/RHI/RHITexture.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class RHIBindingLayout;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderLegacy;
    class RHIShaderResourceView;
    class RHIVertexInputLayout;
    class RHICommandList;
    class VertexBuffer;
    class VertexDefinition;

    class PostProcessPass : public RenderPassBase
    {
        friend class RenderPipeline;

    public:
        PostProcessPass() = default;
        virtual ~PostProcessPass() = default;

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

    public:
        RHITextureRef m_SceneColorTexture;

    private:
        std::shared_ptr<VertexBuffer> m_ScreenQuadVertexBuffer;
        std::shared_ptr<VertexDefinition> m_ScreenQuadVertexDefinition;
        std::shared_ptr<RHIVertexInputLayout> m_ScreenQuadVertexLayout;
        std::shared_ptr<RHIShaderLegacy> m_PostProcessShader;
        std::shared_ptr<RHIShader> m_PostProcessShaderRHI;
        std::shared_ptr<RHIGraphicsPipelineState> m_PostProcessPipelineState;
        std::shared_ptr<RHIBindingLayout> m_SceneColorBindingLayout;
        std::shared_ptr<RHIShaderResourceView> m_SceneColorSRV;

        void Render(RHICommandList& cmdList);
    };
}
