#pragma once
#include "Core.h"
#include "RenderPassBase.h"


namespace minEngine
{
    class VertexDefinition;
    class VertexBuffer;
    class RHIShader;
    class RHITexture2D;

    class PresentPass : public RenderPassBase
    {
    public:
        PresentPass() = default;
        virtual ~PresentPass() = default;

        void Initialize();

        virtual void Execute() override;

    private:
        std::shared_ptr<VertexBuffer> m_ScreenQuadVertexBuffer;
        std::shared_ptr<VertexDefinition> m_ScreenQuadVertexDefinition;
        std::shared_ptr<RHIShader> m_ScreenQuadShader;

        virtual void Render() override;

    public:
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;

    };
}