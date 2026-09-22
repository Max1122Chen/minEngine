#include "Frustum.h"

#include "Render/RHI/RHIClipSpaceCapabilities.h"

#include <cmath>

namespace minEngine::Math::Geometry
{
    void Plane::Normalize()
    {
        const float length = std::sqrt(
            Normal.x * Normal.x + Normal.y * Normal.y + Normal.z * Normal.z);
        if (length <= 1e-8f)
        {
            return;
        }

        const float invLength = 1.0f / length;
        Normal *= invLength;
        Distance *= invLength;
    }

    float Plane::SignedDistance(const Vector3& point) const
    {
        return Normal.x * point.x + Normal.y * point.y + Normal.z * point.z + Distance;
    }

    Vector4 Frustum::MakeMatrixRow(const Matrix4& matrix, int row)
    {
        return Vector4(
            matrix[0][row],
            matrix[1][row],
            matrix[2][row],
            matrix[3][row]);
    }

    Plane Frustum::MakePlaneFromCoeffs(const Vector4& coeffs)
    {
        Plane plane;
        plane.Normal = Vector3(coeffs.x, coeffs.y, coeffs.z);
        plane.Distance = coeffs.w;
        plane.Normalize();
        return plane;
    }

    Frustum Frustum::FromViewProjection(const Matrix4& viewProjection)
    {
        return FromViewProjection(viewProjection, GetFrustumNdcZNear(), GetFrustumNdcZFar());
    }

    Frustum Frustum::FromViewProjection(
        const Matrix4& viewProjection,
        float ndcZNear,
        float ndcZFar)
    {
        const Vector4 row0 = MakeMatrixRow(viewProjection, 0);
        const Vector4 row1 = MakeMatrixRow(viewProjection, 1);
        const Vector4 row2 = MakeMatrixRow(viewProjection, 2);
        const Vector4 row3 = MakeMatrixRow(viewProjection, 3);

        Frustum frustum;
        frustum.m_Planes[static_cast<size_t>(FrustumPlane::Left)] =
            MakePlaneFromCoeffs(row3 + row0);
        frustum.m_Planes[static_cast<size_t>(FrustumPlane::Right)] =
            MakePlaneFromCoeffs(row3 - row0);
        frustum.m_Planes[static_cast<size_t>(FrustumPlane::Bottom)] =
            MakePlaneFromCoeffs(row3 + row1);
        frustum.m_Planes[static_cast<size_t>(FrustumPlane::Top)] =
            MakePlaneFromCoeffs(row3 - row1);
        frustum.m_Planes[static_cast<size_t>(FrustumPlane::Near)] =
            MakePlaneFromCoeffs(row2 - ndcZNear * row3);
        frustum.m_Planes[static_cast<size_t>(FrustumPlane::Far)] =
            MakePlaneFromCoeffs(ndcZFar * row3 - row2);
        return frustum;
    }

    bool Frustum::Intersect(const AABB& bounds) const
    {
        for (size_t i = 0; i < static_cast<size_t>(FrustumPlane::Count); ++i)
        {
            const Plane& plane = m_Planes[i];
            const Vector3 positiveVertex(
                plane.Normal.x >= 0.0f ? bounds.Max.x : bounds.Min.x,
                plane.Normal.y >= 0.0f ? bounds.Max.y : bounds.Min.y,
                plane.Normal.z >= 0.0f ? bounds.Max.z : bounds.Min.z);

            if (plane.SignedDistance(positiveVertex) < 0.0f)
            {
                return false;
            }
        }

        return true;
    }

    const Plane& Frustum::GetPlane(FrustumPlane plane) const
    {
        return m_Planes[static_cast<size_t>(plane)];
    }
}
