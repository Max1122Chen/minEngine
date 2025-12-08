#pragma once
#include "Runtime/Core/Core.h"


namespace minEngine
{
    class RenderScene;
    class RHI;
    class RenderCamera;
    class PrimitiveSceneProxy;


    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        // Initialize
        void Initialize();
        void Shutdown();

        void Tick(float deltaTime);

        std::shared_ptr<RenderCamera> GetMainCamera() const { return m_Camera; }

    private:


    public:
        std::shared_ptr<RenderScene> m_RenderScene;
    
    private:
        std::shared_ptr<RHI> m_RHI;
        std::shared_ptr<RenderCamera> m_Camera;
    };
}