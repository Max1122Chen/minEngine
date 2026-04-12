#include "RuntimeGlobalContext.h"

#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{
    RuntimeGlobalContext& RuntimeGlobalContext::GetRuntimeGlobalContext()
    {
        static RuntimeGlobalContext instance;
        return instance;
    }

    void RuntimeGlobalContext::StartSystems()
    {
        // Initialize global systems

        m_AssetManager = std::make_shared<AssetManager>();
        m_AssetManager->Initialize();

        m_WindowSystem = std::make_shared<GLFWWindowSystem>(1600, 900);
        m_WindowSystem->Initialize();

        m_InputSystem = std::make_shared<InputSystem>();
        m_InputSystem->Initialize();

        m_RenderSystem = std::make_shared<RenderSystem>();
        m_RenderSystem->Initialize();

        m_SceneManager = std::make_shared<SceneManager>();
        m_SceneManager->Initialize();
    }

    void RuntimeGlobalContext::ShutdownSystems()
    {
        if (m_SceneManager)
        {
            m_SceneManager->Shutdown();
            m_SceneManager.reset();
        }

        if (m_InputSystem)
        {
            m_InputSystem->Shutdown();
            m_InputSystem.reset();
        }

        if (m_RenderSystem)
        {
            m_RenderSystem->Shutdown();
            m_RenderSystem.reset();
        }

        if (m_WindowSystem)
        {
            m_WindowSystem->Shutdown();
            m_WindowSystem.reset();
        }

        if (m_AssetManager)
        {
            m_AssetManager->Shutdown();
            m_AssetManager.reset();
        }

    }
}