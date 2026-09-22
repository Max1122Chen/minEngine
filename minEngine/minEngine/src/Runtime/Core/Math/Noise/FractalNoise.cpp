#include "FractalNoise.h"

#include <algorithm>

namespace minEngine::Math::Noise
{
    void FractalNoise::SetSource(std::shared_ptr<Noise> source)
    {
        m_Source = std::move(source);
    }

    void FractalNoise::SetFrequency(float frequency)
    {
        m_Frequency = (frequency > 0.0f) ? frequency : m_Frequency;
    }

    void FractalNoise::SetOctaves(int octaves)
    {
        m_Octaves = std::clamp(octaves, kMinOctaves, kMaxOctaves);
    }

    void FractalNoise::SetLacunarity(float lacunarity)
    {
        m_Lacunarity = lacunarity;
    }

    void FractalNoise::SetGain(float gain)
    {
        m_Gain = gain;
    }

    float FractalNoise::Sample1D(float x) const
    {
        if (!m_Source)
        {
            return 0.0f;
        }

        float sum = 0.0f;
        float amp = 1.0f;
        float freq = m_Frequency;
        float norm = 0.0f;

        for (int i = 0; i < m_Octaves; ++i)
        {
            sum += m_Source->Sample1D(x * freq) * amp;
            norm += amp;
            amp *= m_Gain;
            freq *= m_Lacunarity;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    float FractalNoise::Sample2D(float x, float y) const
    {
        if (!m_Source)
        {
            return 0.0f;
        }

        float sum = 0.0f;
        float amp = 1.0f;
        float freq = m_Frequency;
        float norm = 0.0f;

        for (int i = 0; i < m_Octaves; ++i)
        {
            sum += m_Source->Sample2D(x * freq, y * freq) * amp;
            norm += amp;
            amp *= m_Gain;
            freq *= m_Lacunarity;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }

    float FractalNoise::Sample3D(float x, float y, float z) const
    {
        if (!m_Source)
        {
            return 0.0f;
        }

        float sum = 0.0f;
        float amp = 1.0f;
        float freq = m_Frequency;
        float norm = 0.0f;

        for (int i = 0; i < m_Octaves; ++i)
        {
            sum += m_Source->Sample3D(x * freq, y * freq, z * freq) * amp;
            norm += amp;
            amp *= m_Gain;
            freq *= m_Lacunarity;
        }

        return (norm > 0.0f) ? (sum / norm) : 0.0f;
    }
}