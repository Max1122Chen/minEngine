#pragma once
#include "Core.h"
#include "RenderPasses/BasePass.h"
#include "RenderPasses/TranslucencyPass.h"
#include "RenderPasses/PresentPass.h"


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
        

        std::shared_ptr<FrameBuffer> m_SceneBuffer;
        
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;
        std::shared_ptr<RHITexture2D> m_SceneDepthTexture;

        BasePass m_BasePass;
        TranslucencyPass m_TranslucentPass;
        PresentPass m_PresentPass;

        std::vector<MeshDrawCommand> m_OpaqueQueue;
        std::vector<MeshDrawCommand> m_TranslucentQueue;

    private:
        void BuildRenderQueue();
    };
}