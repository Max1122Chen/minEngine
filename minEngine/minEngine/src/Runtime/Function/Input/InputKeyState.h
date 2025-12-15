#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    struct InputKeyState
    {
        Vector3 RawValue;

        bool bDown = false;
    };
}