#pragma once
#include "Core.h"
#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTexture.h"
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
    class RenderGraphFrameResources;
    class RenderPassBuilder;

    class PostProcessPass : public RenderPassBase, public IRenderPass
    {
        friend class RenderPipeline;

    public:
        PostProcessPass() = default;
        virtual ~PostProcessPass() = default;

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void SetGraphTextureNames(const char* inputName, const char* outputName);
        void SetOutputDesc(uint32_t width, uint32_t height);

        void Setup(RenderPassBuilder& builder) override;
        void PreparePass(RenderGraphFrameResources& frameResources) override;
        void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) override;

    private:
        RHIBufferRef m_ScreenQuadVertexBuffer;
        RHIVertexInputLayoutRef m_ScreenQuadVertexLayout;
        std::shared_ptr<RHIShader> m_PostProcessShader;
        std::shared_ptr<RHIGraphicsPipelineState> m_PostProcessPipelineState;
        std::shared_ptr<RHIShaderBindingSetLayout> m_PostShaderBindingSetLayout;
        std::shared_ptr<RHIPipelineLayout> m_PostPipelineLayout;
        RHIBufferRef m_PostParamsUniformBuffer;
        RHITextureViewCache m_TextureViewCache;

        const char* m_InputTextureName = kRDGSceneColor;
        const char* m_OutputTextureName = kRDGPostBufferA;
        RDGTextureDesc m_OutputDesc{};

        RHIShaderResourceViewRef m_InputSRV;
        RHIShaderBindingSetRef m_PostShaderBindingSet;
        RHITexture* m_CachedInputTexture = nullptr;
        RHITexture* m_OutputTexture = nullptr;
        RenderGraphFrameResources* m_ActiveFrameResources = nullptr;
        MeshDrawPacket m_DrawPacket;

        void Render(RHICommandList& cmdList);
        void PrepareDrawPacket(RHICommandList& cmdList, RHITexture* inputTexture);
    };
}
