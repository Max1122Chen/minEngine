#include "ScreenUIOrientationTest.h"

#include "Runtime/Function/Render/RHI/RHIClipSpace.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"

#include "doctest.h"

#include <array>
#include <cmath>

namespace minEngine
{
    namespace
    {
        struct QuadVertexSample
        {
            Vector2 Pos;
            Vector2 Uv;
        };

        /** Reads the shared quad's CPU definition so this oracle tracks geometry changes. */
        std::array<QuadVertexSample, SpriteQuadMesh::VertexCount> ReadQuadSamples()
        {
            std::array<QuadVertexSample, SpriteQuadMesh::VertexCount> samples{};
            const float* data = SpriteQuadMesh::GetInterleavedVertexData();
            for (uint32_t i = 0; i < SpriteQuadMesh::VertexCount; ++i)
            {
                const float* vertex = data + i * SpriteQuadMesh::FloatsPerVertex;
                samples[i].Pos = Vector2(vertex[0], vertex[1]);
                samples[i].Uv = Vector2(
                    vertex[SpriteQuadMesh::TexCoordFloatOffset],
                    vertex[SpriteQuadMesh::TexCoordFloatOffset + 1]);
            }
            return samples;
        }

        /** Screen Y as a fraction from the top row (0 = top, 1 = bottom); NDC y+ is the top row. */
        float ScreenYFromTop(const Matrix4& modelToClip, const Vector2& modelPos)
        {
            const Vector4 clip = modelToClip * Vector4(modelPos.x, modelPos.y, 0.0f, 1.0f);
            const float ndcY = clip.y / clip.w;
            return (1.0f - ndcY) * 0.5f;
        }

        /** Mean screen Y of the quad corners carrying texture row v (0 = texture bottom). */
        float MeanScreenYAtTextureV(const Matrix4& modelToClip, float wantedV)
        {
            float sum = 0.0f;
            int count = 0;
            for (const QuadVertexSample& sample : ReadQuadSamples())
            {
                if (std::fabs(sample.Uv.y - wantedV) < 1.0e-6f)
                {
                    sum += ScreenYFromTop(modelToClip, sample.Pos);
                    ++count;
                }
            }
            REQUIRE(count == 2);
            return sum / static_cast<float>(count);
        }

        /** +1 when texture bottom (v=0) sits below texture top (v=1) on screen. */
        float OrientationSign(const Matrix4& modelToClip)
        {
            const float textureBottom = MeanScreenYAtTextureV(modelToClip, 0.0f);
            const float textureTop = MeanScreenYAtTextureV(modelToClip, 1.0f);
            if (std::fabs(textureBottom - textureTop) < 1.0e-6f)
            {
                return 0.0f;
            }
            return textureBottom > textureTop ? 1.0f : -1.0f;
        }
    }

    TEST_CASE("screen-ui-orientation: UI widget keeps texture bottom at screen bottom [smoke]")
    {
        using namespace minEngine;

        // Widget occupying pixels x:[40,160], y:[20,80] in a 640x360 viewport.
        const Matrix4 model = ScreenUICoords::MakeWidgetModelMatrix(Vector2(40.0f, 20.0f), Vector2(120.0f, 60.0f));
        const Matrix4 proj = ScreenUICoords::MakePixelOrthoProjection(640.0f, 360.0f);
        const Matrix4 widgetToClip = proj * model;

        CHECK(OrientationSign(widgetToClip) == doctest::Approx(1.0f));
        CHECK(MeanScreenYAtTextureV(widgetToClip, 0.0f) == doctest::Approx(80.0f / 360.0f));
        CHECK(MeanScreenYAtTextureV(widgetToClip, 1.0f) == doctest::Approx(20.0f / 360.0f));
    }

    TEST_CASE("screen-ui-orientation: world sprite keeps texture bottom at screen bottom [smoke]")
    {
        using namespace minEngine;

        // SpriteComponent at the origin with Size(1,1): model matrix is the shared quad unchanged.
        // A standard Y-up view-projection is what the scene camera provides.
        const Matrix4 worldToClip = RHIClipSpace::MakeOrthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

        CHECK(OrientationSign(worldToClip) == doctest::Approx(1.0f));
    }

    TEST_CASE("screen-ui-orientation: world sprite and UI image agree on screen orientation [smoke]")
    {
        using namespace minEngine;

        const Matrix4 uiWidgetToClip =
            ScreenUICoords::MakePixelOrthoProjection(1920.0f, 1080.0f)
            * ScreenUICoords::MakeWidgetModelMatrix(Vector2(100.0f, 100.0f), Vector2(200.0f, 200.0f));
        const Matrix4 worldToClip = RHIClipSpace::MakeOrthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

        CHECK(OrientationSign(worldToClip) == OrientationSign(uiWidgetToClip));
    }

    TEST_CASE("screen-ui-orientation: widget model mirrors the shared quad Y axis [smoke]")
    {
        using namespace minEngine;

        const Matrix4 model = ScreenUICoords::MakeWidgetModelMatrix(Vector2(0.0f, 0.0f), Vector2(100.0f, 40.0f));
        const Vector4 textureBottomEdge = model * Vector4(0.0f, -0.5f, 0.0f, 1.0f);
        const Vector4 textureTopEdge = model * Vector4(0.0f, 0.5f, 0.0f, 1.0f);

        // Quad -Y carries uv v=0, and must land on the widget's bottom edge (pixel Y grows down).
        CHECK(textureBottomEdge.y == doctest::Approx(40.0f));
        CHECK(textureTopEdge.y == doctest::Approx(0.0f));
    }

    TEST_CASE("screen-ui-orientation: local pixel text matrix stays unmirrored [smoke]")
    {
        using namespace minEngine;

        // Text meshes are authored Y down in widget-local pixels; that path must not be mirrored.
        const Matrix4 model = ScreenUICoords::MakeWidgetLocalPixelModelMatrix(Vector2(10.0f, 20.0f), 2.0f);
        CHECK((model * Vector4(0.0f, 0.0f, 0.0f, 1.0f)).y == doctest::Approx(20.0f));
        CHECK((model * Vector4(0.0f, 10.0f, 0.0f, 1.0f)).y == doctest::Approx(40.0f));
    }

    TEST_CASE("screen-ui-orientation: 2x2 probe top-left cell maps to GL upper half [smoke]")
    {
        using namespace minEngine;

        // Probe texture is 2x2; the top-left cell is pixel rect (0,0,1,1) and must sample the
        // upper half of the GL texture (v in [0.5,1]), because loaded rows are bottom-up.
        const Vector4 uv = Sprite::PixelRectToUvRectTopLeft(Vector4(0.0f, 0.0f, 1.0f, 1.0f), 2u, 2u);
        CHECK(uv.x == doctest::Approx(0.0f));
        CHECK(uv.y == doctest::Approx(0.5f));
        CHECK(uv.z == doctest::Approx(0.5f));
        CHECK(uv.w == doctest::Approx(1.0f));
    }
}
