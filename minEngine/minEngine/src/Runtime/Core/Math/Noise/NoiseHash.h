#pragma once

#include "EngineAPI.h"

#include <cstdint>

namespace minEngine::Math::Noise
{
    /** Deterministic lattice hash helpers for Noise implementations. */
    class MINENGINE_API NoiseHash
    {
    public:
        static uint32_t Mix(uint32_t value);
        static uint32_t Hash1(int32_t x, uint32_t seed);
        static uint32_t Hash2(int32_t x, int32_t y, uint32_t seed);
        static uint32_t Hash3(int32_t x, int32_t y, int32_t z, uint32_t seed);

        /** Map hash to [0, 1). */
        static float ToFloat01(uint32_t hash);

        /** Map hash to approximately [-1, 1]. */
        static float ToFloatNeg1To1(uint32_t hash);
    };
}
