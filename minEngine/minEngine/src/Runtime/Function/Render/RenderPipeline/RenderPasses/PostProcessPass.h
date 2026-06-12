#pragma once
#include "Core.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHITextureViewCache.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class RHIShaderBindingSetLayout;
    class RHIPipelineLayout;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderResourceView;
    class RHIVertexInputLayout;
    class RHICommandList;
    class RHIBuffer;

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
        RHIBufferRef m_ScreenQuadVertexBuffer;
        RHIVertexInputLayoutRef m_ScreenQuadVertexLayout;
        std::shared_ptr<RHIShader> m_PostProcessShader;
        std::shared_ptr<RHIGraphicsPipelineState> m_PostProcessPipelineState;
        std::shared_ptr<RHIShaderBindingSetLayout> m_PostShaderBindingSetLayout;
        std::shared_ptr<RHIPipelineLayout> m_PostPipelineLayout;
        std::shared_ptr<RHIShaderResourceView> m_SceneColorSRV;
        RHIShaderBindingSetRef m_PostShaderBindingSet;
        RHIBufferRef m_PostParamsUniformBuffer;
        RHITextureViewCache m_TextureViewCache;
        RHITexture* m_CachedSceneColorTexture = nullptr;

        void Render(RHICommandList& cmdList);
    };
}
