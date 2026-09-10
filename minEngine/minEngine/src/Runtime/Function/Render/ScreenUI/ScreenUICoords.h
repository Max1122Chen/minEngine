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

        /** Letterbox: fit ref resolution inside viewport, preserve aspect, center. */
        struct LetterboxMapping
        {
            float Scale = 1.0f;
            Vector2 Offset{ 0.0f, 0.0f };

            Vector2 MapPoint(const Vector2& refPoint) const
            {
                return Offset + refPoint * Scale;
            }

            Vector2 MapSize(const Vector2& refSize) const
            {
                return refSize * Scale;
            }

            /**
             * Inverse of MapPoint. Returns false if Scale is invalid or the point
             * lies outside the letterboxed content rect (black bars).
             * Skeleton note: math is filled; HitTester election still TBD.
             */
            bool TryUnmapPoint(
                const Vector2& viewportPoint,
                float refWidth,
                float refHeight,
                Vector2& outRefPoint) const;
        };

        static LetterboxMapping MakeLetterboxMapping(
            float refWidth,
            float refHeight,
            float viewportWidth,
            float viewportHeight);
    };
}
