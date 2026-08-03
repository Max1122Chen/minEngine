#pragma once
#include "Core.h"
#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
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
    class RenderGraph;
    class RenderPass;

    class PostProcessPass : public RenderPassBase, public IRenderPass
    {
        friend class ForwardRenderer;

    public:
        PostProcessPass() = default;
        virtual ~PostProcessPass() = default;

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void SetGraphTextureNames(const char* inputName, const char* outputName);
        void SetOutputDesc(uint32_t width, uint32_t height);

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;
        bool NeedRenderPass() const override { return m_CanRender; }

        void SetPredecessor(PostProcessPass* predecessor) { m_Predecessor = predecessor; }

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
        uint32_t m_OutputWidth = 0;
        uint32_t m_OutputHeight = 0;

        RHIShaderResourceViewRef m_InputSRV;
        RHIShaderBindingSetRef m_PostShaderBindingSet;
        RHITexture* m_CachedInputTexture = nullptr;
        RHITexture* m_OutputTexture = nullptr;
        MeshDrawPacket m_DrawPacket;
        PostProcessPass* m_Predecessor = nullptr;
        bool m_CanRender = false;

        void Render(RHICommandList& cmdList);
        void PrepareDrawPacket(RHICommandList& cmdList, RHITexture* inputTexture);
    };
}
