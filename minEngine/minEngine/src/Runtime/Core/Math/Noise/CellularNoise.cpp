#include "CellularNoise.h"

#include "Math/Noise/NoiseHash.h"

#include <algorithm>
#include <cmath>

namespace minEngine::Math::Noise
{
    CellularNoise::CellularNoise(uint32_t seed)
        : m_Seed(seed)
    {
    }

    void CellularNoise::SetSeed(uint32_t seed)
    {
        m_Seed = seed;
    }

    void CellularNoise::SetFrequency(float frequency)
    {
        m_Frequency = (frequency > 0.0f) ? frequency : m_Frequency;
    }

    void CellularNoise::SetJitter(float jitter)
    {
        m_Jitter = std::clamp(jitter, 0.0f, 1.0f);
    }

    float CellularNoise::NormalizeDistance(float distance)
    {
        const float clamped = std::clamp(distance, 0.0f, 1.0f);
        return clamped * 2.0f - 1.0f;
    }

    float CellularNoise::Sample1D(float x) const
    {
        x *= m_Frequency;
        const float x0f = std::floor(x);
        const int32_t xi = static_cast<int32_t>(x0f);
        float minDist = 1.0e9f;

        for (int32_t ox = -1; ox <= 1; ++ox)
        {
            const int32_t cx = xi + ox;
            const uint32_t h = NoiseHash::Hash1(cx, m_Seed);
            const float feature = static_cast<float>(cx) + (NoiseHash::ToFloat01(h) - 0.5f) * m_Jitter;
            const float dx = feature - x;
            minDist = std::min(minDist, std::abs(dx));
        }

        return NormalizeDistance(minDist);
    }

    float CellularNoise::Sample2D(float x, float y) const
    {
        x *= m_Frequency;
        y *= m_Frequency;
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        float minDistSq = 1.0e9f;

        for (int32_t ox = -1; ox <= 1; ++ox)
        {
            for (int32_t oy = -1; oy <= 1; ++oy)
            {
                const int32_t cx = xi + ox;
                const int32_t cy = yi + oy;
                const uint32_t h = NoiseHash::Hash2(cx, cy, m_Seed);
                const float fx = static_cast<float>(cx) + (NoiseHash::ToFloat01(h) - 0.5f) * m_Jitter;
                const float fy = static_cast<float>(cy) + (NoiseHash::ToFloat01(NoiseHash::Mix(h ^ 0x9e3779b9u)) - 0.5f) * m_Jitter;
                const float dx = fx - x;
                const float dy = fy - y;
                minDistSq = std::min(minDistSq, dx * dx + dy * dy);
            }
        }

        return NormalizeDistance(std::sqrt(minDistSq));
    }

    float CellularNoise::Sample3D(float x, float y, float z) const
    {
        x *= m_Frequency;
        y *= m_Frequency;
        z *= m_Frequency;
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        const int32_t zi = static_cast<int32_t>(std::floor(z));
        float minDistSq = 1.0e9f;

        for (int32_t ox = -1; ox <= 1; ++ox)
        {
            for (int32_t oy = -1; oy <= 1; ++oy)
            {
                for (int32_t oz = -1; oz <= 1; ++oz)
                {
                    const int32_t cx = xi + ox;
                    const int32_t cy = yi + oy;
                    const int32_t cz = zi + oz;
                    const uint32_t h0 = NoiseHash::Hash3(cx, cy, cz, m_Seed);
                    const uint32_t h1 = NoiseHash::Mix(h0 ^ 0x85ebca6bu);
                    const uint32_t h2 = NoiseHash::Mix(h0 ^ 0xc2b2ae35u);
                    const float fx = static_cast<float>(cx) + (NoiseHash::ToFloat01(h0) - 0.5f) * m_Jitter;
                    const float fy = static_cast<float>(cy) + (NoiseHash::ToFloat01(h1) - 0.5f) * m_Jitter;
                    const float fz = static_cast<float>(cz) + (NoiseHash::ToFloat01(h2) - 0.5f) * m_Jitter;
                    const float dx = fx - x;
                    const float dy = fy - y;
                    const float dz = fz - z;
                    minDistSq = std::min(minDistSq, dx * dx + dy * dy + dz * dz);
                }
            }
        }

        return NormalizeDistance(std::sqrt(minDistSq));
    }
}