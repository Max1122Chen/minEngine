#pragma once
#include "Math/Math.h"

namespace minEngine::Math::Geometry
{
    struct Ray
    {
        Vector3 Origin;
        Vector3 Direction;

        Ray() = default;

        Ray(const Vector3& origin, const Vector3& direction)
            : Origin(origin), Direction(glm::normalize(direction))
        {}
    };
}