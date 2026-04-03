#pragma once
#include "Core.h"

namespace minEngine
{
    enum InputKeyAction
    {
        Press,
        Down,
        Release
    };

    enum class InputAxisType : uint8_t
    {
        None,
        Button,
        Axis1D,
        Axis2D,
        Axis3D
    };
}