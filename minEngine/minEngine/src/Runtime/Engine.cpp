#include "Engine.h"

#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Profiling/Profile.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/GameInstance/GameInstance.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Project/ProjectSettings.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/SceneRendererKind.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"
#include "Runtime/Function/Debug/DebugDrawService.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"
#include "Runtime/Platform/FileDialog/FileDialogService.h"
#include "Runtime/Function/Framework/Time/GameplayFixedTickClock.h"

#include <vector>

namespace minEngine
{
    Engine* Engine::s_Instance = nullptr;

    Engine::Engine() = default;

    Engine::~Engine() = default;

    Engine& Engine::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "Engine is not initialized");
        return *s_Instance;
    }

    void Engine::BeginEngineProfileStartup()
    {
        Profile::Initialize();
        if (!Profile::IsAutoEngineSessionEnabled())
        {
            return;
        }

        Profile::SetEnabled(true);
        Profile::ProfileSessionDesc desc;
        desc.Label = "Engine";
        Profile::StartSession(desc);
        Profile::BeginPhase("Engine.Startup");
    }

    void Engine::EndEngineProfileStartupAndEnterRuntime()
    {
        if (!Profile::IsSessionActive())
        {
            return;
        }

        Profile::EndPhase();
        Profile::BeginPhase("Engine.Runtime");
    }

    void Engine::BeginEngineProfileShutdown()
    {
        if (!Profile::IsSessionActive())
        {
            return;
        }

        Profile::EndPhase();
        Profile::BeginPhase("Engine.Shutdown");
    }

    void Engine::EndEngineProfileShutdown()
    {
        if (!Profile::IsSessionActive())
        {
            return;
        }

        Profile::EndPhase();
        Profile::StopSession();
    }

    void Engine::Initialize(const CommandLineResult& commandLine)
    {
        ME_ASSERT(s_Instance == nullptr, "Engine is already initialized");
        s_Instance = this;

        RHIBackendSelection::Set(commandLine.RHIBackend);
        m_SceneRendererKind = commandLine.SceneRenderer;

        LogSystem::Get().Initialize();
        BeginEngineProfileStartup();
        {
            ME_PROFILE_SCOPE("Engine.Initialize");
            FinializeReflection();

            m_EnginePathConfigLoaded =
                PathRegistry::Get().LoadEngineConfiguration(commandLine, m_EngineConfig);

            {
                ME_PROFILE_SCOPE("Engine.StartSystems");
                StartSystems();
            }

            // Eager-init DebugDrawService on the main thread before efsw / first frame enqueue.
            (void)DebugDrawService::Get();

            // Sky/EnvMap shaders and validation resources — required on every RHI (VK parity).
            if (m_RenderSystem && m_EnginePathConfigLoaded)
            {
                ME_PROFILE_SCOPE("Engine.LoadEngineRenderingAssets");
                m_RenderSystem->LoadEngineRenderingAssets();
            }
        }
        EndEngineProfileStartupAndEnterRuntime();
    }

    void Engine::Initialize(int argc, char** argv)
    {
        ME_ASSERT(s_Instance == nullptr, "Engine is already initialized");
        s_Instance = this;

        LogSystem::Get().Initialize();
        BeginEngineProfileStartup();
        {
            ME_PROFILE_SCOPE("Engine.Initialize");
            FinializeReflection();

            m_EnginePathConfigLoaded =
                PathRegistry::Get().LoadEngineConfiguration(argc, argv, m_EngineConfig);

            {
                ME_PROFILE_SCOPE("Engine.StartSystems");
                StartSystems();
            }

            if (m_RenderSystem && m_EnginePathConfigLoaded)
            {
                ME_PROFILE_SCOPE("Engine.LoadEngineRenderingAssets");
                m_RenderSystem->LoadEngineRenderingAssets();
            }
        }
        EndEngineProfileStartupAndEnterRuntime();
    }

    void Engine::Shutdown()
    {
        ME_LOG(LogCore, Info, "Engine Shutdown Started");
        BeginEngineProfileShutdown();
        {
            ME_PROFILE_SCOPE("Engine.ShutdownSystems");
            ShutdownSystems();
        }
        EndEngineProfileShutdown();
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

        // Retired scenes may have been referenced by draws submitted before retirement; the
        // renderer consumed them just now, so this is the first safe point to destroy them.
        if (m_SceneManager)
        {
            m_SceneManager->FlushRetiredScenes();
        }
    }

    void Engine::TickOneFrame(float deltaTime)
    {
        Profile::BeginFrame();
        {
            ME_PROFILE_SCOPE("Engine.Frame");
            PollEvents();
            {
                ME_PROFILE_SCOPE("Engine.LogicalTick");
                TickLogicalFrame(deltaTime);
            }
            {
                ME_PROFILE_SCOPE("Engine.RendererTick");
                TickRendererFrame(deltaTime);
            }
        }
        Profile::EndFrame();
    }

    void Engine::FinializeReflection()
    {
        Reflection::ReflectionSystem::Get().FinalizeReflection();
        const std::vector<std::string>& reflectionErrors = Reflection::ReflectionSystem::Get().GetLastErrors();
        if (!reflectionErrors.empty())
        {
            for (const std::string& error : reflectionErrors)
            {
                ME_LOG(LogCore, Error, error);
            }
            ME_ASSERT(false, "Reflection System finalization failed. See previous errors for details.");
        }
        else
        {
            ME_LOG(LogCore, Info, "Reflection System finalized successfully.");
            Reflection::ReflectionSystem::Get().ClearErrors();
        }
    }

    void Engine::InitializeEngineSubsystems()
    {
        if (m_EngineSubsystems.IsInitialized())
        {
            return;
        }

        SetClass(Engine::StaticClass());
        m_EngineSubsystems.Initialize(this);
    }

    void Engine::DeinitializeEngineSubsystems()
    {
        if (m_EngineSubsystems.IsInitialized())
        {
            m_EngineSubsystems.Deinitialize();
        }
    }

    GameInstance* Engine::GetGameInstance() const
    {
        if (m_GameInstance)
        {
            return m_GameInstance.get();
        }
        return GetGameInstanceForPIE(0);
    }

    GameInstance* Engine::GetGameInstanceForPIE(int32_t pieInstanceId) const
    {
        const auto it = m_PIEGameInstances.find(pieInstanceId);
        if (it == m_PIEGameInstances.end())
        {
            return nullptr;
        }
        return it->second.get();
    }

    GameInstance* Engine::FindGameInstance(int32_t pieId) const
    {
        if (pieId < 0)
        {
            return m_GameInstance.get();
        }
        return GetGameInstanceForPIE(pieId);
    }

    void Engine::SetGameInstanceClassOverride(const Reflection::MEClass* gameInstanceClass)
    {
        m_GameInstanceClassOverride = gameInstanceClass;
    }

    const Reflection::MEClass* Engine::ResolveGameInstanceClass() const
    {
        const Reflection::MEClass* defaultClass = GameInstance::StaticClass();
        ME_ASSERT(defaultClass != nullptr, "GameInstance::StaticClass missing");

        const Reflection::MEClass* candidate = m_GameInstanceClassOverride;
        if (candidate == nullptr && ProjectManager::HasInstance())
        {
            const std::string& className =
                ProjectManager::Get().GetCurrentProjectCtx().Settings.GameInstanceClass;
            if (!className.empty())
            {
                candidate = Reflection::ReflectionSystem::Get().FindClass(className);
                if (candidate == nullptr)
                {
                    ME_LOG(LogCore, Error,
                        "ResolveGameInstanceClass: unable to find '{}'; falling back to GameInstance.",
                        className);
                    return defaultClass;
                }
            }
        }

        if (candidate == nullptr)
        {
            return defaultClass;
        }

        if (!candidate->IsA(defaultClass))
        {
            ME_LOG(LogCore, Error,
                "ResolveGameInstanceClass: '{}' is not a GameInstance; falling back to GameInstance.",
                candidate->GetName());
            return defaultClass;
        }

        return candidate;
    }

    std::shared_ptr<GameInstance> Engine::CreateGameInstanceOfResolvedClass(
        const std::string& objectName,
        Scene* primaryScene,
        int32_t pieInstanceId)
    {
        const Reflection::MEClass* giClass = ResolveGameInstanceClass();
        std::shared_ptr<MEObject> created = NewObject(giClass, objectName, this);
        std::shared_ptr<GameInstance> gameInstance = Cast<GameInstance>(created);
        ME_ASSERT(gameInstance != nullptr, "Failed to NewObject GameInstance class");

        gameInstance->SetPIEInstanceId(pieInstanceId);
        if (primaryScene != nullptr)
        {
            gameInstance->SetPrimaryScene(primaryScene);
        }
        gameInstance->Init();
        return gameInstance;
    }

    std::shared_ptr<GameInstance> Engine::CreateGameInstance(Scene* primaryScene)
    {
        ME_ASSERT(ObjectManager::HasInstance(), "CreateGameInstance requires ObjectManager");
        ME_ASSERT(m_GameInstance == nullptr, "Standalone GameInstance already exists");

        m_GameInstance = CreateGameInstanceOfResolvedClass("GameInstance", primaryScene, -1);
        return m_GameInstance;
    }

    void Engine::DestroyGameInstance()
    {
        if (m_GameInstance == nullptr)
        {
            return;
        }

        m_GameInstance->Shutdown();
        ObjectManager::Get().UnregisterObject(m_GameInstance.get());
        m_GameInstance.reset();
    }

    std::shared_ptr<GameInstance> Engine::CreateGameInstanceForPIE(int32_t pieInstanceId, Scene* primaryScene)
    {
        ME_ASSERT(ObjectManager::HasInstance(), "CreateGameInstanceForPIE requires ObjectManager");
        ME_ASSERT(pieInstanceId >= 0, "PIE instance id must be >= 0");
        ME_ASSERT(m_PIEGameInstances.find(pieInstanceId) == m_PIEGameInstances.end(),
            "GameInstance already exists for this PIE instance");

        std::shared_ptr<GameInstance> gameInstance = CreateGameInstanceOfResolvedClass(
            "GameInstance_PIE_" + std::to_string(pieInstanceId),
            primaryScene,
            pieInstanceId);
        m_PIEGameInstances[pieInstanceId] = gameInstance;
        return gameInstance;
    }

    void Engine::DestroyGameInstanceForPIE(int32_t pieInstanceId)
    {
        const auto it = m_PIEGameInstances.find(pieInstanceId);
        if (it == m_PIEGameInstances.end())
        {
            return;
        }

        if (it->second)
        {
            it->second->Shutdown();
            ObjectManager::Get().UnregisterObject(it->second.get());
        }
        m_PIEGameInstances.erase(it);
    }

    void Engine::DestroyAllPIEGameInstances()
    {
        std::vector<int32_t> pieIds;
        pieIds.reserve(m_PIEGameInstances.size());
        for (const auto& [pieId, unused] : m_PIEGameInstances)
        {
            (void)unused;
            pieIds.push_back(pieId);
        }
        for (int32_t pieId : pieIds)
        {
            DestroyGameInstanceForPIE(pieId);
        }
    }

    void Engine::StartSystems()
    {
        // Process-level facility (not Engine-owned); safe if tests already injected an instance.
        ObjectManager::Initialize();
        InitializeEngineSubsystems();

        m_GameplayTagManager = std::make_shared<GameplayTagManager>();
        GameplayTagManager::SetInstance(m_GameplayTagManager.get());
        m_GameplayTagManager->Initialize();

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
        m_RenderSystem->Initialize(m_SceneRendererKind);

        m_LuaScriptSystem = std::make_shared<LuaScriptSystem>();
        LuaScriptSystem::SetInstance(m_LuaScriptSystem.get());
        m_LuaScriptSystem->Initialize();

        m_SceneManager = std::make_shared<SceneManager>();
        SceneManager::SetInstance(m_SceneManager.get());
        m_SceneManager->Initialize();

        m_AudioSystem = std::make_shared<AudioSystem>();
        AudioSystem::SetInstance(m_AudioSystem.get());
        m_AudioSystem->Initialize();

        m_PhysicsSystem = std::make_shared<PhysicsSystem>();
        PhysicsSystem::SetInstance(m_PhysicsSystem.get());
        m_PhysicsSystem->Initialize();

        m_UISystem = std::make_shared<UISystem>();
        UISystem::SetInstance(m_UISystem.get());
        m_UISystem->Initialize();
    }

    void Engine::ShutdownSystems()
    {
        DestroyAllPIEGameInstances();
        DestroyGameInstance();
        DeinitializeEngineSubsystems();

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

        if (m_AudioSystem)
        {
            m_AudioSystem->Shutdown();
            AudioSystem::SetInstance(nullptr);
            m_AudioSystem.reset();
        }

        if (m_PhysicsSystem)
        {
            m_PhysicsSystem->Shutdown();
            PhysicsSystem::SetInstance(nullptr);
            m_PhysicsSystem.reset();
        }

        if (m_UISystem)
        {
            m_UISystem->Shutdown();
            UISystem::SetInstance(nullptr);
            m_UISystem.reset();
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

        ObjectManager::Shutdown();

        if (m_GameplayTagManager)
        {
            m_GameplayTagManager->Shutdown();
            GameplayTagManager::SetInstance(nullptr);
            m_GameplayTagManager.reset();
        }
    }

    void Engine::TickTickableSubsystems(float deltaTime)
    {
        if (m_GameInstance)
        {
            m_GameInstance->TickSubsystems(deltaTime);
        }
        for (const auto& [pieId, gameInstance] : m_PIEGameInstances)
        {
            (void)pieId;
            if (gameInstance)
            {
                gameInstance->TickSubsystems(deltaTime);
            }
        }

        if (SceneManager::HasInstance())
        {
            SceneManager::Get().TickWorldSubsystems(deltaTime);
        }
    }

    void Engine::TickGameplayVariablePhase(float deltaTime)
    {
        if (m_GameInstance)
        {
            m_GameInstance->TickSubsystems(deltaTime);
        }
        for (const auto& [pieId, gameInstance] : m_PIEGameInstances)
        {
            (void)pieId;
            if (gameInstance)
            {
                gameInstance->TickSubsystems(deltaTime);
            }
        }

        if (SceneManager::HasInstance())
        {
            SceneManager::Get().TickVariableGameplay(deltaTime);
        }
    }

    void Engine::TickGameplayFixedPhase(float frameDt)
    {
        if (!m_GameplayFixedTickClock)
        {
            return;
        }

        GameplayFixedTickClock& clock = *m_GameplayFixedTickClock;
        const int stepCount = clock.BeginFrameAndConsumeSteps(frameDt);
        if (stepCount <= 0)
        {
            return;
        }

        const float fixedDt = clock.GetIntervalSeconds();
        for (int step = 0; step < stepCount; ++step)
        {
            if (m_GameInstance)
            {
                m_GameInstance->OnFixedTick(fixedDt);
            }
            for (const auto& [pieId, gameInstance] : m_PIEGameInstances)
            {
                (void)pieId;
                if (gameInstance)
                {
                    gameInstance->OnFixedTick(fixedDt);
                }
            }

            if (SceneManager::HasInstance())
            {
                SceneManager::Get().FixedTickGameplay(fixedDt);
            }

            clock.NotifyStepCompleted();
        }

        clock.FlushPendingInterval();
    }

    void Engine::SetGameplayFixedTickInterval(float seconds)
    {
        if (!m_GameplayFixedTickClock)
        {
            m_GameplayFixedTickClock = std::make_unique<GameplayFixedTickClock>();
        }
        m_GameplayFixedTickClock->SetIntervalSeconds(seconds);
    }

    float Engine::GetGameplayFixedTickInterval() const
    {
        if (!m_GameplayFixedTickClock)
        {
            return 0.0f;
        }
        return m_GameplayFixedTickClock->GetIntervalSeconds();
    }

    void Engine::SetGameplayFixedTicksPerSecond(float ticksPerSecond)
    {
        if (!m_GameplayFixedTickClock)
        {
            m_GameplayFixedTickClock = std::make_unique<GameplayFixedTickClock>();
        }
        m_GameplayFixedTickClock->SetTicksPerSecond(ticksPerSecond);
    }

    void Engine::SetGameplayFixedTimeScale(float scale)
    {
        if (!m_GameplayFixedTickClock)
        {
            m_GameplayFixedTickClock = std::make_unique<GameplayFixedTickClock>();
        }
        m_GameplayFixedTickClock->SetTimeScale(scale);
    }

    float Engine::GetGameplayFixedTimeScale() const
    {
        if (!m_GameplayFixedTickClock)
        {
            return 1.0f;
        }
        return m_GameplayFixedTickClock->GetTimeScale();
    }

    void Engine::SetGameplayFixedMaxStepsPerFrame(int maxSteps)
    {
        if (!m_GameplayFixedTickClock)
        {
            m_GameplayFixedTickClock = std::make_unique<GameplayFixedTickClock>();
        }
        m_GameplayFixedTickClock->SetMaxStepsPerFrame(maxSteps);
    }

    void Engine::SetGameplayFixedTickPaused(bool paused)
    {
        if (!m_GameplayFixedTickClock)
        {
            m_GameplayFixedTickClock = std::make_unique<GameplayFixedTickClock>();
        }
        m_GameplayFixedTickClock->SetPaused(paused);
    }

    bool Engine::IsGameplayFixedTickEnabled() const
    {
        return m_GameplayFixedTickClock && m_GameplayFixedTickClock->IsEnabled();
    }

    uint64_t Engine::GetGameplayFixedTickIndex() const
    {
        if (!m_GameplayFixedTickClock)
        {
            return 0;
        }
        return m_GameplayFixedTickClock->GetTickIndex();
    }

    void Engine::LogicalTick(float deltaTime)
    {
        m_InputSystem->Tick(deltaTime);
        TickGameplayFixedPhase(deltaTime);
        TickGameplayVariablePhase(deltaTime);
        if (m_AudioSystem)
        {
            m_AudioSystem->Tick(deltaTime);
        }
        if (m_PhysicsSystem)
        {
            m_PhysicsSystem->SimulateActiveScene(deltaTime);
        }
        if (m_UISystem)
        {
            m_UISystem->Tick(deltaTime);
        }
        m_SceneManager->SendAllEndOfFrameUpdates();
        m_SceneManager->FlushDeferredStructuralEdits();
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
