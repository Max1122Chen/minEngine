#pragma once
#include "Core.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class RHICommandList;
    class RHIBindingLayout;
    class RHIBindingSet;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderResourceView;
    class RHITexture;
    class RHITexture2D;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class VertexDefinition;

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
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;

    private:
        std::shared_ptr<VertexBuffer> m_ScreenQuadVertexBuffer;
        std::shared_ptr<VertexDefinition> m_ScreenQuadVertexDefinition;
        std::shared_ptr<RHIVertexInputLayout> m_ScreenQuadVertexLayout;
        std::shared_ptr<RHIShader> m_ScreenQuadShader;
        std::shared_ptr<RHIGraphicsPipelineState> m_PresentPipelineState;
        std::shared_ptr<RHIBindingLayout> m_PresentBindingLayout;
        std::shared_ptr<RHIShaderResourceView> m_SceneColorSRV;

        void Render(RHICommandList& cmdList);
        void Render();
    };
}
