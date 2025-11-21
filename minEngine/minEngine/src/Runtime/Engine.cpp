#include "Engine.h"

#include "Runtime/Function/RuntimeGlobalContext.h"

namespace minEngine
{
    void Engine::Initialize()
    {
        RuntimeGlobalContext::GetInstance().StartSystems();
    }

    void Engine::Shutdown()
    {
        RuntimeGlobalContext::GetInstance().ShutdownSystems();
        ME_CORE_INFO("Engine Shutdown");   
    }

    void Engine::Run()
    {
        // TODO: change to proper game loop
        while(true)
        {
            TickOneFrame(0.016f); // Assume 60 FPS for now
        }
    }

    // Tick one frame
    void Engine::TickOneFrame(float deltaTime)
    {
        // Logical tick
        LogicalTick(deltaTime);

        // Renderer tick
        RendererTick(deltaTime);
    }

    void Engine::LogicalTick(float deltaTime)
    {
        // TODO: implement logical tick
    }

    void Engine::RendererTick(float deltaTime)
    {
        m_RenderSystem->Tick(deltaTime);
    }
}
