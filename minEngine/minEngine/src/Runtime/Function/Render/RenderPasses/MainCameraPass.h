#pragma once
#include "Core.h"

namespace minEngine
{
    class MainCameraPass
    {
    public:
        MainCameraPass() = default;
        virtual ~MainCameraPass() = default;

        virtual void Render();
    };
}