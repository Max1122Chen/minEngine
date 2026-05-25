#include "Runtime/Core/Math/Color.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    namespace
    {
        float Clamp01(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        float LinearChannelToSrgb(float linear)
        {
            const float clamped = Clamp01(linear);
            if (clamped <= 0.0031308f)
            {
                return clamped * 12.92f;
            }

            return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
        }

        float SrgbChannelToLinear(float srgb)
        {
            const float clamped = Clamp01(srgb);
            if (clamped <= 0.04045f)
            {
                return clamped / 12.92f;
            }

            return std::pow((clamped + 0.055f) / 1.055f, 2.4f);
        }

        uint8_t FloatToByte(float normalized)
        {
            const float clamped = Clamp01(normalized);
            return static_cast<uint8_t>(std::lround(clamped * 255.0f));
        }

        float ByteToFloat(uint8_t channel)
        {
            return static_cast<float>(channel) / 255.0f;
        }
    }

    Color LinearColor::ToColor() const
    {
        return Color(
            FloatToByte(LinearChannelToSrgb(R)),
            FloatToByte(LinearChannelToSrgb(G)),
            FloatToByte(LinearChannelToSrgb(B)),
            FloatToByte(Clamp01(A)));
    }

    LinearColor Color::ToLinearColor() const
    {
        return LinearColor(
            SrgbChannelToLinear(ByteToFloat(R)),
            SrgbChannelToLinear(ByteToFloat(G)),
            SrgbChannelToLinear(ByteToFloat(B)),
            Clamp01(ByteToFloat(A)));
    }

    LinearColor LinearColor::FromColor(const Color& srgb)
    {
        return srgb.ToLinearColor();
    }

    Color Color::FromLinearColor(const LinearColor& linear)
    {
        return linear.ToColor();
    }

    bool LinearColor::operator==(const LinearColor& other) const
    {
        return R == other.R && G == other.G && B == other.B && A == other.A;
    }

    bool Color::operator==(const Color& other) const
    {
        return R == other.R && G == other.G && B == other.B && A == other.A;
    }
}
