#include "OpenGLVertexArrayObject.h"


namespace minEngine
{
    static GLenum VertexElementTypeToOpenGLType(VertexElementType type)
    {
        switch (type)
        {
        case VertexElementType::Float:   return GL_FLOAT;
        case VertexElementType::Float2:  return GL_FLOAT;
        case VertexElementType::Float3:  return GL_FLOAT;
        case VertexElementType::Float4:  return GL_FLOAT;
        case VertexElementType::Mat3:    return GL_FLOAT;
        case VertexElementType::Mat4:    return GL_FLOAT;
        case VertexElementType::Int:     return GL_INT;
        case VertexElementType::Int2:    return GL_INT;
        case VertexElementType::Int3:    return GL_INT;
        case VertexElementType::Int4:    return GL_INT;
        case VertexElementType::Bool:    return GL_BOOL;
        }

        // Should not reach here, and maybe we will write a assert here later
        __debugbreak();
        return 0;
    }

    OpenGLVertexArrayObject::OpenGLVertexArrayObject(std::initializer_list<VertexElement> elements)
        : VertexDefinition(elements)
    {
        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        uint32_t index = 0;
        for(const auto& element : m_Elements)
        {
            glVertexAttribPointer(
                index,
                element.Size,
                VertexElementTypeToOpenGLType(element.Type),
                element.bNormalized ? GL_TRUE : GL_FALSE,
                m_Stride,
                (const void*)(uintptr_t)element.Offset
            );
            glEnableVertexAttribArray(index);
            index++;
        }
    }
    void OpenGLVertexArrayObject::Bind() const
    {
        glBindVertexArray(m_VAO);
    }

    void OpenGLVertexArrayObject::Unbind() const
    {
        glBindVertexArray(0);
    }
}