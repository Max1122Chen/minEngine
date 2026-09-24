#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include <cstdint>

namespace minEngine
{
    /** Metrics + atlas UV for one Unicode codepoint (reference pixels, Y down). */
    struct FontGlyphInfo
    {
        uint32_t Codepoint = 0;
        float AdvanceX = 0.0f;
        float BearingX = 0.0f;
        float BearingY = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;
        /** Atlas UV: (U0, V0, U1, V1), V increases downward. */
        Vector4 UVRect{ 0.0f, 0.0f, 0.0f, 0.0f };
        bool bValid = false;
    };
}
