#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPasses/BasePass.h"
#include "Runtime/Function/Render/RenderPasses/TranslucencyPass.h"

namespace minEngine
{
    class FrameBuffer;
    class RHITexture2D;

    class RenderPipeline
    {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        void Initialize();
        void Shutdown();
        void Execute();

    private:
        std::shared_ptr<FrameBuffer> m_sceneBuffer;
        
        std::shared_ptr<RHITexture2D> m_sceneColorTexture;
        std::shared_ptr<RHITexture2D> m_sceneDepthTexture;

        BasePass m_BasePass;
        TranslucencyPass m_TranslucentPass;

        std::vector<MeshDrawCommand> m_OpaqueQueue;
        std::vector<MeshDrawCommand> m_TranslucentQueue;

    private:
        void BuildRenderQueue();
    };
}