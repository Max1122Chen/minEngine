#include "SpriteTranslucency.h"

namespace minEngine
{
    namespace
    {
        constexpr float kSpriteAlphaEps = 1.0f / 255.0f;
    }

    bool ColorAlphaIsTranslucent(float alpha)
    {
        return alpha < (1.0f - kSpriteAlphaEps);
    }

    bool TextureMayHaveAlpha(const Texture2D* texture)
    {
        if (texture == nullptr)
        {
            return false;
        }

        return texture->GetChannels() >= 4;
    }

    bool ComputeSpriteNeedsTranslucentPass(
        const LinearColor& color,
        const Texture2D* texture,
        bool materialIsTranslucent)
    {
        if (materialIsTranslucent)
        {
            return true;
        }

        if (ColorAlphaIsTranslucent(color.A))
        {
            return true;
        }

        return TextureMayHaveAlpha(texture);
    }
}
