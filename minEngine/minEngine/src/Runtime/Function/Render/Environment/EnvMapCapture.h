#pragma once

#include "Core.h"

#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class RHITexture;
    class TextureCube;

    class EnvMapCapture
    {
    public:
        static constexpr uint32_t kDefaultCubeFaceSize = 512;
        static constexpr uint32_t kDefaultIrradianceFaceSize = 32;
        /** Keep in sync with `kMaterialPBRMaxReflectionLod` in MaterialIBL.glslinc. */
        static constexpr uint32_t kMaterialPBRMaxReflectionLod = 7;

        static uint32_t PrefilterMipLevelCount() { return kMaterialPBRMaxReflectionLod + 1; }

        static std::shared_ptr<TextureCube> EquirectToCubemap(
            RHI& rhi,
            RHITexture& equirectTexture,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultCubeFaceSize,
            std::string* outError = nullptr);

        /** Diffuse IBL cubemap from an existing environment cubemap (LearnOpenGL irradiance pass). */
        static std::shared_ptr<TextureCube> ConvolveIrradiance(
            RHI& rhi,
            RHITexture& environmentCube,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultIrradianceFaceSize,
            std::string* outError = nullptr);

        /** Specular IBL cubemap with mips 0..kMaterialPBRMaxReflectionLod (importance sampling). */
        static std::shared_ptr<TextureCube> PrefilterEnvironment(
            RHI& rhi,
            RHITexture& environmentCube,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultCubeFaceSize,
            std::string* outError = nullptr);
    };
}
