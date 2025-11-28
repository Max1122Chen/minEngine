#pragma once
#include "Core.h"

namespace minEngine
{
    enum class VertexElementType
    {
        None = 0,
        Float, Float2, Float3, Float4,
        Mat3, Mat4,
        Int, Int2, Int3, Int4,
        Bool
    };

    static uint32_t VertexElementTypeSize(VertexElementType type)
    {
        switch (type)
        {
        case VertexElementType::Float:   return 4;
        case VertexElementType::Float2:  return 4 * 2;
        case VertexElementType::Float3:  return 4 * 3;
        case VertexElementType::Float4:  return 4 * 4;
        case VertexElementType::Mat3:    return 4 * 3 * 3;
        case VertexElementType::Mat4:    return 4 * 4 * 4;
        case VertexElementType::Int:     return 4;
        case VertexElementType::Int2:    return 4 * 2;
        case VertexElementType::Int3:    return 4 * 3;
        case VertexElementType::Int4:    return 4 * 4;
        case VertexElementType::Bool:    return 1;
        default:                         return 0;
        }

        // Should not reach here, and maybe we will write a assert here later
        __debugbreak();
        return 0;
    }

    static uint32_t VertexElementSize(VertexElementType type)
    {
        switch (type)
        {
        case VertexElementType::Float:   return 1;
        case VertexElementType::Float2:  return 2;
        case VertexElementType::Float3:  return 3;
        case VertexElementType::Float4:  return 4;
        case VertexElementType::Mat3:    return 3 * 3;
        case VertexElementType::Mat4:    return 4 * 4;
        case VertexElementType::Int:     return 1;
        case VertexElementType::Int2:    return 2;
        case VertexElementType::Int3:    return 3;
        case VertexElementType::Int4:    return 4;
        case VertexElementType::Bool:    return 1;
        default:                         return 0;
        }

        // Should not reach here, and maybe we will write a assert here later
        __debugbreak();
        return 0;
    }




    struct VertexElement
    { 
        std::string Name;
        VertexElementType Type;              // data type
        uint32_t Size;                       // number of components
        bool bNormalized = false;
        uint32_t Offset;

        VertexElement(const std::string& name, VertexElementType type, bool normalized = false)
            : Name(name), Type(type), Size(VertexElementSize(type)), bNormalized(normalized), Offset(0)
        {}
    };

    class VertexDefinition
    {
    public:
        VertexDefinition(std::initializer_list<VertexElement> elements)
            : m_Elements(elements)
        {
            // Calculate offsets and stride
            uint32_t offset = 0;
            for (auto& element : m_Elements)
            {
                element.Offset = offset;
                offset += VertexElementTypeSize(element.Type);
            }
            m_Stride = offset;
        }

        VertexDefinition() {}

        inline const std::vector<VertexElement>& GetElements() const { return m_Elements; }

        std::vector<VertexElement>::iterator begin() { return m_Elements.begin(); }
        std::vector<VertexElement>::iterator end() { return m_Elements.end(); }
        
    protected:
        std::vector<VertexElement> m_Elements;
        uint32_t m_Stride = 0;
    };
    

    class VertexBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

    };

    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
        virtual uint32_t GetCount() const = 0;
    };
}