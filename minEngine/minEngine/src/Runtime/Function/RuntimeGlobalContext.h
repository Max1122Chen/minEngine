#pragma once
#include "Core.h"

namespace minEngine
{
    class LogSystem;
    class AssetManager;
    class WindowSystem;
    class InputSystem;
    class RenderSystem;
    class WorldManager;


    class RuntimeGlobalContext
    {
    public:
        RuntimeGlobalContext() = default;
        ~RuntimeGlobalContext() = default;

        static RuntimeGlobalContext& GetRuntimeGlobalContext() { return *s_Instance; }


        void StartSystems();
        void ShutdownSystems();

    public:
    
    // Add global systems and contexts here
    std::shared_ptr<LogSystem> m_LogSystem;
    std::shared_ptr<AssetManager> m_AssetManager;
    std::shared_ptr<WindowSystem> m_WindowSystem;
    std::shared_ptr<InputSystem> m_InputSystem;
    std::shared_ptr<RenderSystem> m_RenderSystem;
    std::shared_ptr<WorldManager> m_WorldManager;


    private:
        // Singleton instance
        static std::shared_ptr<RuntimeGlobalContext> s_Instance;


    
    };
}