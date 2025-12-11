#include "OpenGLRHI.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/RuntimeGlobalContext.h"

#include "OpenGLBuffer.h"
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

    void OpenGLRHI::EnableDepthTest()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRHI::DisableDepthTest()
    {
        glDisable(GL_DEPTH_TEST);
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

    std::shared_ptr<RHITexture> OpenGLRHI::CreateTexture(const char *filepath, uint32_t unit)
    {
        return std::make_shared<OpenGLTexture>(filepath, unit);
    }

    std::shared_ptr<RHIShader> OpenGLRHI::CreateShader(const char *vertexSource, const char *fragmentSource)
    {
        return std::make_shared<OpenGLShader>(vertexSource, fragmentSource);
    }
}
