#pragma once

#include "Core.h"

#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class RHITexture2D;
    class TextureCube;

    class EnvMapCapture
    {
    public:
        static constexpr uint32_t kDefaultCubeFaceSize = 512;

        static std::shared_ptr<TextureCube> EquirectToCubemap(
            RHI& rhi,
            RHITexture2D& equirectTexture,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultCubeFaceSize,
            std::string* outError = nullptr);
    };
}
