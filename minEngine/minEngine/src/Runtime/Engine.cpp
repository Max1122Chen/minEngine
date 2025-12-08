#include "Engine.h"

#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Framework/World/WorldManager.h"

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
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetInstance();
        WindowSystem* windowSystem = globalContext.m_WindowSystem.get();
        while (!windowSystem->ShouldClose())
        {
            float deltaTime = CalculateDeltaTime();
            TickOneFrame(deltaTime);
        }
    }

    // Tick one frame
    void Engine::TickOneFrame(float deltaTime)
    {
        LogicalTick(deltaTime);

        RendererTick(deltaTime);

        RuntimeGlobalContext::GetInstance().m_WindowSystem->PollEvents();
    }

    void Engine::LogicalTick(float deltaTime)
    {
        // TODO: implement logical tick
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetInstance();
        globalContext.m_WorldManager->Tick(deltaTime);
        globalContext.m_InputSystem->Tick(deltaTime);

        globalContext.m_WorldManager->SendAllEndOfFrameUpdates();
    
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
