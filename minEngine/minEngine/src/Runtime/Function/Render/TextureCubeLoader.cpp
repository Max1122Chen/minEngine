#include "TextureCubeLoader.h"

#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "RenderSystem.h"
#include "RHI/RHI.h"
#include "Texture.h"

#include "Runtime/Resource/Loaders/ImageLoader.h"

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

        outFaceSet.OwnedFaceImages.resize(6);
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

            ImagePixels& faceImage = outFaceSet.OwnedFaceImages[faceIndex];
            if (!ImageLoader::LoadLdr(path, faceImage, false, outError))
            {
                FreeFaceSet(outFaceSet);
                return false;
            }

            const int width = faceImage.Width;
            const int height = faceImage.Height;
            const int channels = faceImage.Channels;

            if (faceIndex == 0)
            {
                outFaceSet.Channels = channels;
                outFaceSet.FaceSize = static_cast<uint32_t>(width);
            }
            else if (width != static_cast<int>(outFaceSet.FaceSize)
                || height != static_cast<int>(outFaceSet.FaceSize)
                || channels != outFaceSet.Channels)
            {
                if (outError)
                {
                    *outError = "Cubemap faces must share size and channel count: " + path;
                }
                FreeFaceSet(outFaceSet);
                return false;
            }

            outFaceSet.FacePixels[faceIndex] = faceImage.U8;
            outFaceSet.FaceWidths[faceIndex] = width;
            outFaceSet.FaceHeights[faceIndex] = height;
            outFaceSet.FaceChannels[faceIndex] = channels;
        }

        return true;
    }

    void TextureCubeLoader::FreeFaceSet(TextureCubeFaceSet& faceSet)
    {
        for (ImagePixels& faceImage : faceSet.OwnedFaceImages)
        {
            ImageLoader::Free(faceImage);
        }

        faceSet = {};
    }

    std::shared_ptr<TextureCube> TextureCubeLoader::CreateTextureCubeFromFaceSet(
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

        std::shared_ptr<TextureCube> texture = std::make_shared<TextureCube>();
        const void* faceDataPtr = faceSet.FacePixels.data();
        RHITextureCreateDesc cubeDesc = MakeTextureCubeBindingDesc(faceSet.FaceSize, format, 1);
        if (generateMipmaps)
        {
            cubeDesc.NumMips = ComputeTextureMipCount(faceSet.FaceSize);
            cubeDesc.Flags = cubeDesc.Flags | RHITextureCreateFlags::GenerateMips;
        }
        texture->m_RHITexture = rhi.RHICreateTexture2D(cubeDesc, faceDataPtr);
        if (!texture->m_RHITexture)
        {
            if (outError)
            {
                *outError = "RHI failed to create cubemap.";
            }
            return nullptr;
        }

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

        return CreateTextureCubeFromFaceSet(rhi, faceSet, TextureFormat::RGBA8, false, outError);
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

        std::shared_ptr<TextureCube> cube = CreateTextureCubeFromFaceSet(
            rhi,
            faceSet,
            TextureFormat::None,
            generateMipmaps,
            outError);
        FreeFaceSet(faceSet);
        return cube;
    }

    RHITextureRef TextureCubeLoader::CreateRenderTargetCube(
        RHI& rhi,
        uint32_t faceSize,
        TextureFormat format,
        uint32_t numMips)
    {
        if (faceSize == 0 || format == TextureFormat::None)
        {
            return nullptr;
        }

        return rhi.RHICreateTexture2D(
            MakeTextureCubeRenderTargetDesc(faceSize, format, numMips),
            nullptr);
    }

    std::shared_ptr<TextureCube> TextureCubeLoader::WrapTextureCube(
        RHITextureRef texture,
        uint32_t faceSize,
        uint32_t channels)
    {
        if (!texture || faceSize == 0)
        {
            return nullptr;
        }

        std::shared_ptr<TextureCube> cube = std::make_shared<TextureCube>();
        cube->m_RHITexture = std::move(texture);
        cube->m_Size = faceSize;
        cube->m_Channels = channels;
        cube->m_Wrapping = TextureWrapping::ClampToEdge;
        cube->m_Filtering = TextureFiltering::Linear;
        return cube;
    }
}
