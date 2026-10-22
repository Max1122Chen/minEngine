#pragma once
#include "Core.h"
#include "Runtime/Core/CLI/CommandLineResult.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Subsystems/Subsystem.h"
#include "Runtime/Function/Framework/Subsystems/SubsystemCollection.h"
#include "Runtime/Function/Render/SceneRendererKind.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace minEngine::Reflection
{
    class MEClass;
}

namespace minEngine
{
    class ProjectManager;
    class AssetManager;
    class WindowSystem;
    class InputSystem;
    class RenderSystem;
    class SceneManager;
    class PhysicsSystem;
    class AudioSystem;
    class UISystem;
    class FileDialogService;
    class LuaScriptSystem;
    class GameplayTagManager;
    class GameInstance;
    class Scene;
    class GameplayFixedTickClock;

    ME_CLASS()
    class MINENGINE_API Engine : public MEObject
    {
        ME_GENERATED_BODY()

    public:
        Engine();
        ~Engine() override;

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void Initialize(int argc = 0, char** argv = nullptr);
        void Initialize(const CommandLineResult& commandLine);
        void Shutdown();
        void Run();

        static Engine& Get();
        static bool HasInstance();

        void TickOneFrame(float deltaTime);
        void PollEvents();
        void TickLogicalFrame(float deltaTime);
        void TickRendererFrame(float deltaTime);
        /**
         * Variable gameplay phase: GI Tickable then Scene WorldTickable + GO.
         * LogicalTick calls this before Physics.
         */
        void TickGameplayVariablePhase(float deltaTime);
        /** GI + World Tickable only (no GO). Prefer TickGameplayVariablePhase for full phase. */
        void TickTickableSubsystems(float deltaTime);
        /**
         * Fixed gameplay phase: drain FixedTick clock (GI OnFixedTick then World + GO).
         * LogicalTick calls this after Input and before the variable phase.
         */
        void TickGameplayFixedPhase(float frameDt);

        /** Fixed clock API (Interval <= 0 disables). */
        void SetGameplayFixedTickInterval(float seconds);
        float GetGameplayFixedTickInterval() const;
        void SetGameplayFixedTicksPerSecond(float ticksPerSecond);
        void SetGameplayFixedTimeScale(float scale);
        float GetGameplayFixedTimeScale() const;
        void SetGameplayFixedMaxStepsPerFrame(int maxSteps);
        void SetGameplayFixedTickPaused(bool paused);
        bool IsGameplayFixedTickEnabled() const;
        uint64_t GetGameplayFixedTickIndex() const;

        float CalculateDeltaTime();
        float CalculateFPS(float deltaTime);

        bool IsEnginePathConfigLoaded() const { return m_EnginePathConfigLoaded; }
        const EngineConfig& GetEngineConfig() const { return m_EngineConfig; }

        SubsystemCollection<EngineSubsystem>& GetEngineSubsystemCollection() { return m_EngineSubsystems; }
        const SubsystemCollection<EngineSubsystem>& GetEngineSubsystemCollection() const
        {
            return m_EngineSubsystems;
        }

        template<typename T>
        T* GetEngineSubsystem() const
        {
            return m_EngineSubsystems.GetSubsystem<T>();
        }

        /** Standalone GI, or primary PIE (id 0) when Standalone is absent. */
        GameInstance* GetGameInstance() const;
        GameInstance* GetGameInstanceForPIE(int32_t pieInstanceId) const;
        /** pieId == -1 → Standalone; otherwise PIE map. */
        GameInstance* FindGameInstance(int32_t pieId) const;

        void InitializeEngineSubsystems();
        void DeinitializeEngineSubsystems();

        std::shared_ptr<GameInstance> CreateGameInstance(Scene* primaryScene = nullptr);
        void DestroyGameInstance();

        std::shared_ptr<GameInstance> CreateGameInstanceForPIE(int32_t pieInstanceId, Scene* primaryScene = nullptr);
        void DestroyGameInstanceForPIE(int32_t pieInstanceId);
        void DestroyAllPIEGameInstances();

        /**
         * Resolve project (or override) GameInstance class. Always non-null; invalid names
         * fall back to GameInstance. Standalone and PIE share this path.
         */
        const Reflection::MEClass* ResolveGameInstanceClass() const;

        /** Test / tooling bypass for ProjectSettings.GameInstanceClass. nullptr clears. */
        void SetGameInstanceClassOverride(const Reflection::MEClass* gameInstanceClass);

    private:
        std::shared_ptr<GameInstance> CreateGameInstanceOfResolvedClass(
            const std::string& objectName,
            Scene* primaryScene,
            int32_t pieInstanceId);
        void FinializeReflection();
        void StartSystems();
        void ShutdownSystems();
        void LogicalTick(float deltaTime);
        void RendererTick(float deltaTime);
        void BeginEngineProfileStartup();
        void EndEngineProfileStartupAndEnterRuntime();
        void BeginEngineProfileShutdown();
        void EndEngineProfileShutdown();

        static Engine* s_Instance;

        SubsystemCollection<EngineSubsystem> m_EngineSubsystems;
        std::shared_ptr<GameInstance> m_GameInstance;
        std::unordered_map<int32_t, std::shared_ptr<GameInstance>> m_PIEGameInstances;
        const Reflection::MEClass* m_GameInstanceClassOverride = nullptr;

        std::shared_ptr<ProjectManager> m_ProjectManager;
        std::shared_ptr<AssetManager> m_AssetManager;
        std::shared_ptr<WindowSystem> m_WindowSystem;
        std::shared_ptr<InputSystem> m_InputSystem;
        std::shared_ptr<RenderSystem> m_RenderSystem;
        std::shared_ptr<SceneManager> m_SceneManager;
        std::shared_ptr<PhysicsSystem> m_PhysicsSystem;
        std::shared_ptr<AudioSystem> m_AudioSystem;
        std::shared_ptr<UISystem> m_UISystem;
        std::shared_ptr<FileDialogService> m_FileDialogService;
        std::shared_ptr<LuaScriptSystem> m_LuaScriptSystem;
        std::shared_ptr<GameplayTagManager> m_GameplayTagManager;

        EngineConfig m_EngineConfig;
        bool m_EnginePathConfigLoaded = false;
        SceneRendererKind m_SceneRendererKind = SceneRendererKind::Forward;

        std::chrono::steady_clock::time_point m_LastTickTimePoint{std::chrono::steady_clock::now()};
        std::unique_ptr<GameplayFixedTickClock> m_GameplayFixedTickClock;
    };
}

#include "Engine.gen.h"
