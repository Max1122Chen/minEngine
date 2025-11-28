#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHIBuffer.h"
#include <glad/glad.h>

namespace minEngine
{
    class OpenGLVertexArrayObject final : public VertexDefinition
    {
    public:
        OpenGLVertexArrayObject(std::initializer_list<VertexElement> elements);
        ~OpenGLVertexArrayObject() = default;

        void Bind() const;
        void Unbind() const;

    private:
        uint32_t m_VAO;    
    };
}