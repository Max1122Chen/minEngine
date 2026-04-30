#pragma once
#include "Math/Math.h"

namespace minEngine::Math::Geometry
{
    struct Ray;

    struct AABB
    {
        Vector3 Min = Vector3(std::numeric_limits<float>::max());
        Vector3 Max = Vector3(-std::numeric_limits<float>::max());
        Vector3 GetCenter() const { return (Min + Max) * 0.5f; }
        Vector3 GetExtent() const { return (Max - Min) * 0.5f; }
        Vector3 GetSize() const { return Max - Min; }
        bool Intersect(const AABB& other) const;
        bool IntersectRay(const Ray& ray, float& outDistance) const;
        bool Contains(const Vector3& point) const;
        void Encapsulate(const Vector3& point);

        AABB() = default;
        AABB(const Vector3& min, const Vector3& max)
            : Min(min), Max(max)
        {}
        AABB(const AABB& other)
        {
            Min = other.Min;
            Max = other.Max;
        }

        AABB& operator=(const AABB& other)
        {
            if(this != &other)
            {
                Min = other.Min;
                Max = other.Max;
            }
            return *this;
        }
        AABB operator+(const Vector3& offset) const
        {
            return AABB(Min + offset, Max + offset);
        }
        AABB& operator+=(const Vector3& offset)
        {
            Min += offset;
            Max += offset;
            return *this;
        }
        AABB operator-(const Vector3& offset) const
        {
            return AABB(Min - offset, Max - offset);
        }
        AABB& operator-=(const Vector3& offset)
        {
            Min -= offset;
            Max -= offset;
            return *this;
        }
        AABB operator*(const Vector3& scale) const
        {
            return AABB(Min * scale, Max * scale);
        }
        AABB& operator*=(const Vector3& scale)
        {
            Min *= scale;
            Max *= scale;
            return *this;
        }
        AABB operator/(const Vector3& scale) const
        {
            return AABB(Min / scale, Max / scale);
        }
        AABB& operator/=(const Vector3& scale)
        {
            Min /= scale;
            Max /= scale;
            return *this;
        }
        bool operator==(const AABB& other) const
        {
            return Min == other.Min && Max == other.Max;
        }
    };

    AABB Transform(const AABB& aabb, const Matrix4& transformMat);
}