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
        virtual void AttachDepthBufferLayer(std::shared_ptr<RHITexture2DArray> texture, uint32_t layer) override;
        virtual void AttachDepthCubeFace(std::shared_ptr<RHITextureCube> texture, uint32_t face) override;
        virtual void AttachColorCubeFace(std::shared_ptr<RHITextureCube> texture, uint32_t face) override;
        virtual void AttachStencilBuffer(std::shared_ptr<RHITexture2D> texture) override;
        virtual void AttachDepthStencilBuffer(std::shared_ptr<RHITexture2D> texture) override;  

    private:
        uint32_t m_FBO;
    };

    class OpenGLUniformBuffer : public UniformBuffer
    {
    public:
        OpenGLUniformBuffer(uint32_t size, uint32_t bindingPoint = 0);
        virtual ~OpenGLUniformBuffer() override;

        OpenGLUniformBuffer(const OpenGLUniformBuffer&) = delete;
        OpenGLUniformBuffer& operator=(const OpenGLUniformBuffer&) = delete;

        // virtual void Bind() const override;
        // virtual void Unbind() const override;
        virtual void BindToBindingPoint(uint32_t bindingPoint) const override;
        virtual void BindToBindingPoint(uint32_t bindingPoint, uint32_t offset, uint32_t size) const override;

        virtual void UpdateData(const void* data, uint32_t offset, uint32_t size) const override;

    private:
        uint32_t m_UBO;
    };
}