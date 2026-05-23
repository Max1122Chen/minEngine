#pragma once

#include "Core.h"

#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class Texture2D;

    class BrdfLutGenerator
    {
    public:
        static constexpr uint32_t kDefaultLutSize = 256;

        static std::shared_ptr<Texture2D> CreateIntegratedBrdfLut(
            RHI& rhi,
            uint32_t size = kDefaultLutSize,
            std::string* outError = nullptr);
    };
}
