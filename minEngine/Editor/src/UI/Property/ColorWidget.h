#pragma once

#include "Runtime/Core/Math/Color.h"

namespace minEngine
{
    /** Edits LinearColor storage with ImGui ColorEdit (sRGB display). */
    class ColorWidget
    {
    public:
        static bool DrawLinearColor(LinearColor* value, float itemWidth = -1.0f);
    };
}
