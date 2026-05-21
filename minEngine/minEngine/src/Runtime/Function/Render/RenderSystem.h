#pragma once
#include "Core.h"
#include "RenderPipeline/RenderPipeline.h"

namespace minEngine
{
    class Engine;
    class RenderScene;
    class RHI;
    class RenderCamera;
    class RHITexture2D;


    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        // Initialize
        void Initialize();
        void Shutdown();

        static RenderSystem& Get();


        void Tick(float deltaTime);

        void SetPresentPassEnabled(bool enabled);
        const std::shared_ptr<RHITexture2D>& GetSceneColorTexture() const;
        void RequestSceneViewportResize(float widthRatio, float heightRatio);
        Vector2 GetSceneBufferSize() const;

        RHI* GetRHI() const { return m_RHI.get(); }

        RenderCamera* GetMainCamera() const { return m_MainCamera.get(); }
        void SetMainCamera(std::shared_ptr<RenderCamera> inCamera) { m_MainCamera = inCamera; }

    public:
        static constexpr uint32_t MAX_POINT_LIGHTS = ::minEngine::MAX_POINT_LIGHTS;
        static constexpr uint32_t MAX_SPOT_LIGHTS = ::minEngine::MAX_SPOT_LIGHTS;



    public:
        std::shared_ptr<RenderScene> m_RenderScene;
    
    private:
        friend class Engine;

        static void SetInstance(RenderSystem* instance);
        static RenderSystem* s_Instance;

        std::shared_ptr<RHI> m_RHI;
        std::shared_ptr<RenderCamera> m_MainCamera;

        RenderPipeline m_RenderPipeline;
        uint32_t m_PendingSceneWidth = 0;
        uint32_t m_PendingSceneHeight = 0;
        bool m_HasPendingSceneResize = false;
    };
}