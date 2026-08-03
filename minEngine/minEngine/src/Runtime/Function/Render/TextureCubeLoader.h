#pragma once

#include "Core.h"
#include "RHI/RHITexture.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class RHI;
    class TextureCube;

    struct TextureCubeFaceSet
    {
        std::vector<ImagePixels> OwnedFaceImages;
        std::vector<unsigned char*> FacePixels;
        std::vector<int> FaceWidths;
        std::vector<int> FaceHeights;
        std::vector<int> FaceChannels;
        int Channels = 0;
        uint32_t FaceSize = 0;
    };

    class TextureCubeLoader
    {
    public:
        /** OpenGL cubemap face order: +X, -X, +Y, -Y, +Z, -Z. */
        static constexpr const char* kFaceSuffixes[6] = {
            "posx", "negx", "posy", "negy", "posz", "negz",
        };

        static bool LoadFaceSetFromFiles(
            const std::string& directory,
            const std::string& namePrefix,
            TextureCubeFaceSet& outFaceSet,
            std::string* outError = nullptr);

        static void FreeFaceSet(TextureCubeFaceSet& faceSet);

        static std::shared_ptr<TextureCube> CreateTextureCubeFromFaceSet(
            RHI& rhi,
            const TextureCubeFaceSet& faceSet,
            TextureFormat format,
            bool generateMipmaps = false,
            std::string* outError = nullptr);

        static std::shared_ptr<TextureCube> CreateSolidColorCube(
            RHI& rhi,
            uint32_t faceSize,
            const std::array<uint8_t, 4> faceColors[6],
            std::string* outError = nullptr);

        static std::shared_ptr<TextureCube> LoadCubeMapFromDirectory(
            RHI& rhi,
            const std::string& directory,
            const std::string& namePrefix,
            bool generateMipmaps = false,
            std::string* outError = nullptr);

        static RHITextureRef CreateRenderTargetCube(
            RHI& rhi,
            uint32_t faceSize,
            TextureFormat format,
            uint32_t numMips = 1);

        static std::shared_ptr<TextureCube> WrapTextureCube(
            RHITextureRef texture,
            uint32_t faceSize,
            uint32_t channels);
    };
}
