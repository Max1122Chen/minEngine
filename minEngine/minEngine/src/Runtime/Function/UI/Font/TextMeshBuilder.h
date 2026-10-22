#pragma once

#include "Core.h"
#include "Runtime/Function/UI/UITypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class FontGlyphAtlas;
    class RHI;
    class RHIBuffer;
    class RHIVertexInputLayout;

    struct TextMeshBuildResult
    {
        std::vector<float> InterleavedVertices;
        std::vector<uint32_t> Indices;
        uint32_t QuadCount = 0;
    };

    /**
     * Builds ScreenUI text geometry in Widget-local pixels (origin top-left, Y down).
     * Vertex layout matches SpriteQuadMesh (Position3/TexCoord2/Normal3/Tangent4).
     */
    class MINENGINE_API TextMeshBuilder
    {
    public:
        static bool Build(
            FontGlyphAtlas& atlas,
            const std::string& utf8Text,
            const Vector2& widgetSize,
            EUITextHorizontalAlignment alignment,
            TextMeshBuildResult& outResult);

        static RHIVertexInputLayout* GetOrCreateVertexInputLayout(RHI& rhi);
    };
}
