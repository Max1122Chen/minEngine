#pragma once

#include "Core.h"
#include "Runtime/Function/UI/Font/FontGlyphAtlas.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace minEngine
{
    class Font;

    /** Process-wide cache of FontGlyphAtlas keyed by Font identity + pixel size. */
    class FontGlyphAtlasCache
    {
    public:
        static FontGlyphAtlasCache& Get();

        std::shared_ptr<FontGlyphAtlas> GetOrCreate(const Font& font, float pixelSize);
        void Clear();

    private:
        FontGlyphAtlasCache() = default;

        struct Key
        {
            const Font* FontPtr = nullptr;
            int PixelSizeKey = 0;

            bool operator==(const Key& other) const
            {
                return FontPtr == other.FontPtr && PixelSizeKey == other.PixelSizeKey;
            }
        };

        struct KeyHash
        {
            size_t operator()(const Key& key) const
            {
                return std::hash<const void*>{}(key.FontPtr) ^ (static_cast<size_t>(key.PixelSizeKey) << 1);
            }
        };

        std::unordered_map<Key, std::shared_ptr<FontGlyphAtlas>, KeyHash> m_Atlases;
    };
}
