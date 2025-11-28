#pragma once
#include "Runtime/Function/Render/RHI.h"
#include "Core.h"

#include "OpenGLShader.h"
#include "OpenGLBuffer.h"


namespace minEngine
{
    class WindowSystem;

    class OpenGLRHI : public RHI
    {
    public:
        friend class RenderSystem;

        OpenGLRHI() = default;
        virtual ~OpenGLRHI() = default;

        virtual void Initialize() override;
        virtual void Shutdown() override;

        virtual void EnableDepthTest();
        virtual void DisableDepthTest();
       
    

    private:
        // OpenGL specific resources can be declared here
        std::shared_ptr<WindowSystem> m_WindowSystem;
    };
}