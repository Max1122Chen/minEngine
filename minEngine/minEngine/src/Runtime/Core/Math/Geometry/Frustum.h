#pragma once

#include "EngineAPI.h"
#include "Math/Geometry/AABB.h"
#include "Math/Math.h"

#include <cstdint>

namespace minEngine::Math::Geometry
{
    /** ax + by + cz + d = 0; Normal points toward the frustum interior. */
    struct Plane
    {
        Vector3 Normal{ 0.0f, 0.0f, 0.0f };
        float Distance = 0.0f;

        void Normalize();
        float SignedDistance(const Vector3& point) const;
    };

    enum class FrustumPlane : uint8_t
    {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count
    };

    /**
     * View frustum as six planes in the same space as the source view-projection
     * (typically world). Independent of RenderCamera / Proxy types.
     */
    class MINENGINE_API Frustum
    {
    public:
        Frustum() = default;

        /**
         * Extract 6 planes from view-projection using active RHI clip depth
         * (GetFrustumNdcZNear / GetFrustumNdcZFar).
         */
        static Frustum FromViewProjection(const Matrix4& viewProjection);

        /**
         * Extract 6 planes with explicit NDC Z near/far (for tests and callers
         * that already know clip depth).
         */
        static Frustum FromViewProjection(
            const Matrix4& viewProjection,
            float ndcZNear,
            float ndcZFar);

        /**
         * Conservative intersection. Precondition: bounds.IsValid().
         * Caller must fail-open on invalid bounds without calling this.
         */
        bool Intersect(const AABB& bounds) const;

        const Plane& GetPlane(FrustumPlane plane) const;
        const Plane* GetPlanes() const { return m_Planes; }

    private:
        static Vector4 MakeMatrixRow(const Matrix4& matrix, int row);
        static Plane MakePlaneFromCoeffs(const Vector4& coeffs);

        Plane m_Planes[static_cast<size_t>(FrustumPlane::Count)]{};
    };
}
