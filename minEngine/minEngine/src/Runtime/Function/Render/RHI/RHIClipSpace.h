#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "Math/Math.h"
#include "RHIClipSpaceCapabilities.h"

namespace minEngine
{
    class MINENGINE_API RHIClipSpace
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
