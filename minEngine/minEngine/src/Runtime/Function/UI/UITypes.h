#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Reflection/ReflectionAnnotations.h"

#include <cstdint>

namespace minEngine
{
    /** Screen-space UI canvas render mode. World reserved for later Features. */
    ME_ENUM()
    enum class EUICanvasRenderMode : uint8_t
    {
        ScreenSpace = 0,
        WorldSpace,
    };

    /**
     * How Canvas reference resolution maps to the viewport.
     * MVP implements Letterbox only; Stretch is reserved.
     */
    ME_ENUM()
    enum class EUICanvasScaleMode : uint8_t
    {
        Letterbox = 0,
        Stretch,
    };

    /**
     * Editor-facing anchor presets. Apply writes AnchorMin/Max (Unity/UE style).
     * Y increases downward in ScreenUI reference space (top-left origin).
     */
    ME_ENUM()
    enum class EUIAnchorPreset : uint8_t
    {
        TopLeft = 0,
        TopCenter,
        TopRight,
        MiddleLeft,
        Center,
        MiddleRight,
        BottomLeft,
        BottomCenter,
        BottomRight,
        TopStretch,
        MiddleStretch,
        BottomStretch,
        LeftStretch,
        CenterStretch,
        RightStretch,
        StretchAll,
        Custom,
    };

    /** Axis-aligned rect in Canvas reference pixels (top-left origin, +Y down). */
    struct UIRect
    {
        Vector2 TopLeft{ 0.0f, 0.0f };
        Vector2 Size{ 0.0f, 0.0f };

        float Left() const { return TopLeft.x; }
        float Top() const { return TopLeft.y; }
        float Right() const { return TopLeft.x + Size.x; }
        float Bottom() const { return TopLeft.y + Size.y; }
        float Width() const { return Size.x; }
        float Height() const { return Size.y; }

        static UIRect FromLTRB(float left, float top, float right, float bottom)
        {
            UIRect rect;
            rect.TopLeft = Vector2(left, top);
            rect.Size = Vector2(right - left, bottom - top);
            return rect;
        }

        /** Inclusive of left/top edges; exclusive of right/bottom (pixel-top-left convention). */
        bool Contains(const Vector2& point) const
        {
            return point.x >= Left() && point.x < Right() && point.y >= Top() && point.y < Bottom();
        }
    };
}

#include "Generated/Reflection/UITypes.gen.h"
