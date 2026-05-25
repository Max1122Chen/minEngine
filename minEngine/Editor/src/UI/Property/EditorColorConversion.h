#pragma once

#include "Runtime/Core/Math/Color.h"

struct ImVec4;

namespace minEngine
{
    /**
     * Editor-only bridge between LinearColor storage and ImGui ColorEdit (sRGB display).
     * Runtime Color/LinearColor types must not depend on ImGui.
     */
    struct EditorSrgbEditColor
    {
        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;
        float A = 1.0f;
    };

    class EditorColorConversion
    {
    public:
        static EditorSrgbEditColor ToSrgbEditColor(const LinearColor& linear);
        static LinearColor FromSrgbEditColor(const EditorSrgbEditColor& srgbEdit);
        static ImVec4 ToImGuiDisplayColor(const LinearColor& linear);
    };
}
