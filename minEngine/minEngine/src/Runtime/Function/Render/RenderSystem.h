#pragma once
#include "Core.h"



namespace minEngine
{
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

        void Tick(float deltaTime);

        std::shared_ptr<RenderCamera> GetMainCamera() const { return m_Camera; }

    private:


    private:
        std::shared_ptr<RHI> m_RHI;
        std::shared_ptr<RenderCamera> m_Camera;
    };
}