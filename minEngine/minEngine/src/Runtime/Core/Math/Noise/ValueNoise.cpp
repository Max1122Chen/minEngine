#include "ValueNoise.h"

#include "Math/Noise/NoiseHash.h"

#include <cmath>

namespace minEngine::Math::Noise
{
    ValueNoise::ValueNoise(uint32_t seed)
        : m_Seed(seed)
    {
    }

    void ValueNoise::SetSeed(uint32_t seed)
    {
        m_Seed = seed;
    }

    void ValueNoise::SetFrequency(float frequency)
    {
        m_Frequency = (frequency > 0.0f) ? frequency : m_Frequency;
    }

    float ValueNoise::Fade(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float ValueNoise::Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    float ValueNoise::Sample1D(float x) const
    {
        x *= m_Frequency;
        const float x0f = std::floor(x);
        const int32_t x0 = static_cast<int32_t>(x0f);
        const int32_t x1 = x0 + 1;
        const float tx = Fade(x - x0f);

        const float n0 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash1(x0, m_Seed));
        const float n1 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash1(x1, m_Seed));
        return Lerp(n0, n1, tx);
    }

    float ValueNoise::Sample2D(float x, float y) const
    {
        x *= m_Frequency;
        y *= m_Frequency;
        const float x0f = std::floor(x);
        const float y0f = std::floor(y);
        const int32_t x0 = static_cast<int32_t>(x0f);
        const int32_t y0 = static_cast<int32_t>(y0f);
        const int32_t x1 = x0 + 1;
        const int32_t y1 = y0 + 1;
        const float tx = Fade(x - x0f);
        const float ty = Fade(y - y0f);

        const float n00 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash2(x0, y0, m_Seed));
        const float n10 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash2(x1, y0, m_Seed));
        const float n01 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash2(x0, y1, m_Seed));
        const float n11 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash2(x1, y1, m_Seed));

        const float nx0 = Lerp(n00, n10, tx);
        const float nx1 = Lerp(n01, n11, tx);
        return Lerp(nx0, nx1, ty);
    }

    float ValueNoise::Sample3D(float x, float y, float z) const
    {
        x *= m_Frequency;
        y *= m_Frequency;
        z *= m_Frequency;
        const float x0f = std::floor(x);
        const float y0f = std::floor(y);
        const float z0f = std::floor(z);
        const int32_t x0 = static_cast<int32_t>(x0f);
        const int32_t y0 = static_cast<int32_t>(y0f);
        const int32_t z0 = static_cast<int32_t>(z0f);
        const int32_t x1 = x0 + 1;
        const int32_t y1 = y0 + 1;
        const int32_t z1 = z0 + 1;
        const float tx = Fade(x - x0f);
        const float ty = Fade(y - y0f);
        const float tz = Fade(z - z0f);

        const float n000 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x0, y0, z0, m_Seed));
        const float n100 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x1, y0, z0, m_Seed));
        const float n010 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x0, y1, z0, m_Seed));
        const float n110 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x1, y1, z0, m_Seed));
        const float n001 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x0, y0, z1, m_Seed));
        const float n101 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x1, y0, z1, m_Seed));
        const float n011 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x0, y1, z1, m_Seed));
        const float n111 = NoiseHash::ToFloatNeg1To1(NoiseHash::Hash3(x1, y1, z1, m_Seed));

        const float nx00 = Lerp(n000, n100, tx);
        const float nx10 = Lerp(n010, n110, tx);
        const float nx01 = Lerp(n001, n101, tx);
        const float nx11 = Lerp(n011, n111, tx);
        const float nxy0 = Lerp(nx00, nx10, ty);
        const float nxy1 = Lerp(nx01, nx11, ty);
        return Lerp(nxy0, nxy1, tz);
    }
}
