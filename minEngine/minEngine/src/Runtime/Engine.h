#pragma once
#include "Core.h"

#include <chrono>
#include <memory>
#include <string>

namespace minEngine
{
    class ObjectManager;
    class ProjectManager;
    class AssetManager;
    class WindowSystem;
    class InputSystem;
    class RenderSystem;
    class SceneManager;

    class Engine
    {
    public:
        Engine() = default;
        ~Engine() = default;

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void Initialize(int argc = 0, char** argv = nullptr);
        void Shutdown();
        void Run();

        static Engine& Get();

        void TickOneFrame(float deltaTime);
        void PollEvents();
        void TickLogicalFrame(float deltaTime);
        void TickRendererFrame(float deltaTime);
        float CalculateDeltaTime();
        float CalculateFPS(float deltaTime);

        void SetEngineDefaultAssetsRoot(std::string path);
        const std::string& GetEngineDefaultAssetsRoot() const { return m_EngineDefaultAssetsRoot; }

    private:
        void FinializeReflection();
        void StartSystems();
        void ShutdownSystems();
        void LogicalTick(float deltaTime);
        void RendererTick(float deltaTime);

        static Engine* s_Instance;

        std::shared_ptr<ObjectManager> m_ObjectManager;
        std::shared_ptr<ProjectManager> m_ProjectManager;
        std::shared_ptr<AssetManager> m_AssetManager;
        std::shared_ptr<WindowSystem> m_WindowSystem;
        std::shared_ptr<InputSystem> m_InputSystem;
        std::shared_ptr<RenderSystem> m_RenderSystem;
        std::shared_ptr<SceneManager> m_SceneManager;

        std::string m_EngineDefaultAssetsRoot;
        std::chrono::steady_clock::time_point m_LastTickTimePoint{std::chrono::steady_clock::now()};
    };
}
