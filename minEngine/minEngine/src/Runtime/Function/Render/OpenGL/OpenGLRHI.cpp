#include "OpenGLRHI.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/RuntimeGlobalContext.h"

#include "OpenGLBuffers.h"
#include "OpenGLVertexArrayObject.h"
#include "OpenGLTexture.h"
#include "OpenGLShader.h"

namespace minEngine
{
    void OpenGLRHI::Initialize()
    {
        // Initialize OpenGL specific resources here
        m_WindowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem;
        
        ME_CORE_INFO("OpenGLRHI Initialized"); 

    }

    void OpenGLRHI::Shutdown()
    {
        // Clean up OpenGL specific resources here
    }

    void OpenGLRHI::SetClearColor(Vector4 clearColor)
    {
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    }

    void OpenGLRHI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    void OpenGLRHI::EnableDepthTest()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRHI::DisableDepthTest()
    {
        glDisable(GL_DEPTH_TEST);
    }

    void OpenGLRHI::SetDepthMask(bool bEnable)
    {
        glDepthMask(bEnable ? GL_TRUE : GL_FALSE);
    }

    void OpenGLRHI::EnableStencilTest()
    {
        glEnable(GL_STENCIL_TEST);
    }

    void OpenGLRHI::DisableStencilTest()
    {
        glDisable(GL_STENCIL_TEST);
    }

    void OpenGLRHI::SetStencilMask(uint32_t mask)
    {
        glStencilMask(mask);
    }

    void OpenGLRHI::EnableBlend()
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Set default blend function for alpha blending
    }

    void OpenGLRHI::DisableBlend()
    {
        glDisable(GL_BLEND);

    }

    void OpenGLRHI::EnableCullFace()
    {
        glEnable(GL_CULL_FACE);
    }

    void OpenGLRHI::DisableCullFace()
    {
        glDisable(GL_CULL_FACE);
    }

    std::shared_ptr<VertexBuffer> OpenGLRHI::CreateVertexBuffer(float *vertices, uint32_t size, uint32_t numVertices)
    {
        return std::make_shared<OpenGLVertexBuffer>(vertices, size, numVertices);
    }

    std::shared_ptr<IndexBuffer> OpenGLRHI::CreateIndexBuffer(uint32_t *indices, uint32_t numIndices)
    {
        return std::make_shared<OpenGLIndexBuffer>(indices, numIndices);
    }

    std::shared_ptr<VertexDefinition> OpenGLRHI::CreateVertexDefinition(std::initializer_list<VertexElement> elements)
    {
        return std::make_shared<OpenGLVertexArrayObject>(elements);
    }

    std::shared_ptr<FrameBuffer> OpenGLRHI::CreateFrameBuffer(uint32_t width, uint32_t height)
    {
        return std::make_shared<OpenGLFrameBuffer>(width, height);
    }

    std::shared_ptr<UniformBuffer> OpenGLRHI::CreateUniformBuffer(uint32_t size, uint32_t bindingPoint)
    {
        return std::make_shared<OpenGLUniformBuffer>(size, bindingPoint);
    }

    std::shared_ptr<RHITexture2D> OpenGLRHI::CreateRHITexture2D(const unsigned char *data, RHITextureDesc desc, int unit)
    {
        return std::make_shared<OpenGLTexture2D>(data, desc, unit);
    }

    std::shared_ptr<RHIShader> OpenGLRHI::CreateShader(const char *vertexSource, const char *fragmentSource)
    {
        return std::make_shared<OpenGLShader>(vertexSource, fragmentSource);
    }

}
