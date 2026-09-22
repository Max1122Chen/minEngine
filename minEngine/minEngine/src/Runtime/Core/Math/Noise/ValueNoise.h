#pragma once

#include "EngineAPI.h"
#include "Math/Noise/Noise.h"

#include <cstdint>

namespace minEngine::Math::Noise
{
    /** Lattice value noise with smooth interpolation. Output ≈ [-1, 1]. */
    class MINENGINE_API ValueNoise final : public Noise
    {
    public:
        ValueNoise() = default;
        explicit ValueNoise(uint32_t seed);

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

        uint32_t m_Seed = 0u;
        float m_Frequency = 1.0f;
    };
}
