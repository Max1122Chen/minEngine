#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    class VertexDefinition;
    struct RHIVertexElement;

    class OpenGLVertexArrayObject final : public VertexDefinition
    {
    public:
        OpenGLVertexArrayObject(std::initializer_list<RHIVertexElement> elements);
        virtual ~OpenGLVertexArrayObject() override;

        OpenGLVertexArrayObject(const OpenGLVertexArrayObject&) = delete;
        OpenGLVertexArrayObject& operator=(const OpenGLVertexArrayObject&) = delete;

        virtual void Bind() const override;
        virtual void Unbind() const override;

        uint32_t GetVAO() const { return m_VAO; }

    private:
        uint32_t m_VAO = 0;
    };
}