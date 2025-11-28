#include "RuntimeGlobalContext.h"

#include "Log/LogSystem.h"
#include "Render/GLFWWindowSystem.h"
#include "Input/InputSystem.h"
#include "Render/RenderSystem.h"

namespace minEngine
{
    std::shared_ptr<RuntimeGlobalContext> RuntimeGlobalContext::s_Instance = std::make_shared<RuntimeGlobalContext>();

    void RuntimeGlobalContext::StartSystems()
    {
        // Initialize global systems
        m_LogSystem = std::make_shared<LogSystem>();
        m_LogSystem->Initialize();

        m_WindowSystem = std::make_shared<GLFWWindowSystem>(1600, 900);
        m_WindowSystem->Initialize();

        m_InputSystem = std::make_shared<InputSystem>();
        m_InputSystem->Initialize();

        m_RenderSystem = std::make_shared<RenderSystem>();
        m_RenderSystem->Initialize();
    }

    void RuntimeGlobalContext::ShutdownSystems()
    {
        // Shutdown global systems
        m_RenderSystem->Shutdown();
        m_LogSystem->Shutdown();
    }
}