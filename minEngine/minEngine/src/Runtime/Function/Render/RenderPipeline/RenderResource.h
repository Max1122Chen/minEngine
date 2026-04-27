#pragma once
#include "Core.h"

namespace minEngine
{
    class RenderResource
    {
    public:
        enum class Type
        {
            Texture,
            Buffer
        };
    private:
        Type m_Type;
        std::string m_Name;
    };

    class RenderTextureResource : public RenderResource
    {
    };

    class RenderBufferResource : public RenderResource
    {
    };
}