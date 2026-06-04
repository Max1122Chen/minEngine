#pragma once
#include "Core.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class VertexDefinition;
    class VertexBuffer;
    class RHIShaderLegacy;
    class RHITexture2D;

    class PostProcessPass : public RenderPassBase
    {
        friend class RenderPipeline;
    public:
        PostProcessPass() = default;
        virtual ~PostProcessPass() = default;

        void Initialize();
        virtual void Execute() override;


    public:
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;
        std::shared_ptr<RHITexture2D> m_SceneDepthTexture;

    private:
        std::shared_ptr<VertexBuffer> m_ScreenQuadVertexBuffer;
        std::shared_ptr<VertexDefinition> m_ScreenQuadVertexDefinition;
        std::shared_ptr<RHIShaderLegacy> m_PostProcessShader;
        virtual void Render() override;
    };
}
