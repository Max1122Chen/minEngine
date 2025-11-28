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
        ME_CORE_INFO("Engine Shutdown Started"); 
        RuntimeGlobalContext::GetInstance().ShutdownSystems();          
    }

    void Engine::Run()
    {
        // TODO: change to proper game loop
        TickOneFrame(0.016f); // Assume 60 FPS for now
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
        RuntimeGlobalContext::GetInstance().m_RenderSystem->Tick(deltaTime);
    }

    float Engine::CalculateDeltaTime()
    {
        float deltaTime = 0;
        {
            using namespace std::chrono;
            steady_clock::time_point tickTimePoint = steady_clock::now();
            duration<float> timeSpan = tickTimePoint - m_LastTickTimePoint;
            
            deltaTime = timeSpan.count();

            m_LastTickTimePoint = tickTimePoint;
        }
        return deltaTime;
    }
}
