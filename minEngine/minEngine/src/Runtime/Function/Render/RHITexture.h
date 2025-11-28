#pragma once
#include "Core.h"

namespace minEngine
{
    enum class TextureType
    {
        None = 0,
    };

    enum class TextureWrapping
    {
        None = 0,
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    enum class TextureFiltering
    {
        None = 0,
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    class RHITexture
    {
    public:
        RHITexture() = default;
        virtual ~RHITexture() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;

    protected:
        uint32_t m_Unit;
        TextureType m_Type;
        TextureWrapping m_Wrapping;
        TextureFiltering m_Filtering;
    };
}