#pragma once
#include "Core.h"

namespace minEngine
{
    class RHI;

    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        // Initialize
        void Initialize();


        void Shutdown();

        void Tick(float deltaTime);


    private:
        std::shared_ptr<RHI> m_RHI;
        
    };
}