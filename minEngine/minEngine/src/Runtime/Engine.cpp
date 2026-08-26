#include "Engine.h"

#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"
#include "Runtime/Platform/FileDialog/FileDialogService.h"

namespace minEngine
{
    Engine* Engine::s_Instance = nullptr;

    Engine& Engine::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "Engine is not initialized");
        return *s_Instance;
    }

    void Engine::Initialize(const CommandLineResult& commandLine)
    {
        ME_ASSERT(s_Instance == nullptr, "Engine is already initialized");
        s_Instance = this;

        RHIBackendSelection::Set(commandLine.RHIBackend);

        LogSystem::Get().Initialize();
        FinializeReflection();

        m_EnginePathConfigLoaded =
            PathRegistry::Get().LoadEngineConfiguration(commandLine, m_EngineConfig);

        StartSystems();

        // Sky / EnvMap shaders and validation resources — required on every RHI (ED-F01 VK parity).
        if (m_RenderSystem && m_EnginePathConfigLoaded)
        {
            m_RenderSystem->LoadEngineRenderingAssets();
        }
    }

    void Engine::Initialize(int argc, char** argv)
    {
        ME_ASSERT(s_Instance == nullptr, "Engine is already initialized");
        s_Instance = this;

        LogSystem::Get().Initialize();
        FinializeReflection();

        m_EnginePathConfigLoaded =
            PathRegistry::Get().LoadEngineConfiguration(argc, argv, m_EngineConfig);

        StartSystems();

        if (m_RenderSystem && m_EnginePathConfigLoaded)
        {
            m_RenderSystem->LoadEngineRenderingAssets();
        }
    }

    void Engine::Shutdown()
    {
        ME_CORE_INFO("Engine Shutdown Started");
        ShutdownSystems();
        s_Instance = nullptr;
    }

    void Engine::Run()
    {
        WindowSystem& windowSystem = WindowSystem::Get();
        while (!windowSystem.ShouldClose())
        {
            const float deltaTime = CalculateDeltaTime();
            TickOneFrame(deltaTime);
            if (m_RenderSystem)
            {
                m_RenderSystem->PresentFrame();
            }
            else
            {
                windowSystem.SwapBuffers();
            }
        }
    }

    void Engine::PollEvents()
    {
        WindowSystem::Get().PollEvents();
    }

    void Engine::TickLogicalFrame(float deltaTime)
    {
        LogicalTick(deltaTime);
    }

    void Engine::TickRendererFrame(float deltaTime)
    {
        (void)deltaTime;
        RendererTick(deltaTime);
    }

    void Engine::TickOneFrame(float deltaTime)
    {
        PollEvents();
        TickLogicalFrame(deltaTime);
        TickRendererFrame(deltaTime);
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

    void Engine::StartSystems()
    {
        m_ObjectManager = std::make_shared<ObjectManager>();
        ObjectManager::SetInstance(m_ObjectManager.get());
        m_ObjectManager->Initialize();

        m_ProjectManager = std::make_shared<ProjectManager>();
        ProjectManager::SetInstance(m_ProjectManager.get());
        m_ProjectManager->Initialize();

        m_AssetManager = std::make_shared<AssetManager>();
        AssetManager::SetInstance(m_AssetManager.get());
        m_AssetManager->Initialize();

        m_WindowSystem = std::make_shared<GLFWWindowSystem>(1600, 900);
        WindowSystem::SetInstance(m_WindowSystem.get());
        m_WindowSystem->Initialize();

        m_FileDialogService = std::make_shared<FileDialogService>();
        FileDialogService::SetInstance(m_FileDialogService.get());
        m_FileDialogService->Initialize();

        m_InputSystem = std::make_shared<InputSystem>();
        InputSystem::SetInstance(m_InputSystem.get());
        m_InputSystem->Initialize();

        m_RenderSystem = std::make_shared<RenderSystem>();
        RenderSystem::SetInstance(m_RenderSystem.get());
        m_RenderSystem->Initialize();

        m_LuaScriptSystem = std::make_shared<LuaScriptSystem>();
        LuaScriptSystem::SetInstance(m_LuaScriptSystem.get());
        m_LuaScriptSystem->Initialize();

        m_SceneManager = std::make_shared<SceneManager>();
        SceneManager::SetInstance(m_SceneManager.get());
        m_SceneManager->Initialize();

        m_PhysicsSystem = std::make_shared<PhysicsSystem>();
        PhysicsSystem::SetInstance(m_PhysicsSystem.get());
        m_PhysicsSystem->Initialize();
    }

    void Engine::ShutdownSystems()
    {
        if (m_FileDialogService)
        {
            m_FileDialogService->Shutdown();
            FileDialogService::SetInstance(nullptr);
            m_FileDialogService.reset();
        }

        if (m_ProjectManager)
        {
            m_ProjectManager->Shutdown();
            ProjectManager::SetInstance(nullptr);
            m_ProjectManager.reset();
        }

        if (m_SceneManager)
        {
            m_SceneManager->Shutdown();
            SceneManager::SetInstance(nullptr);
            m_SceneManager.reset();
        }

        if (m_PhysicsSystem)
        {
            m_PhysicsSystem->Shutdown();
            PhysicsSystem::SetInstance(nullptr);
            m_PhysicsSystem.reset();
        }

        if (m_LuaScriptSystem)
        {
            m_LuaScriptSystem->Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            m_LuaScriptSystem.reset();
        }

        if (m_InputSystem)
        {
            m_InputSystem->Shutdown();
            InputSystem::SetInstance(nullptr);
            m_InputSystem.reset();
        }

        if (m_RenderSystem)
        {
            m_RenderSystem->Shutdown();
            RenderSystem::SetInstance(nullptr);
            m_RenderSystem.reset();
        }

        if (m_WindowSystem)
        {
            m_WindowSystem->Shutdown();
            WindowSystem::SetInstance(nullptr);
            m_WindowSystem.reset();
        }

        if (m_AssetManager)
        {
            m_AssetManager->Shutdown();
            AssetManager::SetInstance(nullptr);
            m_AssetManager.reset();
        }

        if (m_ObjectManager)
        {
            m_ObjectManager->Shutdown();
            ObjectManager::SetInstance(nullptr);
            m_ObjectManager.reset();
        }
    }

    void Engine::LogicalTick(float deltaTime)
    {
        m_InputSystem->Tick(deltaTime);
        m_SceneManager->Tick(deltaTime);
        if (m_PhysicsSystem)
        {
            m_PhysicsSystem->SimulateActiveScene(deltaTime);
        }
        m_SceneManager->SendAllEndOfFrameUpdates();
    }

    void Engine::RendererTick(float deltaTime)
    {
        (void)deltaTime;
        m_RenderSystem->Tick(deltaTime);
    }

    float Engine::CalculateDeltaTime()
    {
        using namespace std::chrono;
        const steady_clock::time_point tickTimePoint = steady_clock::now();
        const duration<float> timeSpan = tickTimePoint - m_LastTickTimePoint;
        m_LastTickTimePoint = tickTimePoint;
        return timeSpan.count();
    }

    float Engine::CalculateFPS(float deltaTime)
    {
        return 1.0f / deltaTime;
    }
}
