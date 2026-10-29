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

    /**
     * What one capture produced, in the only distinction its caller can act on.
     *
     * A capture that fails because a program is still queued for the graphics role is a call to
     * repeat later; a capture the driver refused is not. Collapsing the two into "null" is what
     * made the caller retry a whole bake on every frame.
     */
    enum class EEnvMapCaptureStatus : uint8_t
    {
        Succeeded,
        /** A GPU object the capture needs is not created yet; the same call succeeds later. */
        RetryLater,
        /** Retrying produces the same failure; the caller's error string says why. */
        Failed,
    };

    class EnvMapCapture
    {
    public:
        static constexpr uint32_t kDefaultCubeFaceSize = 512;
        static constexpr uint32_t kDefaultIrradianceFaceSize = 32;
        /** Keep in sync with `kMaterialPBRMaxReflectionLod` in MaterialIBL.glslinc. */
        static constexpr uint32_t kMaterialPBRMaxReflectionLod = 7;

        static uint32_t PrefilterMipLevelCount() { return kMaterialPBRMaxReflectionLod + 1; }

        /**
         * Every capture below records one burst of commands, replays it before it returns, and
         * hands back the cubemap it wrote. It therefore needs the graphics role and an
         * uninterrupted command stream; see EnvironmentMap::GetEnvironmentBakeAdmission for the
         * check its caller runs first.
         */
        static std::shared_ptr<TextureCube> EquirectToCubemap(
            RHI& rhi,
            RHITexture& equirectTexture,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultCubeFaceSize,
            std::string* outError = nullptr,
            EEnvMapCaptureStatus* outStatus = nullptr);

        /** Diffuse IBL cubemap from an existing environment cubemap (LearnOpenGL irradiance pass). */
        static std::shared_ptr<TextureCube> ConvolveIrradiance(
            RHI& rhi,
            RHITexture& environmentCube,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultIrradianceFaceSize,
            std::string* outError = nullptr,
            EEnvMapCaptureStatus* outStatus = nullptr);

        /** Specular IBL cubemap with mips 0..kMaterialPBRMaxReflectionLod (importance sampling). */
        static std::shared_ptr<TextureCube> PrefilterEnvironment(
            RHI& rhi,
            RHITexture& environmentCube,
            const std::filesystem::path& engineDefaultAssetsRoot,
            uint32_t faceSize = kDefaultCubeFaceSize,
            std::string* outError = nullptr,
            EEnvMapCaptureStatus* outStatus = nullptr);
    };
}
