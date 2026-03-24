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
        ~OpenGLVertexArrayObject() = default;

        virtual void Bind() const override;
        virtual void Unbind() const override;

    private:
        uint32_t m_VAO;    
    };
}