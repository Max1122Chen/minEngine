#pragma once

#include "Core.h"
#include "Runtime/Function/UI/Font/FontGlyphTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class Font;
    class RHI;
    class Texture2D;

    /**
     * FreeType-backed glyph atlas for one Font byte source x pixel size.
     * CPU atlas is always available; GPU Texture2D is created on demand.
     */
    class FontGlyphAtlas
    {
    public:
        static constexpr uint32_t kDefaultAtlasSize = 512;

        FontGlyphAtlas() = default;
        ~FontGlyphAtlas();

        FontGlyphAtlas(const FontGlyphAtlas&) = delete;
        FontGlyphAtlas& operator=(const FontGlyphAtlas&) = delete;

        bool Initialize(const std::vector<uint8_t>& fontFileBytes, float pixelSize, uint32_t atlasSize = kDefaultAtlasSize);
        bool InitializeFromFont(const Font& font, float pixelSize, uint32_t atlasSize = kDefaultAtlasSize);
        void Shutdown();

        bool IsValid() const { return m_bValid; }
        float GetPixelSize() const { return m_PixelSize; }
        float GetAscender() const { return m_Ascender; }
        float GetLineHeight() const { return m_LineHeight; }
        uint32_t GetAtlasWidth() const { return m_AtlasWidth; }
        uint32_t GetAtlasHeight() const { return m_AtlasHeight; }
        const std::vector<uint8_t>& GetAtlasRGBA() const { return m_AtlasRGBA; }

        /** Ensure glyph is in atlas. Missing glyphs return false (skipped; no '?'). */
        bool EnsureGlyph(uint32_t codepoint, FontGlyphInfo& outInfo);
        bool TryGetGlyph(uint32_t codepoint, FontGlyphInfo& outInfo) const;

        /** Advance width for UTF-8 (skips missing glyphs). */
        float MeasureTextWidth(const std::string& utf8Text);

        /** Ensure all glyphs for UTF-8; returns false if FreeType face invalid. */
        bool EnsureTextGlyphs(const std::string& utf8Text);

        std::shared_ptr<Texture2D> GetOrCreateTexture(RHI& rhi);
        void InvalidateGpuTexture();

    private:
        struct ShelfState
        {
            uint32_t CursorX = 1;
            uint32_t CursorY = 1;
            uint32_t ShelfHeight = 0;
        };

        bool RasterizeAndPack(uint32_t codepoint, FontGlyphInfo& outInfo);
        bool PackBitmap(uint32_t bitmapWidth, uint32_t bitmapHeight, uint32_t& outX, uint32_t& outY);

        bool m_bValid = false;
        float m_PixelSize = 0.0f;
        float m_Ascender = 0.0f;
        float m_LineHeight = 0.0f;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;
        std::vector<uint8_t> m_AtlasRGBA;
        ShelfState m_Shelf;
        std::unordered_map<uint32_t, FontGlyphInfo> m_Glyphs;
        bool m_bAtlasGpuDirty = true;
        std::shared_ptr<Texture2D> m_GpuTexture;

        void* m_FTLibrary = nullptr;
        void* m_FTFace = nullptr;
    };
}
