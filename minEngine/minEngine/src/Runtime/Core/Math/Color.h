#pragma once

#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"

#include <cstdint>

namespace minEngine
{
    /**
     * Linear RGBA (scene / rendering / theme storage).
     * All persisted editor appearance colors use this type.
     */
    ME_STRUCT()
    struct LinearColor
    {
        ME_GENERATED_BODY(LinearColor)

        ME_PROPERTY(EditAnywhere)
        float R = 0.0f;

        ME_PROPERTY(EditAnywhere)
        float G = 0.0f;

        ME_PROPERTY(EditAnywhere)
        float B = 0.0f;

        ME_PROPERTY(EditAnywhere)
        float A = 1.0f;

        LinearColor() = default;
        LinearColor(float inR, float inG, float inB, float inA = 1.0f)
            : R(inR)
            , G(inG)
            , B(inB)
            , A(inA)
        {
        }

        static LinearColor FromColor(const struct Color& srgb);
        struct Color ToColor() const;

        bool operator==(const LinearColor& other) const;
        bool operator!=(const LinearColor& other) const { return !(*this == other); }
    };

    /**
     * sRGB 8-bit color (display / compact exchange). Not the primary storage type for themes.
     */
    ME_STRUCT()
    struct Color
    {
        ME_GENERATED_BODY(Color)

        ME_PROPERTY(EditAnywhere)
        uint8_t R = 255;

        ME_PROPERTY(EditAnywhere)
        uint8_t G = 255;

        ME_PROPERTY(EditAnywhere)
        uint8_t B = 255;

        ME_PROPERTY(EditAnywhere)
        uint8_t A = 255;

        Color() = default;
        Color(uint8_t inR, uint8_t inG, uint8_t inB, uint8_t inA = 255)
            : R(inR)
            , G(inG)
            , B(inB)
            , A(inA)
        {
        }

        static Color FromLinearColor(const LinearColor& linear);
        LinearColor ToLinearColor() const;

        bool operator==(const Color& other) const;
        bool operator!=(const Color& other) const { return !(*this == other); }
    };
}

#include "Generated/Reflection/Color.gen.h"
