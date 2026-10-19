#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include <cstdint>

namespace minEngine
{
    class WidgetComponent;

    enum class EUIPointerButton : uint8_t
    {
        None = 0,
        Left,
        Right,
        Middle,
    };

    struct UIKeyModifiers
    {
        bool bShift = false;
        bool bCtrl = false;
        bool bAlt = false;
        bool bLeftShift = false;
        bool bRightShift = false;
        bool bLeftCtrl = false;
        bool bRightCtrl = false;
        bool bLeftAlt = false;
        bool bRightAlt = false;
    };

    struct UIPointerEvent
    {
        EUIPointerButton Button = EUIPointerButton::None;
        UIKeyModifiers Modifiers{};
        Vector2 ReferencePx{ 0.0f, 0.0f };
        int32_t ClickCount = 0;
        WidgetComponent* Widget = nullptr;
    };
}
