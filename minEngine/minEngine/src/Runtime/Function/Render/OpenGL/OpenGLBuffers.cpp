#include "OpenGLBuffers.h"
#include "glad/glad.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "OpenGLTexture.h"

namespace minEngine
{
    // OpenGLVertexBuffer
    OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size, uint32_t numVertices)
    {
        m_NumVertices = numVertices;
        glGenBuffers(1, &m_VBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer()
    {
        glDeleteBuffers(1, &m_VBO);
    }

    void OpenGLVertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    }

    void OpenGLVertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // OpenGLIndexBuffer
    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t numIndices)
    {
        m_NumIndices = numIndices;
        glGenBuffers(1, &m_EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_NumIndices * sizeof(uint32_t), indices, GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        glDeleteBuffers(1, &m_EBO);
    }

    void OpenGLIndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    }

    void OpenGLIndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // OpenGLFrameBuffer
    OpenGLFrameBuffer::OpenGLFrameBuffer(uint32_t width, uint32_t height)
    {
        glGenFramebuffers(1, &m_FBO);
    }

    OpenGLFrameBuffer::~OpenGLFrameBuffer()
    {
        glDeleteFramebuffers(1, &m_FBO);
    }

    void OpenGLFrameBuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    }

    void OpenGLFrameBuffer::Unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFrameBuffer::AttachColorBuffer(std::shared_ptr<RHITexture2D> texture)
    {
        FrameBuffer::AttachColorBuffer(texture);

        Bind();
        OpenGLTexture2D* glTexture = static_cast<OpenGLTexture2D*>(texture.get());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTexture->GetID(), 0);
        Unbind();
    }

    void OpenGLFrameBuffer::AttachDepthBuffer(std::shared_ptr<RHITexture2D> texture)
    {
        FrameBuffer::AttachDepthBuffer(texture);
        // Currently, we do not support depth attachment in OpenGLFrameBuffer.
    }

    void OpenGLFrameBuffer::AttachStencilBuffer(std::shared_ptr<RHITexture2D> texture)
    {
        FrameBuffer::AttachStencilBuffer(texture);
        // Not implemented yet
    }

    void OpenGLFrameBuffer::AttachDepthStencilBuffer(std::shared_ptr<RHITexture2D> texture)
    {
        FrameBuffer::AttachDepthStencilBuffer(texture);
        
        Bind();
        OpenGLTexture2D* glTexture = static_cast<OpenGLTexture2D*>(texture.get());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, glTexture->GetID(), 0);
        Unbind();
    }

    OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t bindingPoint)
    {
        glGenBuffers(1, &m_UBO);
        glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
        glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_UBO);
    }

    void OpenGLUniformBuffer::BindToBindingPoint(uint32_t bindingPoint) const
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_UBO);
    }

    void OpenGLUniformBuffer::BindToBindingPoint(uint32_t bindingPoint, uint32_t offset, uint32_t size) const
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, bindingPoint, m_UBO, offset, size);
    }

    void OpenGLUniformBuffer::UpdateData(const void *data, uint32_t offset, uint32_t size) const
    {
        glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    }
}