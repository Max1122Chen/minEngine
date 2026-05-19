#pragma once
#include "Core.h"

#include <string>

namespace minEngine
{
    class LogSystem;
    class ObjectManager;
    class ProjectManager;
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

        static RuntimeGlobalContext& Get();


        void StartSystems();
        void ShutdownSystems();

        // Engine-bundled assets root (from EngineConfig.EngineDefaultAssetsRoot). Used e.g. for material shader templates.
        void SetEngineDefaultAssetsRoot(std::string path);
        const std::string& GetEngineDefaultAssetsRoot() const { return m_EngineDefaultAssetsRoot; }

        // Add global systems and contexts here
    std::shared_ptr<ObjectManager> m_ObjectManager;
    std::shared_ptr<ProjectManager> m_ProjectManager;
    std::shared_ptr<AssetManager> m_AssetManager;
    std::shared_ptr<WindowSystem> m_WindowSystem;
    std::shared_ptr<InputSystem> m_InputSystem;
    std::shared_ptr<RenderSystem> m_RenderSystem;
    std::shared_ptr<SceneManager> m_SceneManager;

    private:
        std::string m_EngineDefaultAssetsRoot;
    };
}