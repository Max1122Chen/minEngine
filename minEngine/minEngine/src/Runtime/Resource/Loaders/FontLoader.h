#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class Font;

    class MINENGINE_API FontLoader
    {
    public:
        static std::shared_ptr<Font> LoadFromAssetMeta(const AssetMeta& meta);

        /** Create an in-memory Font (tests / runtime helpers). Does not register as a disk asset. */
        static std::shared_ptr<Font> CreateFromBytes(
            std::vector<uint8_t> bytes,
            const std::string& extension = ".ttf",
            const std::string& name = "RuntimeFont");

    private:
        static bool ReadFontFileBytes(
            const std::filesystem::path& absolutePath,
            std::vector<uint8_t>& outBytes,
            std::string* outError = nullptr);

        static std::string NormalizeSourceExtension(const std::filesystem::path& path);
    };
}
