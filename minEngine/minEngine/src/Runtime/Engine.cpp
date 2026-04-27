#include "Engine.h"

#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Render/RenderSystem.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/AssetManager.h"

#include <array>
#include <filesystem>

namespace minEngine
{
    void Engine::Initialize(int argc, char** argv)
    {
        LogSystem::Get().Initialize();

        FinializeReflection();
        RuntimeGlobalContext::Get().StartSystems();
    }

    void Engine::Shutdown()
    {
        ME_CORE_INFO("Engine Shutdown Started"); 
        RuntimeGlobalContext::Get().ShutdownSystems();
    }

    void Engine::Run()
    {
        // TODO: change to proper game loop
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::Get();
        WindowSystem* windowSystem = globalContext.m_WindowSystem.get();
        while (!windowSystem->ShouldClose())
        {
            float deltaTime = CalculateDeltaTime();
            // windowSystem->SetTitle(("minEngine - FPS: " + std::to_string(CalculateFPS(deltaTime))).c_str());
            TickOneFrame(deltaTime);
            windowSystem->SwapBuffers();
        }
    }

    // Tick one frame
    void Engine::TickOneFrame(float deltaTime)
    {
        RuntimeGlobalContext::Get().m_WindowSystem->PollEvents();
        LogicalTick(deltaTime);

        RendererTick(deltaTime);
    }

    void Engine::FinializeReflection()
    {
        Reflection::ReflectionSystem::Get().FinalizeReflection();
        const std::vector<std::string>& reflectionErrors = Reflection::ReflectionSystem::Get().GetLastErrors();
        if (!reflectionErrors.empty())
        {
            for (const std::string& error : reflectionErrors)
            {
                ME_CORE_ERROR(error);
            }
            ME_ASSERT(false, "Reflection System finalization failed. See previous errors for details.");
        }
        else
        {
            ME_CORE_INFO("Reflection System finalized successfully.");
            Reflection::ReflectionSystem::Get().ClearErrors();
        }
    }

    void Engine::LogicalTick(float deltaTime)
    {
        // TODO: implement logical tick
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::Get();

        globalContext.m_InputSystem->Tick(deltaTime);
        globalContext.m_SceneManager->Tick(deltaTime);


        globalContext.m_SceneManager->SendAllEndOfFrameUpdates();
    
    }

    void Engine::RendererTick(float deltaTime)
    {
        RuntimeGlobalContext::Get().m_RenderSystem->Tick(deltaTime);
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
