#include "ImageLoader.h"

#include "Runtime/Core/Log/LogSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace minEngine
{
    void ImagePixels::Reset()
    {
        if (U8 != nullptr)
        {
            stbi_image_free(U8);
        }
        else if (F32 != nullptr)
        {
            stbi_image_free(F32);
        }

        Storage = ImageStorage::UInt8;
        U8 = nullptr;
        F32 = nullptr;
        Width = 0;
        Height = 0;
        Channels = 0;
    }

    bool ImageLoader::IsHdrPath(const std::string& path)
    {
        const std::filesystem::path filePath(path);
        std::string extension = filePath.extension().string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (extension == ".hdr")
        {
            return true;
        }

        return stbi_is_hdr(path.c_str()) != 0;
    }

    bool ImageLoader::LoadLdr(
        const std::string& path,
        ImagePixels& outPixels,
        bool flipVertical,
        std::string* outError)
    {
        Free(outPixels);

        stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!data)
        {
            const char* reason = stbi_failure_reason();
            if (outError)
            {
                *outError = std::string("Failed to load LDR image: ") + path
                    + (reason ? (std::string(" (") + reason + ")") : std::string());
            }
            ME_CORE_ERROR("ImageLoader: failed to load LDR image {}. {}", path, reason ? reason : "unknown");
            return false;
        }

        outPixels.Storage = ImageStorage::UInt8;
        outPixels.U8 = data;
        outPixels.Width = width;
        outPixels.Height = height;
        outPixels.Channels = channels;
        return true;
    }

    bool ImageLoader::LoadHdr(
        const std::string& path,
        ImagePixels& outPixels,
        bool flipVertical,
        std::string* outError)
    {
        Free(outPixels);

        stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);
        int width = 0;
        int height = 0;
        int channels = 0;
        float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
        if (!data)
        {
            const char* reason = stbi_failure_reason();
            if (outError)
            {
                *outError = std::string("Failed to load HDR image: ") + path
                    + (reason ? (std::string(" (") + reason + ")") : std::string());
            }
            ME_CORE_ERROR("ImageLoader: failed to load HDR image {}. {}", path, reason ? reason : "unknown");
            return false;
        }

        outPixels.Storage = ImageStorage::Float32;
        outPixels.F32 = data;
        outPixels.Width = width;
        outPixels.Height = height;
        outPixels.Channels = channels;
        return true;
    }

    bool ImageLoader::Load(
        const std::string& path,
        ImagePixels& outPixels,
        bool flipVertical,
        std::string* outError)
    {
        if (IsHdrPath(path))
        {
            return LoadHdr(path, outPixels, flipVertical, outError);
        }

        return LoadLdr(path, outPixels, flipVertical, outError);
    }

    void ImageLoader::Free(ImagePixels& pixels)
    {
        pixels.Reset();
    }
}
