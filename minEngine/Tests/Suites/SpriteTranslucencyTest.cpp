#include "Runtime/Function/Render/Sprite/SpriteTranslucency.h"

#include "doctest.h"

TEST_CASE("sprite-translucency: color alpha predicate [full]")
{
    using namespace minEngine;

    CHECK(ColorAlphaIsTranslucent(0.0f));
    CHECK(ColorAlphaIsTranslucent(0.5f));
    CHECK_FALSE(ColorAlphaIsTranslucent(1.0f));
    CHECK_FALSE(TextureMayHaveAlpha(nullptr));
}

TEST_CASE("sprite-translucency: compute pass queue [full]")
{
    using namespace minEngine;

    const LinearColor opaqueWhite(1.0f, 1.0f, 1.0f, 1.0f);
    const LinearColor translucentWhite(1.0f, 1.0f, 1.0f, 0.5f);

    CHECK_FALSE(ComputeSpriteNeedsTranslucentPass(opaqueWhite, nullptr, false));
    CHECK(ComputeSpriteNeedsTranslucentPass(translucentWhite, nullptr, false));
    CHECK(ComputeSpriteNeedsTranslucentPass(opaqueWhite, nullptr, true));
}
