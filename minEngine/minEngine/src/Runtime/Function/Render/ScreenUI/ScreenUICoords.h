#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    /** Pixel top-left screen-space helpers for Path B ScreenUI. */
    class ScreenUICoords
    {
    public:
        /** Ortho proj: origin top-left, +Y down, matches viewport pixels. */
        static Matrix4 MakePixelOrthoProjection(float viewportWidth, float viewportHeight);

        /**
         * Model matrix for shared unit quad (center origin, edge length 1).
         * topLeftPx / sizePx are in viewport pixels (origin top-left).
         */
        static Matrix4 MakeWidgetModelMatrix(const Vector2& topLeftPx, const Vector2& sizePx);
    };
}
