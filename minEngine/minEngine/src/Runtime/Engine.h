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

    ME_CLASS()
    class Engine : public MEObject
    {
        ME_GENERATED_BODY()

    public:
        Engine() = default;
        ~Engine() override = default;

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void Initialize(int argc = 0, char** argv = nullptr);
        void Initialize(const CommandLineResult& commandLine);
        void Shutdown();
        void Run();

        static Engine& Get();
        static bool HasInstance() { return s_Instance != nullptr; }

        void TickOneFrame(float deltaTime);
        void PollEvents();
        void TickLogicalFrame(float deltaTime);
        void TickRendererFrame(float deltaTime);
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

    private:
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
    };
}

#include "Engine.gen.h"
