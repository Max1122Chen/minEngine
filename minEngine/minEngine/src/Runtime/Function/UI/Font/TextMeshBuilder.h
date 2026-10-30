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

        /**
         * The vertex layout every ScreenUI text draw shares, created on the first call.
         *
         * The layout is held beyond the caller's frame, so it is a process-lifetime handle: it has
         * to be given back before the device goes, which is what ReleaseSharedLayout is for. The
         * factory survives that: a call after a release builds a new layout.
         */
        static RHIVertexInputLayout* GetOrCreateVertexInputLayout(RHI& rhi);

        /**
         * Gives the shared layout's reference back.
         *
         * Call it while the device is still alive, after the draws that used the layout are gone
         * and before the device's destruction queue is drained: the handle then takes the ordinary
         * deferred-destroy path instead of being released by a static destructor that runs after
         * the device is dead, where the native object can no longer be destroyed at all.
         */
        static void ReleaseSharedLayout();

        /** True while a layout is held on behalf of the callers of GetOrCreateVertexInputLayout. */
        static bool HasSharedLayout();
    };
}
