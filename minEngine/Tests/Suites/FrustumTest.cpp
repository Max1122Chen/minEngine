#include "Math/Geometry/Frustum.h"
#include "Math/Math.h"
#include "Render/RHI/RHIClipSpace.h"
#include "Render/RHI/RHIClipSpaceCapabilities.h"

#include "doctest.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    minEngine::Math::Matrix4 MakeLookAtViewProj(float ndcZNear)
    {
        using namespace minEngine;
        using namespace minEngine::Math;

        const Matrix4 view = glm::lookAt(
            Vector3(0.0f, 0.0f, 5.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(0.0f, 1.0f, 0.0f));

        const float fovY = glm::radians(60.0f);
        const float aspect = 1.0f;
        const float zNear = 0.1f;
        const float zFar = 100.0f;

        const Matrix4 proj = (ndcZNear < 0.0f)
            ? glm::perspectiveRH_NO(fovY, aspect, zNear, zFar)
            : glm::perspectiveRH_ZO(fovY, aspect, zNear, zFar);

        return proj * view;
    }
}

TEST_CASE("frustum: unit box at origin intersects look-at frustum [smoke]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum frustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);
    const AABB box(Vector3(-0.5f), Vector3(0.5f));

    CHECK(box.IsValid());
    CHECK(frustum.Intersect(box));
}

TEST_CASE("frustum: box behind camera is culled [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum frustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);
    // Camera at z=5 looking toward origin (-Z); box behind camera at z=10.
    const AABB behind(Vector3(-0.5f, -0.5f, 9.5f), Vector3(0.5f, 0.5f, 10.5f));

    CHECK(behind.IsValid());
    CHECK_FALSE(frustum.Intersect(behind));
}

TEST_CASE("frustum: straddling plane counts as intersecting [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum frustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);
    const AABB straddling(Vector3(-20.0f, -20.0f, -20.0f), Vector3(20.0f, 20.0f, 20.0f));

    CHECK(straddling.IsValid());
    CHECK(frustum.Intersect(straddling));
}

TEST_CASE("frustum: outside to the side is culled [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum frustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);
    const AABB side(Vector3(50.0f, -0.5f, -0.5f), Vector3(51.0f, 0.5f, 0.5f));

    CHECK(side.IsValid());
    CHECK_FALSE(frustum.Intersect(side));
}

TEST_CASE("frustum: zero-to-one near plane differs from negative-one [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    // Same clip matrix; only NDC near differs — Near plane coefficients must diverge.
    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum openGlFrustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);
    const Frustum vulkanFrustum = Frustum::FromViewProjection(viewProj, 0.0f, 1.0f);

    const AABB box(Vector3(-0.5f), Vector3(0.5f));
    CHECK(openGlFrustum.Intersect(box));

    const Plane& openGlNear = openGlFrustum.GetPlane(FrustumPlane::Near);
    const Plane& vulkanNear = vulkanFrustum.GetPlane(FrustumPlane::Near);
    const bool sameNear =
        openGlNear.Normal == vulkanNear.Normal && openGlNear.Distance == vulkanNear.Distance;
    CHECK_FALSE(sameNear);
}

TEST_CASE("frustum: active-clip FromViewProjection matches explicit NDC helpers [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 view = glm::lookAt(
        Vector3(0.0f, 0.0f, 5.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(0.0f, 1.0f, 0.0f));
    const Matrix4 proj = RHIClipSpace::MakePerspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const Matrix4 viewProj = proj * view;

    const Frustum fromActive = Frustum::FromViewProjection(viewProj);
    const Frustum fromExplicit =
        Frustum::FromViewProjection(viewProj, GetFrustumNdcZNear(), GetFrustumNdcZFar());

    for (uint8_t i = 0; i < static_cast<uint8_t>(FrustumPlane::Count); ++i)
    {
        const FrustumPlane planeId = static_cast<FrustumPlane>(i);
        const Plane& a = fromActive.GetPlane(planeId);
        const Plane& b = fromExplicit.GetPlane(planeId);
        CHECK(a.Normal.x == doctest::Approx(b.Normal.x));
        CHECK(a.Normal.y == doctest::Approx(b.Normal.y));
        CHECK(a.Normal.z == doctest::Approx(b.Normal.z));
        CHECK(a.Distance == doctest::Approx(b.Distance));
    }
}
