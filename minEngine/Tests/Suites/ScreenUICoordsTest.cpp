#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"

#include "doctest.h"

#include <glm/gtc/matrix_transform.hpp>

TEST_CASE("screen-ui-coords: widget model places center at pixel center [full]")
{
    using namespace minEngine;

    const Vector2 topLeft(10.0f, 20.0f);
    const Vector2 size(100.0f, 40.0f);
    const Matrix4 model = ScreenUICoords::MakeWidgetModelMatrix(topLeft, size);

    const Vector4 localOrigin(0.0f, 0.0f, 0.0f, 1.0f);
    const Vector4 world = model * localOrigin;
    CHECK(world.x == doctest::Approx(60.0f));
    CHECK(world.y == doctest::Approx(40.0f));
}

TEST_CASE("screen-ui-coords: pixel ortho maps top-left to NDC upper-left [full]")
{
    using namespace minEngine;

    const Matrix4 proj = ScreenUICoords::MakePixelOrthoProjection(200.0f, 100.0f);
    const Vector4 clip = proj * Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(clip.x == doctest::Approx(-1.0f));
    CHECK(clip.y == doctest::Approx(1.0f));
}
