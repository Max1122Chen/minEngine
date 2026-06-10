#pragma once
#include "Core.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

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

       ME_ASSERT(false, "Unknown VertexElementType!"); 
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

        ME_ASSERT(false, "Unknown VertexElementType!");
        return 0;
    }

    struct RHIVertexElement
    {
        std::string Name;
        VertexElementType Type = VertexElementType::None;
        uint32_t Size = 0;
        bool bNormalized = false;
        uint32_t Offset = 0;

        RHIVertexElement(const std::string& name, VertexElementType type, bool normalized = false)
            : Name(name)
            , Type(type)
            , Size(VertexElementSize(type))
            , bNormalized(normalized)
            , Offset(0)
        {
        }
    };

    enum class RHIBufferUsage : uint8_t
    {
        Vertex,
        Index,
        Uniform,
        Staging,
    };

    struct RHIBufferCreateDesc
    {
        RHIBufferUsage Usage = RHIBufferUsage::Vertex;
        uint32_t ByteSize = 0;
        uint32_t Stride = 0;
        uint32_t ElementCount = 0;
    };

    class RHIBuffer
    {
    public:
        virtual ~RHIBuffer() = default;

        virtual const RHIBufferCreateDesc& GetDesc() const = 0;
        virtual void UpdateSubresource(const void* data, uint32_t offset, uint32_t size) = 0;
    };

    using RHIBufferRef = std::shared_ptr<RHIBuffer>;

    class RHIVertexInputLayout
    {
    public:
        virtual ~RHIVertexInputLayout() = default;

        virtual const std::vector<RHIVertexElement>& GetElements() const = 0;
        virtual uint32_t GetStride() const = 0;
    };

    using RHIVertexInputLayoutRef = std::shared_ptr<RHIVertexInputLayout>;
}
