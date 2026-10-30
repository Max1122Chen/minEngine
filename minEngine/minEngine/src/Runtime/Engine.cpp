#include "Engine.h"


#include "Runtime/Core/Assert/AssertLog.h"
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
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/SceneRendererKind.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/Pointer/PointerPolicyService.h"
#include "Runtime/Function/Pointer/WindowSystemPointerModeSink.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"
#include "Runtime/Function/Debug/DebugDrawService.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"
#include "Runtime/Platform/FileDialog/FileDialogService.h"
#include "Runtime/Function/Framework/Time/GameplayFixedTickClock.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHICommandListPool.h"

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

    bool Engine::HasInstance()
    {
        return s_Instance != nullptr;
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

    void Engine::InitializeFrameThreading(std::optional<FrameThreadMode> commandLineOverride)
    {
        const std::optional<FrameThreadMode> resolved = FrameThreading::ResolveMode(commandLineOverride);
        if (!resolved.has_value())
        {
            ME_LOG(
                LogCore,
                Error,
                "Invalid MINENGINE_FRAME_THREADS / --frame-threads value. Expected single|multi.");
            // Startup continues with the default topology: a wrong mode string is a configuration
            // defect, and the frame roles it would have built are the default ones anyway.
            ME_ENSURE(false, "Invalid FrameThreadMode configuration");
            return;
        }

        m_FrameThreading = std::make_unique<FrameThreading>();
        m_FrameThreading->Initialize(*resolved);
    }

    void Engine::InitializeRenderCommandPipe()
    {
        m_RenderCommandPipe = std::make_unique<RenderCommandPipe>();
        m_RenderCommandPipe->Initialize();
        m_GameLogicFrameIndex = 0;
    }

    void Engine::Initialize(const CommandLineResult& commandLine)
    {
        ME_ASSERT(s_Instance == nullptr, "Engine is already initialized");
        s_Instance = this;

        RHIBackendSelection::Set(commandLine.RHIBackend);
        m_SceneRendererKind = commandLine.SceneRenderer;

        LogSystem::Get().Initialize();
        InitializeFrameThreading(commandLine.FrameThreads);
        InitializeRenderCommandPipe();
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
        InitializeFrameThreading(std::nullopt);
        InitializeRenderCommandPipe();
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

    void Engine::TickRHIFrameDefault()
    {
        if (RHICommandListPool::HasInstance())
        {
            while (RHICommandList* list = RHICommandListPool::Get().TryTakeFinished())
            {
                list->Replay();
                RHICommandListPool::Get().Release(list);
            }
        }
        if (m_RenderSystem)
        {
            m_RenderSystem->PresentFrame();
        }
        else if (WindowSystem::HasInstance())
        {
            WindowSystem::Get().SwapBuffers();
        }
    }

    bool Engine::TryStartMultiFrameRoles(
        TaskSystem::RenderFrameTaskFn renderOverride,
        TaskSystem::RHIFrameTaskFn rhiOverride)
    {
        if (!m_FrameThreading || !m_FrameThreading->IsMulti())
        {
            // Single topology: the game thread owns every role, so there is nothing to start.
            return true;
        }
        if (!m_TaskSystem || !m_TaskSystem->IsRunning())
        {
            ME_LOG(LogCore, Error, "StartMultiFrameRoles: TaskSystem is not running; frame roles cannot start.");
            return false;
        }
        if (m_TaskSystem->AreFrameRolesRunning())
        {
            return true;
        }
        if (!m_RenderSystem || m_RenderSystem->GetRHI() == nullptr)
        {
            ME_LOG(LogCore, Error, "StartMultiFrameRoles: RenderSystem/RHI is required; frame roles cannot start.");
            return false;
        }

        m_RHICommandListPool = std::make_unique<RHICommandListPool>();
        m_RHICommandListPool->Initialize(m_RenderSystem->GetRHI());

        TaskSystem::RenderFrameTaskFn renderTask = renderOverride
            ? std::move(renderOverride)
            : TaskSystem::RenderFrameTaskFn([this](float dt) { TickRendererFrame(dt); });
        TaskSystem::RHIFrameTaskFn rhiTask = rhiOverride
            ? std::move(rhiOverride)
            : TaskSystem::RHIFrameTaskFn([this]() { TickRHIFrameDefault(); });

        const bool combineGraphics = RHIBackendSelection::IsOpenGL();
        m_TaskSystem->StartFrameRoles(std::move(renderTask), std::move(rhiTask), combineGraphics);

        if (!m_TaskSystem->AreFrameRolesRunning())
        {
            ME_LOG(LogCore, Error, "StartMultiFrameRoles: frame roles did not start; the engine stays on the game thread.");
            return false;
        }
        return true;
    }

    void Engine::StartMultiFrameRoles(
        TaskSystem::RenderFrameTaskFn renderOverride,
        TaskSystem::RHIFrameTaskFn rhiOverride)
    {
        (void)TryStartMultiFrameRoles(std::move(renderOverride), std::move(rhiOverride));
    }

    void Engine::StopMultiFrameRoles()
    {
        if (m_TaskSystem && m_TaskSystem->AreFrameRolesRunning())
        {
            m_TaskSystem->StopFrameRoles();
        }
        if (m_RHICommandListPool)
        {
            m_RHICommandListPool->RequestShutdown();
            m_RHICommandListPool->Shutdown();
            m_RHICommandListPool.reset();
        }
    }

    bool Engine::AreMultiFrameRolesRunning() const
    {
        return m_TaskSystem && m_TaskSystem->AreFrameRolesRunning();
    }

    namespace
    {
        /**
         * True when a retired Scene is waiting for a frame boundary.
         * Asking before the submission is what keeps a normal frame asynchronous: only a frame that
         * has something to release waits for its own completion, because only that frame is the
         * point after which the retired Scene can no longer be referenced.
         */
        bool HasFrameReleasesPending(const SceneManager* sceneManager)
        {
            return sceneManager != nullptr && sceneManager->HasRetiredScenes();
        }

        /** Baseline for the work-loss summary. Written by the game role, which is its only caller. */
        FrameWorkLossCounters s_LastFrameWorkLoss;

        /**
         * The frame's one reporting point, and the reason the contract records survive outside
         * Engine::Run: publishing a staged violation needs a moment with no log lock held, and a
         * host that drives the frame itself - the editor does - never reaches that loop.
         *
         * Publishes the violations staged since the previous frame, then at most one line saying
         * what the frame roles lost in the same interval, so a frame that dropped N commands shows
         * up in the log as N instead of as N lines. A frame that lost nothing logs nothing.
         *
         * tasks is null on a host that owns no task pool; the violations are still published, and
         * only the counters that pool owns stay at zero.
         */
        void ReportFrameWorkLoss(const TaskSystem* tasks)
        {
            // The frame boundary owns the one moment with no log lock held, so it is where the
            // violations recorded since the previous frame become log records - and where the count
            // the summary reports is read, in the same call, so the summary can never describe
            // records the log has not received yet.
            const ContractViolationFrameSample contractViolations =
                PublishAndSampleContractViolations();

            FrameWorkLossCounters current;
            if (RenderCommandPipe::HasInstance())
            {
                const RenderCommandPipe::Counters pipe = RenderCommandPipe::Get().GetCounters();
                current.DroppedCommands = pipe.DroppedStaleCommands + pipe.DroppedFullCommands;
                current.DroppedViewIntents = pipe.DroppedStaleViewIntents + pipe.DroppedFullViewIntents;
                current.ApplyFailures = pipe.FlushFailureCount;
            }
            if (tasks != nullptr)
            {
                current.FailedFrames = tasks->GetFailedFrameCount();
                current.DroppedFrames = tasks->GetDroppedQueuedFrameCount();
                current.RenderWritesDuringFrame = tasks->GetGameRoleRenderWriteCount();
            }
            current.ContractViolations = contractViolations.Counters.Total();
            // Both are process-wide totals, like the contract count above: a summed count keeps
            // rising, so the delta in the line is what happened between two frames.
            current.OffRoleCoreServiceCalls = ObjectManager::GetOffRoleCoreServiceCallCountTotal();
            current.OffRoleRenderCommands = RHICommandList::GetOffRoleCommandRefusalCount();

            const std::string line = DescribeFrameWorkLossDelta(s_LastFrameWorkLoss, current);
            s_LastFrameWorkLoss = current;
            if (!line.empty())
            {
                ME_LOG(LogCore, Warn, "{}", line);
            }
        }
    }

    void Engine::Run()
    {
        if (m_FrameThreading && m_FrameThreading->IsMulti() && !TryStartMultiFrameRoles())
        {
            // Falling back to TickOneFrame here used to look harmless and then asserted in the
            // renderer: in Multi the game thread does not own the Render role, so drawing on it is
            // a contract violation, not a slower mode.
            ME_LOG(
                LogCore,
                Error,
                "Engine::Run: Multi frame roles could not start; refusing to run. The single-thread "
                "fallback is not available in the Multi topology.");
            return;
        }

        WindowSystem& windowSystem = WindowSystem::Get();
        while (!windowSystem.ShouldClose())
        {
            const float deltaTime = CalculateDeltaTime();
            if (AreMultiFrameRolesRunning())
            {
                Profile::BeginFrame();
                {
                    ME_PROFILE_SCOPE("Engine.Frame");
                    PollEvents();
                    {
                        ME_PROFILE_SCOPE("Frame.Game.Logical");
                        TickLogicalFrame(deltaTime);
                    }
                    // The frame submitted here is the first one that can no longer reference what
                    // the game role just released, so its completion is the release gate. Waiting
                    // for it costs a stall only on the frames that actually retired or handed
                    // something over.
                    const bool bReleasesPending = HasFrameReleasesPending(m_SceneManager.get());
                    m_TaskSystem->SubmitRenderFrameTask(deltaTime, bReleasesPending);
                    if (bReleasesPending)
                    {
                        // The release boundary reports the frame's diagnostics along with the release.
                        CompleteFrameReleases();
                    }
                    else
                    {
                        // Nothing had to be released, so the boundary above is not reached this
                        // frame. The diagnostics hang off the frame, not off a queue that happened
                        // to be non-empty, so they are reported here instead.
                        ReportFrameWorkLoss(m_TaskSystem.get());
                    }
                }
                Profile::EndFrame();
            }
            else
            {
                TickOneFrame(deltaTime);
            }
        }

        StopMultiFrameRoles();
    }

    void Engine::PollEvents()
    {
        WindowSystem::Get().PollEvents();
    }

    void Engine::TickLogicalFrame(float deltaTime)
    {
        if (FrameThreading::HasInstance())
        {
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Game, "TickLogicalFrame");
        }
        LogicalTick(deltaTime);

        // End of the game tick, before the frame is handed to the render role: material parameter
        // writes were staged during the tick, and this is the one point where they are published.
        // A snapshot taken here is what the next recorded frame draws with, so the delay between a
        // Set* call and its pixels is at most one tick - and every write inside a tick coalesces.
        MaterialRenderProxyPublisher::PublishPendingGameData();

        if (RenderCommandPipe::HasInstance())
        {
            // The game role owns the logical frame number. The tick that just finished has now
            // enqueued every command of that frame, so the game publishes it as the render role's
            // drain target together with the world generation it committed; the next tick tags its
            // commands with the following frame. Single topology walks the same path - both roles
            // share the thread but only this call moves the frame on.
            RenderCommandPipe& pipe = RenderCommandPipe::Get();
            pipe.SetPublishedWorldGen(m_GameLogicFrameIndex);
            pipe.PublishGameFrame(m_GameLogicFrameIndex, m_GameLogicFrameIndex + 1);
            ++m_GameLogicFrameIndex;
        }
    }

    void Engine::TickRendererFrame(float deltaTime)
    {
        (void)deltaTime;
        if (FrameThreading::HasInstance())
        {
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Render, "TickRendererFrame");
        }

        // Frame start, before this frame records anything: create the GPU objects that were asked
        // for from a thread that cannot drive the graphics API. This is the only moment that has
        // both the graphics context and the guarantee that nothing has named those objects yet,
        // so a resource created during the previous game tick is fully usable from this frame on.
        if (m_RenderSystem != nullptr)
        {
            if (RHI* rhi = m_RenderSystem->GetRHI())
            {
                ME_PROFILE_SCOPE("Frame.Render.CreateResources");
                rhi->RHIDrainDeferredCreates();
            }
        }

        // The render role never advances either frame number: it reads the drain target the game
        // role published and records its own progress while draining. A dedicated render role with
        // its own frame counter used to be the owner here, which made the generation check below
        // compare a value with itself.
        uint64_t completedFrame = UINT64_MAX;
        if (RenderCommandPipe::HasInstance())
        {
            RenderCommandPipe& pipe = RenderCommandPipe::Get();
            completedFrame = pipe.GetCompletedGameFrame();

            // Two independent sources: the generation the game role published, and the frame this
            // pipe drained for the render role. A failure means the render role has drawn a world
            // the game role never committed. It is read before the drain below on purpose: the
            // drain is what moves the drawn frame, so afterwards the two numbers would only be
            // compared against each other for this frame.
            std::string generationDiagnostic;
            if (!pipe.CheckPublishedGenerationForDrawnFrame(&generationDiagnostic))
            {
                ME_LOG(LogCore, Error, "{}", generationDiagnostic);
                // The frame is wrong, not the memory: the pipe already refused the commands for a
                // world the game role never committed, so the run continues and the violation is
                // counted.
                ME_ENSURE(false, "%s", generationDiagnostic.c_str());
            }

            {
                // The frame's scene changes are applied before anything is recorded, so what this
                // frame draws is the state the game role handed over - not the state of one frame
                // ago. Everything at or below the completed game frame is applied in order: a
                // command that reached the queue late still lands in this drain instead of being
                // parked until it stalls the producer.
                ME_PROFILE_SCOPE("Frame.Render.Apply");
                const bool bAllCommandsApplied = pipe.FlushApplyForFrameChecked(completedFrame);
                if (!bAllCommandsApplied && m_TaskSystem)
                {
                    // The isolated command failure already logged which command threw; the frame is
                    // settled as not completed so the game side sees a frame it cannot trust instead
                    // of a frame that merely "finished".
                    m_TaskSystem->MarkCurrentFrameIncomplete("render command flush");
                }
            }

            if (completedFrame != UINT64_MAX)
            {
                ME_PROFILE_SCOPE("Frame.Render.View");
                std::vector<SceneDrawDesc> intents;
                pipe.TakeViewIntents(completedFrame, intents);
                if (m_RenderSystem)
                {
                    m_RenderSystem->ReplacePendingDraws(std::move(intents));
                }
            }
        }

        {
            ME_PROFILE_SCOPE("Frame.Render.Record");
            RendererTick(deltaTime);
        }

        // Frame end, after the last draw command of this frame has been recorded: the proxies the
        // drain above parked are destroyed here. Recording is the step that reads a scene proxy -
        // a recorded command carries GPU handles copied out of it, so once the last one is out, no
        // later step of this frame can name the proxy again, and the game role can only tag new
        // commands for a later frame. What a parked proxy held - its GPU buffers and its material
        // render object - is released here; the native objects behind them are retired through the
        // RHI queue and die at the tail of the batch that carries this frame's draws, which is the
        // submission point of those draws and not this one. A frame that draws nothing reaches this
        // point too: it hangs off the frame, not off a queue that happened to be non-empty.
        RenderScene::FlushPendingProxyDeletes();

        // No Scene release happens here: this function only records the frame. The RHI work that
        // actually consumes the recording (Replay/Present) has not run yet, so a Scene retired for
        // this frame may still be referenced by everything queued behind it. The gate is
        // CompleteFrameReleases on the game role, once that work is finished.
    }

    void Engine::CompleteFrameReleases()
    {
        if (FrameThreading::HasInstance())
        {
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Game, "CompleteFrameReleases");
        }

        // A retired Scene is exactly the thing the frame that just finished could still reference,
        // so this is the earliest point after which destroying it is safe.
        if (m_SceneManager)
        {
            m_SceneManager->FlushRetiredScenes();
        }

        // The frame's diagnostics belong to this boundary, not to Engine::Run: a host that drives
        // its own frame loop - the editor does, in both topologies - reaches exactly this call per
        // frame, and everything it staged during the frame (contract records, work lost by the frame
        // roles) becomes log records here.
        ReportFrameWorkLoss(m_TaskSystem.get());
    }

    void Engine::TickOneFrame(float deltaTime)
    {
        Profile::BeginFrame();
        {
            ME_PROFILE_SCOPE("Engine.Frame");
            PollEvents();
            {
                ME_PROFILE_SCOPE("Frame.Game.Logical");
                TickLogicalFrame(deltaTime);
            }
            {
                // Single mode: Render + RHI roles share this OS thread and this tick, so the frame's
                // recording and its present both finish inside this call.
                ME_PROFILE_SCOPE("Frame.Render.Record");
                TickRendererFrame(deltaTime);
            }
            if (m_RenderSystem)
            {
                m_RenderSystem->PresentFrame();
            }
            else if (WindowSystem::HasInstance())
            {
                WindowSystem::Get().SwapBuffers();
            }
            // Also the frame's one reporting point, so it is not repeated here.
            CompleteFrameReleases();
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
            // Each error is logged above; reflection stays usable without its finalize pass, so this
            // is a recorded violation rather than a reason to stop a running editor.
            ME_ENSURE(false, "Reflection System finalization failed. See previous errors for details.");
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

        m_TaskSystem = std::make_unique<TaskSystem>();
        TaskSystem::SetInstance(m_TaskSystem.get());
        m_TaskSystem->Init();

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

        m_PointerPolicyService = std::make_unique<PointerPolicyService>();
        PointerPolicyService::SetInstance(m_PointerPolicyService.get());
        m_PointerModeSink = std::make_unique<WindowSystemPointerModeSink>(*m_WindowSystem);
        m_PointerPolicyService->SetSink(m_PointerModeSink.get());
        m_PointerPolicyService->Initialize();
        m_WindowSystem->RegisterOnWindowFocusCallback(
            [](bool focused)
            {
                if (!PointerPolicyService::HasInstance())
                {
                    return;
                }
                if (focused)
                {
                    PointerPolicyService::Get().OnWindowFocusGained();
                }
                else
                {
                    PointerPolicyService::Get().OnWindowFocusLost();
                }
            });
    }

    void Engine::ShutdownSystems()
    {
        // Stop the role threads first: StopMultiFrameRoles refuses new submissions, drains what is
        // already queued and joins. Tearing down the subsystems and the world before that leaves
        // Render/RHI ticking objects that are being destroyed underneath them.
        StopMultiFrameRoles();

        DestroyAllPIEGameInstances();
        DestroyGameInstance();
        DeinitializeEngineSubsystems();

        if (m_AssetManager)
        {
            m_AssetManager->FlushAsyncLoadsForShutdown();
        }

        if (m_TaskSystem)
        {
            m_TaskSystem->Shutdown();
            TaskSystem::SetInstance(nullptr);
            m_TaskSystem.reset();
        }

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
            // The scene teardown assumes nothing is left that ticks the world: the frame roles were
            // stopped at the top of this function and the pool and the TaskSystem are gone, so no
            // role can be reading the scene graph as the teardown releases it. The order is decided
            // here and nowhere else, which is why it is stated here.
            ME_ENSURE(
                m_TaskSystem == nullptr,
                "Engine::ShutdownSystems reached the scene teardown with the TaskSystem still alive: "
                "the frame roles may still be running, and the teardown releases render state they "
                "read.");
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

        if (m_PointerPolicyService)
        {
            m_PointerPolicyService->Shutdown();
            PointerPolicyService::SetInstance(nullptr);
            m_PointerPolicyService.reset();
            m_PointerModeSink.reset();
        }

        // The asset and object strands go before the device does: releasing a Material, a Texture2D
        // or any other object with a render proxy hands its native handles to the RHI's deferred
        // destroy queue, and a handle queued after the backend shut down has nobody left to destroy
        // it. Everything that still holds a GPU handle is released above this line, so the render
        // system can drain the queue and only then take the device down.
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

        if (m_RenderCommandPipe)
        {
            m_RenderCommandPipe->Shutdown();
            m_RenderCommandPipe.reset();
        }

        if (m_FrameThreading)
        {
            m_FrameThreading->Shutdown();
            m_FrameThreading.reset();
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
        if (TaskSystem::HasInstance() && TaskSystem::Get().IsRunning())
        {
            TaskSystem::Get().DrainCompletions();
        }
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
        if (PointerPolicyService::HasInstance())
        {
            PointerPolicyService::Get().BeginFrame();
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
