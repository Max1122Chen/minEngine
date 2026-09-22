#pragma once

#include "EngineAPI.h"
#include "Math/Noise/Noise.h"

#include <cstdint>

namespace minEngine::Math::Noise
{
    /** Classic gradient (Perlin-style) noise. Output ≈ [-1, 1]. */
    class MINENGINE_API PerlinNoise final : public Noise
    {
    public:
        PerlinNoise() = default;
        explicit PerlinNoise(uint32_t seed);

        void SetSeed(uint32_t seed);
        uint32_t GetSeed() const { return m_Seed; }

        void SetFrequency(float frequency);
        float GetFrequency() const { return m_Frequency; }

        float Sample1D(float x) const override;
        float Sample2D(float x, float y) const override;
        float Sample3D(float x, float y, float z) const override;

    private:
        static float Fade(float t);
        static float Lerp(float a, float b, float t);
        static float Grad1(uint32_t hash, float x);
        static float Grad2(uint32_t hash, float x, float y);
        static float Grad3(uint32_t hash, float x, float y, float z);

        uint32_t m_Seed = 0u;
        float m_Frequency = 1.0f;
    };
}
