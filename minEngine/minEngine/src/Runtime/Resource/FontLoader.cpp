#include "FontLoader.h"

#include "AssetManager.h"
#include "Font.h"
#include "Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace minEngine
{
    std::string FontLoader::NormalizeSourceExtension(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return extension;
    }

    bool FontLoader::ReadFontFileBytes(
        const std::filesystem::path& absolutePath,
        std::vector<uint8_t>& outBytes,
        std::string* outError)
    {
        std::ifstream inputFile(absolutePath, std::ios::binary | std::ios::ate);
        if (!inputFile.is_open())
        {
            const std::string message = "Failed to open font file: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        const std::streamsize fileSize = inputFile.tellg();
        if (fileSize <= 0)
        {
            const std::string message = "Font file is empty: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        outBytes.resize(static_cast<size_t>(fileSize));
        inputFile.seekg(0, std::ios::beg);
        if (!inputFile.read(reinterpret_cast<char*>(outBytes.data()), fileSize))
        {
            outBytes.clear();
            const std::string message = "Failed to read font file: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        return true;
    }

    std::shared_ptr<Font> FontLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        const std::filesystem::path absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath);
        std::vector<uint8_t> fontBytes;
        std::string error;
        if (!ReadFontFileBytes(absoluteAssetPath, fontBytes, &error))
        {
            ME_CORE_ERROR("FontLoader: failed to load '{}' ({}).", meta.AssetPath, error);
            return nullptr;
        }

        std::shared_ptr<Font> font = NewObject<Font>(meta.AssetName, nullptr, meta.Guid);
        font->m_FontFileBytes = std::move(fontBytes);
        font->m_SourceExtension = NormalizeSourceExtension(absoluteAssetPath);
        return font;
    }

    template<>
    std::shared_ptr<Font> AssetManager::LoadAsset_Impl<Font>(const AssetMeta& meta)
    {
        return FontLoader::LoadFromAssetMeta(meta);
    }
}
