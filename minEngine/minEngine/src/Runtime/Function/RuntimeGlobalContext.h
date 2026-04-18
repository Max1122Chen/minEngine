#pragma once
#include "Core.h"

namespace minEngine
{
    class LogSystem;
    class ObjectManager;
    class AssetManager;
    class WindowSystem;
    class InputSystem;
    class RenderSystem;
    class SceneManager;


    class RuntimeGlobalContext
    {
    public:
        RuntimeGlobalContext() = default;
        ~RuntimeGlobalContext() = default;

        RuntimeGlobalContext(const RuntimeGlobalContext&) = delete;
        RuntimeGlobalContext& operator=(const RuntimeGlobalContext&) = delete;
        RuntimeGlobalContext(RuntimeGlobalContext&&) = delete;
        RuntimeGlobalContext& operator=(RuntimeGlobalContext&&) = delete;

        static RuntimeGlobalContext& GetRuntimeGlobalContext();


        void StartSystems();
        void ShutdownSystems();

    public:
    
    // Add global systems and contexts here
    std::shared_ptr<ObjectManager> m_ObjectManager;
    std::shared_ptr<AssetManager> m_AssetManager;
    std::shared_ptr<WindowSystem> m_WindowSystem;
    std::shared_ptr<InputSystem> m_InputSystem;
    std::shared_ptr<RenderSystem> m_RenderSystem;
    std::shared_ptr<SceneManager> m_SceneManager;


    };
}