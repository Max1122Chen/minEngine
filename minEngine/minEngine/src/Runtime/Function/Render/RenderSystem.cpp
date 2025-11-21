#include "RenderSystem.h"

// Include RHI implementations
#include "OpenGLRHI.h"

namespace minEngine
{
    void RenderSystem::Initialize()
    {
        // TODO : Create RHI based on configuration
        m_RHI = std::make_shared<OpenGLRHI>();
        m_RHI->Initialize();

        

        ME_CORE_INFO("RenderSystem Initialized");
    }

    void RenderSystem::Shutdown()
    {
        // TODO: implement shutdown logic
        ME_CORE_INFO("RenderSystem Shutdown");
    }

    void RenderSystem::Tick(float deltaTime)
    {
        // Rendering logic per frame would go here

    }
}