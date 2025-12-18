#pragma once
#include "Runtime/Function/Render/RHI/RHIBuffer.h"

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

        virtual void AttachTexture(RHITexture2D* texture) override;
        virtual std::shared_ptr<RHITexture2D> GetColorAttachment() const override { return m_ColorAttachment; }

        void AttachRenderBuffer(uint32_t RBO);

    private:
        std::shared_ptr<RHITexture2D> m_ColorAttachment;
        uint32_t m_FBO;
        uint32_t m_RBO;

    };
}