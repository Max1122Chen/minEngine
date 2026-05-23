#pragma once

#include "Core.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    enum class ImageStorage : uint8_t
    {
        UInt8 = 0,
        Float32,
    };

    struct ImagePixels
    {
        ImageStorage Storage = ImageStorage::UInt8;
        unsigned char* U8 = nullptr;
        float* F32 = nullptr;
        int Width = 0;
        int Height = 0;
        int Channels = 0;

        bool IsValid() const
        {
            return Width > 0 && Height > 0 && Channels > 0
                && ((Storage == ImageStorage::UInt8 && U8 != nullptr)
                    || (Storage == ImageStorage::Float32 && F32 != nullptr));
        }

        void Reset();
    };

    class ImageLoader
    {
    public:
        static bool LoadLdr(
            const std::string& path,
            ImagePixels& outPixels,
            bool flipVertical = true,
            std::string* outError = nullptr);

        static bool LoadHdr(
            const std::string& path,
            ImagePixels& outPixels,
            bool flipVertical = false,
            std::string* outError = nullptr);

        static bool Load(
            const std::string& path,
            ImagePixels& outPixels,
            bool flipVertical = true,
            std::string* outError = nullptr);

        static void Free(ImagePixels& pixels);
        static bool IsHdrPath(const std::string& path);
    };
}
