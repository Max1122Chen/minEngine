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

    class FontLoader
    {
    public:
        static std::shared_ptr<Font> LoadFromAssetMeta(const AssetMeta& meta);

    private:
        static bool ReadFontFileBytes(
            const std::filesystem::path& absolutePath,
            std::vector<uint8_t>& outBytes,
            std::string* outError = nullptr);

        static std::string NormalizeSourceExtension(const std::filesystem::path& path);
    };
}
