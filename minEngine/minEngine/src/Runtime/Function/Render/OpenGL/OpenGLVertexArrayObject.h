#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    class VertexDefinition;
    struct VertexElement;

    class OpenGLVertexArrayObject final : public VertexDefinition
    {
    public:
        OpenGLVertexArrayObject(std::initializer_list<VertexElement> elements);
        virtual ~OpenGLVertexArrayObject() override;

        OpenGLVertexArrayObject(const OpenGLVertexArrayObject&) = delete;
        OpenGLVertexArrayObject& operator=(const OpenGLVertexArrayObject&) = delete;

        virtual void Bind() const override;
        virtual void Unbind() const override;

    private:
        uint32_t m_VAO;    
    };
}