#include "TextureCubeLoader.h"

#include "OpenGL/OpenGLTexture.h"
#include "RenderSystem.h"
#include "RHI/RHI.h"
#include "Texture.h"

#include <stb_image.h>

#include <array>
#include <filesystem>

namespace minEngine
{
    namespace
    {
        TextureFormat TextureFormatFromChannels(int channels)
        {
            switch (channels)
            {
            case 1:
                return TextureFormat::RED;
            case 3:
                return TextureFormat::RGB8;
            case 4:
                return TextureFormat::RGBA8;
            default:
                return TextureFormat::None;
            }
        }
    }

    bool TextureCubeLoader::LoadFaceSetFromFiles(
        const std::string& directory,
        const std::string& namePrefix,
        TextureCubeFaceSet& outFaceSet,
        std::string* outError)
    {
        FreeFaceSet(outFaceSet);

        outFaceSet.FacePixels.resize(6, nullptr);
        outFaceSet.FaceWidths.resize(6, 0);
        outFaceSet.FaceHeights.resize(6, 0);
        outFaceSet.FaceChannels.resize(6, 0);

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            const std::string path =
                directory + "/" + namePrefix + "_" + kFaceSuffixes[faceIndex] + ".png";
            if (!std::filesystem::exists(path))
            {
                if (outError)
                {
                    *outError = "Cubemap face not found: " + path;
                }
                FreeFaceSet(outFaceSet);
                return false;
            }

            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_set_flip_vertically_on_load(false);
            unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
            if (!pixels)
            {
                if (outError)
                {
                    *outError = "Failed to decode cubemap face: " + path;
                }
                FreeFaceSet(outFaceSet);
                return false;
            }

            if (faceIndex == 0)
            {
                outFaceSet.Channels = channels;
                outFaceSet.FaceSize = static_cast<uint32_t>(width);
            }
            else if (width != static_cast<int>(outFaceSet.FaceSize)
                || height != static_cast<int>(outFaceSet.FaceSize)
                || channels != outFaceSet.Channels)
            {
                stbi_image_free(pixels);
                if (outError)
                {
                    *outError = "Cubemap faces must share size and channel count: " + path;
                }
                FreeFaceSet(outFaceSet);
                return false;
            }

            outFaceSet.FacePixels[faceIndex] = pixels;
            outFaceSet.FaceWidths[faceIndex] = width;
            outFaceSet.FaceHeights[faceIndex] = height;
            outFaceSet.FaceChannels[faceIndex] = channels;
        }

        return true;
    }

    void TextureCubeLoader::FreeFaceSet(TextureCubeFaceSet& faceSet)
    {
        for (unsigned char* pixels : faceSet.FacePixels)
        {
            if (pixels != nullptr)
            {
                stbi_image_free(pixels);
            }
        }

        faceSet = {};
    }

    std::shared_ptr<TextureCube> TextureCubeLoader::CreateRHITextureCubeFromFaceSet(
        RHI& rhi,
        const TextureCubeFaceSet& faceSet,
        TextureFormat format,
        bool generateMipmaps,
        std::string* outError)
    {
        if (faceSet.FacePixels.size() != 6 || faceSet.FaceSize == 0)
        {
            if (outError)
            {
                *outError = "Invalid TextureCubeFaceSet (expected 6 faces).";
            }
            return nullptr;
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            if (faceSet.FacePixels[faceIndex] == nullptr)
            {
                if (outError)
                {
                    *outError = "Cubemap face " + std::to_string(faceIndex) + " is null.";
                }
                return nullptr;
            }
        }

        if (format == TextureFormat::None)
        {
            format = TextureFormatFromChannels(faceSet.Channels);
        }

        if (format == TextureFormat::None)
        {
            if (outError)
            {
                *outError = "Unsupported cubemap channel count.";
            }
            return nullptr;
        }

        std::vector<unsigned char*> facePointers = faceSet.FacePixels;
        std::shared_ptr<RHITextureCube> rhiTexture = rhi.CreateRHITextureCube(
            facePointers,
            RHITextureDesc{
                .Width = faceSet.FaceSize,
                .Height = faceSet.FaceSize,
                .Format = format,
                .Usage = TextureUsage::TextureBinding,
            },
            generateMipmaps);

        if (!rhiTexture)
        {
            if (outError)
            {
                *outError = "RHI failed to create cubemap.";
            }
            return nullptr;
        }

        if (!rhiTexture)
        {
            return nullptr;
        }

        std::shared_ptr<TextureCube> texture = std::make_shared<TextureCube>();
        texture->m_RHITexture = std::move(rhiTexture);
        texture->m_Size = faceSet.FaceSize;
        texture->m_Channels = faceSet.Channels;
        texture->m_Wrapping = TextureWrapping::ClampToEdge;
        texture->m_Filtering = TextureFiltering::Linear;
        return texture;
    }

    std::shared_ptr<TextureCube> TextureCubeLoader::CreateSolidColorCube(
        RHI& rhi,
        uint32_t faceSize,
        const std::array<uint8_t, 4> faceColors[6],
        std::string* outError)
    {
        if (faceSize == 0)
        {
            if (outError)
            {
                *outError = "Cubemap faceSize must be > 0.";
            }
            return nullptr;
        }

        TextureCubeFaceSet faceSet;
        faceSet.FaceSize = faceSize;
        faceSet.Channels = 4;
        faceSet.FacePixels.resize(6);
        faceSet.FaceWidths.assign(6, static_cast<int>(faceSize));
        faceSet.FaceHeights.assign(6, static_cast<int>(faceSize));
        faceSet.FaceChannels.assign(6, 4);

        std::vector<std::vector<unsigned char>> ownedFaces(6);
        const size_t byteCount = static_cast<size_t>(faceSize) * faceSize * 4;
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            ownedFaces[faceIndex].resize(byteCount);
            for (size_t pixelIndex = 0; pixelIndex < byteCount; pixelIndex += 4)
            {
                ownedFaces[faceIndex][pixelIndex + 0] = faceColors[faceIndex][0];
                ownedFaces[faceIndex][pixelIndex + 1] = faceColors[faceIndex][1];
                ownedFaces[faceIndex][pixelIndex + 2] = faceColors[faceIndex][2];
                ownedFaces[faceIndex][pixelIndex + 3] = faceColors[faceIndex][3];
            }

            faceSet.FacePixels[faceIndex] = ownedFaces[faceIndex].data();
        }

        return CreateRHITextureCubeFromFaceSet(rhi, faceSet, TextureFormat::RGBA8, false, outError);
    }

    std::shared_ptr<TextureCube> TextureCubeLoader::LoadCubeMapFromDirectory(
        RHI& rhi,
        const std::string& directory,
        const std::string& namePrefix,
        bool generateMipmaps,
        std::string* outError)
    {
        TextureCubeFaceSet faceSet;
        if (!LoadFaceSetFromFiles(directory, namePrefix, faceSet, outError))
        {
            return nullptr;
        }

        std::shared_ptr<TextureCube> cube = CreateRHITextureCubeFromFaceSet(
            rhi,
            faceSet,
            TextureFormat::None,
            generateMipmaps,
            outError);
        FreeFaceSet(faceSet);
        return cube;
    }
}
