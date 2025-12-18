#include "OpenGLBuffer.h"
#include "glad/glad.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    //----------------------------------------
    // OpenGLVertexBuffer
    //----------------------------------------

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

    //----------------------------------------
    // OpenGLIndexBuffer
    //----------------------------------------

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

    OpenGLFrameBuffer::OpenGLFrameBuffer(uint32_t width, uint32_t height)
    {
        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
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

    void OpenGLFrameBuffer::AttachTexture(RHITexture2D *texture)
    {
        m_ColorAttachment = std::shared_ptr<RHITexture2D>(texture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->GetID(), 0);
    }

    void OpenGLFrameBuffer::AttachRenderBuffer(uint32_t RBO)
    {
        m_RBO = RBO;
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
    }

}