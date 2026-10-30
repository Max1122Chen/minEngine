#include "Math/Geometry/Frustum.h"
#include "Math/Math.h"
#include "Render/DrawCommands/MeshBatchCollector.h"
#include "Render/DrawCommands/MeshCollectContext.h"
#include "Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Render/RenderCamera.h"
#include "Render/RHI/RHIClipSpace.h"
#include "Render/RHI/RHIClipSpaceCapabilities.h"

#include "doctest.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>

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

    /** Test-only multi-section proxy: emits one batch per section AABB that AllowsWorldBounds. */
    class FakeMultiSectionSceneProxy : public minEngine::PrimitiveSceneProxy
    {
    public:
        std::array<minEngine::Math::Geometry::AABB, 2> SectionWorldBounds{};

        void GetMeshBatches(
            minEngine::MeshBatchCollector& collector,
            const minEngine::MeshCollectContext& context) const override
        {
            for (const minEngine::Math::Geometry::AABB& sectionBounds : SectionWorldBounds)
            {
                if (!context.AllowsWorldBounds(sectionBounds))
                {
                    continue;
                }

                minEngine::MeshBatch batch;
                batch.m_BoundingBox = sectionBounds;
                batch.m_Proxy = this;
                collector.AddMesh(batch);
            }
        }
    };
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

TEST_CASE("mesh-collect: MainView frustum skips off-screen section [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum frustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);

    FakeMultiSectionSceneProxy proxy;
    // In front of camera (look-at origin from z=5).
    proxy.SectionWorldBounds[0] = AABB(Vector3(-0.5f), Vector3(0.5f));
    // Behind camera.
    proxy.SectionWorldBounds[1] = AABB(Vector3(-0.5f, -0.5f, 9.5f), Vector3(0.5f, 0.5f, 10.5f));

    MeshCollectContext context;
    context.Purpose = MeshCollectPurpose::MainView;
    context.ViewFrustum = &frustum;

    MeshBatchCollector collector;
    proxy.GetMeshBatches(collector, context);

    CHECK(collector.GetBatches().size() == 1);
    CHECK(collector.GetBatches()[0].m_BoundingBox == proxy.SectionWorldBounds[0]);
}

TEST_CASE("mesh-collect: ShadowOnly with null frustum emits all sections [full]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    const Matrix4 viewProj = MakeLookAtViewProj(-1.0f);
    const Frustum frustum = Frustum::FromViewProjection(viewProj, -1.0f, 1.0f);

    FakeMultiSectionSceneProxy proxy;
    proxy.SectionWorldBounds[0] = AABB(Vector3(-0.5f), Vector3(0.5f));
    proxy.SectionWorldBounds[1] = AABB(Vector3(-0.5f, -0.5f, 9.5f), Vector3(0.5f, 0.5f, 10.5f));

    MeshCollectContext context;
    context.Purpose = MeshCollectPurpose::ShadowOnly;
    context.ViewFrustum = nullptr;
    (void)frustum;

    MeshBatchCollector collector;
    proxy.GetMeshBatches(collector, context);

    CHECK(collector.GetBatches().size() == 2);
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

TEST_CASE("frustum: orthographic projection matches clip-space ortho [smoke]")
{
    using namespace minEngine;

    RenderCamera camera;
    camera.m_ProjectionMode = ECameraProjectionMode::Orthographic;
    camera.m_OrthoHeight = 10.0f;
    camera.m_AspectRatio = 2.0f;
    camera.m_zNear = 0.5f;
    camera.m_zFar = 50.0f;
    camera.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    camera.SetRotationEulerDegrees(Vector3(0.0f, 0.0f, 0.0f));
    camera.Initialize();

    const Matrix4 expected = RHIClipSpace::MakeOrthographic(-10.0f, 10.0f, -5.0f, 5.0f, 0.5f, 50.0f);
    const Matrix4 actual = camera.GetProjectionMatrix();
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            CHECK(actual[column][row] == doctest::Approx(expected[column][row]));
        }
    }
}

TEST_CASE("frustum: orthographic screen rays stay parallel [smoke]")
{
    using namespace minEngine;
    using namespace minEngine::Math::Geometry;

    RenderCamera camera;
    camera.m_ProjectionMode = ECameraProjectionMode::Orthographic;
    camera.m_OrthoHeight = 20.0f;
    camera.m_AspectRatio = 1.0f;
    camera.m_zNear = 1.0f;
    camera.m_zFar = 100.0f;
    camera.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    camera.SetRotationEulerDegrees(Vector3(0.0f, 0.0f, 0.0f));
    camera.Initialize();

    const Vector2 bufferSize(100.0f, 100.0f);
    const Geometry::Ray center = camera.ScreenPointToRay(Vector2(50.0f, 50.0f), bufferSize);
    const Geometry::Ray corner = camera.ScreenPointToRay(Vector2(10.0f, 10.0f), bufferSize);
    const Vector3 forward = camera.GetForwardDirection();

    CHECK(glm::dot(center.Direction, forward) == doctest::Approx(1.0f).epsilon(1.0e-4f));
    CHECK(glm::dot(corner.Direction, forward) == doctest::Approx(1.0f).epsilon(1.0e-4f));
    CHECK(glm::dot(center.Direction, corner.Direction) == doctest::Approx(1.0f).epsilon(1.0e-4f));
    CHECK(glm::length(center.Origin - corner.Origin) > 0.1f);
}
