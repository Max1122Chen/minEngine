#include "FontGlyphAtlas.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHISampler.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/UI/Font/FontUtf8.h"
#include "Runtime/Resource/Font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cmath>

namespace minEngine
{
    FontGlyphAtlas::~FontGlyphAtlas()
    {
        Shutdown();
    }

    bool FontGlyphAtlas::InitializeFromFont(const Font& font, float pixelSize, uint32_t atlasSize)
    {
        return Initialize(font.GetFontFileBytes(), pixelSize, atlasSize);
    }

    bool FontGlyphAtlas::Initialize(const std::vector<uint8_t>& fontFileBytes, float pixelSize, uint32_t atlasSize)
    {
        Shutdown();

        if (fontFileBytes.empty() || pixelSize <= 0.0f || atlasSize < 64)
        {
            ME_LOG(LogUI, Warn, "FontGlyphAtlas: invalid Initialize args.");
            return false;
        }

        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library) != 0)
        {
            ME_LOG(LogUI, Error, "FontGlyphAtlas: FT_Init_FreeType failed.");
            return false;
        }

        FT_Face face = nullptr;
        if (FT_New_Memory_Face(
                library,
                fontFileBytes.data(),
                static_cast<FT_Long>(fontFileBytes.size()),
                0,
                &face)
            != 0)
        {
            ME_LOG(LogUI, Error, "FontGlyphAtlas: FT_New_Memory_Face failed.");
            FT_Done_FreeType(library);
            return false;
        }

        const FT_UInt reqSize = static_cast<FT_UInt>(std::lround(pixelSize));
        if (FT_Set_Pixel_Sizes(face, 0, reqSize) != 0)
        {
            ME_LOG(LogUI, Error, "FontGlyphAtlas: FT_Set_Pixel_Sizes failed.");
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return false;
        }

        m_FTLibrary = library;
        m_FTFace = face;
        m_PixelSize = pixelSize;
        m_AtlasWidth = atlasSize;
        m_AtlasHeight = atlasSize;
        m_AtlasRGBA.assign(static_cast<size_t>(atlasSize) * static_cast<size_t>(atlasSize) * 4u, 0);
        m_Shelf = ShelfState{};
        m_Glyphs.clear();
        m_bAtlasGpuDirty = true;
        m_GpuTexture.reset();

        m_Ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
        const float descender = static_cast<float>(face->size->metrics.descender) / 64.0f;
        m_LineHeight = m_Ascender - descender;
        m_bValid = true;
        return true;
    }

    void FontGlyphAtlas::Shutdown()
    {
        m_GpuTexture.reset();
        m_Glyphs.clear();
        m_AtlasRGBA.clear();
        m_bValid = false;
        m_bAtlasGpuDirty = true;

        if (m_FTFace != nullptr)
        {
            FT_Done_Face(static_cast<FT_Face>(m_FTFace));
            m_FTFace = nullptr;
        }
        if (m_FTLibrary != nullptr)
        {
            FT_Done_FreeType(static_cast<FT_Library>(m_FTLibrary));
            m_FTLibrary = nullptr;
        }
    }

    bool FontGlyphAtlas::TryGetGlyph(uint32_t codepoint, FontGlyphInfo& outInfo) const
    {
        const auto it = m_Glyphs.find(codepoint);
        if (it == m_Glyphs.end() || !it->second.bValid)
        {
            return false;
        }
        outInfo = it->second;
        return true;
    }

    bool FontGlyphAtlas::EnsureGlyph(uint32_t codepoint, FontGlyphInfo& outInfo)
    {
        if (!m_bValid)
        {
            return false;
        }

        if (TryGetGlyph(codepoint, outInfo))
        {
            return true;
        }

        // Space: advance only, no bitmap.
        if (codepoint == ' ')
        {
            FT_Face face = static_cast<FT_Face>(m_FTFace);
            if (FT_Load_Char(face, codepoint, FT_LOAD_DEFAULT) != 0)
            {
                return false;
            }

            FontGlyphInfo info;
            info.Codepoint = codepoint;
            info.AdvanceX = static_cast<float>(face->glyph->advance.x) / 64.0f;
            info.bValid = true;
            m_Glyphs[codepoint] = info;
            outInfo = info;
            return true;
        }

        return RasterizeAndPack(codepoint, outInfo);
    }

    bool FontGlyphAtlas::PackBitmap(uint32_t bitmapWidth, uint32_t bitmapHeight, uint32_t& outX, uint32_t& outY)
    {
        const uint32_t pad = 1;
        const uint32_t needW = bitmapWidth + pad;
        const uint32_t needH = bitmapHeight + pad;

        if (needW + 1 > m_AtlasWidth || needH + 1 > m_AtlasHeight)
        {
            return false;
        }

        if (m_Shelf.CursorX + needW > m_AtlasWidth)
        {
            m_Shelf.CursorX = 1;
            m_Shelf.CursorY += m_Shelf.ShelfHeight + pad;
            m_Shelf.ShelfHeight = 0;
        }

        if (m_Shelf.CursorY + needH > m_AtlasHeight)
        {
            ME_LOG(LogUI, Warn, "FontGlyphAtlas: atlas full ({}x{}).", m_AtlasWidth, m_AtlasHeight);
            return false;
        }

        outX = m_Shelf.CursorX;
        outY = m_Shelf.CursorY;
        m_Shelf.CursorX += needW;
        if (needH > m_Shelf.ShelfHeight)
        {
            m_Shelf.ShelfHeight = needH;
        }
        return true;
    }

    bool FontGlyphAtlas::RasterizeAndPack(uint32_t codepoint, FontGlyphInfo& outInfo)
    {
        FT_Face face = static_cast<FT_Face>(m_FTFace);
        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER) != 0)
        {
            return false;
        }

        FT_GlyphSlot slot = face->glyph;
        const uint32_t bw = slot->bitmap.width;
        const uint32_t bh = slot->bitmap.rows;

        FontGlyphInfo info;
        info.Codepoint = codepoint;
        info.AdvanceX = static_cast<float>(slot->advance.x) / 64.0f;
        info.BearingX = static_cast<float>(slot->bitmap_left);
        info.BearingY = static_cast<float>(slot->bitmap_top);
        info.Width = static_cast<float>(bw);
        info.Height = static_cast<float>(bh);

        if (bw == 0 || bh == 0)
        {
            info.bValid = true;
            m_Glyphs[codepoint] = info;
            outInfo = info;
            return true;
        }

        uint32_t packX = 0;
        uint32_t packY = 0;
        if (!PackBitmap(bw, bh, packX, packY))
        {
            return false;
        }

        const unsigned char* src = slot->bitmap.buffer;
        const int srcPitch = slot->bitmap.pitch;
        for (uint32_t row = 0; row < bh; ++row)
        {
            for (uint32_t col = 0; col < bw; ++col)
            {
                const unsigned char coverage = src[static_cast<int>(row) * srcPitch + static_cast<int>(col)];
                const size_t dstIndex =
                    (static_cast<size_t>(packY + row) * m_AtlasWidth + static_cast<size_t>(packX + col)) * 4u;
                m_AtlasRGBA[dstIndex + 0] = 255;
                m_AtlasRGBA[dstIndex + 1] = 255;
                m_AtlasRGBA[dstIndex + 2] = 255;
                m_AtlasRGBA[dstIndex + 3] = coverage;
            }
        }

        const float invW = 1.0f / static_cast<float>(m_AtlasWidth);
        const float invH = 1.0f / static_cast<float>(m_AtlasHeight);
        info.UVRect = Vector4(
            static_cast<float>(packX) * invW,
            static_cast<float>(packY) * invH,
            static_cast<float>(packX + bw) * invW,
            static_cast<float>(packY + bh) * invH);
        info.bValid = true;
        m_Glyphs[codepoint] = info;
        m_bAtlasGpuDirty = true;
        outInfo = info;
        return true;
    }

    bool FontGlyphAtlas::EnsureTextGlyphs(const std::string& utf8Text)
    {
        if (!m_bValid)
        {
            return false;
        }

        const char* cursor = utf8Text.data();
        const char* end = cursor + utf8Text.size();
        uint32_t codepoint = 0;
        while (FontUtf8::DecodeNext(cursor, end, codepoint))
        {
            FontGlyphInfo info;
            EnsureGlyph(codepoint, info);
        }
        return true;
    }

    float FontGlyphAtlas::MeasureTextWidth(const std::string& utf8Text)
    {
        if (!m_bValid)
        {
            return 0.0f;
        }

        float width = 0.0f;
        const char* cursor = utf8Text.data();
        const char* end = cursor + utf8Text.size();
        uint32_t codepoint = 0;
        while (FontUtf8::DecodeNext(cursor, end, codepoint))
        {
            FontGlyphInfo info;
            if (EnsureGlyph(codepoint, info))
            {
                width += info.AdvanceX;
            }
        }
        return width;
    }

    void FontGlyphAtlas::InvalidateGpuTexture()
    {
        m_bAtlasGpuDirty = true;
        m_GpuTexture.reset();
    }

    std::shared_ptr<Texture2D> FontGlyphAtlas::GetOrCreateTexture(RHI& rhi)
    {
        if (!m_bValid || m_AtlasRGBA.empty())
        {
            return nullptr;
        }

        if (m_GpuTexture && m_GpuTexture->GetRHITexture() != nullptr && !m_bAtlasGpuDirty)
        {
            return m_GpuTexture;
        }

        std::shared_ptr<Texture2D> texture = Texture2D::CreateFromRGBA8(
            rhi,
            m_AtlasWidth,
            m_AtlasHeight,
            m_AtlasRGBA.data());
        if (!texture)
        {
            return nullptr;
        }

        RHISamplerDesc sampler;
        sampler.MinFilter = RHISamplerFilter::Linear;
        sampler.MagFilter = RHISamplerFilter::Linear;
        sampler.AddressU = RHISamplerAddress::ClampToEdge;
        sampler.AddressV = RHISamplerAddress::ClampToEdge;
        sampler.AddressW = RHISamplerAddress::ClampToEdge;
        texture->SetPreferredSamplerDesc(sampler);

        m_GpuTexture = texture;
        m_bAtlasGpuDirty = false;
        return m_GpuTexture;
    }
}
