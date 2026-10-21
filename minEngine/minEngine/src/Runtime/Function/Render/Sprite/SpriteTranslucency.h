#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    /** Path A translucency predicate. */
    bool ColorAlphaIsTranslucent(float alpha);
    bool TextureMayHaveAlpha(const Texture2D* texture);

    bool ComputeSpriteNeedsTranslucentPass(
        const LinearColor& color,
        const Texture2D* texture,
        bool materialIsTranslucent);
}