#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    /** Path A translucency predicate. */
    MINENGINE_API bool ColorAlphaIsTranslucent(float alpha);
    MINENGINE_API bool TextureMayHaveAlpha(const Texture2D* texture);

    MINENGINE_API bool ComputeSpriteNeedsTranslucentPass(
        const LinearColor& color,
        const Texture2D* texture,
        bool materialIsTranslucent);
}