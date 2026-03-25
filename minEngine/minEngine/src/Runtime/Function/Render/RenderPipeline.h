#pragma once
#include "Core.h"
#include "RenderPasses/BasePass.h"
#include "RenderPasses/TranslucencyPass.h"
#include "RenderPasses/PresentPass.h"


namespace minEngine
{
    class UniformBuffer;
    class FrameBuffer;
    class RHITexture2D;

    /**
     * UBO binding point layout:
     * - 0: Per-frame data (view/proj matrices, camera position, etc.) 
     * 
     */



    struct PerFrameData
    {
        Matrix4 View;
        Matrix4 Proj;
        Matrix4 ViewProj;

        Vector4 CameraPos;
    };

    class RenderPipeline
    {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        void Initialize();
        void Shutdown();
        void Execute();

    private:
        std::shared_ptr<UniformBuffer> m_PerFrameUniformBuffer;

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