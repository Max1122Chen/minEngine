#include "TextMeshBuilder.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/UI/Font/FontGlyphAtlas.h"
#include "Runtime/Function/UI/Font/FontUtf8.h"

namespace minEngine
{
    bool TextMeshBuilder::Build(
        FontGlyphAtlas& atlas,
        const std::string& utf8Text,
        const Vector2& widgetSize,
        EUITextHorizontalAlignment alignment,
        TextMeshBuildResult& outResult)
    {
        outResult = TextMeshBuildResult{};
        if (!atlas.IsValid() || widgetSize.x <= 0.0f || widgetSize.y <= 0.0f)
        {
            return false;
        }

        if (!atlas.EnsureTextGlyphs(utf8Text))
        {
            return false;
        }

        const float textWidth = atlas.MeasureTextWidth(utf8Text);
        float penX = 0.0f;
        if (alignment == EUITextHorizontalAlignment::Center)
        {
            penX = (widgetSize.x - textWidth) * 0.5f;
        }
        else if (alignment == EUITextHorizontalAlignment::Right)
        {
            penX = widgetSize.x - textWidth;
        }

        // Top-aligned baseline: place baseline so ascender fits under top edge.
        const float baselineY = atlas.GetAscender();

        FontUtf8::ForEachCodepoint(utf8Text, [&](uint32_t codepoint)
        {
            FontGlyphInfo glyph;
            if (!atlas.EnsureGlyph(codepoint, glyph) || !glyph.bValid)
            {
                return;
            }

            if (glyph.Width > 0.0f && glyph.Height > 0.0f)
            {
                const float left = penX + glyph.BearingX;
                const float top = baselineY - glyph.BearingY;
                const float right = left + glyph.Width;
                const float bottom = top + glyph.Height;

                const float u0 = glyph.UVRect.x;
                const float v0 = glyph.UVRect.y;
                const float u1 = glyph.UVRect.z;
                const float v1 = glyph.UVRect.w;

                const uint32_t base = static_cast<uint32_t>(outResult.InterleavedVertices.size() / 12u);

                auto pushVertex = [&](float x, float y, float u, float v)
                {
                    // pos
                    outResult.InterleavedVertices.push_back(x);
                    outResult.InterleavedVertices.push_back(y);
                    outResult.InterleavedVertices.push_back(0.0f);
                    // uv
                    outResult.InterleavedVertices.push_back(u);
                    outResult.InterleavedVertices.push_back(v);
                    // normal +Z
                    outResult.InterleavedVertices.push_back(0.0f);
                    outResult.InterleavedVertices.push_back(0.0f);
                    outResult.InterleavedVertices.push_back(1.0f);
                    // tangent
                    outResult.InterleavedVertices.push_back(1.0f);
                    outResult.InterleavedVertices.push_back(0.0f);
                    outResult.InterleavedVertices.push_back(0.0f);
                    outResult.InterleavedVertices.push_back(1.0f);
                };

                // Y-down: top edge first (smaller Y), then bottom.
                pushVertex(left, top, u0, v0);
                pushVertex(right, top, u1, v0);
                pushVertex(right, bottom, u1, v1);
                pushVertex(left, bottom, u0, v1);

                outResult.Indices.push_back(base + 0);
                outResult.Indices.push_back(base + 1);
                outResult.Indices.push_back(base + 2);
                outResult.Indices.push_back(base + 2);
                outResult.Indices.push_back(base + 3);
                outResult.Indices.push_back(base + 0);
                ++outResult.QuadCount;
            }

            penX += glyph.AdvanceX;
        });

        return outResult.QuadCount > 0 || utf8Text.empty();
    }

    RHIVertexInputLayout* TextMeshBuilder::GetOrCreateVertexInputLayout(RHI& rhi)
    {
        static RHIVertexInputLayoutRef s_Layout;
        if (!s_Layout)
        {
            s_Layout = rhi.RHICreateVertexInputLayout({
                RHIVertexElement("a_Position", VertexElementType::Float3),
                RHIVertexElement("a_TexCoord", VertexElementType::Float2),
                RHIVertexElement("a_Normal", VertexElementType::Float3),
                RHIVertexElement("a_Tangent", VertexElementType::Float4),
            });
        }
        return s_Layout.get();
    }
}
