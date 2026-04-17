#pragma once
#include "Core.h"

namespace minEngine
{
    ME_ENUM()
    enum InputKeyAction
    {
        Idle = 1 << 0,
        Press = 1 << 1,
        Down = 1 << 2,
        Release = 1 << 3,
        Repeat = 1 << 4
    };

    ME_ENUM()
    enum class InputAxisType : uint8_t
    {
        None,
        Button,
        Axis1D,
        Axis2D,
        Axis3D
    };
}

#include "Generated/Reflection/InputKeyTypes.gen.h"