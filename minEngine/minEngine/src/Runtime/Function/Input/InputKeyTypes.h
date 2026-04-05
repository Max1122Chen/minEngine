#pragma once
#include "Core.h"

namespace minEngine
{
    ME_ENUM()
    enum InputKeyAction
    {
        Press,
        Down,
        Release
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