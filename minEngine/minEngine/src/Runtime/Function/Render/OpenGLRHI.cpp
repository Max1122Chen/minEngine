#include "OpenGLRHI.h"

#include "GLFWWindowSystem.h"

namespace minEngine
{
    void OpenGLRHI::Initialize()
    {
        // Initialize OpenGL specific resources here
        m_WindowSystem = std::make_shared<GLFWWindowSystem>(800, 600);
        m_WindowSystem->Initialize();
        
        ME_CORE_INFO("OpenGLRHI Initialized"); 

    }

    void OpenGLRHI::Shutdown()
    {
        // Clean up OpenGL specific resources here
    }
}


