#include "Engine.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

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
    void Engine::Initialize()
    {
        LogSystem::Get().Initialize();

        FinializeReflection();
    
        ME_CORE_INFO("Engine Initialization Started");
        RuntimeGlobalContext::GetRuntimeGlobalContext().StartSystems();

        // Scan assets after all systems are initialized, in case asset loading requires any of the systems (e.g., RenderSystem for creating GPU resources for textures)
        const std::array<std::string, 4> scanCandidates = {
            "Assets/EngineDefault",
            "../Assets/EngineDefault",
            "../../Assets/EngineDefault",
            "minEngine/Assets/EngineDefault"
        };

        bool scanned = false;
        for (const std::string& relativePath : scanCandidates)
        {
            const std::filesystem::path candidatePath = std::filesystem::absolute(relativePath).lexically_normal();
            if (!std::filesystem::exists(candidatePath) || !std::filesystem::is_directory(candidatePath))
            {
                continue;
            }

            ME_CORE_INFO("Scanning engine default assets: {}", candidatePath.string());
            AssetManager::Get().ScanAssets(candidatePath.string());
            scanned = true;
            break;
        }

        if (!scanned)
        {
            ME_CORE_WARN("Engine default asset directory was not found. Expected one of the EngineDefault candidates around current working directory.");
        }
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
            // windowSystem->SetTitle(("minEngine - FPS: " + std::to_string(CalculateFPS(deltaTime))).c_str());
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
        RuntimeGlobalContext& globalContext = RuntimeGlobalContext::GetRuntimeGlobalContext();

        globalContext.m_InputSystem->Tick(deltaTime);
        globalContext.m_SceneManager->Tick(deltaTime);


        globalContext.m_SceneManager->SendAllEndOfFrameUpdates();
    
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
