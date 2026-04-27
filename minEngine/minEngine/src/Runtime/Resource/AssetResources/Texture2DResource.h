#pragma once
#include "Core.h"

namespace minEngine
{
    ME_CLASS()
    class Texture2DResource
    {
        ME_GENERATED_BODY(Texture2DResource)
    public:
        Texture2DResource() = default;
        ~Texture2DResource() = default;

        ME_PROPERTY()
        std::string assetPath;
    private:
       
    };
}

#include "Texture2DResource.gen.h"