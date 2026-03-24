#pragma once
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{

    // Vertex Buffer for OpenGL
    class OpenGLVertexBuffer : public VertexBuffer
    {
    public:
        OpenGLVertexBuffer(float *vertices, uint32_t size, uint32_t numVertices);
        virtual ~OpenGLVertexBuffer();

        virtual void Bind() const override;
        virtual void Unbind() const override;

    private:
        uint32_t m_VBO;
    };

    // Index Buffer for OpenGL
    class OpenGLIndexBuffer : public IndexBuffer
    {
    public:
        OpenGLIndexBuffer(uint32_t *indices, uint32_t numIndices);
        virtual ~OpenGLIndexBuffer();

        virtual void Bind() const override;
        virtual void Unbind() const override;

    private:
        uint32_t m_EBO;
    };

    class OpenGLFrameBuffer : public FrameBuffer
    {
    public:
        OpenGLFrameBuffer(uint32_t width, uint32_t height);
        virtual ~OpenGLFrameBuffer();

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual void AttachColorBuffer(std::shared_ptr<RHITexture2D> texture) override;
        virtual void AttachDepthBuffer(std::shared_ptr<RHITexture2D> texture) override;
        virtual void AttachStencilBuffer(std::shared_ptr<RHITexture2D> texture) override;
        virtual void AttachDepthStencilBuffer(std::shared_ptr<RHITexture2D> texture) override;  

    private:
        uint32_t m_FBO;
    };
}