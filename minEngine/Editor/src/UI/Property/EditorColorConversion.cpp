#include "UI/Property/EditorColorConversion.h"

#include <cmath>

namespace minEngine
{
    namespace
    {
        uint8_t NormalizedFloatToByte(float normalized)
        {
            const float clamped = std::clamp(normalized, 0.0f, 1.0f);
            return static_cast<uint8_t>(std::lround(clamped * 255.0f));
        }
    }

    EditorSrgbEditColor EditorColorConversion::ToSrgbEditColor(const LinearColor& linear)
    {
        const Color srgb = linear.ToColor();
        return EditorSrgbEditColor{
            static_cast<float>(srgb.R) / 255.0f,
            static_cast<float>(srgb.G) / 255.0f,
            static_cast<float>(srgb.B) / 255.0f,
            static_cast<float>(srgb.A) / 255.0f,
        };
    }

    LinearColor EditorColorConversion::FromSrgbEditColor(const EditorSrgbEditColor& srgbEdit)
    {
        const Color srgb{
            NormalizedFloatToByte(srgbEdit.R),
            NormalizedFloatToByte(srgbEdit.G),
            NormalizedFloatToByte(srgbEdit.B),
            NormalizedFloatToByte(srgbEdit.A),
        };
        return srgb.ToLinearColor();
    }
}
