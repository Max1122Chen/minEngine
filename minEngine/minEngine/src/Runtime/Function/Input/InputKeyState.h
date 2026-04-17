#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    /**
     * @brief 
     * A simple struct to hold the state of an input key
     */
    struct InputKeyState
    {
        Vector3 RawValue;

        InputKeyAction action = InputKeyAction::Release;
    };
}