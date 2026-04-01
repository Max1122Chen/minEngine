#include "Engine.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Framework/World/WorldManager.h"

namespace minEngine
{
    void Engine::Initialize()
    {
        RuntimeGlobalContext::GetRuntimeGlobalContext().StartSystems();
    }

    void Engine::Shutdown()
    {
        ME_CORE_INFO("Engine Shutdown Started"); 
        RuntimeGlobalContext::GetRuntimeGlobalContext().ShutdownSystems();
    }

    void Engine::Run()
    {
        // TODO: change to proper game loop
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetRuntimeGlobalContext();
        WindowSystem* windowSystem = globalContext.m_WindowSystem.get();
        while (!windowSystem->ShouldClose())
        {
            float deltaTime = CalculateDeltaTime();
            windowSystem->SetTitle(("minEngine - FPS: " + std::to_string(CalculateFPS(deltaTime))).c_str());
            TickOneFrame(deltaTime);
            windowSystem->SwapBuffers();
        }
    }

    // Tick one frame
    void Engine::TickOneFrame(float deltaTime)
    {
        RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->PollEvents();
        LogicalTick(deltaTime);

        RendererTick(deltaTime);
    }

    void Engine::LogicalTick(float deltaTime)
    {
        // TODO: implement logical tick
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetRuntimeGlobalContext();

        globalContext.m_InputSystem->Tick(deltaTime);
        globalContext.m_WorldManager->Tick(deltaTime);


        globalContext.m_WorldManager->SendAllEndOfFrameUpdates();
    
    }

    void Engine::RendererTick(float deltaTime)
    {
        RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->Tick(deltaTime);
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

    float Engine::CalculateFPS(float deltaTime)
    {
        return 1.0f / deltaTime;
    }
}
