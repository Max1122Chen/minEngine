#pragma once

#include "Math/Math.h"
#include "RHIClipSpaceCapabilities.h"

namespace minEngine
{
    class RHIClipSpace
    {
    public:
        static Matrix4 MakePerspective(float fovYRadians, float aspect, float zNear, float zFar);
        static Matrix4 MakeOrthographic(
            float left,
            float right,
            float bottom,
            float top,
            float zNear,
            float zFar);
    };

} // namespace minEngine
