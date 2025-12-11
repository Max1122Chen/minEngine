#pragma once
#include "Runtime/Function/Render/RHIBuffer.h"

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

}