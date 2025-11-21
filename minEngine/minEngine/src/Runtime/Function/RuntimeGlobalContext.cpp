#include "RuntimeGlobalContext.h"

#include "Render/RenderSystem.h"

namespace minEngine
{
    std::shared_ptr<RuntimeGlobalContext> RuntimeGlobalContext::s_Instance = std::make_shared<RuntimeGlobalContext>();

    void RuntimeGlobalContext::StartSystems()
    {
        // Initialize global systems
        m_LogSystem = std::make_shared<LogSystem>();
        m_LogSystem->Initialize();

        m_RenderSystem = std::make_shared<RenderSystem>();
        m_RenderSystem->Initialize();
    }

    void RuntimeGlobalContext::ShutdownSystems()
    {
        // Shutdown global systems
    }
}