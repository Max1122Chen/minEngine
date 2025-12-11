#include "RuntimeGlobalContext.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Framework/World/WorldManager.h"

namespace minEngine
{
    std::shared_ptr<RuntimeGlobalContext> RuntimeGlobalContext::s_Instance = std::make_shared<RuntimeGlobalContext>();

    void RuntimeGlobalContext::StartSystems()
    {
        // Initialize global systems
        m_LogSystem = std::make_shared<LogSystem>();
        m_LogSystem->Initialize();

        m_AssetManager = std::make_shared<AssetManager>();
        m_AssetManager->Initialize();

        m_WindowSystem = std::make_shared<GLFWWindowSystem>(1600, 900);
        m_WindowSystem->Initialize();

        m_InputSystem = std::make_shared<InputSystem>();
        m_InputSystem->Initialize();

        m_RenderSystem = std::make_shared<RenderSystem>();
        m_RenderSystem->Initialize();

        m_WorldManager = std::make_shared<WorldManager>();
        m_WorldManager->Initialize();
    }

    void RuntimeGlobalContext::ShutdownSystems()
    {

    }
}