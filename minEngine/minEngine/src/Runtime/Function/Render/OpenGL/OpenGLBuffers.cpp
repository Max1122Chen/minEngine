#include "OpenGLBuffers.h"
#include "glad/glad.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

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
    OpenGLFrameBuffer::OpenGLFrameBuffer(uint32_t width, uint32_t height, bool bHasDepth)
    {
        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // By default, we create a color attachment. Temporary implementation.
        unsigned int texColorBuffer;
        glGenTextures(1, &texColorBuffer);
        glBindTexture(GL_TEXTURE_2D, texColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);
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
        // Currently, we do not support multiple color attachments in OpenGLFrameBuffer.
    }

    void OpenGLFrameBuffer::AttachDepthBuffer(std::shared_ptr<RHITexture2D> texture)
    {
        FrameBuffer::AttachDepthBuffer(texture);
        // Currently, we do not support depth attachment in OpenGLFrameBuffer.
    }
}