#pragma once

#include "EngineAPI.h"
#include "Math/Noise/Noise.h"

#include <cstdint>

namespace minEngine::Math::Noise
{
    /** Worley-like cellular noise. Returns normalized Distance in approx [-1, 1]. */
    class MINENGINE_API CellularNoise final : public Noise
    {
    public:
        CellularNoise() = default;
        explicit CellularNoise(uint32_t seed);

        void SetSeed(uint32_t seed);
        uint32_t GetSeed() const { return m_Seed; }

        void SetFrequency(float frequency);
        float GetFrequency() const { return m_Frequency; }

        void SetJitter(float jitter);
        float GetJitter() const { return m_Jitter; }

        float Sample1D(float x) const override;
        float Sample2D(float x, float y) const override;
        float Sample3D(float x, float y, float z) const override;

    private:
        static float NormalizeDistance(float distance);

        uint32_t m_Seed = 0u;
        float m_Frequency = 1.0f;
        float m_Jitter = 1.0f;
    };
}