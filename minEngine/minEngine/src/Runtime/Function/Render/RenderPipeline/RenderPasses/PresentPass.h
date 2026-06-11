#pragma once
#include "Core.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHITextureViewCache.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class RHICommandList;
    class RHIBindingLayout;
    class RHIPipelineLayout;
    class RHIBindingSet;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderResourceView;
    class RHITexture;
    class RHIBuffer;
    class RHIVertexInputLayout;

    class PresentPass : public RenderPassBase
    {
        friend class RenderPipeline;

    public:
        PresentPass() = default;
        virtual ~PresentPass() = default;

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

    public:
        RHITextureRef m_SceneColorTexture;

    private:
        RHIBufferRef m_ScreenQuadVertexBuffer;
        RHIVertexInputLayoutRef m_ScreenQuadVertexLayout;
        std::shared_ptr<RHIShader> m_ScreenQuadShader;
        std::shared_ptr<RHIGraphicsPipelineState> m_PresentPipelineState;
        std::shared_ptr<RHIBindingLayout> m_PresentBindingLayout;
        std::shared_ptr<RHIPipelineLayout> m_PresentPipelineLayout;
        std::shared_ptr<RHIShaderResourceView> m_SceneColorSRV;
        RHIBindingSetRef m_PresentBindingSet;
        RHITextureViewCache m_TextureViewCache;
        RHITexture* m_CachedSceneColorTexture = nullptr;

        void Render(RHICommandList& cmdList);
        void Render();
    };
}
