#include "NoiseHash.h"

namespace minEngine::Math::Noise
{
    uint32_t NoiseHash::Mix(uint32_t value)
    {
        value ^= value >> 16u;
        value *= 0x7feb352du;
        value ^= value >> 15u;
        value *= 0x846ca68bu;
        value ^= value >> 16u;
        return value;
    }

    uint32_t NoiseHash::Hash1(int32_t x, uint32_t seed)
    {
        return Mix(static_cast<uint32_t>(x) ^ (seed * 0x9e3779b9u));
    }

    uint32_t NoiseHash::Hash2(int32_t x, int32_t y, uint32_t seed)
    {
        uint32_t h = seed;
        h ^= static_cast<uint32_t>(x) * 0x85ebca6bu;
        h = Mix(h);
        h ^= static_cast<uint32_t>(y) * 0xc2b2ae35u;
        return Mix(h);
    }

    uint32_t NoiseHash::Hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
    {
        uint32_t h = seed;
        h ^= static_cast<uint32_t>(x) * 0x85ebca6bu;
        h = Mix(h);
        h ^= static_cast<uint32_t>(y) * 0xc2b2ae35u;
        h = Mix(h);
        h ^= static_cast<uint32_t>(z) * 0x27d4eb2du;
        return Mix(h);
    }

    float NoiseHash::ToFloat01(uint32_t hash)
    {
        return static_cast<float>(hash >> 8u) * (1.0f / 16777216.0f);
    }

    float NoiseHash::ToFloatNeg1To1(uint32_t hash)
    {
        return ToFloat01(hash) * 2.0f - 1.0f;
    }
}
