#pragma once
#include "RHI.h"
#include "Core.h"


namespace minEngine
{
    class WindowSystem;

    class OpenGLRHI : public RHI
    {
    public:
        OpenGLRHI() = default;
        virtual ~OpenGLRHI() = default;

        virtual void Initialize() override;
        virtual void Shutdown() override;
        
    private:
        // OpenGL specific resources can be declared here
        std::shared_ptr<WindowSystem> m_WindowSystem;
    };
}