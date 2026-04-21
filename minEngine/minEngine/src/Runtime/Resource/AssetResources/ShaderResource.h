#pragma once
#include "Core.h"

namespace minEngine
{
    ME_CLASS()
    class ShaderResource
    {
        ME_REFLECTION_FRIEND(ShaderResource)
    public:
        ShaderResource() = default;
        ~ShaderResource() = default;

        ME_PROPERTY()
        std::string m_VertexPath;

        ME_PROPERTY()
        std::string m_FragmentPath;
    private:
       
    };
}

#include "ShaderResource.gen.h"