#include "FontGlyphAtlasCache.h"

#include "Runtime/Resource/Font.h"

#include <cmath>

namespace minEngine
{
    FontGlyphAtlasCache& FontGlyphAtlasCache::Get()
    {
        static FontGlyphAtlasCache instance;
        return instance;
    }

    std::shared_ptr<FontGlyphAtlas> FontGlyphAtlasCache::GetOrCreate(const Font& font, float pixelSize)
    {
        if (!font.IsValid() || pixelSize <= 0.0f)
        {
            return nullptr;
        }

        Key key;
        key.FontPtr = &font;
        key.PixelSizeKey = static_cast<int>(std::lround(pixelSize));

        const auto it = m_Atlases.find(key);
        if (it != m_Atlases.end() && it->second && it->second->IsValid())
        {
            return it->second;
        }

        std::shared_ptr<FontGlyphAtlas> atlas = std::make_shared<FontGlyphAtlas>();
        if (!atlas->InitializeFromFont(font, static_cast<float>(key.PixelSizeKey)))
        {
            return nullptr;
        }

        m_Atlases[key] = atlas;
        return atlas;
    }

    void FontGlyphAtlasCache::Clear()
    {
        m_Atlases.clear();
    }
}
