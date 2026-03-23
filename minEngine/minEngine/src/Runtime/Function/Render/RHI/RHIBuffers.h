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

       MINENGINE_ASSERT(false, "Unknown VertexElementType!"); 
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

        MINENGINE_ASSERT(false, "Unknown VertexElementType!");
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


        static std::shared_ptr<VertexDefinition> Create(std::initializer_list<VertexElement> elements);


        inline const std::vector<VertexElement>& GetElements() const { return m_Elements; }
        inline const uint32_t GetStride() const { return m_Stride; }

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

        static std::shared_ptr<VertexBuffer> Create(float* vertices, uint32_t size, uint32_t numVertices);

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
        uint32_t GetNumVertices() const { return m_NumVertices; }

    protected:
        uint32_t m_NumVertices = 0;
    };

    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        static std::shared_ptr<IndexBuffer> Create(uint32_t* indices, uint32_t numIndices);

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
        uint32_t GetNumIndices() const { return m_NumIndices; }

    protected:
        uint32_t m_NumIndices = 0;
    };

    class RHITexture2D;

    enum class FrameBufferAttachmentType
    {
        Color = 0,
        Depth,
        Stencil,
        DepthStencil
    };

    struct FrameBufferAttachment
    {
        FrameBufferAttachmentType Type;
        RHITexture2D* Texture;
    };

    class FrameBuffer
    {
    public:
        virtual ~FrameBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        const std::vector<std::shared_ptr<RHITexture2D>>& GetColorBuffers() const { return m_ColorBuffers; }
        virtual void AttachColorBuffer(std::shared_ptr<RHITexture2D> texture)
        {
            m_ColorBuffers.push_back(texture);
        }
        
        const std::shared_ptr<RHITexture2D>& GetDepthBuffer() const { return m_DepthBuffer; }
        virtual void AttachDepthBuffer(std::shared_ptr<RHITexture2D> texture)
        {
            m_DepthBuffer = texture;
        }

    private:
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        std::vector<std::shared_ptr<RHITexture2D>> m_ColorBuffers;
        std::shared_ptr<RHITexture2D> m_DepthBuffer;
        std::shared_ptr<RHITexture2D> m_StencilBuffer;
        std::shared_ptr<RHITexture2D> m_DepthStencilBuffer;
    };
}