#include "PerlinNoise.h"

#include "Math/Noise/NoiseHash.h"

#include <cmath>

namespace minEngine::Math::Noise
{
    PerlinNoise::PerlinNoise(uint32_t seed)
        : m_Seed(seed)
    {
    }

    void PerlinNoise::SetSeed(uint32_t seed)
    {
        m_Seed = seed;
    }

    void PerlinNoise::SetFrequency(float frequency)
    {
        m_Frequency = (frequency > 0.0f) ? frequency : m_Frequency;
    }

    float PerlinNoise::Fade(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float PerlinNoise::Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    float PerlinNoise::Grad1(uint32_t hash, float x)
    {
        return ((hash & 1u) == 0u) ? x : -x;
    }

    float PerlinNoise::Grad2(uint32_t hash, float x, float y)
    {
        const uint32_t h = hash & 7u;
        const float u = (h < 4u) ? x : y;
        const float v = (h < 4u) ? y : x;
        return ((h & 1u) == 0u ? u : -u) + ((h & 2u) == 0u ? v : -v);
    }

    float PerlinNoise::Grad3(uint32_t hash, float x, float y, float z)
    {
        const uint32_t h = hash & 15u;
        const float u = (h < 8u) ? x : y;
        const float v = (h < 4u) ? y : ((h == 12u || h == 14u) ? x : z);
        return ((h & 1u) == 0u ? u : -u) + ((h & 2u) == 0u ? v : -v);
    }

    float PerlinNoise::Sample1D(float x) const
    {
        x *= m_Frequency;
        const float x0f = std::floor(x);
        const int32_t x0 = static_cast<int32_t>(x0f);
        const int32_t x1 = x0 + 1;
        const float dx0 = x - x0f;
        const float dx1 = dx0 - 1.0f;
        const float tx = Fade(dx0);

        const float n0 = Grad1(NoiseHash::Hash1(x0, m_Seed), dx0);
        const float n1 = Grad1(NoiseHash::Hash1(x1, m_Seed), dx1);
        // Scale so typical range stays near [-1, 1].
        return Lerp(n0, n1, tx);
    }

    float PerlinNoise::Sample2D(float x, float y) const
    {
        x *= m_Frequency;
        y *= m_Frequency;
        const float x0f = std::floor(x);
        const float y0f = std::floor(y);
        const int32_t x0 = static_cast<int32_t>(x0f);
        const int32_t y0 = static_cast<int32_t>(y0f);
        const int32_t x1 = x0 + 1;
        const int32_t y1 = y0 + 1;
        const float dx0 = x - x0f;
        const float dy0 = y - y0f;
        const float dx1 = dx0 - 1.0f;
        const float dy1 = dy0 - 1.0f;
        const float tx = Fade(dx0);
        const float ty = Fade(dy0);

        const float n00 = Grad2(NoiseHash::Hash2(x0, y0, m_Seed), dx0, dy0);
        const float n10 = Grad2(NoiseHash::Hash2(x1, y0, m_Seed), dx1, dy0);
        const float n01 = Grad2(NoiseHash::Hash2(x0, y1, m_Seed), dx0, dy1);
        const float n11 = Grad2(NoiseHash::Hash2(x1, y1, m_Seed), dx1, dy1);

        const float nx0 = Lerp(n00, n10, tx);
        const float nx1 = Lerp(n01, n11, tx);
        return Lerp(nx0, nx1, ty) * 0.70710678f;
    }

    float PerlinNoise::Sample3D(float x, float y, float z) const
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
        const float dx0 = x - x0f;
        const float dy0 = y - y0f;
        const float dz0 = z - z0f;
        const float dx1 = dx0 - 1.0f;
        const float dy1 = dy0 - 1.0f;
        const float dz1 = dz0 - 1.0f;
        const float tx = Fade(dx0);
        const float ty = Fade(dy0);
        const float tz = Fade(dz0);

        const float n000 = Grad3(NoiseHash::Hash3(x0, y0, z0, m_Seed), dx0, dy0, dz0);
        const float n100 = Grad3(NoiseHash::Hash3(x1, y0, z0, m_Seed), dx1, dy0, dz0);
        const float n010 = Grad3(NoiseHash::Hash3(x0, y1, z0, m_Seed), dx0, dy1, dz0);
        const float n110 = Grad3(NoiseHash::Hash3(x1, y1, z0, m_Seed), dx1, dy1, dz0);
        const float n001 = Grad3(NoiseHash::Hash3(x0, y0, z1, m_Seed), dx0, dy0, dz1);
        const float n101 = Grad3(NoiseHash::Hash3(x1, y0, z1, m_Seed), dx1, dy0, dz1);
        const float n011 = Grad3(NoiseHash::Hash3(x0, y1, z1, m_Seed), dx0, dy1, dz1);
        const float n111 = Grad3(NoiseHash::Hash3(x1, y1, z1, m_Seed), dx1, dy1, dz1);

        const float nx00 = Lerp(n000, n100, tx);
        const float nx10 = Lerp(n010, n110, tx);
        const float nx01 = Lerp(n001, n101, tx);
        const float nx11 = Lerp(n011, n111, tx);
        const float nxy0 = Lerp(nx00, nx10, ty);
        const float nxy1 = Lerp(nx01, nx11, ty);
        return Lerp(nxy0, nxy1, tz) * 0.8660254f;
    }
}
