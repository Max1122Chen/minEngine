#pragma once
#include "Core.h"
#include "RenderPipeline.h"

namespace minEngine
{
    class RenderScene;
    class RHI;
    class RenderCamera;


    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        // Initialize
        void Initialize();
        void Shutdown();
        static RenderSystem& GetRenderSystem();


        void Tick(float deltaTime);

        RHI* GetRHI() const { return m_RHI.get(); }

        RenderCamera* GetMainCamera() const { return m_MainCamera.get(); }
        void SetMainCamera(std::shared_ptr<RenderCamera> inCamera) { m_MainCamera = inCamera; }

    public:
        static constexpr uint32_t MAX_POINT_LIGHTS = ::minEngine::MAX_POINT_LIGHTS;
        static constexpr uint32_t MAX_SPOT_LIGHTS = ::minEngine::MAX_SPOT_LIGHTS;



    public:
        std::shared_ptr<RenderScene> m_RenderScene;
    
    private:
        std::shared_ptr<RHI> m_RHI;
        std::shared_ptr<RenderCamera> m_MainCamera;

        RenderPipeline m_RenderPipeline;
    };
}