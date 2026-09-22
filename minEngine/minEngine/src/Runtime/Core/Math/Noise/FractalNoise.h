#pragma once

#include "EngineAPI.h"
#include "Math/Noise/Noise.h"

#include <memory>

namespace minEngine::Math::Noise
{
    /** FBM compositor over any Noise source. Output ≈ [-1, 1] after amplitude normalization. */
    class MINENGINE_API FractalNoise final : public Noise
    {
    public:
        FractalNoise() = default;

        void SetSource(std::shared_ptr<Noise> source);
        std::shared_ptr<Noise> GetSource() const { return m_Source; }

        void SetFrequency(float frequency);
        float GetFrequency() const { return m_Frequency; }

        void SetOctaves(int octaves);
        int GetOctaves() const { return m_Octaves; }

        void SetLacunarity(float lacunarity);
        float GetLacunarity() const { return m_Lacunarity; }

        void SetGain(float gain);
        float GetGain() const { return m_Gain; }

        float Sample1D(float x) const override;
        float Sample2D(float x, float y) const override;
        float Sample3D(float x, float y, float z) const override;

    private:
        static constexpr int kMinOctaves = 1;
        static constexpr int kMaxOctaves = 16;

        std::shared_ptr<Noise> m_Source;
        float m_Frequency = 1.0f;
        int m_Octaves = 4;
        float m_Lacunarity = 2.0f;
        float m_Gain = 0.5f;
    };
}