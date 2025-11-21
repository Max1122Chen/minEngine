#pragma once
#include <cstdint>
#include <vector>
#include <string>

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

    struct VertexElement
    { 
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
            default:                      return 0;
            }

            // Should not reach here, and maybe we will write a assert here later
            __debugbreak();
            return 0;
        }

        std::string Name;
        VertexElementType Type;
        uint32_t Size;
        uint32_t Offset;

        VertexElement(const std::string& name, VertexElementType type)
            : Name(name), Type(type), Size(VertexElementTypeSize(type)), Offset(0)
        {}
    };

    class BufferLayout
    {
    public:
        BufferLayout(std::initializer_list<VertexElement> elements)
            : m_Elements(elements)
        {
            // Calculate offsets and stride
            uint32_t offset = 0;
            for (auto& element : m_Elements)
            {
                element.Offset = offset;
                offset += element.Size;
            }
            m_Stride = offset;
        }

        BufferLayout() {}

        inline const std::vector<VertexElement>& GetAttributes() const { return m_Elements; }

        std::vector<VertexElement>::iterator begin() { return m_Elements.begin(); }
        std::vector<VertexElement>::iterator end() { return m_Elements.end(); }
        
    private:
        std::vector<VertexElement> m_Elements;
        uint32_t m_Stride = 0;
    };
    

    class VertexBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual const BufferLayout& GetLayout() const = 0;
        virtual void SetLayout(const BufferLayout& layout) = 0;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        static VertexBuffer* Create(float* vertices, uint32_t size);
    };

    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        static IndexBuffer* Create(uint32_t* indices, uint32_t count);
    };
}