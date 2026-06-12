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
    class RHICommandList;
    class RHIShaderBindingSetLayout;
    class RHIPipelineLayout;
    class RHIShaderBindingSet;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderResourceView;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class RenderGraphFrameResources;
    class RenderPassBuilder;

    class PresentPass : public RenderPassBase, public IRenderPass
    {
        friend class RenderPipeline;

    public:
        PresentPass() = default;
        virtual ~PresentPass() = default;

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void SetInputTextureName(const char* inputName);

        void Setup(RenderPassBuilder& builder) override;
        void PreparePass(RenderGraphFrameResources& frameResources) override;
        void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) override;

    private:
        RHIBufferRef m_ScreenQuadVertexBuffer;
        RHIVertexInputLayoutRef m_ScreenQuadVertexLayout;
        std::shared_ptr<RHIShader> m_ScreenQuadShader;
        std::shared_ptr<RHIGraphicsPipelineState> m_PresentPipelineState;
        std::shared_ptr<RHIShaderBindingSetLayout> m_PresentShaderBindingSetLayout;
        std::shared_ptr<RHIPipelineLayout> m_PresentPipelineLayout;
        RHIShaderResourceViewRef m_InputSRV;
        RHIShaderBindingSetRef m_PresentShaderBindingSet;
        RHITextureViewCache m_TextureViewCache;

        const char* m_InputTextureName = kRDGSceneColor;
        RHITexture* m_CachedInputTexture = nullptr;
        RHITexture* m_InputTexture = nullptr;
        RenderGraphFrameResources* m_ActiveFrameResources = nullptr;
        MeshDrawPacket m_DrawPacket;

        void Render(RHICommandList& cmdList);
        void Render();
        void PrepareDrawPacket(RHICommandList& cmdList, RHITexture* inputTexture);
    };
}
