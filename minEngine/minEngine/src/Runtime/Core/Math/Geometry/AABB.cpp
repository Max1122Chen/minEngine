#include "AABB.h"
#include "Ray.h"

#include <algorithm>
#include <limits>

namespace minEngine::Math::Geometry
{
    bool AABB::Intersect(const AABB& other) const
    {
        if (Max.x < other.Min.x || Min.x > other.Max.x) return false;
        if (Max.y < other.Min.y || Min.y > other.Max.y) return false;
        if (Max.z < other.Min.z || Min.z > other.Max.z) return false;
        return true;
    }
    
    bool AABB::IntersectRay(const Ray& ray, float& outDistance) const
    {
        constexpr float epsilon = 1e-6f;
        float tEnter = -std::numeric_limits<float>::infinity();
        float tExit = std::numeric_limits<float>::infinity();

        const float origin[3] = { ray.Origin.x, ray.Origin.y, ray.Origin.z };
        const float direction[3] = { ray.Direction.x, ray.Direction.y, ray.Direction.z };
        const float boundsMin[3] = { Min.x, Min.y, Min.z };
        const float boundsMax[3] = { Max.x, Max.y, Max.z };

        // Robust slab intersection: parallel rays are valid if origin lies in the slab.
        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::abs(direction[axis]) < epsilon)
            {
                if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis])
                {
                    return false;
                }
                continue;
            }

            const float invD = 1.0f / direction[axis];
            float t0 = (boundsMin[axis] - origin[axis]) * invD;
            float t1 = (boundsMax[axis] - origin[axis]) * invD;
            if (t0 > t1)
            {
                std::swap(t0, t1);
            }

            tEnter = std::max(tEnter, t0);
            tExit = std::min(tExit, t1);
            if (tEnter > tExit)
            {
                return false;
            }
        }

        // Entire intersection interval is behind the ray origin.
        if (tExit < 0.0f)
        {
            return false;
        }

        // If origin is inside the box, tEnter can be negative, so use the first forward hit.
        outDistance = (tEnter >= 0.0f) ? tEnter : tExit;
        return true;
    }

    bool AABB::Contains(const Vector3& point) const
    {
        return (point.x >= Min.x && point.x <= Max.x) &&
               (point.y >= Min.y && point.y <= Max.y) &&
               (point.z >= Min.z && point.z <= Max.z);
    }

    void AABB::Encapsulate(const Vector3 &point)
    {
        Min.x = std::min(Min.x, point.x);
        Min.y = std::min(Min.y, point.y);
        Min.z = std::min(Min.z, point.z);

        Max.x = std::max(Max.x, point.x);
        Max.y = std::max(Max.y, point.y);
        Max.z = std::max(Max.z, point.z);
    }

    AABB Transform(const AABB &aabb, const Matrix4 &transformMat)
    {
        // Get the 8 corners of the AABB
        const Vector3& Min = aabb.Min;
        const Vector3& Max = aabb.Max;
        Vector3 corners[8] = {
            Vector3(Min.x, Min.y, Min.z),
            Vector3(Max.x, Min.y, Min.z),
            Vector3(Min.x, Max.y, Min.z),
            Vector3(Max.x, Max.y, Min.z),
            Vector3(Min.x, Min.y, Max.z),
            Vector3(Max.x, Min.y, Max.z),
            Vector3(Min.x, Max.y, Max.z),
            Vector3(Max.x, Max.y, Max.z)
        };

        // Transform all corners and find new min/max
        Vector3 newMin = transformMat * Vector4(corners[0], 1.0f);
        Vector3 newMax = newMin;

        for (int i = 1; i < 8; ++i)
        {
            Vector3 transformedCorner = transformMat * Vector4(corners[i], 1.0f);
            newMin = glm::min(newMin, transformedCorner);
            newMax = glm::max(newMax, transformedCorner);
        }

        return AABB(newMin, newMax);
    }

}